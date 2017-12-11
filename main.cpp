#include "Utils/vec4.h"
#include "Utils/mat4.h"
#include "Utils/vision.h"

void CreateRay(const CCamera& camera, const vec4& posShift, unsigned r, unsigned c, vec4* rayData)
{
#pragma HLS PIPELINE
	vec4 samplePoint(posShift.data[0] + myType(0.5) + c,
					 posShift.data[1] + myType(0.5) + r,
					 0);
	CRay ray = camera.GetCameraRayForPixel(samplePoint);
	rayData[0] = ray.direction;
	rayData[1] = ray.origin;
}

void AssignMatrix(const vec4* mats, vec4* mat, unsigned pos)
{
#pragma HLS PIPELINE
	AssignMatrix_:for (unsigned i = 0; i < 3; ++i)
	{
#pragma HLS UNROLL
		mat[i] = mats[pos * 3 + i];
	}
}

void TransformRay(const vec4* mat, const vec4* rayData, vec4* transformedRayData)
{
#pragma HLS PIPELINE
#pragma HLS INLINE
	// DIRECTION
	transformedRayData[0].data[0] = mat[0].Dot3(rayData[0]);
	transformedRayData[0].data[1] = mat[1].Dot3(rayData[0]);
	transformedRayData[0].data[2] = mat[2].Dot3(rayData[0]);

	// Transformation may change the magnitude
//	transformedRayData[0] = transformedRayData[0].Normalize();

	// ORIGIN
	transformedRayData[1].data[0] = mat[0].Dot3(rayData[1]) + mat[0].data[3];
	transformedRayData[1].data[1] = mat[1].Dot3(rayData[1]) + mat[1].data[3];
	transformedRayData[1].data[2] = mat[2].Dot3(rayData[1]) + mat[2].data[3];
}

myType SphereTest(const vec4* rayData)
{
#pragma HLS PIPELINE
	myType b = rayData[0].Dot3(rayData[1]);
	myType c = myType(1) - rayData[1].Magnitude();
	myType d2 = b * b - c;

	if (d2 < myType(0))
	{
		return myType(-1);
	}

	myType d = hls::sqrt(d2);

	myType t = -b - d;
	if (t > myType(0))
	{
		return t;
	}
	t = -b + d;
	if (t > myType(0))
	{
		return t;
	}

	return myType(-1);
}

myType PlaneTest(const vec4* rayData)
{
#pragma HLS PIPELINE
// XZ - plane pointing +Y
	myType t = -rayData[1].data[1] * (myType(1) / rayData[0].data[1]);
	if (t > myType(0))
	{
		return t;
	}
	return myType(-1);
}

myType PerformHits(const vec4* transformedRayData, unsigned objType)
{
//#pragma HLS INLINE
#pragma HLS PIPELINE
	myType res = myType(-1);

	switch(objType)
	{
	case SPHERE:
		res = SphereTest(transformedRayData);
		break;
	case PLANE:
		res = PlaneTest(transformedRayData);
		break;
	default:
		break;
	}

	return res;
}

#define OBJ_NUM 10
#define WIDTH 200
#define HEIGHT 200

int FFCore(const vec4* dataIn, unsigned dataInSize,
		   const unsigned* objTypeIn,
			pixelColorType* outColor)
{
#pragma HLS INTERFACE s_axilite port=dataIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=dataIn offset=slave bundle=MAXI_IN_1

#pragma HLS INTERFACE s_axilite port=dataInSize bundle=AXI_LITE_1

#pragma HLS INTERFACE s_axilite port=objTypeIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objTypeIn offset=slave bundle=MAXI_IN_2

#pragma HLS INTERFACE s_axilite port=outColor bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=outColor offset=slave bundle=MAXI_FRAME

#pragma HLS INTERFACE s_axilite port=return bundle=AXI_LITE_1

	CRay ray;
	CCamera camera;

	vec4 posShift(myType(-0.5) * (WIDTH - myType(1.0)),
				  myType(-0.5) * (HEIGHT - myType(1.0)),
				  myType(0.0));

	vec4 loadedRayData[2], transformedRayData[2], vecColor;
#pragma HLS ARRAY_PARTITION variable=loadedRayData complete dim=1
#pragma HLS ARRAY_PARTITION variable=transformedRayData complete dim=1
#pragma HLS ARRAY_PARTITION variable=vecColor complete dim=1

	vec4 mats[OBJ_NUM * 3];
	memcpy(mats, dataIn, sizeof(vec4) * 3 * OBJ_NUM);

	pixelColorType color[3], frameBuffer[3 * WIDTH * HEIGHT];
#pragma HLS ARRAY_PARTITION variable=color complete dim=1
//#pragma HLS ARRAY_PARTITION variable=frameBuffer complete dim=1
	myType currentClosest;
	int currentObjIdx;

	unsigned objType[OBJ_NUM];
#pragma HLS ARRAY_PARTITION variable=objType complete dim=1
	memcpy(objType, objTypeIn, sizeof(unsigned) * OBJ_NUM);

	for (unsigned w = 0; w < WIDTH; ++w)
	{
// TODO: ENCLOSE IN FUNCTION -> DATAFLOW (calculation + save) -> possible gain in time
		InnerLoop: for (unsigned h = 0; h < HEIGHT; ++h)
		{
//#pragma HLS DATAFLOW
#pragma HLS PIPELINE
DO_PRAGMA(HLS UNROLL factor=OUTER_LOOP_UNROLL_FACTOR)
			CreateRay(camera, posShift, w, h, loadedRayData);
			currentClosest = myType(-1);
			currentObjIdx = -1;

			vec4 mat[3];
#pragma HLS ARRAY_PARTITION variable=mat complete dim=1

			ObjectsLoop: for (unsigned n = 0; n < OBJ_NUM; ++n)
			{
//#pragma HLS UNROLL
#pragma HLS PIPELINE
				AssignMatrix(mats, mat, n);
				TransformRay(mat, loadedRayData, transformedRayData);
				myType res = PerformHits(transformedRayData, objType[n]);

				if (res != myType(-1) && res < currentClosest)
				{
					currentClosest = res;
					currentObjIdx = n;
				}
			}
			color[0] = currentClosest;
			color[1] = currentObjIdx;
			color[2] = currentClosest;

			frameBuffer[(w * HEIGHT + h) * 3 + 0] = color[0];
			frameBuffer[(w * HEIGHT + h) * 3 + 1] = color[1];
			frameBuffer[(w * HEIGHT + h) * 3 + 2] = color[2];

//			frameBuffer[h * 3 + 0] = color[0];
//			frameBuffer[h * 3 + 1] = color[1];
//			frameBuffer[h * 3 + 2] = color[2];

		}
//		memcpy(outColor, frameBuffer, sizeof(pixelColorType) * 3 * HEIGHT);
	}

	memcpy(outColor, frameBuffer, sizeof(pixelColorType) * 3 * WIDTH * HEIGHT);

	return 0;

}
