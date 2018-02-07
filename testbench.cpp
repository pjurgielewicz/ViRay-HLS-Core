#include "main.h"
#include "Utils/bitmap_image.hpp"
#include "Utils/TextureGenerator.h"

#include "iostream"
#include "fstream"
#include <string>
#include <time.h>
using namespace std;

vec3& loadVectorFromStream(ifstream& file, vec3& vec)
{
	file >> vec.data[0] >> vec.data[1] >> vec.data[2];
	return vec;
}

unsigned getTextureDescriptionCode(//unsigned char textureIdx = 0,
									unsigned char textureType = ViRay::CMaterial::CONSTANT,
									unsigned char textureMapping = ViRay::CMaterial::PLANAR,
									unsigned short textureWidth = 128,
									unsigned short textureHeight = 128)
{
	return 	//((textureIdx & 0x3F) << 26) +
			((textureType & 0x7) << 23) +
			((textureMapping & 0x7) << 20) +
			((textureWidth & 0x3FF) << 10) +
			((textureHeight) & 0x3FF);
}

int main()
{
//	string dataPath("C:\\Users\\pjurgiel\\Source\\FFCore\\src\\SimData\\");
	string dataPath("D:\\Dokumenty\\WorkspaceXilinx\\FFCore\\src\\SimData\\");

	ifstream dataFile((dataPath + "data.dat").c_str());
	ifstream lightFile((dataPath + "light.dat").c_str());
	ifstream materialFile((dataPath + "material.dat").c_str());
	ifstream cameraFile((dataPath + "camera.dat").c_str());

	vec3 tmp;

	/////////////////////////////////////////////////////////

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
	mat4* objTransform = new mat4[OBJ_NUM];
	mat4* objInvTransform = new mat4[OBJ_NUM];
	mat4 S, R, T;

	myType* transformationArray = new myType[OBJ_NUM * 2 * sizeof(mat4)];

	// CUBE I
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(PI * 0.3, vec3(myType(1.0), myType(1.0), myType(1.0)));
	S.ScaleMatrix(vec3(1.0));
	objTransform[0] = T * R * S;
	objInvTransform[0] = objTransform[0].Inverse();

	// SPHERE II
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(PI * 0.5, vec3(myType(1.0), myType(0.0), myType(0.0)));
	S.ScaleMatrix(vec3(myType(2.0), myType(1.0), myType(1.0)));
	objTransform[1] = T * R * S;
	objInvTransform[1] = objTransform[1].Inverse();

	// PLANE I
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(0, vec3(1.0, 0.0, 0.0));
	objTransform[2] = T * R;
	objInvTransform[2] = objTransform[2].Inverse();

	// PLANE II
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(myType(PI * 0.5), vec3(1.0, 0.0, 0.0));
	S.ScaleMatrix(vec3(5.0, 1.0, 5.0));
	objTransform[3] = T * R * S;
	objInvTransform[3] = objTransform[3].Inverse();

	// PLANE III
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(myType(PI * 0.5), vec3(0.0, 0.0, 1.0));
	S.ScaleMatrix(vec3(4.0));
	objTransform[4] = T * R * S;
	objInvTransform[4] = objTransform[4].Inverse();

	// PLANE IV
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(myType(-PI * 0.5), vec3(0.0, 0.0, 1.0));
	S.ScaleMatrix(vec3(4.0));
	objTransform[5] = T * R * S;
	objInvTransform[5] = objTransform[5].Inverse();

	// SPHERE I
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(PI * 0.0, vec3(myType(1.0), myType(0.0), myType(0.0)));
	S.ScaleMatrix(vec3(1.0, 1.0, 1.0));
	objTransform[6] = T * R * S;
	objInvTransform[6] = objTransform[6].Inverse();


	// EMPTY
	for (unsigned i = 7; i < OBJ_NUM; ++i)
	{
		objTransform[i].IdentityMatrix();
		objInvTransform[i].IdentityMatrix();
	}

	unsigned objTransformBufferPos = 0;
	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		for (unsigned j = 0; j < 12; ++j)
		{
			transformationArray[objTransformBufferPos++] = objTransform[i].data[j];
		}
		for (unsigned j = 0; j < 12; ++j)
		{
			transformationArray[objTransformBufferPos++] = objInvTransform[i].data[j];
		}
	}

