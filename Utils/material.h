#ifndef MATERIAL__H_
#define MATERIAL__H_

#include "vec3.h"

namespace ViRay {
	namespace ViRayUtils {
		myType Acos(myType x);
		myType Atan2(myType y, myType x);
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
		CYLINDRICAL,
		SPHERICAL,
	};

private:
	struct MaterialDescription
	{
		vec3 k;						// 0 - diffuse, 1 - specular, 2 - specular exp
		vec3 fresnelData;			// 0 - use Fresnel, 1 - eta, 2 - invEtaSqr

		vec3 ambientColor;
		vec3 primaryColor, secondaryColor;
		vec3 specularColor;
	};

	struct TextureDescription
	{
		struct InterpolationData
		{
			unsigned short c1, c2, r1, r2;		// row & column inside bitmap
			myType wc1, wc2, wr1, wr2;			// weight of column and row
		};


		vec3 texturePos, textureScale;

		unsigned      baseAddr;
//		unsigned char textureIdx;
		unsigned char textureType;
		unsigned char textureMapping;
		unsigned short textureWidth, textureHeight;

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
//			textureIdx  	= (textureDescriptionCode >> 26) & 0x3F;
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
			texturePos		= pos;
			textureScale	= scale;
		}

		/*
		 * Calculate right position to source pixel data from
		 * including pixel interpolation data
		 *
		 * If ADVANCED_TEXTURE_MAPPING_ENABLE is specified allow to use SPHERICAL and CYLINDRICAL mapping to [u, v] coordinates
		 */
		void GetTexelCoord(const vec3& localHitPoint,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
									const vec3& orientation,
#endif
									InterpolationData& interpolationData) const
		{
		#pragma HLS INLINE

#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
			myType vx, vy, vz;
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

			myType dummy;
			vx = hls::modf(vx, &dummy);
			vy = hls::modf(vy, &dummy);
			vz = hls::modf(vz, &dummy);

#ifdef ADVANCED_TEXTURE_MAPPING_ENABLE
			myType phi 		= ViRayUtils::Atan2(vx, vz) + PI;
			myType theta	= ViRayUtils::Acos(vy);
#endif

			myType u, v;

			switch (textureMapping)
			{
#ifdef ADVANCED_TEXTURE_MAPPING_ENABLE
			case SPHERICAL:
				u = phi * INV_TWOPI;
				v = myType(1.0) - theta  * INV_PI;
				break;
			case CYLINDRICAL:
				u = phi * INV_TWOPI;
				v = (vy + myType(1.0)) * myType(0.5);
				break;
#endif
			default: // RECTANGULAR
				u = (vx + myType(1.0)) * myType(0.5);
				v = (vz + myType(1.0)) * myType(0.5);
				break;
			}

			// TRANSFORM TEXTURE & FIND MAJOR PIXEL
			myType realColumn 	= (textureWidth  - 1) * (u + texturePos[0]) * textureScale[0];
			myType realRow 		= (textureHeight - 1) * (v + texturePos[1]) * textureScale[1];

			myType dc, dr;
			myType fc = hls::modf(realColumn, &dc);
			myType fr = hls::modf(realRow, 	  &dr);

			unsigned short uc = (unsigned short)(dc) % textureWidth;
			unsigned short ur = (unsigned short)(dr) % textureHeight;

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
		vec3 ConvertFloatBufferToRGB(float_union buffVal) const
		{
		#pragma HLS INLINE
			const myType conversionFactor = myType(0.003921568627451); // = 1.0 / 255.0

			unsigned rawBits = buffVal.raw_bits;
			myType R = ((rawBits & 0xFF0000) >> 16);
			myType G = ((rawBits & 0xFF00) >> 8);
			myType B = ((rawBits & 0xFF));

			return vec3(R, G, B) * conversionFactor;
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
	 * to reduce texture sharpness while introducing blur (which amount depends on the texture size)
	 */
	vec3 GetDiffuseColor(const vec3& localHitPoint,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
							const vec3& orientation,
#endif
							const float_union* bitmap) const
	{
		//CRITICAL INLINE
#pragma HLS INLINE
		TextureDescription::InterpolationData interpolationData;
		myType mix;

		textureDesc.GetTexelCoord(localHitPoint,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
									orientation,
#endif
									interpolationData);

		switch (textureDesc.textureType)
		{
		case CONSTANT:
			return materialDesc.primaryColor;

		case BITMAP_MASK:
#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
			mix = 	bitmap[textureDesc.baseAddr + interpolationData.c1 * textureDesc.textureHeight + interpolationData.r1].fp_num * interpolationData.wc1 * interpolationData.wr1 +
					bitmap[textureDesc.baseAddr + interpolationData.c1 * textureDesc.textureHeight + interpolationData.r2].fp_num * interpolationData.wc1 * interpolationData.wr2 +
					bitmap[textureDesc.baseAddr + interpolationData.c2 * textureDesc.textureHeight + interpolationData.r2].fp_num * interpolationData.wc2 * interpolationData.wr2 +
					bitmap[textureDesc.baseAddr + interpolationData.c2 * textureDesc.textureHeight + interpolationData.r1].fp_num * interpolationData.wc2 * interpolationData.wr1;
#else
			mix = 	bitmap[textureDesc.baseAddr + interpolationData.c1 * textureDesc.textureHeight + interpolationData.r1].fp_num;
#endif
			return materialDesc.primaryColor * mix + materialDesc.secondaryColor * (myType(1.0) - mix);
		case PIXEL_MAP:
#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
			return 	textureDesc.ConvertFloatBufferToRGB(bitmap[textureDesc.baseAddr + interpolationData.c1 * textureDesc.textureHeight + interpolationData.r1]) * interpolationData.wc1 * interpolationData.wr1 +
					textureDesc.ConvertFloatBufferToRGB(bitmap[textureDesc.baseAddr + interpolationData.c1 * textureDesc.textureHeight + interpolationData.r2]) * interpolationData.wc1 * interpolationData.wr2 +
					textureDesc.ConvertFloatBufferToRGB(bitmap[textureDesc.baseAddr + interpolationData.c2 * textureDesc.textureHeight + interpolationData.r2]) * interpolationData.wc2 * interpolationData.wr2 +
					textureDesc.ConvertFloatBufferToRGB(bitmap[textureDesc.baseAddr + interpolationData.c2 * textureDesc.textureHeight + interpolationData.r1]) * interpolationData.wc2 * interpolationData.wr1;
#else
			return 	TextureDescription::ConvertFloatBufferToRGB(bitmap[textureDesc.baseAddr + interpolationData.c1 * textureDesc.textureHeight + interpolationData.r1]);
#endif
		}

		return vec3(1.0, 1.0, 0.0);
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

						m.textureDesc.texturePos << "\n" <<
						m.textureDesc.textureScale << "\n" <<
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
