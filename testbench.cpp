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

vec3 getInverseScale(const vec3& vec)
{
	return vec3(1.0 / vec[0], 1.0 / vec[1], 1.0 / vec[2]);
}

void copyVector(myType* dst, const vec3& vec, unsigned& pos)
{
	for (unsigned i = 0; i < 3; ++i)
	{
		dst[pos + i] = vec[i];
	}
	pos += 3;
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

	int* objTypeIn = new int[OBJ_NUM];

	/////////////////////////////////////////////////////////

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
	mat4 T, R, S;

	myType* transformationArray = new myType[OBJ_NUM * 2 * sizeof(mat4)];

	unsigned objTransformBufferPos = 0;
	mat4 objTransform, objInvTransform;

	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		loadVectorFromStream(dataFile, tmp);
		objTypeIn[i] = int(tmp[0]);
		myType rotation = PI * tmp[1];

		T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
		R.RotationMatrix(rotation, loadVectorFromStream(dataFile, tmp));
		S.ScaleMatrix(loadVectorFromStream(dataFile, tmp));

		objTransform = T * R * S;
		objInvTransform = objTransform.Inverse();

		for (unsigned j = 0; j < 12; ++j)
		{
			transformationArray[objTransformBufferPos++] = objTransform.data[j];
		}
		for (unsigned j = 0; j < 12; ++j)
		{
			transformationArray[objTransformBufferPos++] = objInvTransform.data[j];
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
		objTypeIn[i]	= int(loadVectorFromStream(dataFile, tmp)[0]);
//		cout << objTypeIn[i] << "\n" << endl;
		translation[i] 	= loadVectorFromStream(dataFile, tmp);
		orientation[i] 	= loadVectorFromStream(dataFile, tmp);
		scale[i]		= loadVectorFromStream(dataFile, tmp);
		invScale[i]		= getInverseScale(scale[i]);
	}

	// SAVING TO LINE BUFFER: T -> R -> S -> IS
	unsigned objTransformBufferPos = 0;
	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		copyVector(transformationArray, translation[i], objTransformBufferPos);
		copyVector(transformationArray, orientation[i], objTransformBufferPos);
		copyVector(transformationArray, scale[i], objTransformBufferPos);
		copyVector(transformationArray, invScale[i], objTransformBufferPos);
	}

	for (unsigned i = 0; i < objTransformBufferPos; ++i)
	{
		if (i % 12 == 0) cout << "\n";
		else if (i % 3 == 0) cout << "\t";
		cout << transformationArray[i] << " ";
	}
	cout << endl;

#endif

/////////////////////////////////////////////////////////

	vec3* lightPosition = new vec3[LIGHTS_NUM];
	vec3* lightDir 		= new vec3[LIGHTS_NUM];
	vec3* lightColor 	= new vec3[LIGHTS_NUM];
	vec3* lightCoeff 	= new vec3[LIGHTS_NUM];

	myType* lightArray 	= new myType[4 * sizeof(vec3) * LIGHTS_NUM];

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
		copyVector(lightArray, lightPosition[i], lightBufferPos);
		copyVector(lightArray, lightDir[i], lightBufferPos);
		copyVector(lightArray, lightColor[i], lightBufferPos);
		copyVector(lightArray, lightCoeff[i], lightBufferPos);
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

		// Oren-Nayar diffuse model coefficients
		myType sigmaSqr = materialCoeff[i * 2 + 1][2];
		myType onA = myType(1.0) - myType(0.5) * sigmaSqr / (sigmaSqr + myType(0.33));
		myType onB = myType(0.45) * sigmaSqr / (sigmaSqr + myType(0.09));

		materialCoeff[i * 2 + 1][2] = myType(1.0) / (materialCoeff[i * 2 + 1][1] * materialCoeff[i * 2 + 1][1]);
		// SAVE INFO EG. WHETHER TO USE TORRANCE-SPARROW SPECULAR REFLECTION OR NOT
		float_union materialInfo;
		materialInfo.raw_bits = unsigned(materialCoeff[i * 2 + 1][0]);
		materialCoeff[i * 2 + 1][0] = materialInfo.fp_num;

		for (unsigned j = 0; j < 4; ++j)
		{
			materialColors[i * 4 + j] = loadVectorFromStream(materialFile, tmp);
		}

		for (unsigned j = 0; j < 2; ++j)
		{
			materialTransform[i * 2 + j] = loadVectorFromStream(materialFile, tmp);
		}

		// quick fix for conductors
		if (materialInfo.raw_bits & 0x4/*ViRay::CMaterial::MaterialDescription::FRESNEL_CONDUCTOR*/)
		{
			myType conductorAbsorptionCoeff = materialTransform[i * 2 + 1][2]; // scale[2] (scale.z)
			materialCoeff[i * 2 + 1][2] = materialCoeff[i * 2 + 1][1] * materialCoeff[i * 2 + 1][1] + conductorAbsorptionCoeff * conductorAbsorptionCoeff;
		}

		materialColors[i * 4 + 1] *= materialCoeff[i * 2 + 0][0]; 		// PRIMARY_DIFFUSE 		* K[0]
		materialColors[i * 4 + 2] *= materialCoeff[i * 2 + 0][0]; 		// SECONDARY_DIFFUSE 	* K[0]
		materialColors[i * 4 + 3] *= materialCoeff[i * 2 + 0][1];  		// SPECULAR 			* K[1]

		// specular exp + ks
		materialCoeff[i * 2 + 0][2] += materialCoeff[i * 2 + 0][1];

		// NOW K[0] AND K[1] CAN BE SUBSTITUTED BY <A> AND <B> ACCORDINGLY
		materialCoeff[i * 2 + 0][0]	= onA;
		materialCoeff[i * 2 + 0][1] = onB;

		// SAVING TO LINE BUFFER
		for (unsigned j = 0; j < 2; ++j)
		{
			copyVector(materialArray, materialCoeff[i * 2 + j], materialBufferPos);
		}
		for (unsigned j = 0; j < 4; ++j)
		{
			copyVector(materialArray, materialColors[i * 4 + j], materialBufferPos);
		}

		for (unsigned j = 0; j < 2; ++j)
		{
			copyVector(materialArray, materialTransform[i * 2 + j], materialBufferPos);
		}
	}