#else
	vec3* translation 	= new vec3[OBJ_NUM];
	vec3* scale 		= new vec3[OBJ_NUM];
	vec3* invScale 		= new vec3[OBJ_NUM];
	vec3* orientation 	= new vec3[OBJ_NUM];

	myType* transformationArray = new myType[OBJ_NUM * 4 * sizeof(vec3)];

	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		translation[i] 	= vec3(myType(0.0));
		scale[i] 		= vec3(myType(1.0));
		invScale[i] 	= vec3(myType(1.0));
		orientation[i] 	= vec3(myType(0.0), myType(1.0), myType(0.0));
	}

	// SPHERE
	translation[0] = loadVectorFromStream(dataFile, tmp);
	scale[0][0] = myType(1.0);	invScale[0][0] = myType(1.0) / scale[0][0];
	orientation[0] = vec3(myType(0.0), myType(0.0), myType(1.0));
	// CYLINDER
	translation[1] = loadVectorFromStream(dataFile, tmp);
	scale[1][0] = myType(2.0);	invScale[1][0] = myType(1.0) / scale[1][0];
	orientation[1] = vec3(myType(0.0), myType(.0), myType(1.0));
	// PLANE
	translation[2] = loadVectorFromStream(dataFile, tmp);
	orientation[2] = vec3(myType(0.0), myType(1.0), myType(0.0));

	// DISK
	translation[3] = loadVectorFromStream(dataFile, tmp);
	orientation[3] = vec3(myType(0.0), myType(0.0), myType(1.0));
	scale[3][0] = myType(5.0); invScale[3][0] = myType(1.0) / scale[3][0];
	scale[3][1] = myType(5.0); invScale[3][1] = myType(1.0) / scale[3][1];
	// SQUARE
	translation[4] = loadVectorFromStream(dataFile, tmp);
	orientation[4] = vec3(myType(1.0), myType(0.0), myType(0.0));
	scale[4][1] = myType(4.0); invScale[4][1] = myType(1.0) / scale[4][1];
	scale[4][2] = myType(4.0); invScale[4][2] = myType(1.0) / scale[4][2];
	// SQUARE
	translation[5] = loadVectorFromStream(dataFile, tmp);
	orientation[5] = vec3(myType(-1.0), myType(0.0), myType(0.0));
	scale[5][1] = myType(4.0); invScale[5][1] = myType(1.0) / scale[5][1];
	scale[5][2] = myType(4.0); invScale[5][2] = myType(1.0) / scale[5][2];

	// CONE
	translation[6] = loadVectorFromStream(dataFile, tmp);
//	scale[6][0] = myType(0.5);	invScale[6][0] = myType(1.0) / scale[6][0];

	// SAVING TO LINE BUFFER
	unsigned objTransformBufferPos = 0;
	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		for (unsigned j = 0; j < 3; ++j)
		{
			transformationArray[objTransformBufferPos++] = orientation[i][j];
		}
		for (unsigned j = 0; j < 3; ++j)
		{
			transformationArray[objTransformBufferPos++] = scale[i][j];
		}
		for (unsigned j = 0; j < 3; ++j)
		{
			transformationArray[objTransformBufferPos++] = invScale[i][j];
		}
		for (unsigned j = 0; j < 3; ++j)
		{
			transformationArray[objTransformBufferPos++] = translation[i][j];
		}
	}

#endif

/////////////////////////////////////////////////////////

	int* objTypeIn = new int[OBJ_NUM];
	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		switch(i)
		{
		case 0: objTypeIn[i] = SPHERE; break;
		case 1: objTypeIn[i] = CYLINDER; break;
		case 2: objTypeIn[i] = PLANE; break;
		case 3: objTypeIn[i] = DISK; break;
		case 4: objTypeIn[i] = SQUARE; break;
		case 5: objTypeIn[i] = SQUARE; break;
		case 6: objTypeIn[i] = SPHERE; break;
		default: objTypeIn[i] = INVALID; break;
		}
	}

