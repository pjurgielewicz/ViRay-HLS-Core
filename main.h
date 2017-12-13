#ifndef FFCORE__H_
#define FFCORE__H_

#include "Common/typedefs.h"
#include "Utils/vec4.h"
#include "Utils/vision.h"

#define OBJ_NUM 10
#define WIDTH 1000
#define HEIGHT 1000
#define FRAME_BUFFER_SIZE (HEIGHT)

struct ShadeRec{
	vec4 normal;
	vec4 localHitPoint;
	vec4 hitPoint;

	myType distance;

	int objIdx;

	ShadeRec()
	{
		normal = vec4();
		localHitPoint = vec4();
		hitPoint = vec4();
		distance = 1000;
		objIdx = -1;
	}
};

int FFCore(const vec4* objTransformIn,
			const vec4* objInvTransformIn,
			unsigned dataInSize,
			const int* objTypeIn,
			pixelColorType* outColor);

#endif