/////////////////////////////////////////////////////////

	myType* cameraArray = new myType[4 * sizeof(vec3)];
	myType zoom;

	cameraFile >> zoom;
	for (unsigned i = 0; i < 12; ++i)
	{
		cameraFile >> cameraArray[i];
	}

/////////////////////////////////////////////////////////

	float_union* textureData = new float_union[TEXT_PAGE_SIZE];
	unsigned* textureDescriptionData = new unsigned[OBJ_NUM];
	unsigned* textureBaseAddr = new unsigned[OBJ_NUM];

	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		textureDescriptionData[i] 	= (128 << 10) + 128; // REQUIRED DUMMY
		textureBaseAddr[i]			= 0;
	}

/*	CTextureGenerator rgbDots(128, 128, CTextureGenerator::XOR);
	CTextureGenerator checkerbox(128, 128, CTextureGenerator::CHECKERBOARD, 6, 6);
	CTextureGenerator wood(128, 128, CTextureGenerator::WOOD, 6, 6, 1784301, 5, 0.2);
	CTextureGenerator marble(128, 128, CTextureGenerator::MARBLE, 6, 4, 1784301, 3, 0.57);*/

	CTextureHelper textureHelper(textureData, textureDescriptionData, textureBaseAddr);
	unsigned addr, code;
	addr = textureHelper.SaveTexture(CTextureGenerator(128, 128, CTextureGenerator::CHECKERBOARD, 6, 6), ViRay::CMaterial::PLANAR, code, true, 1);
	textureHelper.BindTextureToObject(6, addr, textureHelper.GetTextureDescriptionCode(ViRay::CMaterial::BITMAP_MASK, ViRay::CMaterial::CYLINDRICAL, 128, 128));
	textureHelper.BindTextureToObject(7, addr, textureHelper.GetTextureDescriptionCode(ViRay::CMaterial::BITMAP_MASK, ViRay::CMaterial::CYLINDRICAL, 128, 128));
	addr = textureHelper.SaveTexture(CTextureGenerator(128, 128, CTextureGenerator::MARBLE, 6, 4, 1784301, 5, 1.87), ViRay::CMaterial::SPHERICAL, code, true, 5);
	textureHelper.BindTextureToObject(4, addr, code);
//	textureHelper.BindTextureToObject(2, addr, code);
	textureHelper.SaveTexture(CTextureGenerator(128, 128, CTextureGenerator::WOOD, 6, 6, 1784301, 5, 0.2), ViRay::CMaterial::SPHERICAL, code, true, 3);

/////////////////////////////////////////////////////////

	pixelColorType* frame = new pixelColorType[WIDTH * HEIGHT];

/////////////////////////////////////////////////////////

	clock_t timer;
	timer = clock();

	ViRayMain(
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
