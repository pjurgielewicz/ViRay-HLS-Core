#ifndef FFCORE__H_
#define FFCORE__H_

#include "Common/typedefs.h"
#include "Utils/mat4.h"
#include "Utils/vec3.h"
#include "Utils/vision.h"

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

int FFCore(const mat4* objTransformIn,
			const mat4* objInvTransformIn,
			const int* objTypeIn,

			vec3* lightPositionIn,
			vec3* lightColorIn,

			vec3* materialCoeffIn,
			vec3* materialColorsIn,

			vec3* cameraDataIn,
			myType cameraZoom,

			pixelColorType* outColor);

#endif
