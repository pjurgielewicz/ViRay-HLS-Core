#ifndef MATERIAL__H_
#define MATERIAL__H_

#include "vec3.h"

namespace ViRay {
	namespace ViRayUtils {
		myTypeNC Acos(myTypeNC x);
		myTypeNC Atan2(myTypeNC y, myTypeNC x);
	};
};

namespace ViRay {

class CMaterial {

public:
	enum TextureType {
		CONSTANT = 0,
		BITMAP_MASK,
		PIXEL_MAP,
	};

	enum TextureMapping {
		PLANAR = 0,
		PLANE_PLANAR,
		CYLINDRICAL,
		SPHERICAL,
	};

private:
	struct MaterialDescription
	{
		vec3 k;						// 0 - diffuse, 1 - specular, 2 - specular exp
		myType ks, specExp;
		bool useFresnelReflection;
		bool useTorranceSparrowSpecularReflection;
		bool isConductor;

		vec3 fresnelData;			// 0 - use Fresnel, 1 - eta, 2 - invEtaSqr / absorption coeff

		vec3 ambientColor;
		vec3 primaryColor, secondaryColor;
		vec3 specularColor;

		enum MaterialInfo
		{
			FRESNEL_REFLECTION 			= 1,
			TORRANCE_SPARROW_SPECULAR 	= 2,
			FRESNEL_CONDUCTOR			= 4,
		};
	};

	struct TextureDescription
	{
		struct InterpolationData
		{
			unsigned short c1, c2, r1, r2;		// row & column inside bitmap
			myTypeNC wc1, wc2, wr1, wr2;			// weight of column and row

			unsigned short idx11, idx12, idx22, idx21;
			myTypeNC w11, w12, w22, w21;
		};

		unsigned      baseAddr;
		unsigned char textureType;
		unsigned char textureMapping;
		unsigned short textureWidth, textureHeight;

		myType 			s, t;
		myType			ss, st;

		/*
		 * THE WIDTH OF EACH ELEMENT IS BIGGER THAN NECESSARY FOR THE CURRENT IMPLEMENTATION
		 * LEAVING THE ROOM FOR FUTURE UPGRADES
		 *
		 * MSB
		 * |
		 * 6b  : < none >
		 * 3b  : textureType
		 * 3b  : textureMapping
		 * 10b : textureWidth
		 * 10b : textureHeight
		 * |
		 * LSB
		 */
		TextureDescription() {}	// dummy because of HLS requirements
		TextureDescription( unsigned textureDescriptionCode,
							unsigned baseAddr,
							const vec3& pos, const vec3& scale)
		{
			this->baseAddr	= baseAddr;
#ifdef TEXTURE_ENABLE
			textureType		= (textureDescriptionCode >> 23) & 0x7;
#else
			textureType		= ViRay::CMaterial::CONSTANT;
#endif

#ifdef ADVANCED_TEXTURE_MAPPING_ENABLE
			textureMapping 	= (textureDescriptionCode >> 20) & 0x7;
#else
			textureMapping 	= ViRay::CMaterial::PLANAR;
#endif

			textureWidth	= (textureDescriptionCode >> 10) & 0x3FF;
			textureHeight	= (textureDescriptionCode      ) & 0x3FF;

			// TEXTURE TRANSFORM
			s 				= pos[0];		// s := x
			t				= pos[1];		// t := y

			ss				= scale[0];		// ss := sx
			st				= scale[1];		// st := sy
		}

