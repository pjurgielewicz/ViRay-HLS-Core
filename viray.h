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

		myType localDistance;
		myType distanceSqr;

		unsigned char objIdx;

		bool isHit;

		ShadeRec()
		{
			localNormal = vec3();
			normal = vec3(myType(0.0), myType(-1.0), myType(0.0));

			localHitPoint = vec3();
			hitPoint = vec3();

			localDistance = myType(MAX_DISTANCE);
			distanceSqr = myType(MAX_DISTANCE);

			objIdx = 0;
			isHit = false;
		}
	};

	struct Light{
		vec3 position;
		vec3 dir;
		vec3 color;
		vec3 coeff;					// 0 - cos(outer angle), 1 - cos(inner angle), 2 - ?

		myType innerMinusOuterInv; 	// inv cone cos(angle) difference
	};

	struct Material{
		vec3 k;						// 0 - diffuse, 1 - specular, 2 - specular exp
		myType eta, invEtaSqr;

		vec3 ambientColor;
		vec3 diffuseColor;
		vec3 specularColor;
	};

	void RenderScene(const CCamera& camera,
			const myType* posShift,
			const mat4* objTransform,
			const mat4* objTransformInv,
			const unsigned* objType,

			const Light* lights,
			const Material* materials,

			pixelColorType* frameBuffer,
			pixelColorType* outColor);

	void InnerLoop(const CCamera& camera,
			const myType* posShift,
			const mat4* objTransform,
			const mat4* objTransformInv,
			const unsigned* objType,
			unsigned short h,

			const Light* lights,
			const Material* materials,

			pixelColorType* frameBuffer);

	void VisibilityTest(const CRay& ray,
						const mat4* objTransform,
						const mat4* objTransformInv,
						const unsigned* objType,
						ShadeRec& closestSr);

	void ShadowVisibilityTest(	const CRay& shadowRay,
								const mat4* objTransform,
								const mat4* objTransformInv,
								const unsigned* objType,
								const ShadeRec& closestSr,
								myType d2,
								ShadeRec& shadowSr);

	vec3 Shade(	const ShadeRec& closestSr,
				const CRay& ray,

				const mat4* objTransform,
				const mat4* objTransformInv,
				const unsigned* objType,

				const Light* lights,
				const Material* materials);

	myType GetFresnelReflectionCoeff(const vec3& rayDirection, const vec3 surfaceNormal, const myType& relativeEta, const myType& invRelativeEtaSqr);

	void CreateRay(const CCamera& camera, const myType* posShift, unsigned short r, unsigned short c, CRay& ray);

	void TransformRay(const mat4& mat, const CRay& ray, CRay& transformedRay);

	myType SphereTest(const CRay& transformedRay, const CRay& transformedRayReal);
	myType PlaneTest(const CRay& transformedRay);
	myType CubeTest(const CRay& transformedRay, unsigned char& face);
	vec3 GetCubeNormal(const unsigned char& faceIdx);

	void PerformHits(const CRay& transformedRay, unsigned objType, ShadeRec& sr);

	void UpdateClosestObject(ShadeRec& current, const mat4& transform, const unsigned char& n, const CRay& ray, ShadeRec& best);
	void UpdateClosestObjectShadow(const ShadeRec& current, const mat4& transform, const CRay& shadowRay, myType distanceToLightSqr, ShadeRec& best);

	void SaveColorToBuffer(vec3 color, pixelColorType& colorOut);

	/*
	 * ViRay Utility Functions
	 */
	namespace ViRayUtils
	{

		myType NaturalPow(myType valIn, unsigned char n);
		myType Clamp(myType val, myType min = myType(0.0), myType max = myType(1.0));
		myType InvSqrt(myType val);
		myType Sqrt(myType val);
		myType Divide(myType N, myType D);
	}

}

#endif
