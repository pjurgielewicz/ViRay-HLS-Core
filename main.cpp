#include "main.h"
#include "iostream"
using namespace std;

// TODO: change to more effective types (vec4 -> vec3), (vec4[3] -> mat4)


void CreateRay(const CCamera& camera, const vec4& posShift, unsigned r, unsigned c, CRay& ray)
{
#pragma HLS PIPELINE
// TODO: Numerical errors may be probably reduced when values are normalized to the 'viewDistance'
// TODO: Remove 'halfs' addition

	vec4 samplePoint(posShift.data[0] + myType(0.5) + c,
					 posShift.data[1] + myType(0.5) + r,
					 0);
	ray = camera.GetCameraRayForPixel(samplePoint);

//	cout << samplePoint.data[0] << " x " << samplePoint.data[1] << "\t->\t";
//	for (unsigned i = 0; i < 3; ++i)
//	{
//		cout << ray.direction.data[i] << ", ";
//	}
//	cout << endl;
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

//	for (unsigned i = 0; i < 3; ++i)
//	{
//		cout << transformedRay.direction.data[i] << ", ";
//	}
//	cout << endl;
}

myType SphereTest(const CRay& transformedRay, ShadeRec& sr)
{
#pragma HLS PIPELINE
// unit radius sphere in the global center
	myType b = transformedRay.direction.Dot3(transformedRay.origin);
	myType c = transformedRay.origin.Dot3(transformedRay.origin) - myType(1.0);
	myType d2 = b * b - c;

	if (d2 >= myType(0.0))
	{
		myType d = hls::sqrt(d2);

		myType t = -b - d;
		if (t > CORE_BIAS)
		{
			return t;
		}
		t = -b + d;
		if (t > CORE_BIAS)
		{
			return t;
		}
	}

	return myType(-1);
}

myType PlaneTest(const CRay& transformedRay, ShadeRec& sr)
{
#pragma HLS PIPELINE
// XZ - plane pointing +Y
	myType t = -transformedRay.origin.data[1] * (myType(1) / (transformedRay.direction.data[1] + CORE_BIAS)); // BIAS removes zero division problem
	if (t > CORE_BIAS)
	{
		return t;
	}
	return myType(-1);
}

void PerformHits(const CRay& transformedRay, unsigned objType, ShadeRec& sr)
{
//#pragma HLS INLINE
#pragma HLS PIPELINE
	myType res = myType(-1);

	switch(objType)
	{
	case SPHERE:
		res = SphereTest(transformedRay, sr);
		sr.normal = transformedRay.origin + transformedRay.direction * res;
		break;
	case PLANE:
		res = PlaneTest(transformedRay, sr);
		sr.normal = vec4(myType(0), myType(1), myType(0));
		break;
	default:
		sr.normal = vec4(myType(-1), myType(-1), myType(-1));
		break;
	}

	sr.localHitPoint = transformedRay.origin + transformedRay.direction * res;
	sr.distance = res;
}

void UpdateClosestObject(const ShadeRec& current, int n, ShadeRec& best)
{
#pragma HLS INLINE
	if (current.distance != myType(-1) && current.distance < best.distance)
	{
		best.distance = current.distance;
		best.localHitPoint = current.localHitPoint;
		best.normal = current.normal;

		best.objIdx = n;
	}
}

