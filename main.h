#ifndef FFCORE__H_
#define FFCORE__H_

#include "viray.h"

int ViRayMain(
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
			const mat4* objTransformIn,
			const mat4* objInvTransformIn,
#else
			const vec3* orientationIn,
			const vec3* scaleIn,
			const vec3* invScaleIn,
			const vec3* translationIn,
#endif
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
