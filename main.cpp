#include "main.h"

int ViRayMain(const mat4* objTransformIn,
			const mat4* objInvTransformIn,
			const int* objTypeIn,

			vec3* lightPositionIn,
			vec3* lightColorIn,

			vec3* materialCoeffIn,
			vec3* materialColorsIn,

			vec3* cameraDataIn,
			myType cameraZoom,

			pixelColorType* outColor)
{
	/*
	 * OBJECTS
	 */

#pragma HLS INTERFACE s_axilite port=objTransformIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objTransformIn offset=slave bundle=MAXI_IN_1

#pragma HLS INTERFACE s_axilite port=objInvTransformIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objInvTransformIn offset=slave bundle=MAXI_IN_1

#pragma HLS INTERFACE s_axilite port=objTypeIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objTypeIn offset=slave bundle=MAXI_IN_2

	/*
	 * LIGHTS
	 */

#pragma HLS INTERFACE s_axilite port=lightPositionIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=lightPositionIn offset=slave bundle=MAXI_IN_LIGHT

#pragma HLS INTERFACE s_axilite port=lightColorIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=lightColorIn offset=slave bundle=MAXI_IN_LIGHT

	/*
	 * MATERIALS
	 */

#pragma HLS INTERFACE s_axilite port=materialCoeffIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=materialCoeffIn offset=slave bundle=MAXI_IN_MATERIALS

#pragma HLS INTERFACE s_axilite port=materialColorsIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=materialColorsIn offset=slave bundle=MAXI_IN_MATERIALS

	/*
	 * CAMERA
	 */

#pragma HLS INTERFACE s_axilite port=cameraDataIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=cameraDataIn offset=slave bundle=MAXI_IN_CAMERA

#pragma HLS INTERFACE s_axilite port=cameraZoom bundle=AXI_LITE_1

	/*
	 * FRAMEBUFFER
	 */

#pragma HLS INTERFACE s_axilite port=outColor bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=outColor offset=slave bundle=MAXI_FRAME

#pragma HLS INTERFACE s_axilite port=return bundle=AXI_LITE_1

	CRay ray;

	vec3 cameraData[3];
	memcpy(cameraData, cameraDataIn, sizeof(vec3) * 3);

	CCamera camera(cameraZoom, cameraZoom * (myType(WIDTH) / myType(HEIGHT)),
				   HEIGHT, WIDTH,
				   cameraData[0], cameraData[1], cameraData[2]);

	myType posShift[2] = {	myType(-0.5) * (WIDTH - myType(1.0)),
				  	 		myType(-0.5) * (HEIGHT - myType(1.0))};

	mat4 objTransform[OBJ_NUM], objInvTransform[OBJ_NUM];
#pragma HLS ARRAY_PARTITION variable=objInvTransform cyclic factor=2 dim=1
//#pragma HLS ARRAY_PARTITION variable=objInvTransform complete dim=1
	memcpy(objTransform, objTransformIn, sizeof(mat4) * OBJ_NUM);
	memcpy(objInvTransform, objInvTransformIn, sizeof(mat4) * OBJ_NUM);

	pixelColorType frameBuffer[FRAME_BUFFER_SIZE];
//#pragma HLS ARRAY_PARTITION variable=frameBuffer complete dim=1
	myType currentClosest;
	int currentObjIdx;

	unsigned objType[OBJ_NUM];
#pragma HLS ARRAY_PARTITION variable=objType complete dim=1
	memcpy(objType, objTypeIn, sizeof(unsigned) * OBJ_NUM);

	/*
	 * LIGHTING AND MATERIALS
	 */

	ViRay::Light lights[LIGHTS_NUM];
//#pragma HLS ARRAY_PARTITION variable=lights complete dim=1
	for (unsigned i = 0; i < LIGHTS_NUM; ++i)
	{
#pragma HLS PIPELINE
		lights[i].position = lightPositionIn[i];
		lights[i].color = lightColorIn[i];
	}

	ViRay::Material materials[OBJ_NUM];
//#pragma HLS ARRAY_PARTITION variable=materials complete dim=1
	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
#pragma HLS PIPELINE
		materials[i].k = materialCoeffIn[i];
		materials[i].ambientColor = materialColorsIn[i * 3 + 0];
		materials[i].diffuseColor = materialColorsIn[i * 3 + 1];
		materials[i].specularColor = materialColorsIn[i * 3 + 2];
	}

	/*
	 *
	 */

	ViRay::RenderScene(camera, posShift, objTransform, objInvTransform, objType, lights, materials, frameBuffer, outColor);

	return 0;

}
