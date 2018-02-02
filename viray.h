#ifndef VIRAY__H_
#define VIRAY__H_

#include "Common/typedefs.h"
#include "Utils/mat4.h"
#include "Utils/vec3.h"
#include "Utils/vision.h"

namespace ViRay
{

	struct ShadeRec{
		vec3 localNormal;
		vec3 normal;

		vec3 localHitPoint;
		vec3 hitPoint;

		vec3 orientation;

		myType distanceSqr;

		unsigned char objIdx;

		bool isHit;

		ShadeRec()
		{
			/*
			 * DO NOT TOUCH INITIALIZATION SINCE IT MAY RUIN SHADING
			 * WITH GREAT PROBABILITY
			 */

			localNormal 	= vec3(myType(0.0), myType(-1.0), myType(0.0));
			normal 			= vec3(myType(0.0), myType(-1.0), myType(0.0));

			localHitPoint 	= vec3(myType(0.0));
			hitPoint 		= vec3(myType(0.0));

			orientation		= vec3(myType(0.0));

			distanceSqr 	= myType(MAX_DISTANCE);

			objIdx 			= 0;
			isHit 			= false;
		}
	};

	struct Light{
		vec3 position;
		vec3 dir;
		vec3 color;
		vec3 coeff;					// 0 - cos(outer angle), 1 - inv cone cos(angle) difference, 2 - ?

//		myType innerMinusOuterInv; 	// inv cone cos(angle) difference
	};

	struct Material{
		vec3 k;						// 0 - diffuse, 1 - specular, 2 - specular exp
		vec3 fresnelData;			// 0 - use Fresnel, 1 - eta, 2 - invEtaSqr

		vec3 ambientColor;
		vec3 specularColor;

		// JUST PREPARATION
		vec3 primaryColor, secondaryColor;
		unsigned textureType;
		unsigned textureIdx;
		unsigned textureMapping;

		enum TextureType{
			CONSTANT,
			BITMAP_MASK,
			PIXEL_MAP,
		};

		enum TextureMapping{
			PLANAR,
			CYLINDRICAL,
			SPHERICAL,
		};

		vec3 GetDiffuseColor(const vec3& localHitPoint,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
							 const vec3& orientation,
#endif
							 const myType_union bitmap[MAX_TEXTURE_NUM][TEXT_WIDTH * TEXT_HEIGHT]) const
		{
			//CRITICAL INLINE
#pragma HLS INLINE
			InterpolationData interpolationData;
			myType mix;

			GetTexelCoord(localHitPoint,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
							orientation,
#endif
							interpolationData);

			switch(textureType)
			{
			case CONSTANT:
				return primaryColor;

			case BITMAP_MASK:
#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
				mix = 	bitmap[textureIdx][interpolationData.c1 * TEXT_HEIGHT + interpolationData.r1].fp_num * interpolationData.wc1 * interpolationData.wr1 +
						bitmap[textureIdx][interpolationData.c1 * TEXT_HEIGHT + interpolationData.r2].fp_num * interpolationData.wc1 * interpolationData.wr2 +
						bitmap[textureIdx][interpolationData.c2 * TEXT_HEIGHT + interpolationData.r2].fp_num * interpolationData.wc2 * interpolationData.wr2 +
						bitmap[textureIdx][interpolationData.c2 * TEXT_HEIGHT + interpolationData.r1].fp_num * interpolationData.wc2 * interpolationData.wr1;
#else
				mix =	bitmap[textureIdx][interpolationData.c1 * TEXT_HEIGHT + interpolationData.r1].fp_num;
#endif
				return primaryColor * mix + secondaryColor * (myType(1.0) - mix);
			case PIXEL_MAP:
#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
				return 	ConvertFloatBufferToRGB(bitmap[textureIdx][interpolationData.c1 * TEXT_HEIGHT + interpolationData.r1]) * interpolationData.wc1 * interpolationData.wr1 +
						ConvertFloatBufferToRGB(bitmap[textureIdx][interpolationData.c1 * TEXT_HEIGHT + interpolationData.r2]) * interpolationData.wc1 * interpolationData.wr2 +
						ConvertFloatBufferToRGB(bitmap[textureIdx][interpolationData.c2 * TEXT_HEIGHT + interpolationData.r2]) * interpolationData.wc2 * interpolationData.wr2 +
						ConvertFloatBufferToRGB(bitmap[textureIdx][interpolationData.c2 * TEXT_HEIGHT + interpolationData.r1]) * interpolationData.wc2 * interpolationData.wr1;
#else
				return 	ConvertFloatBufferToRGB(bitmap[textureIdx][interpolationData.c1 * TEXT_HEIGHT + interpolationData.r1]);
#endif
			}

			return vec3(0.0);
		}
	private:
		struct InterpolationData
		{
			unsigned short c1, c2, r1, r2;		// row & column inside bitmap
			myType wc1, wc2, wr1, wr2;			// weight of column and row
		};
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
			myType phi 		= hls::atan2(vx, vz) + PI;
			myType theta 	= hls::acos(vy);
#endif

			myType u, v;

			switch(textureMapping)
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

			myType realColumn = (TEXT_WIDTH  - 1) * u;
			myType realRow    = (TEXT_HEIGHT - 1) * v;

			myType dc, dr;
			myType fc = hls::modf(realColumn, &dc);
			myType fr = hls::modf(realRow,    &dr);

			unsigned short uc = dc;
			unsigned short ur = dr;

#ifdef BILINEAR_TEXTURE_FILTERING_ENABLE
			interpolationData.c1 = uc;
			interpolationData.c2 = uc + 1;
			if (interpolationData.c2 == TEXT_WIDTH) interpolationData.c2 = interpolationData.c1;
			interpolationData.wc1 = myType(1.0) - fc;
			interpolationData.wc2 = fc;