void InnerLoop(const CCamera& camera,
				const vec4& posShift,
				const vec4* objTransform,
				const vec4* objTransformInv,
				const unsigned* objType,
				int h,
				pixelColorType* frameBuffer)
{
#pragma HLS INLINE

	InnerLoop: for (int w = 0; w < WIDTH; ++w)
	{
//#pragma HLS DATAFLOW
#pragma HLS PIPELINE
DO_PRAGMA(HLS UNROLL factor=OUTER_LOOP_UNROLL_FACTOR)

		CRay ray, transformedRay;
		ShadeRec sr, closestSr;

		CreateRay(camera, posShift, h, w, ray);

		ObjectsLoop: for (unsigned n = 0; n < OBJ_NUM; ++n)
		{
//#pragma HLS UNROLL
#pragma HLS PIPELINE
			vec4 transformInv[3];
#pragma HLS ARRAY_PARTITION variable=transformInv complete dim=1
//#pragma HLS ARRAY_PARTITION variable=transformInv cyclic factor=3 dim=1
			AssignMatrix(objTransformInv, transformInv, n);
			TransformRay(transformInv, ray, transformedRay);
			PerformHits(transformedRay, objType[n], sr);
			UpdateClosestObject(sr, n, closestSr);
		}
		vec4 transform[3];
#pragma HLS ARRAY_PARTITION variable=transform complete dim=1
//#pragma HLS ARRAY_PARTITION variable=transform cyclic factor=3 dim=1

		if (closestSr.objIdx != -1)
		{
			AssignMatrix(objTransform, transform, closestSr.objIdx);


			closestSr.hitPoint = vec4(transform[0].Dot3(closestSr.localHitPoint) + transform[0].data[3],
										transform[1].Dot3(closestSr.localHitPoint) + transform[1].data[3],
										transform[2].Dot3(closestSr.localHitPoint) + transform[2].data[3]);

			closestSr.normal = closestSr.normal.Normalize();

		}
//		 TODO: The final result will consist of RGBA value (32 bit)
		frameBuffer[w] = closestSr.hitPoint.data[0] + closestSr.normal.data[1];

//	***	DEBUG ***
//		frameBuffer[w] = pixelColorType(closestSr.objIdx);


	}
}

int FFCore(const vec4* objTransformIn,
			const vec4* objInvTransformIn,
			unsigned dataInSize, // ???
			const int* objTypeIn,
			pixelColorType* outColor)
{
#pragma HLS INTERFACE s_axilite port=objTransformIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objTransformIn offset=slave bundle=MAXI_IN_1

#pragma HLS INTERFACE s_axilite port=objInvTransformIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objInvTransformIn offset=slave bundle=MAXI_IN_1

#pragma HLS INTERFACE s_axilite port=dataInSize bundle=AXI_LITE_1

#pragma HLS INTERFACE s_axilite port=objTypeIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objTypeIn offset=slave bundle=MAXI_IN_2

#pragma HLS INTERFACE s_axilite port=outColor bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=outColor offset=slave bundle=MAXI_FRAME

#pragma HLS INTERFACE s_axilite port=return bundle=AXI_LITE_1

	CRay ray;
	CCamera camera(myType(2.0), myType(2.0),
				   HEIGHT, WIDTH);

	vec4 posShift(myType(-0.5) * (WIDTH - myType(1.0)),
				  myType(-0.5) * (HEIGHT - myType(1.0)),
				  myType(0.0));

	vec4 objTransform[OBJ_NUM * 3], objInvTransform[OBJ_NUM * 3];
// WARNING: MAKES THINGS BLAZINGLY FAST
//#pragma HLS ARRAY_PARTITION variable=objInvTransform cyclic factor=3 dim=1
//#pragma HLS ARRAY_PARTITION variable=objInvTransform complete dim=1
	memcpy(objTransform, objTransformIn, sizeof(vec4) * 3 * OBJ_NUM);
	memcpy(objInvTransform, objInvTransformIn, sizeof(vec4) * 3 * OBJ_NUM);

	pixelColorType frameBuffer[FRAME_BUFFER_SIZE];
//#pragma HLS ARRAY_PARTITION variable=frameBuffer complete dim=1
	myType currentClosest;
	int currentObjIdx;

	unsigned objType[OBJ_NUM];
#pragma HLS ARRAY_PARTITION variable=objType complete dim=1
	memcpy(objType, objTypeIn, sizeof(unsigned) * OBJ_NUM);

	for (int h = 0; h < HEIGHT; ++h)
	{
#pragma HLS DATAFLOW
		InnerLoop(camera,
					posShift,
					objTransform,
					objInvTransform,
					objType,
					HEIGHT - 1 - h,
					frameBuffer);

		//TODO: Change output color to RGBA (4x8b) unsigned -> will reduce copy time by 3
		memcpy(outColor + FRAME_BUFFER_SIZE * h, frameBuffer, sizeof(pixelColorType) * FRAME_BUFFER_SIZE);
	}

	return 0;

}