/////////////////////////////////////////////////////////

	vec3* lightPosition = new vec3[LIGHTS_NUM];
	vec3* lightDir = new vec3[LIGHTS_NUM];
	vec3* lightColor = new vec3[LIGHTS_NUM];
	vec3* lightCoeff = new vec3[LIGHTS_NUM];

	myType* lightArray = new myType[4 * sizeof(vec3) * LIGHTS_NUM];

	unsigned lightBufferPos = 0;
	for (unsigned i = 0; i < LIGHTS_NUM; ++i)
	{
		lightPosition[i] = loadVectorFromStream(lightFile, tmp);
		lightDir[i] = loadVectorFromStream(lightFile, tmp).Normalize();
		lightColor[i] = loadVectorFromStream(lightFile, tmp);

		// OFFLOADING FPGA DESIGN
		vec3 temp = loadVectorFromStream(lightFile, tmp);

		myType outerMinusInner = temp[1] - temp[0];
		myType outerMinusInnerInv = (outerMinusInner > CORE_BIAS) ? myType(1.0) / (outerMinusInner) : MAX_DISTANCE;

		lightCoeff[i] = vec3(temp[0], outerMinusInnerInv, temp[2]);

		// SAVING TO LINE BUFFER
		for (unsigned j = 0; j < 3; ++j)
		{
			lightArray[lightBufferPos++] = lightPosition[i][j];
		}
		for (unsigned j = 0; j < 3; ++j)
		{
			lightArray[lightBufferPos++] = lightDir[i][j];
		}
		for (unsigned j = 0; j < 3; ++j)
		{
			lightArray[lightBufferPos++] = lightColor[i][j];
		}
		for (unsigned j = 0; j < 3; ++j)
		{
			lightArray[lightBufferPos++] = lightCoeff[i][j];
		}
	}

/////////////////////////////////////////////////////////

	vec3* materialCoeff 	= new vec3[OBJ_NUM * 2];
	vec3* materialColors 	= new vec3[OBJ_NUM * 4];
	vec3* materialTransform = new vec3[OBJ_NUM * 2];

	myType* materialArray 	= new myType[8 * sizeof(vec3) * OBJ_NUM];

	unsigned materialBufferPos = 0;
	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		for (unsigned j = 0; j < 2; ++j)
		{
			materialCoeff[i * 2 + j] = loadVectorFromStream(materialFile, tmp);
		}

		materialCoeff[i * 2 + 1][2] = myType(1.0) / (materialCoeff[i * 2 + 1][1] * materialCoeff[i * 2 + 1][1]);

		for (unsigned j = 0; j < 4; ++j)
		{
			materialColors[i * 4 + j] = loadVectorFromStream(materialFile, tmp);
		}

		for (unsigned j = 0; j < 2; ++j)
		{
			materialTransform[i * 2 + j] = loadVectorFromStream(materialFile, tmp);
		}

		materialColors[i * 4 + 1] *= materialCoeff[i * 2 + 0][0]; 	// PRIMARY_DIFFUSE 		* K[0]
		materialColors[i * 4 + 2] *= materialCoeff[i * 2 + 0][0]; 	// SECONDARY_DIFFUSE 	* K[0]
		materialColors[i * 4 + 3] *= materialCoeff[i * 2 + 0][1];  	// SPECULAR 			* K[1] /*???? TODEBUG*/

		// SAVING TO LINE BUFFER
		for (unsigned j = 0; j < 2; ++j)
		{
			for (unsigned k = 0; k < 3; ++k)
			{
				materialArray[materialBufferPos++] = materialCoeff[i * 2 + j][k];
			}
		}
		for (unsigned j = 0; j < 4; ++j)
		{
			for (unsigned k = 0; k < 3; ++k)
			{
				materialArray[materialBufferPos++] = materialColors[i * 4 + j][k];
			}
		}

		for (unsigned j = 0; j < 2; ++j)
		{
			for (unsigned k = 0; k < 3; ++k)
			{
				materialArray[materialBufferPos++] = materialTransform[i * 2 + j][k];
			}
		}
	}

/////////////////////////////////////////////////////////

	myType* cameraArray = new myType[3 * sizeof(vec3)];
	myType zoom;

	cameraFile >> zoom;
	for (unsigned i = 0; i < 9; ++i)
	{
		cameraFile >> cameraArray[i];
	}

