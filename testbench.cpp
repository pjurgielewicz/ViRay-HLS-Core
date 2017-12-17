#include "main.h"

#include "iostream"
using namespace std;

int main()
{

	mat4* objTransform = new mat4[OBJ_NUM];
	mat4* objInvTransform = new mat4[OBJ_NUM];

	// SPHERE IS AT (0, 0, -2.5)
	objTransform[0].TranslationMatrix(vec3(0.0, 0.0, -2.5));
	objInvTransform[0].TranslationMatrix(vec3(0.0, 0.0, 2.5));


//	objTransform[0] = vec4(1, 0, 0, 0);
//	objTransform[1] = vec4(0, 1, 0, 0);
//	objTransform[2] = vec4(0, 0, 1, -2.5);
//
//	objInvTransform[0] = vec4(1, 0, 0, 0);
//	objInvTransform[1] = vec4(0, 1, 0, 0);
//	objInvTransform[2] = vec4(0, 0, 1, 2.5);

	// PLANE
	objTransform[1].TranslationMatrix(vec3(0.0, -2.0, 0.0));
	objInvTransform[1].TranslationMatrix(vec3(0.0, 2.0, 0.0));

//	objTransform[3] = vec4(1, 0, 0, 0);
//	objTransform[4] = vec4(0, 1, 0, -2);
//	objTransform[5] = vec4(0, 0, 1, 0);
//
//	objInvTransform[3] = vec4(1, 0, 0, 0);
//	objInvTransform[4] = vec4(0, 1, 0, 2);
//	objInvTransform[5] = vec4(0, 0, 1, 0);

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
		lightPosition[i] = vec3(0, 10, 0);
		if (i == 0) lightColor[i] = vec3(0, 0, 0);
		else lightColor[i] = vec3(0, 0, 1);
	}

/////////////////////////////////////////////////////////

	vec3* materialCoeff = new vec3[OBJ_NUM];
	vec3* materialColors = new vec3[OBJ_NUM * 3];

	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		materialCoeff[i] = vec3(0, 1, 0);				// GO ONLY WITH DIFFUSE
		materialColors[i * 3 + 1] = vec3(0, 0, 1);		// SET DIFFUSE RESPONSE TO FULL BLUE
	}

/////////////////////////////////////////////////////////

	pixelColorType* frame = new pixelColorType[WIDTH * HEIGHT];

	FFCore(objTransform,
			objInvTransform,
			objTypeIn,

			lightPosition,
			lightColor,

			materialCoeff,
			materialColors,

			frame);

	for (unsigned h = 0; h < HEIGHT; ++h)
	{
		for (unsigned w = 0; w < WIDTH; ++w)
		{
			if (frame[h * WIDTH + w] != -1){
				unsigned val = (frame[h * WIDTH + w] >> 8);
				cout << val << " ";
				if (val < 10) cout << "  ";
				else if (val < 100) cout << " ";
			}
			else cout << "    ";
		}
		cout << endl;
	}

/////////////////////////////////////////////////////////

	delete[] frame;

	delete[] materialColors;
	delete[] materialCoeff;

	delete[] lightColor;
	delete[] lightPosition;

	delete[] objTypeIn;
	delete[] objInvTransform;
	delete[] objTransform;

	return 0;
}