		/*
		 * Calculate right position to source pixel data from
		 * including pixel interpolation data
		 *
		 * If ADVANCED_TEXTURE_MAPPING_ENABLE is specified allow to use SPHERICAL and CYLINDRICAL mapping to [u, v] coordinates
		 */
		InterpolationData GetTexelCoord(const vec3& localHitPoint
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
									,const vec3& orientation
#endif
									) const
		{
//		#pragma HLS INLINE
#pragma HLS PIPELINE II=4
		myType vx, vy, vz;
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
			if (orientation[0] != myType(0.0))
			{
				vx = localHitPoint[2];
				vy = localHitPoint[0];
				vz = -localHitPoint[1];
			}
			else if (orientation[1] != myType(0.0))
			{
				vx = localHitPoint[0];
				vy = localHitPoint[1];
				vz = localHitPoint[2];
			}
			else
			{
				vx = localHitPoint[0];
				vy = localHitPoint[2];
				vz = localHitPoint[1];
			}
#else
			vx = localHitPoint[0];
			vy = localHitPoint[1];
			vz = localHitPoint[2];
#endif

//			myType dummy;
//			vx = hls::modf(vx, &dummy);
//			vx -= (dummy > 0) ? 0.01 : -0.01;
//			vy = hls::modf(vy, &dummy);
//			vz = hls::modf(vz, &dummy);

#ifdef ADVANCED_TEXTURE_MAPPING_ENABLE
			myType phi 		= ViRayUtils::Atan2(vx, vz) + myType(PI);
			myType theta		= ViRayUtils::Acos(vy);
#endif

			myType u, v;

			switch (textureMapping)
			{
#ifdef ADVANCED_TEXTURE_MAPPING_ENABLE
			case SPHERICAL:
				u = phi * myType(INV_TWOPI);
				v = myType(1.0) - theta  * myType(INV_PI);
				break;
			case CYLINDRICAL:
				u = phi * myType(INV_TWOPI);
				v = (vy + myType(1.0)) * myType(0.5);
				break;
#endif
			case PLANE_PLANAR:
				u = vx;
				v = vz;
				break;
			default: // RECTANGULAR
				u = (vx + myType(1.0)) * myType(0.5);
				v = (vz + myType(1.0)) * myType(0.5);
				break;
			}

			// TRANSFORM TEXTURE & FIND MAJOR PIXEL
			myType realColumn 	= (textureWidth  ) * (u + this->s) * this->ss; // hls::fabs ??
			myType realRow 		= (textureHeight ) * (v + this->t) * this->st; // hls::fabs ??

//			realColumn = hls::fabs(realColumn);
//			realRow    = hls::fabs(realRow);

			myType dc, dr;
#ifndef UC_OPERATION
#ifdef USE_FIXEDPOINT
			myType fc = realColumn 	& LOW_BITS;
			myType fr = realRow 	& LOW_BITS;
			dc 		  = realColumn 	& HIGH_BITS;
			dr 		  = realRow 	& HIGH_BITS;
#else
			myType fc = hls::modf(realColumn, &dc);
			myType fr = hls::modf(realRow, 	  &dr);
#endif
#else
			myType fc = std::modf(realColumn, &dc);
			myType fr = std::modf(realRow, 	  &dr);
#endif

			if (fc <= myType(0.0)) fc += myType(1.0);
			if (fr <= myType(0.0)) fr += myType(1.0);

//			dc = hls::fabs(dc);
//			dr = hls::fabs(dr);

			/*
			 * BUG IN THE REAL SYSTEM:
			 * HLS SEEMS TO PERFORM hls::fabs() ON dc & dr
			 * BEFORE CASTING TO USHORT THUS CREATING ARTIFACTS NOT SEEN IN THE TESTBENCH
			 * SOLUTION(?) PROVIDED BELOW
			 */
//			unsigned short uc = (unsigned short)(dc) % textureWidth;
//			unsigned short ur = (unsigned short)(dr) % textureHeight;

			InterpolationData interpolationData;

			/*
			 * IF THIS IS BETTER THAN PREVIOUS IMPLEMENTATION HAS TO BE CHECKED
			 * IN THE REAL SYSTEM...
			 */
			unsigned short uc;
			unsigned short ur;
			if (dc < myType(0.0))
			{
				uc = textureWidth - (unsigned short)(-dc) % textureWidth;
				ur = textureHeight - (unsigned short)(-dr) % textureHeight;
			}
			else
			{
				uc = (unsigned short)(dc) % textureWidth;
				ur = (unsigned short)(dr) % textureHeight;
			}
			/*
			 * ALTERNATIVE SOLUTION
			 * WARNING: IT IS NOT PRODUCING VALID RESULTS IN TESTBENCH
			 */
//			uc = (unsigned short)(dc) % textureWidth;
//			uc = (dc < myType(0.0)) ? textureWidth - uc : uc;
//			ur = (unsigned short)(dr) % textureHeight;
//			ur = (dr < myType(0.0)) ? textureWidth - ur : ur;

#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
			/*			interpolationData.c1 = uc;
			interpolationData.c2 = uc + 1;
			if (interpolationData.c2 == textureWidth) interpolationData.c2 = interpolationData.c1;
			interpolationData.wc1 = myType(1.0) - fc;
			interpolationData.wc2 = fc;

			interpolationData.r1 = ur;
			interpolationData.r2 = ur + 1;
			if (interpolationData.r2 == textureHeight) interpolationData.r2 = interpolationData.r1;
			interpolationData.wr1 = myType(1.0) - fr;
			interpolationData.wr2 = fr;*/

			/*
			* 2nd version: pixel hit center is in [0.5, 0.5]
			*/
			// HORIZONTAL FILTER
			interpolationData.c1 = uc;
			if (fc < myType(0.5))
			{
				interpolationData.c2 = uc - 1;
				if (interpolationData.c1 == 0) interpolationData.c2 = textureWidth - 1;
				interpolationData.wc1 = myType(0.5) + fc;
				interpolationData.wc2 = myType(0.5) - fc;
			}
			else
			{
				interpolationData.c2 = uc + 1;
				if (interpolationData.c2 == textureWidth) interpolationData.c2 = 0;
				interpolationData.wc1 = myType(1.5) - fc;
				interpolationData.wc2 = myType(-0.5) + fc;
			}

			// VERTICAL FILTER
			interpolationData.r1 = ur;
			if (fr < myType(0.5))
			{
				interpolationData.r2 = ur - 1;
				if (interpolationData.r1 == 0) interpolationData.r2 = textureHeight - 1;
				interpolationData.wr1 = myType(0.5) + fr;
				interpolationData.wr2 = myType(0.5) - fr;
			}
			else
			{
				interpolationData.r2 = ur + 1;
				if (interpolationData.r2 == textureHeight) interpolationData.r2 = 0;
				interpolationData.wr1 = myType(1.5) - fr;
				interpolationData.wr2 = myType(-0.5) + fr;
			}

#else
			interpolationData.c1 = uc;
			interpolationData.r1 = ur;
#endif



			interpolationData.idx11 = baseAddr + interpolationData.c1 * textureHeight + interpolationData.r1;
	#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
			interpolationData.idx12 = baseAddr + interpolationData.c1 * textureHeight + interpolationData.r2;
			interpolationData.idx22 = baseAddr + interpolationData.c2 * textureHeight + interpolationData.r2;
			interpolationData.idx21 = baseAddr + interpolationData.c2 * textureHeight + interpolationData.r1;

			interpolationData.w11 = interpolationData.wc1 * interpolationData.wr1;
			interpolationData.w12 = interpolationData.wc1 * interpolationData.wr2;
			interpolationData.w22 = interpolationData.wc2 * interpolationData.wr2;
			interpolationData.w21 = interpolationData.wc2 * interpolationData.wr1;
	#endif

			return interpolationData;
		}

