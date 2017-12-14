#include "main.h"

#include "iostream"
using namespace std;

int main()
{

	vec4* objTransform = new vec4[OBJ_NUM * 3];
	vec4* objInvTransform = new vec4[OBJ_NUM * 3];

	// SPHERE IS AT (0, 0, -2.5)
	objTransform[0] = vec4(1, 0, 0, 0);
	objTransform[1] = vec4(0, 1, 0, 0);
	objTransform[2] = vec4(0, 0, 1, -2.5);

	objInvTransform[0] = vec4(1, 0, 0, 0);
	objInvTransform[1] = vec4(0, 1, 0, 0);
	objInvTransform[2] = vec4(0, 0, 1, 2.5);

	// PLANE
	objTransform[3] = vec4(1, 0, 0, 0);
	objTransform[4] = vec4(0, 1, 0, -2);
	objTransform[5] = vec4(0, 0, 1, 0);

	objInvTransform[3] = vec4(1, 0, 0, 0);
	objInvTransform[4] = vec4(0, 1, 0, 2);
	objInvTransform[5] = vec4(0, 0, 1, 0);

	// EMPTY
	for (unsigned i = 6; i < 3 * OBJ_NUM; ++i)
	{
		objTransform[i] = vec4(1, 1, 1);
		objInvTransform[i] = vec4(1, 1, 1);
	}

/////////////////////////////////////////////////////////

	int* objTypeIn = new int[OBJ_NUM];
	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
		if (i == 0) objTypeIn[i] = SPHERE;
		else if (i == 1) objTypeIn[i] = PLANE;
		else objTypeIn[i] = INVALID;
	}

	pixelColorType* frame = new pixelColorType[WIDTH * HEIGHT];

	FFCore(objTransform, objInvTransform, OBJ_NUM, objTypeIn, frame);

	for (unsigned h = 0; h < HEIGHT; ++h)
	{
		for (unsigned w = 0; w < WIDTH; ++w)
		{
			if (frame[h * WIDTH + w] != -1)
				cout << frame[h * WIDTH + w] << " ";
			else cout << "  ";
		}
		cout << endl;
	}

	delete[] frame;
	delete[] objTypeIn;
	delete[] objInvTransform;
	delete[] objTransform;

	return 0;
}
