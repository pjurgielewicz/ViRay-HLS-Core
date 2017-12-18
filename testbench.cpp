#include "main.h"
#include "Utils/bitmap_image.hpp"

#include "iostream"
#include "fstream"
#include <string>
using namespace std;

vec3& loadVectorFromStream(ifstream& file, vec3& vec)
{
	file >> vec.data[0] >> vec.data[1] >> vec.data[2];
	return vec;
}

int main()
{
	string dataPath("C:\\Users\\pjurgiel\\Source\\FFCore\\src\\SimData\\");

	ifstream dataFile((dataPath + "data.dat").c_str());
	ifstream lightFile((dataPath + "light.dat").c_str());
	ifstream materialFile((dataPath + "material.dat").c_str());
	ifstream cameraFile((dataPath + "camera.dat").c_str());

	vec3 tmp;

	/////////////////////////////////////////////////////////

	mat4* objTransform = new mat4[OBJ_NUM];
	mat4* objInvTransform = new mat4[OBJ_NUM];

	// SPHERE IS AT (0, 0, -2.5)
	objTransform[0].TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	objInvTransform[0] = objTransform[0].Inverse();//TranslationMatrix(vec3(0.0, 0.0, 2.5));

	// PLANE
	objTransform[1].TranslationMatrix(loadVectorFromStream(dataFile, tmp));
	objInvTransform[1] = objTransform[1].Inverse();//TranslationMatrix(vec3(0.0, 2.0, 0.0));

	// EMPTY
	for (unsigned i = 2; i < OBJ_NUM; ++i)
	{
		objTransform[i].IdentityMatrix();
		objInvTransform[i].IdentityMatrix();
	}

/////////////////////////////////////////////////////////

	int* objTypeIn = new int[OBJ_NUM];
	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		if (i == 0) objTypeIn[i] = SPHERE;
		else if (i == 1) objTypeIn[i] = PLANE;
		else objTypeIn[i] = INVALID;
	}

/////////////////////////////////////////////////////////

	vec3* lightPosition = new vec3[LIGHTS_NUM];
	vec3* lightColor = new vec3[LIGHTS_NUM];

	for (unsigned i = 0; i < LIGHTS_NUM; ++i)
	{
		lightPosition[i] = loadVectorFromStream(lightFile, tmp);
		lightColor[i] = loadVectorFromStream(lightFile, tmp);
	}

/////////////////////////////////////////////////////////

	vec3* materialCoeff = new vec3[OBJ_NUM];
	vec3* materialColors = new vec3[OBJ_NUM * 3];

	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		materialCoeff[i] = loadVectorFromStream(materialFile, tmp);

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

	FFCore(objTransform,
			objInvTransform,
			objTypeIn,

			lightPosition,
			lightColor,

			materialCoeff,
			materialColors,

			cameraData,
			zoom,

			frame);

	bitmap_image img((int)WIDTH, (int)HEIGHT);
	for (unsigned h = 0; h < HEIGHT; ++h)
	{
		for (unsigned w = 0; w < WIDTH; ++w)
		{
			unsigned val = (frame[h * WIDTH + w] >> 8);
//			cout << val << " ";
//			if (val < 10) cout << "  ";
//			else if (val < 100) cout << " ";

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

	return 0;
}
