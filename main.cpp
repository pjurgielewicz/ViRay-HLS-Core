#include "Utils/vec4.h"
#include "Utils/mat4.h"
#include "Utils/vision.h"

void CreateRay(const CCamera& camera, const vec4& posShift, unsigned r, unsigned c, vec4* rayData)
{
	vec4 samplePoint(posShift.data[0] + myType(0.5) + c,
					 posShift.data[1] + myType(0.5) + r,
					 0);
	CRay ray = camera.GetCameraRayForPixel(samplePoint);
	rayData[0] = ray.direction;
	rayData[1] = ray.origin;
}

void AssignMatrix(const vec4* mats, vec4* mat, unsigned pos)
{
	AssignMatrix_:for (unsigned i = 0; i < 3; ++i)
	{
		mat[i] = mats[pos * 3 + i];
	}
}

void TransformRay(const vec4* mat, const vec4* rayData, vec4* transformedRayData)
{
	transformedRayData[0].data[0] = mat[0].Dot3(rayData[0]);
	transformedRayData[0].data[1] = mat[1].Dot3(rayData[0]);
	transformedRayData[0].data[2] = mat[2].Dot3(rayData[0]);

	transformedRayData[1].data[0] = mat[0].Dot3(rayData[1]) + mat[0].data[3];
	transformedRayData[1].data[1] = mat[1].Dot3(rayData[1]) + mat[1].data[3];
	transformedRayData[1].data[2] = mat[2].Dot3(rayData[1]) + mat[2].data[3];
}

myType SphereTest(const vec4* rayData)
{
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
// XZ - plane pointing +Y
	myType t = -rayData[1].data[1] * (myType(1) / rayData[0].data[1]);
	if (t > myType(0))
	{
		return t;
	}
	return myType(-1);
}

int FFCore(const vec4* dataIn, unsigned dataInSize,
			pixelColorType* outColor)
{
#pragma HLS INTERFACE s_axilite port=dataIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=dataIn offset=slave bundle=MAXI_OUT

#pragma HLS INTERFACE s_axilite port=dataInSize bundle=AXI_LITE_1

#pragma HLS INTERFACE s_axilite port=outColor bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=outColor offset=slave bundle=MAXI_OUT

#pragma HLS INTERFACE s_axilite port=return bundle=AXI_LITE_1

	unsigned width = 200;
	unsigned height = 200;

	CRay ray;
	CCamera camera;

	vec4 posShift(myType(-0.5) * (width - myType(1.0)),
				  myType(-0.5) * (height - myType(1.0)),
				  myType(0.0));

	vec4 loadedRayData[2], transformedRayData[2], vecColor;
#pragma HLS ARRAY_PARTITION variable=loadedRayData complete dim=1
#pragma HLS ARRAY_PARTITION variable=transformedRayData complete dim=1
#pragma HLS ARRAY_PARTITION variable=vecColor complete dim=1

	unsigned objs = 12;
	vec4 mats[36];
//	vec4 mat[3], mats[36];
//#pragma HLS ARRAY_PARTITION variable=mat complete dim=1
	memcpy(mats, dataIn, sizeof(vec4) * 3 * objs);

	pixelColorType color[3];
#pragma HLS ARRAY_PARTITION variable=color complete dim=1
	myType currentClosest;


	for (unsigned w = 0; w < width; ++w)
	{
		InnerLoop: for (unsigned h = 0; h < height; ++h)
		{
//#pragma HLS DATAFLOW
#pragma HLS PIPELINE
			CreateRay(camera, posShift, w, h, loadedRayData);
			currentClosest = myType(-1);

			vec4 mat[3];
#pragma HLS ARRAY_PARTITION variable=mat complete dim=1

			ObjectsLoop: for (unsigned n = 0; n < 20/*dataInSize*/; ++n)
			{
#pragma HLS UNROLL factor=2 region
#pragma HLS PIPELINE
//#pragma HLS UNROLL factor=2
//#pragma HLS LOOP_TRIPCOUNT min=12 max=12
				AssignMatrix(mats, mat, n);
				TransformRay(mat, loadedRayData, transformedRayData);
				myType res;

				if (n & 0x1)
				{
					res = SphereTest(transformedRayData);
				}
				else
				{
					res = PlaneTest(transformedRayData);
				}

				if (res != myType(-1) && res < currentClosest)
				{
					currentClosest = res;
				}
			}
			color[0] = currentClosest;
			color[1] = currentClosest;
			color[2] = currentClosest;

			memcpy(outColor + (w * height + h) * 3, &color, sizeof(pixelColorType) * 3);
		}
	}

	return 0;

}