/////////////////////////////////////////////////////////

	float_union* textureData = new float_union[TEXT_PAGE_SIZE];
	unsigned* textureDescriptionData = new unsigned[OBJ_NUM];
	unsigned* textureBaseAddr = new unsigned[OBJ_NUM];

	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		textureDescriptionData[i] 	= getTextureDescriptionCode();
		textureBaseAddr[i]			= 0;
	}

	/*
	 * TODO: Create Texture Helper to easily manage texture binding
	 */

	CTextureGenerator rgbDots(17, 17, CTextureGenerator::RGB_DOTS);
	CTextureGenerator checkerbox(128, 128, CTextureGenerator::CHECKERBOARD, 6, 6);
	CTextureGenerator wood(128, 128, CTextureGenerator::WOOD, 6, 6, 1784301, 5, 0.2);
	CTextureGenerator marble(128, 128, CTextureGenerator::MARBLE, 6, 4, 1784301, 3, 0.57);

	unsigned baseAddress = 0;
	unsigned rgbAddress = baseAddress;
	baseAddress += rgbDots.CopyBitmapInto(textureData + baseAddress);
	unsigned checkerboxAddress = baseAddress;
	baseAddress += checkerbox.CopyBitmapInto(textureData + baseAddress);
	unsigned marbleAddress = baseAddress;
	baseAddress += marble.CopyBitmapInto(textureData + baseAddress);
	unsigned woodAddress = baseAddress;
	baseAddress += wood.CopyBitmapInto(textureData + baseAddress);

	textureDescriptionData[0] = getTextureDescriptionCode(marble.GetTextureType(), ViRay::CMaterial::SPHERICAL, marble.GetTextureWidth(), marble.GetTextureHeight());
	textureDescriptionData[2] = getTextureDescriptionCode(checkerbox.GetTextureType(), ViRay::CMaterial::PLANAR, checkerbox.GetTextureWidth(), checkerbox.GetTextureHeight());
	textureDescriptionData[4] = getTextureDescriptionCode(rgbDots.GetTextureType(), ViRay::CMaterial::PLANAR, rgbDots.GetTextureWidth(), rgbDots.GetTextureHeight());
	textureDescriptionData[5] = getTextureDescriptionCode(wood.GetTextureType(), ViRay::CMaterial::PLANAR, wood.GetTextureWidth(), wood.GetTextureHeight());

	textureBaseAddr[0] = marbleAddress;
	textureBaseAddr[2] = checkerboxAddress;
	textureBaseAddr[4] = rgbAddress;
	textureBaseAddr[5] = woodAddress;

/////////////////////////////////////////////////////////

	pixelColorType* frame = new pixelColorType[WIDTH * HEIGHT];

/////////////////////////////////////////////////////////

	clock_t timer;
	timer = clock();

	ViRayMain(
			transformationArray,
			transformationArray,
			objTypeIn,

			lightArray,
			materialArray,

			cameraArray,
			zoom,

			(myType*)textureData,
			textureDescriptionData,
			textureBaseAddr,

			frame);

	timer = clock() - timer;

	cout << "CPU simulation took: " << (float(timer) / CLOCKS_PER_SEC) << " seconds" << endl;

	bitmap_image img((int)WIDTH, (int)HEIGHT);
	for (unsigned h = 0; h < HEIGHT; ++h)
	{
		for (unsigned w = 0; w < WIDTH; ++w)
		{
			unsigned val = frame[h * WIDTH + w] & 0xFFFFFF;

			unsigned char R = (val >> 16) & 0xFF;
			unsigned char G = (val >> 8 ) & 0xFF;
			unsigned char B = val & 0xFF;

			img.set_pixel(w, h, R, G, B);

		}
//		cout << endl;
	}
	img.save_image("output.bmp");

/////////////////////////////////////////////////////////

	delete[] frame;

	delete[] textureBaseAddr;
	delete[] textureDescriptionData;
	delete[] textureData;

	delete[] cameraArray;

	delete[] materialArray;
	delete[] materialTransform;
	delete[] materialColors;
	delete[] materialCoeff;

	delete[] lightArray;
	delete[] lightColor;
	delete[] lightPosition;

	delete[] objTypeIn;
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
	delete[] objInvTransform;
	delete[] objTransform;
#else
	delete[] orientation;
	delete[] invScale;
	delete[] scale;
	delete[] translation;
#endif

	delete[] transformationArray;

	dataFile.close();
	lightFile.close();
	materialFile.close();
	cameraFile.close();

/////////////////////////////////////////////////////////

	return 0;
}
