#include "main.h"

using namespace std;

vec3 GetVectorFromStream(const myType* s, unsigned& pos)
{
#pragma HLS INLINE
	vec3 tmp(s[pos], s[pos + 1], s[pos + 2]);
	pos += 3;

	return tmp;
}

vec3 GetVectorFromStream(const myType* s, const unsigned& pos)
{
#pragma HLS INLINE
	vec3 tmp(s[pos], s[pos + 1], s[pos + 2]);

	return tmp;
}

int ViRayMain(	const myType* objTransformationArrayIn,
				const int* objTypeIn,

				const myType* lightArrayIn,
				const myType* materialArrayIn,

				const myType* cameraArrayIn,
				myType cameraZoom,
				myType cameraNearPlane,

				const float* textureDataIn,
				const unsigned* textureDescriptionIn,
				const unsigned* textureBaseAddressIn,

				pixelColorType* outColor)
{
	/*
	 * OBJECTS
	 */


#pragma HLS INTERFACE s_axilite port=objTransformationArrayIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objTransformationArrayIn offset=slave bundle=MAXI_DATA

#pragma HLS INTERFACE s_axilite port=objTypeIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objTypeIn offset=slave bundle=MAXI_DATA_2

	/*
	 * LIGHTS
	 */

#pragma HLS INTERFACE s_axilite port=lightArrayIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=lightArrayIn offset=slave bundle=MAXI_DATA

	/*
	 * MATERIALS
	 */

#pragma HLS INTERFACE s_axilite port=materialArrayIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=materialArrayIn offset=slave bundle=MAXI_DATA

	/*
	 * CAMERA
	 */

#pragma HLS INTERFACE s_axilite port=cameraArrayIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=cameraArrayIn offset=slave bundle=MAXI_DATA

#pragma HLS INTERFACE s_axilite port=cameraZoom bundle=AXI_LITE_1
#pragma HLS INTERFACE s_axilite port=cameraNearPlane bundle=AXI_LITE_1

	/*
	 * TEXTURE DATA
	 */

#pragma HLS INTERFACE s_axilite port=textureDataIn bundle=AXI_LITE_1
#if !defined(USE_FIXEDPOINT) && !defined(USE_FLOAT) || defined(USE_FIXEDPOINT)
#pragma HLS INTERFACE m_axi port=textureDataIn offset=slave bundle=MAXI_DATA_3
#else
#pragma HLS INTERFACE m_axi port=textureDataIn offset=slave bundle=MAXI_DATA
#endif

#pragma HLS INTERFACE s_axilite port=textureDescriptionIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=textureDescriptionIn offset=slave bundle=MAXI_DATA_2

#pragma HLS INTERFACE s_axilite port=textureBaseAddressIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=textureBaseAddressIn offset=slave bundle=MAXI_DATA_2

	/*
	 * FRAMEBUFFER
	 */

#pragma HLS INTERFACE s_axilite port=outColor bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=outColor offset=slave bundle=MAXI_FRAME

#pragma HLS INTERFACE s_axilite port=return bundle=AXI_LITE_1

	/*
	 * INTERFACE PRAGMAS END
	 */

	/*
	 * TEXTURES
	 */
	float_union textureData[TEXT_PAGE_SIZE];
#pragma HLS RESOURCE variable=textureData core=RAM_2P_BRAM
	unsigned textureDescription[OBJ_NUM];
	unsigned textureBaseAddress[OBJ_NUM];
#ifdef TEXTURE_URAM_STORAGE
#pragma HLS RESOURCE variable=textureData core=XPM_MEMORY uram
#endif

//#ifdef TEXTURE_ENABLE
	memcpy(textureData, textureDataIn, sizeof(float_union) * TEXT_PAGE_SIZE);
	memcpy(textureDescription, textureDescriptionIn, sizeof(unsigned) * OBJ_NUM);
	memcpy(textureBaseAddress, textureBaseAddressIn, sizeof(unsigned) * OBJ_NUM);
//#endif

#ifdef SELF_RESTART_ENABLE
	ViRaySelfRestartLoop: while(true)
#endif
	{
		/*
		 * CAMERA
		 */
		vec3 cameraData[4]; //pos, u, v, w -> 4
		memcpy(cameraData, cameraArrayIn, sizeof(vec3) * 4);


		CCamera camera(cameraZoom, cameraZoom * (myType(WIDTH) / myType(HEIGHT)), cameraNearPlane,
					   cameraData[0], cameraData[1], cameraData[2], cameraData[3]);

		myType posShift[2] = {	myType(-0.5) * (WIDTH  - myType(1.0)),
								myType(-0.5) * (HEIGHT - myType(1.0))};

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
		mat4 objTransform[OBJ_NUM], objInvTransform[OBJ_NUM];
//#pragma HLS ARRAY_PARTITION variable=objInvTransform cyclic factor=2 dim=1
#pragma HLS ARRAY_PARTITION variable=objInvTransform complete dim=1
//#pragma HLS ARRAY_PARTITION variable=objTransform cyclic factor=2 dim=1
#pragma HLS ARRAY_PARTITION variable=objTransform complete dim=1

		myType objTransformArray[2 * 12 * OBJ_NUM];
		memcpy(objTransformArray, objTransformationArrayIn, 2 * sizeof(mat4) * OBJ_NUM );
#else
		myType objTransformationArray[4 * 3 * OBJ_NUM];
		memcpy(objTransformationArray, objTransformationArrayIn, 4 * sizeof(vec3) * OBJ_NUM);
		ViRay::SimpleTransform objTransform[OBJ_NUM];
#pragma HLS ARRAY_PARTITION variable=objTransform complete dim=1

#endif

#ifdef RENDER_DATAFLOW_ENABLE
		pixelColorType frameBuffer[FRAME_ROW_BUFFER_SIZE];
#endif

		unsigned objType[OBJ_NUM];
#pragma HLS ARRAY_PARTITION variable=objType complete dim=1
		memcpy(objType, objTypeIn, sizeof(unsigned) * OBJ_NUM);

		/*
		 * LIGHTING AND MATERIALS
		 */
		myType lightArray[4 * 3 * LIGHTS_NUM];
		memcpy(lightArray, lightArrayIn, 4 * sizeof(vec3) * LIGHTS_NUM);
		ViRay::Light lights[LIGHTS_NUM];
#pragma HLS ARRAY_PARTITION variable=lights complete dim=1

		myType materialArray[8 * 3 * OBJ_NUM];
		memcpy(materialArray, materialArrayIn, 8 * sizeof(vec3) * OBJ_NUM);
		ViRay::CMaterial materials[OBJ_NUM];
//#pragma HLS ARRAY_PARTITION variable=materials complete dim=1


		AssignmentLoops:{
#pragma HLS LOOP_MERGE

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
			unsigned objTransformBufferPos = 0;
			for (unsigned i = 0; i < OBJ_NUM; ++i)
			{
#pragma HLS PIPELINE
				for (unsigned j = 0; j < 12; ++j)
				{
					objTransform[i].data[j] 	=  objTransformArray[objTransformBufferPos];
					objInvTransform[i].data[j] 	=  objTransformArray[objTransformBufferPos + 12];
					++objTransformBufferPos;
				}
				objTransformBufferPos += 12;
			}
#else
			unsigned objTransformationBufferPos = 0;
			SimpleTransformAssignmentLoop: for (unsigned i = 0; i < OBJ_NUM; ++i)
			{
#pragma HLS PIPELINE
				objTransform[i].translation = GetVectorFromStream(objTransformationArray, objTransformationBufferPos);
				objTransform[i].orientation = GetVectorFromStream(objTransformationArray, objTransformationBufferPos);
				objTransform[i].scale 		= GetVectorFromStream(objTransformationArray, objTransformationBufferPos);
				objTransform[i].invScale 	= GetVectorFromStream(objTransformationArray, objTransformationBufferPos);
			}
#endif

			unsigned lightBufferPos = 0;
			LightsAssignmentLoop: for (unsigned i = 0; i < LIGHTS_NUM; ++i)
			{
#pragma HLS PIPELINE
				lights[i].position 	= GetVectorFromStream(lightArray, lightBufferPos);
				lights[i].dir 		= GetVectorFromStream(lightArray, lightBufferPos);
				lights[i].color 	= GetVectorFromStream(lightArray, lightBufferPos);
				lights[i].coeff 	= GetVectorFromStream(lightArray, lightBufferPos);
			}

			unsigned materialBufferPos = 0;
			MaterialAssignmentLoop: for (unsigned i = 0; i < OBJ_NUM; ++i)
			{
#pragma HLS PIPELINE
				materials[i] = ViRay::CMaterial(GetVectorFromStream(materialArray, materialBufferPos + 0),
												GetVectorFromStream(materialArray, materialBufferPos + 3),
												GetVectorFromStream(materialArray, materialBufferPos + 6),
												GetVectorFromStream(materialArray, materialBufferPos + 9),
												GetVectorFromStream(materialArray, materialBufferPos + 12),
												GetVectorFromStream(materialArray, materialBufferPos + 15),
												textureDescription[i],
												textureBaseAddress[i],
												GetVectorFromStream(materialArray, materialBufferPos + 18),
												GetVectorFromStream(materialArray, materialBufferPos + 21));

	//			cout << " " << i << ":\n" << materials[i] << endl;
				materialBufferPos += 24;
			}
		}

		/*
		 *
		 */

#ifdef RENDER_DATAFLOW_ENABLE
		ViRay::RenderSceneOuterLoop(camera, posShift,
											objTransform,
		#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
											objInvTransform,
		#endif
											objType,
											lights, materials, textureData, frameBuffer, outColor);
#else
		ViRay::RenderScene(camera, posShift,
							objTransform,
		#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
							objInvTransform,
		#endif
							objType,
							lights, materials, textureData, outColor);
#endif


	}
	return 0;

}
