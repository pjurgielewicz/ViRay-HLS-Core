#ifndef VIRAY__H_
#define VIRAY__H_

#include "Common/typedefs.h"
#include "Utils/mat4.h"
#include "Utils/vec3.h"
#include "Utils/vision.h"

namespace ViRay
{

struct ShadeRec{
	vec3 normal;
	vec3 localHitPoint;
	vec3 hitPoint;

	myType distance;

	int objIdx;

	ShadeRec()
	{
		normal = vec3();
		localHitPoint = vec3();
		hitPoint = vec3();
		distance = myType(MAX_DISTANCE);
		objIdx = -1;
	}
};

struct Light{
	vec3 position;
	vec3 color;
};

struct Material{
	vec3 k;
	
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
		int h,

		const Light* lights,
		const Material* materials,

		pixelColorType* frameBuffer);

void CreateRay(const CCamera& camera, const myType* posShift, unsigned r, unsigned c, CRay& ray);

void AssignMatrix(const mat4* mats, mat4& mat, unsigned pos);

void TransformRay(const mat4& mat, const CRay& ray, CRay& transformedRay);

myType SphereTest(const CRay& transformedRay);
myType PlaneTest(const CRay& transformedRay);

void PerformHits(const CRay& transformedRay, unsigned objType, ShadeRec& sr);
void PerformShadowHits(const CRay& transformedRay, unsigned objType, ShadeRec& sr);

void UpdateClosestObject(const ShadeRec& current, int n, ShadeRec& best);
void UpdateClosestObjectShadow(const ShadeRec& current, const mat4& transform, int n, const CRay shadowRay, myType distanceToLightSqr, ShadeRec& best);

void SaveColorToBuffer(vec3 color, pixelColorType& colorOut);
myType NaturalPow(myType valIn, unsigned n);

}

#endif