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

//		myType localDistance;
		myType distanceSqr;

		unsigned char objIdx;

		bool isHit;

		ShadeRec()
		{
			/*
			 * DO NOT TOUCH INITIALIZATION SINCE IT MAY RUIN SHADING
			 * WITH GREAT PROBABILITY
			 */

			localNormal = vec3(myType(0.0), myType(-1.0), myType(0.0));
			normal = vec3(myType(0.0), myType(-1.0), myType(0.0));

			localHitPoint = vec3(myType(0.0));
			hitPoint = vec3(myType(0.0));

//			localDistance = myType(MAX_DISTANCE);
			distanceSqr = myType(MAX_DISTANCE);

			objIdx = 0;
			isHit = false;
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
		vec3 diffuseColor;
		vec3 specularColor;
	};

	struct SimpleTransform{
		vec3 orientation;

		vec3 scale;
		vec3 invScale;				// ???
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
			const SimpleTransform* objTransform,
#endif
			const unsigned* objType,

			const Light* lights,
			const Material* materials,

			pixelColorType* frameBuffer,
			pixelColorType* outColor);

	void InnerLoop(const CCamera& camera,
			const myType* posShift,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
			const mat4* objTransform,
			const mat4* objTransformInv,
#else
			const SimpleTransform* objTransform,
#endif
			const unsigned* objType,
			unsigned short h,

			const Light* lights,
			const Material* materials,

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
				const Material* materials);

	myType GetFresnelReflectionCoeff(const vec3& rayDirection, const vec3 surfaceNormal, const myType& relativeEta, const myType& invRelativeEtaSqr);

	void CreateRay(const CCamera& camera, const myType* posShift, unsigned short r, unsigned short c, CRay& ray);

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
	void TransformRay(const mat4& mat, const CRay& ray, CRay& transformedRay);
#else
	void TransformRay(const SimpleTransform& transform, const CRay& ray, CRay& transformedRay, bool isInverse = true);
#endif

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
	myType PlaneTest(const CRay& transformedRay);
#else
	myType PlaneTest(const CRay& transformedRay, const vec3& orientation);
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
		myType QuadraticObjectSolve(const vec3& abc, const CRay& transformedRay);
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