			interpolationData.r1 = ur;
			interpolationData.r2 = ur + 1;
			if (interpolationData.r2 == TEXT_HEIGHT) interpolationData.r2 = interpolationData.r1;
			interpolationData.wr1 = myType(1.0) - fr;
			interpolationData.wr2 = fr;
#else
			interpolationData.c1 = uc;
			interpolationData.r1 = ur;
#endif
		}
		vec3 ConvertFloatBufferToRGB(myType_union buffVal) const
		{
#pragma HLS INLINE
			const myType conversionFactor = myType(0.003921568627451); // = 1.0 / 255.0

			unsigned rawBits = buffVal.raw_bits;
			myType R = ((rawBits & 0xFF000000) >> 24);
			myType G = ((rawBits & 0xFF0000)   >> 16);
			myType B = ((rawBits & 0xFF00)	   >> 8);

			return vec3(R, G, B) * conversionFactor;
		}
	};

	struct SimpleTransform{
		vec3 orientation;

		vec3 scale;
		vec3 invScale;
		vec3 translation;

		vec3 Transform(const vec3& vec) const
		{
			return vec.CompWiseMul(scale) + translation;
		}
		vec3 TransformInv(const vec3& vec) const
		{
			return (vec - translation).CompWiseMul(invScale);
		}

		/*
		 * TODO: THESE ARE THE SAME FUNCTIONS
		 */
		vec3 TransformDir(const vec3& vec) const
		{
			return vec.CompWiseMul(invScale);
		}
		vec3 TransformDirInv(const vec3& vec) const
		{
			return vec.CompWiseMul(invScale);
		}
		vec3 TransposeTransformDir(const vec3& vec) const
		{
			return vec.CompWiseMul(invScale);
		}
	};

	void RenderScene(const CCamera& camera,
			const myType* posShift,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
			const mat4* objTransform,
			const mat4* objTransformInv,
#else
			const SimpleTransform* objTransform, const SimpleTransform* objTransformCopy,
#endif
			const unsigned* objType,

			const Light* lights,
			const Material* materials,

			const myType_union textureData[MAX_TEXTURE_NUM][TEXT_WIDTH * TEXT_HEIGHT],

			pixelColorType* frameBuffer,
			pixelColorType* outColor);

	void InnerLoop(const CCamera& camera,
			const myType* posShift,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
			const mat4* objTransform,
			const mat4* objTransformInv,
#else
			const SimpleTransform* objTransform, const SimpleTransform* objTransformCopy,
#endif
			const unsigned* objType,
			unsigned short h,

			const Light* lights,
			const Material* materials,

			const myType_union textureData[MAX_TEXTURE_NUM][TEXT_WIDTH * TEXT_HEIGHT],

			pixelColorType* frameBuffer);

	void VisibilityTest(const CRay& ray,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
						const mat4* objTransform,
						const mat4* objTransformInv,
#else
						const SimpleTransform* objTransform,
#endif
						const unsigned* objType,
						ShadeRec& closestSr);

	void ShadowVisibilityTest(	const CRay& shadowRay,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
								const mat4* objTransform,
								const mat4* objTransformInv,
#else
								const SimpleTransform* objTransform,
#endif
								const unsigned* objType,
								const ShadeRec& closestSr,
								myType d2,
								ShadeRec& shadowSr);

	vec3 Shade(	const ShadeRec& closestSr,
				const CRay& ray,

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
				const mat4* objTransform,
				const mat4* objTransformInv,
#else
				const SimpleTransform* objTransform,
#endif
				const unsigned* objType,

				const Light* lights,
				const Material* materials,

				const myType_union textureData[MAX_TEXTURE_NUM][TEXT_WIDTH * TEXT_HEIGHT],

				const myType ndir2min);

	myType GetFresnelReflectionCoeff(/*const vec3& rayDirection, const vec3 surfaceNormal,*/ const myType& cosRefl, const myType& relativeEta, const myType& invRelativeEtaSqr);

	void CreatePrimaryRay(const CCamera& camera, const myType* posShift, unsigned short r, unsigned short c, CRay& ray);

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
	void TransformRay(const mat4& mat, const CRay& ray, CRay& transformedRay);
#else
	void TransformRay(const SimpleTransform& transform, const CRay& ray, CRay& transformedRay);
#endif
	myType CubeTest(const CRay& transformedRay, unsigned char& face);
	vec3 GetCubeNormal(const unsigned char& faceIdx);

	void PerformHits(const CRay& transformedRay,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
						const vec3& objOrientation,
#endif
						unsigned objType, ShadeRec& sr);

	void UpdateClosestObject(ShadeRec& current,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
			const mat4& transform,
#else
			const SimpleTransform& transform,
#endif
			const unsigned char& n,
			const CRay& ray,
			ShadeRec& best);
	void UpdateClosestObjectShadow(const ShadeRec& current,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
									const mat4& transform,
#else
									const SimpleTransform& transform,
#endif
									const CRay& shadowRay,
									myType distanceToLightSqr,
									ShadeRec& best);

	void SaveColorToBuffer(vec3 color, pixelColorType& colorOut);

	/*
	 * ViRay Utility Functions
	 */
	namespace ViRayUtils
	{
		myType GeomObjectSolve(const vec3& abc, const CRay& transformedRay, myType& aInv);
		myType NaturalPow(myType valIn, unsigned char n);
		myType Clamp(myType val, myType min = myType(0.0), myType max = myType(1.0));
		myType InvSqrt(myType val);
		myType Sqrt(myType val);
		myType Divide(myType N, myType D);
		myType Abs(myType val);
		void Swap(myType& val1, myType& val2);
	}

}

#endif
