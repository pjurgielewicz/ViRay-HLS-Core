#ifndef FFCORE__H_
#define FFCORE__H_

#include "viray.h"

int ViRayMain(const mat4* objTransformIn,
			const mat4* objInvTransformIn,
			const int* objTypeIn,

			vec3* lightPositionIn,
			vec3* lightDirIn,
			vec3* lightColorIn,
			vec3* lightCoeffIn,

			vec3* materialCoeffIn,
			vec3* materialColorsIn,

			vec3* cameraDataIn,
			myType cameraZoom,

			pixelColorType* outColor);

#endif
