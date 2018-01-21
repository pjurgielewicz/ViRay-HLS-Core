#ifndef FFCORE__H_
#define FFCORE__H_

#include "viray.h"

int ViRayMain(
				const myType* objTransformationArrayIn,
				const int* objTypeIn,


				const myType* lightArrayIn,
				const myType* materialArrayIn,

				const myType* cameraArrayIn,
				myType cameraZoom,

				pixelColorType* outColor);

#endif