		/*
		 * Extract RGB vec3 from the lower 24 bits of the texture buffer value
		 * and transform them to the 0...1 myType range
		 *
		 * MSB
		 * |
		 * 8b : < not used >
		 * 8b : R
		 * 8b : G
		 * 8b : B
		 * |
		 * LSB
		 */
		static vec3NC ConvertFloatBufferToRGB888(float_union buffVal)
		{
		#pragma HLS INLINE

			unsigned rawBits = buffVal.raw_bits;
			myTypeNC R = myTypeNC((rawBits & 0xFF0000) >> 16);
			myTypeNC G = myTypeNC((rawBits & 0xFF00) >> 8);
			myTypeNC B = myTypeNC((rawBits & 0xFF));

			return vec3NC(R, G, B);
		}
	};

public:

	//////////////////////////////////

	// TODO: fill descriptions...
	CMaterial() {} // dummy because of HLS requirements
	CMaterial(	const vec3& k, const vec3& fresnelData, const vec3& ambientColor, const vec3& primaryColor, const vec3& secondaryColor, const vec3& specularColor,
				unsigned textureDescriptionCode, unsigned textureBaseAddr, const vec3& texturePos, const vec3& textureScale) :
				textureDesc(textureDescriptionCode, textureBaseAddr, texturePos, textureScale)
	{
		materialDesc.k 				= k;
		materialDesc.fresnelData 	= fresnelData;


		materialDesc.ambientColor 	= ambientColor;
		materialDesc.primaryColor 	= primaryColor;
		materialDesc.secondaryColor = secondaryColor;
		materialDesc.specularColor 	= specularColor;

		//////////////////////////////////////////////////
#ifndef UC_OPERATION
#ifdef USE_FIXEDPOINT
		materialDesc.ks				= k[2] & LOW_BITS;
		materialDesc.specExp		= k[2] & HIGH_BITS;
#else
		materialDesc.ks				= hls::modf(k[2], &materialDesc.specExp);
#endif
#else
		materialDesc.ks				= std::modf(k[2], &materialDesc.specExp);
#endif

		float_union materialInfo;
		materialInfo.fp_num			= fresnelData[0];

		materialDesc.useFresnelReflection 					= materialInfo.raw_bits & MaterialDescription::FRESNEL_REFLECTION;
		materialDesc.useTorranceSparrowSpecularReflection 	= materialInfo.raw_bits & MaterialDescription::TORRANCE_SPARROW_SPECULAR;
		materialDesc.isConductor							= materialInfo.raw_bits & MaterialDescription::FRESNEL_CONDUCTOR;
	}

