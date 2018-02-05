#include "main.h"

using namespace std;

vec3 GetVectorFromStream(const myType* s, unsigned& pos)
{
#pragma HLS INLINE
	vec3 tmp(s[pos], s[pos + 1], s[pos + 2]);
	pos += 3;

	return tmp;
}

int ViRayMain(	const myType* objTransformationArrayIn,
				const myType* objTransformationArrayCopyIn,
				const int* objTypeIn,

				const myType* lightArrayIn,
				const myType* materialArrayIn,

				const myType* cameraArrayIn,
				myType cameraZoom,

				const myType* textureDataIn,
				const unsigned* textureDescriptionIn,

				pixelColorType* outColor)
{
	/*
	 * OBJECTS
	 */


#pragma HLS INTERFACE s_axilite port=objTransformationArrayIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objTransformationArrayIn offset=slave bundle=MAXI_DATA

#pragma HLS INTERFACE s_axilite port=objTransformationArrayCopyIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objTransformationArrayCopyIn offset=slave bundle=MAXI_DATA

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

	/*
	 * TEXTURE DATA
	 */

#pragma HLS INTERFACE s_axilite port=textureDataIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=textureDataIn offset=slave bundle=MAXI_DATA

#pragma HLS INTERFACE s_axilite port=textureDescriptionIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=textureDescriptionIn offset=slave bundle=MAXI_DATA_2

	/*
	 * FRAMEBUFFER
	 */

#pragma HLS INTERFACE s_axilite port=outColor bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=outColor offset=slave bundle=MAXI_FRAME

#pragma HLS INTERFACE s_axilite port=return bundle=AXI_LITE_1

	/*
	 * INTERFACE PRAGMAS END
	 */

	vec3 cameraData[3];
	memcpy(cameraData, cameraArrayIn, sizeof(vec3) * 3);


	CCamera camera(cameraZoom, cameraZoom * (myType(WIDTH) / myType(HEIGHT)),
				   HEIGHT, WIDTH,
				   cameraData[0], cameraData[1], cameraData[2]);

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
	myType objTransformationArray[4 * 3 * OBJ_NUM], objTransformationCopyArray[4 * 3 * OBJ_NUM];
	memcpy(objTransformationArray, objTransformationArrayIn, 4 * sizeof(vec3) * OBJ_NUM);
	memcpy(objTransformationCopyArray, objTransformationArrayCopyIn, 4 * sizeof(vec3) * OBJ_NUM);
	ViRay::SimpleTransform objTransform[OBJ_NUM], objTransformCopy[OBJ_NUM];
#pragma HLS ARRAY_PARTITION variable=objTransform complete dim=1
#pragma HLS ARRAY_PARTITION variable=objTransformCopy complete dim=1

#endif

	pixelColorType frameBuffer[FRAME_ROW_BUFFER_SIZE];
//#pragma HLS ARRAY_PARTITION variable=frameBuffer complete dim=1

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

	// TODO: Change Size appropriately: 5 -> 8
	myType materialArray[8 * 3 * OBJ_NUM];
	memcpy(materialArray, materialArrayIn, 8 * sizeof(vec3) * OBJ_NUM);
	ViRay::CMaterial materials[OBJ_NUM];
//#pragma HLS ARRAY_PARTITION variable=materials complete dim=1

	/*
	 * TEXTURES
	 */
	float_union textureData[MAX_TEXTURE_NUM][TEXT_WIDTH * TEXT_HEIGHT];
#ifdef TEXTURE_URAM_STORAGE
#pragma HLS RESOURCE variable=textureData core=XPM_MEMORY uram
#endif
	memcpy(textureData, textureDataIn, sizeof(float_union) * TEXT_WIDTH * TEXT_HEIGHT * MAX_TEXTURE_NUM);
	unsigned textureDescription[OBJ_NUM];
	memcpy(textureDescription, textureDescriptionIn, sizeof(unsigned) * OBJ_NUM);

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
			objTransform[i].orientation = GetVectorFromStream(objTransformationArray, objTransformationBufferPos);
			objTransform[i].scale 		= GetVectorFromStream(objTransformationArray, objTransformationBufferPos);
			objTransform[i].invScale 	= GetVectorFromStream(objTransformationArray, objTransformationBufferPos);
			objTransform[i].translation = GetVectorFromStream(objTransformationArray, objTransformationBufferPos);
		}

		unsigned objTransformationBufferPosCopy = 0;
		SimpleTransformAssignmentCopyLoop: for (unsigned i = 0; i < OBJ_NUM; ++i)
		{
#pragma HLS PIPELINE
			objTransformCopy[i].orientation = GetVectorFromStream(objTransformationCopyArray, objTransformationBufferPosCopy);
			objTransformCopy[i].scale 		= GetVectorFromStream(objTransformationCopyArray, objTransformationBufferPosCopy);
			objTransformCopy[i].invScale 	= GetVectorFromStream(objTransformationCopyArray, objTransformationBufferPosCopy);
			objTransformCopy[i].translation = GetVectorFromStream(objTransformationCopyArray, objTransformationBufferPosCopy);
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
			materials[i].k 				= GetVectorFromStream(materialArray, materialBufferPos);
			materials[i].fresnelData	= GetVectorFromStream(materialArray, materialBufferPos);

			materials[i].ambientColor 	= GetVectorFromStream(materialArray, materialBufferPos);
			materials[i].primaryColor 	= GetVectorFromStream(materialArray, materialBufferPos);
			materials[i].secondaryColor = GetVectorFromStream(materialArray, materialBufferPos);
			materials[i].specularColor 	= GetVectorFromStream(materialArray, materialBufferPos);

			/*
			 * THE USER IS RESPONSIBLE OF CARING ABOUT IDX IS LOWER THAN < MAX_TEXTURE_NUM >
			 * -> LOGIC REDUCTION
			 */

			unsigned description = textureDescription[i];
			/*
			 * THE WIDTH OF EACH ELEMENT IS BIGGER THAN NECESSARY FOR THE CURRENT IMPLEMENTATION
			 * LEAVING ROOM FOR FUTURE UPGRADES
			 *
			 * MSB
			 * |
			 * 6b  : textureIdx
			 * 3b  : textureType
			 * 3b  : textureMapping
			 * 10b : textureWidth
			 * 10b : textureHeight
			 * |
			 * LSB
			 */

			materials[i].textureIdx 	= (description >> 26) & 0x3F;
#ifdef TEXTURE_ENABLE
			materials[i].textureType	= (description >> 23) & 0x7;
#else
			materials[i].textureType	= ViRay::CMaterial::CONSTANT;
#endif

#ifdef ADVANCED_TEXTURE_MAPPING_ENABLE
			materials[i].textureMapping = (description >> 20) & 0x7;
#else
			materials[i].textureMapping = ViRay::CMaterial::PLANAR;
#endif

			materials[i].textureWidth	= (description >> 10) & 0x3FF;
			materials[i].textureHeight	= (description      ) & 0x3FF;

			// TEXTURE TRANSFORM
			materials[i].texturePos		= GetVectorFromStream(materialArray, materialBufferPos);
			materials[i].textureScale	= GetVectorFromStream(materialArray, materialBufferPos);
		}
	}

	/*
	 *
	 */
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
	ViRay::RenderSceneOuterLoop(camera, posShift,
								objTransform, objInvTransform, objType,
								lights, materials, textureData, frameBuffer, outColor);
#else
	ViRay::RenderSceneOuterLoop(camera, posShift,
								objTransform, objTransformCopy,	objType,
								lights, materials, textureData, frameBuffer, outColor);
#endif
	return 0;

}
