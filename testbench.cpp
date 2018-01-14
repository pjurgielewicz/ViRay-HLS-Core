#include "main.h"
#include "Utils/bitmap_image.hpp"

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

	mat4* objTransform = new mat4[OBJ_NUM];
	mat4* objInvTransform = new mat4[OBJ_NUM];
	mat4 S, R, T;

	// SPHERE I
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(PI * 0.3, vec3(myType(1.0), myType(1.0), myType(1.0)));
	S.ScaleMatrix(vec3(0.5, 0.5, 0.5));
	objTransform[0] = T * R * S;
	objInvTransform[0] = objTransform[0].Inverse();

	// SPHERE II
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(PI * 0.2, vec3(myType(1.0), myType(1.0), myType(1.0)));
	S.ScaleMatrix(vec3(myType(1.0), myType(0.5), myType(1.0)));
	objTransform[1] = T * R * S;
	objInvTransform[1] = objTransform[1].Inverse();

	// PLANE I
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(myType(PI), vec3(1.0, 0.0, 0.0));
	objTransform[2] = T * R;
	objInvTransform[2] = objTransform[2].Inverse();

	// PLANE II
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(myType(-PI * 0.6), vec3(1.0, 0.0, 0.0));
	S.ScaleMatrix(vec3(5.0, 1.0, 5.0));
	objTransform[3] = T * R * S;
	objInvTransform[3] = objTransform[3].Inverse();

	// PLANE III
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(myType(PI * 0.6), vec3(0.0, 0.0, 1.0));
	S.ScaleMatrix(vec3(5.0, 1.0, 5.0));
	objTransform[4] = T * R * S;
	objInvTransform[4] = objTransform[4].Inverse();

	// PLANE IV
	T.TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	R.RotationMatrix(myType(-PI * 0.6), vec3(0.0, 0.0, 1.0));
	S.ScaleMatrix(vec3(5.0, 1.0, 5.0));
	objTransform[5] = T * R * S;
	objInvTransform[5] = objTransform[5].Inverse();

	// EMPTY
	for (unsigned i = 6; i < OBJ_NUM; ++i)
	{
		objTransform[i].IdentityMatrix();
		objInvTransform[i].IdentityMatrix();
	}

/////////////////////////////////////////////////////////

	int* objTypeIn = new int[OBJ_NUM];
	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		if (i == 0) objTypeIn[i] = CUBE;
		else if (i == 1) objTypeIn[i] = CYLINDER;
		else if (i == 2) objTypeIn[i] = PLANE;
		else if (i == 3) objTypeIn[i] = DISK;
		else if (i == 4) objTypeIn[i] = SQUARE;
		else if (i == 5) objTypeIn[i] = SQUARE;
		else objTypeIn[i] = INVALID;
	}

/////////////////////////////////////////////////////////

	vec3* lightPosition = new vec3[LIGHTS_NUM];
	vec3* lightDir = new vec3[LIGHTS_NUM];
	vec3* lightColor = new vec3[LIGHTS_NUM];
	vec3* lightCoeff = new vec3[LIGHTS_NUM];

	for (unsigned i = 0; i < LIGHTS_NUM; ++i)
	{
		lightPosition[i] = loadVectorFromStream(lightFile, tmp);
		lightDir[i] = loadVectorFromStream(lightFile, tmp).Normalize();
		lightColor[i] = loadVectorFromStream(lightFile, tmp);
		lightCoeff[i] = loadVectorFromStream(lightFile, tmp);
	}

/////////////////////////////////////////////////////////

	vec3* materialCoeff = new vec3[OBJ_NUM * 2];
	vec3* materialColors = new vec3[OBJ_NUM * 3];

	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		for (unsigned j = 0; j < 2; ++j)
		{
			materialCoeff[i * 2 + j] = loadVectorFromStream(materialFile, tmp);
		}

		for (unsigned j = 0; j < 3; ++j)
		{
			materialColors[i * 3 + j] = loadVectorFromStream(materialFile, tmp);
		}
	}

/////////////////////////////////////////////////////////

	vec3* cameraData = new vec3[3];
	myType zoom;

	cameraFile >> zoom;
	for (unsigned i = 0; i < 3; ++i)
	{
		cameraData[i] = loadVectorFromStream(cameraFile, tmp);
	}

/////////////////////////////////////////////////////////

	pixelColorType* frame = new pixelColorType[WIDTH * HEIGHT];

/////////////////////////////////////////////////////////

	clock_t timer;
	timer = clock();

	ViRayMain(objTransform,
			objInvTransform,
			objTypeIn,

			lightPosition,
			lightDir,
			lightColor,
			lightCoeff,

			materialCoeff,
			materialColors,

			cameraData,
			zoom,

			frame);

	timer = clock() - timer;

	cout << "CPU simulation took: " << (float(timer) / CLOCKS_PER_SEC) << " seconds" << endl;

	bitmap_image img((int)WIDTH, (int)HEIGHT);
	for (unsigned h = 0; h < HEIGHT; ++h)
	{
		for (unsigned w = 0; w < WIDTH; ++w)
		{
			unsigned val = (frame[h * WIDTH + w] >> 8);

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

	delete[] cameraData;

	delete[] materialColors;
	delete[] materialCoeff;

	delete[] lightColor;
	delete[] lightPosition;

	delete[] objTypeIn;
	delete[] objInvTransform;
	delete[] objTransform;

	dataFile.close();
	lightFile.close();
	materialFile.close();
	cameraFile.close();

/////////////////////////////////////////////////////////

	return 0;
}
