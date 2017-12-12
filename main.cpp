#include "Utils/vec4.h"
#include "Utils/vision.h"

#define OBJ_NUM 10
#define WIDTH 200
#define HEIGHT 200
#define FRAME_BUFFER_SIZE (HEIGHT)

void CreateRay(const CCamera& camera, const vec4& posShift, unsigned r, unsigned c, CRay& ray)
{
#pragma HLS PIPELINE
// TODO: Numerical errors may be probably reduced when values are normalized to the 'viewDistance'
// TODO: Remove 'halfs' addition

	vec4 samplePoint(posShift.data[0] + myType(0.5) + c,
					 posShift.data[1] + myType(0.5) + r,
					 0);
	ray = camera.GetCameraRayForPixel(samplePoint);
}

void AssignMatrix(const vec4* mats, vec4* mat, unsigned pos)
{
#pragma HLS PIPELINE
#pragma HLS INLINE
	AssignMatrix_:for (unsigned i = 0; i < 3; ++i)
	{
#pragma HLS UNROLL
		mat[i] = mats[pos * 3 + i];
	}
}

void TransformRay(const vec4* mat, const CRay& ray, CRay& transformedRay)
{
#pragma HLS PIPELINE
#pragma HLS INLINE

	transformedRay.direction = 	vec4(mat[0].Dot3(ray.direction),
								 	 mat[1].Dot3(ray.direction),
									 mat[2].Dot3(ray.direction)).Normalize();

	transformedRay.origin = 	vec4(mat[0].Dot3(ray.origin) + mat[0].data[3],
								 	 mat[1].Dot3(ray.origin) + mat[1].data[3],
									 mat[2].Dot3(ray.origin) + mat[2].data[3]);
}

myType SphereTest(const CRay& transformedRay)
{
#pragma HLS PIPELINE
// unit radius sphere in the global center
	myType b = transformedRay.direction.Dot3(transformedRay.origin);
	myType c = myType(1.0) - transformedRay.origin.Dot3(transformedRay.origin);
	myType d2 = b * b - c;

	if (d2 >= myType(0.0))
	{
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
	}

	return myType(-1);
}

myType PlaneTest(const CRay& transformedRay)
{
#pragma HLS PIPELINE
// XZ - plane pointing +Y
	myType t = -transformedRay.origin.data[1] * (myType(1) / transformedRay.direction.data[1]);
	if (t > myType(0))
	{
		return t;
	}
	return myType(-1);
}

myType PerformHits(const CRay& transformedRay, unsigned objType)
{
//#pragma HLS INLINE
#pragma HLS PIPELINE
	myType res = myType(-1);

	switch(objType)
	{
	case SPHERE:
		res = SphereTest(transformedRay);
		break;
	case PLANE:
		res = PlaneTest(transformedRay);
		break;
	default:
		break;
	}

	return res;
}

void InnerLoop(const CCamera& camera,
				const vec4& posShift,
				const vec4* mats,
				const unsigned* objType,
				unsigned w,
				pixelColorType* frameBuffer)
{
#pragma HLS INLINE

	myType currentClosest;
	int currentObjIdx;

	vec4 loadedRayData[2];
#pragma HLS ARRAY_PARTITION variable=loadedRayData complete dim=1
//	vec4 transformedRayData[2];
//#pragma HLS ARRAY_PARTITION variable=transformedRayData complete dim=1
	vec4 mat[3];
#pragma HLS ARRAY_PARTITION variable=mat cyclic factor=3 dim=1

	CRay ray, transformedRay;

	InnerLoop: for (unsigned h = 0; h < HEIGHT; ++h)
	{
//#pragma HLS DATAFLOW
#pragma HLS PIPELINE
DO_PRAGMA(HLS UNROLL factor=OUTER_LOOP_UNROLL_FACTOR)
		CreateRay(camera, posShift, w, h, ray);
		currentClosest = myType(-1);
		currentObjIdx = -1;


		ObjectsLoop: for (unsigned n = 0; n < OBJ_NUM; ++n)
		{
//#pragma HLS UNROLL
#pragma HLS PIPELINE
			AssignMatrix(mats, mat, n);
			TransformRay(mat, ray, transformedRay);
			myType res = PerformHits(transformedRay, objType[n]);

			if (res != myType(-1) && res < currentClosest)
			{
				currentClosest = res;
				currentObjIdx = n;
			}
		}
//		color[0] = currentClosest;
//		color[1] = currentObjIdx;
//		color[2] = currentClosest;

		// TODO: The final result will consist of RGBA value (32 bit)
		frameBuffer[h * 3 + 0] = currentClosest;
//		frameBuffer[h * 3 + 1] = currentObjIdx;
//		frameBuffer[h * 3 + 2] = currentObjIdx;
	}
}

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

	vec4 mats[OBJ_NUM * 3];
// WARNING: MAKES THINGS BLAZINGLY FAST
//#pragma HLS ARRAY_PARTITION variable=mats complete dim=1
	memcpy(mats, dataIn, sizeof(vec4) * 3 * OBJ_NUM);

	pixelColorType color[3], frameBuffer[FRAME_BUFFER_SIZE];
#pragma HLS ARRAY_PARTITION variable=color complete dim=1
//#pragma HLS ARRAY_PARTITION variable=frameBuffer complete dim=1
	myType currentClosest;
	int currentObjIdx;

	unsigned objType[OBJ_NUM];
#pragma HLS ARRAY_PARTITION variable=objType complete dim=1
	memcpy(objType, objTypeIn, sizeof(unsigned) * OBJ_NUM);

	for (unsigned w = 0; w < WIDTH; ++w)
	{
#pragma HLS DATAFLOW
		InnerLoop(camera, posShift, mats, objType, w, frameBuffer);

		//TODO: Change output color to RGBA (4x8b) unsigned -> will reduce copy time by 3
		memcpy(outColor, frameBuffer, sizeof(pixelColorType) * FRAME_BUFFER_SIZE);
	}

	return 0;

}