	/*
	 * Depending on the material's diffuse texture and hit position in local coordinates
	 * choose color to paint on the surface.
	 *
	 * CONSTANT: fill object with a single color (primary color),
	 * BITMAP_MASK: use floating point texture as a mask to mix between primary and secondary color,
	 * PIXEL_MAP: map texture RGB pixels onto the object.
	 *
	 * If BILINEAR_TEXTURE_FILTERING_ENABLE is specified use texture cross-point filtering (bilinear)
	 * to reduce texture sharpness while introducing blur (which amount depends on the texture size and magnification)
	 */
	vec3 GetDiffuseColor(const vec3& localHitPoint,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
							const vec3& orientation,
#endif
							const float_union* bitmap) const
	{
		//CRITICAL INLINE - THE PROBLEM IS POSED BY ACCESSES TO ARRAYS
		//ALL OF THEM HAVE TO BE INSIDE INLINED PART OF THE FUNCTION
#pragma HLS INLINE
//#pragma HLS PIPELINE II=4

		TextureDescription::InterpolationData interpolationData(textureDesc.GetTexelCoord(localHitPoint
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
																,orientation
#endif
																));

		float_union texelColor11 = bitmap[interpolationData.idx11];
#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
		float_union texelColor12 = bitmap[interpolationData.idx12];
		float_union texelColor22 = bitmap[interpolationData.idx22];
		float_union texelColor21 = bitmap[interpolationData.idx21];
#endif

		vec3 interpolationResult = InterpolateTexture(interpolationData, texelColor11,
#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
														texelColor12, texelColor22, texelColor21
#endif
				);

		// BE THAT WAY...
		switch (textureDesc.textureType)
		{
		case CONSTANT:
			interpolationResult = materialDesc.primaryColor;
			break;
		case BITMAP_MASK:
			interpolationResult = materialDesc.primaryColor * interpolationResult[0] + materialDesc.secondaryColor * interpolationResult[1];
			break;
		}
		return interpolationResult;
	}

	vec3 InterpolateTexture(const TextureDescription::InterpolationData& interpolationData,
							float_union texelColor11,
#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
							float_union texelColor12, float_union texelColor22, float_union texelColor21
#endif
							) const
	{
//#pragma HLS INLINE
#pragma HLS PIPELINE II=4
		const myTypeNC conversionFactor = myTypeNC(0.003921568627451); // = 1.0 / 255.0
		myTypeNC mix;
		vec3NC tmp;
		switch (textureDesc.textureType)
		{
		case CONSTANT:
			break;
		case BITMAP_MASK:
#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
			mix = 	(myTypeNC)texelColor11.fp_num * interpolationData.w11 +
					(myTypeNC)texelColor12.fp_num * interpolationData.w12 +
					(myTypeNC)texelColor22.fp_num * interpolationData.w22 +
					(myTypeNC)texelColor21.fp_num * interpolationData.w21;
#else
			mix = 	(myTypeNC)texelColor11.fp_num;
#endif
			return vec3(mix, (myTypeNC(1.0) - mix), myType(0.0));
		case PIXEL_MAP:
#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
			tmp =  	(TextureDescription::ConvertFloatBufferToRGB888(texelColor11) * interpolationData.w11  +
					 TextureDescription::ConvertFloatBufferToRGB888(texelColor12) * interpolationData.w12  +
					 TextureDescription::ConvertFloatBufferToRGB888(texelColor22) * interpolationData.w22  +
					 TextureDescription::ConvertFloatBufferToRGB888(texelColor21) * interpolationData.w21) * conversionFactor;
#else
			tmp 	TextureDescription::ConvertFloatBufferToRGB888(texelColor11) * conversionFactor;
#endif
			return vec3(tmp[0], tmp[1], tmp[2]);
		}

		return vec3(myType(1.0), myType(1.0), myType(0.0));
	}

	/*
	 * Simple getters
	 */
	const MaterialDescription& GetMaterialDescription() const { return materialDesc; }
	const TextureDescription& GetTextureDescription() const { return textureDesc; }

#ifndef UC_OPERATION
	/*
	 * Flush material content to the std::cout stream
	 */
	friend std::ostream& operator<<(std::ostream& cout, const CMaterial& m)
	{
		return cout << 	m.materialDesc.k << "\n" <<
						m.materialDesc.fresnelData << "\n" <<
						m.materialDesc.ambientColor << "\n" <<
						m.materialDesc.primaryColor << "\n" <<
						m.materialDesc.secondaryColor << "\n" <<
						m.materialDesc.specularColor << "\n\n" <<

						m.textureDesc.s << " " << m.textureDesc.t << "\n" <<
						m.textureDesc.ss << " " << m.textureDesc.st << "\n" <<
						(unsigned)m.textureDesc.baseAddr << " " << (unsigned)m.textureDesc.textureType << " " << (unsigned)m.textureDesc.textureMapping << " " << m.textureDesc.textureWidth << " " << m.textureDesc.textureHeight << "\n";
	}
#endif

private:
	//////////////////////////////////

	MaterialDescription materialDesc;
	TextureDescription textureDesc;
};

}

#endif
