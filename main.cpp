#include "main.h"
#include "iostream"
using namespace std;

// TODO: change to more effective types (vec4 -> vec3), (vec4[3] -> mat4)


void CreateRay(const CCamera& camera, const myType* posShift, unsigned r, unsigned c, CRay& ray)
{
#pragma HLS PIPELINE
// TODO: Numerical errors may be probably reduced when values are normalized to the 'viewDistance'
// TODO: Remove 'halfs' addition

	myType samplePoint[2] = {posShift[0] + myType(0.5) + c,
					 	 	 posShift[1] + myType(0.5) + r};
	ray = camera.GetCameraRayForPixel(samplePoint);

//	cout << samplePoint.data[0] << " x " << samplePoint.data[1] << "\t->\t";
//	for (unsigned i = 0; i < 3; ++i)
//	{
//		cout << ray.direction.data[i] << ", ";
//	}
//	cout << endl;
}

void AssignMatrix(const mat4* mats, mat4& mat, unsigned pos)
{
#pragma HLS PIPELINE
#pragma HLS INLINE
	mat = mats[pos];
}

void TransformRay(const mat4& mat, const CRay& ray, CRay& transformedRay)
{
#pragma HLS PIPELINE
#pragma HLS INLINE

	transformedRay.direction = mat.TransformDir(ray.direction);
	transformedRay.origin = mat.Transform(ray.origin);

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
	myType b = transformedRay.direction * transformedRay.origin;
	myType c = (transformedRay.origin * transformedRay.origin) - myType(1.0);
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

	return myType(-1.0);
}

myType PlaneTest(const CRay& transformedRay, ShadeRec& sr)
{
#pragma HLS PIPELINE
// XZ - plane pointing +Y
	myType t = -transformedRay.origin[1] * (myType(1.0) / (transformedRay.direction[1] + CORE_BIAS)); // BIAS removes zero division problem
	if (t > CORE_BIAS)
	{
		return t;
	}
	return myType(-1.0);
}

void PerformHits(const CRay& transformedRay, unsigned objType, ShadeRec& sr)
{
//#pragma HLS INLINE
#pragma HLS PIPELINE
	myType res = myType(-1.0);

	switch(objType)
	{
	case SPHERE:
		res = SphereTest(transformedRay, sr);
		sr.normal = transformedRay.origin + transformedRay.direction * res;
		break;
	case PLANE:
		res = PlaneTest(transformedRay, sr);
		sr.normal = vec3(myType(0.0), myType(1.0), myType(0.0));
		break;
	default:
		sr.normal = vec3(myType(-1.0), myType(-1.0), myType(-1.0));
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

void SaveColorToBuffer(vec3& color, pixelColorType& colorOut)
{
#pragma HLS INLINE

	pixelColorType tempColor = 0;

	for (unsigned i = 0; i < 3; ++i)
	{
		if (color[i] > myType(1.0))
			color[i] = myType(1.0);

		tempColor += (( unsigned(color[i] * myType(255)) & 0xFF ) << ((3 - i) * 8));
	}
	colorOut = tempColor;
}

void InnerLoop(const CCamera& camera,
				const myType* posShift,
				const mat4* objTransform,
				const mat4* objTransformInv,
				const unsigned* objType,
				int h,

				const Light* lights,
				const Material* materials,

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
			mat4 transformInv;

			AssignMatrix(objTransformInv, transformInv, n);
			TransformRay(transformInv, ray, transformedRay);
			PerformHits(transformedRay, objType[n], sr);
			UpdateClosestObject(sr, n, closestSr);
		}
		mat4 transform;
		vec3 color(0, 0, 0);

		if (closestSr.objIdx != -1)
		{
			AssignMatrix(objTransform, transform, closestSr.objIdx);

			closestSr.hitPoint = transform.Transform(closestSr.localHitPoint);
			closestSr.normal = closestSr.normal.Normalize();

			color = materials[closestSr.objIdx].ambientColor.CompWiseMul(lights[0].color);

			for (unsigned l = 1; l < LIGHTS_NUM; ++l)
			{
#pragma HLS PIPELINE
				vec3 dirToLight = (lights[l].position - closestSr.hitPoint).Normalize();
				myType dot = closestSr.normal * dirToLight;

				if (dot > myType(0.0))
				{
					color += materials[closestSr.objIdx].diffuseColor.CompWiseMul(lights[l].color) * dot;
				}
			}

		}
//		 TODO: The final result will consist of RGBA value (32 bit)
		SaveColorToBuffer(color, frameBuffer[w]);

//	***	DEBUG ***
//		frameBuffer[w] = pixelColorType(closestSr.objIdx);


	}
}

int FFCore(const mat4* objTransformIn,
			const mat4* objInvTransformIn,
			const int* objTypeIn,

			vec3* lightPositionIn,
			vec3* lightColorIn,

			vec3* materialCoeffIn,
			vec3* materialColorsIn,

			pixelColorType* outColor)
{
	/*
	 * OBJECTS
	 */

#pragma HLS INTERFACE s_axilite port=objTransformIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objTransformIn offset=slave bundle=MAXI_IN_1

#pragma HLS INTERFACE s_axilite port=objInvTransformIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objInvTransformIn offset=slave bundle=MAXI_IN_1

#pragma HLS INTERFACE s_axilite port=objTypeIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=objTypeIn offset=slave bundle=MAXI_IN_1

	/*
	 * LIGHTS
	 */

#pragma HLS INTERFACE s_axilite port=lightPositionIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=lightPositionIn offset=slave bundle=MAXI_IN_LIGHT

#pragma HLS INTERFACE s_axilite port=lightColorIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=lightColorIn offset=slave bundle=MAXI_IN_LIGHT

	/*
	 * MATERIALS
	 */

#pragma HLS INTERFACE s_axilite port=materialCoeffIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=materialCoeffIn offset=slave bundle=MAXI_IN_MATERIALS

#pragma HLS INTERFACE s_axilite port=materialColorsIn bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=materialColorsIn offset=slave bundle=MAXI_IN_MATERIALS

	/*
	 * FRAMEBUFFER
	 */

#pragma HLS INTERFACE s_axilite port=outColor bundle=AXI_LITE_1
#pragma HLS INTERFACE m_axi port=outColor offset=slave bundle=MAXI_FRAME

#pragma HLS INTERFACE s_axilite port=return bundle=AXI_LITE_1

	CRay ray;
	CCamera camera(myType(2.0), myType(2.0),
				   HEIGHT, WIDTH);

	myType posShift[2] = {	myType(-0.5) * (WIDTH - myType(1.0)),
				  	 		myType(-0.5) * (HEIGHT - myType(1.0))};

	mat4 objTransform[OBJ_NUM], objInvTransform[OBJ_NUM];
//#pragma HLS ARRAY_PARTITION variable=objInvTransform cyclic factor=2 dim=1
	memcpy(objTransform, objTransformIn, sizeof(mat4) * OBJ_NUM);
	memcpy(objInvTransform, objInvTransformIn, sizeof(mat4) * OBJ_NUM);

	pixelColorType frameBuffer[FRAME_BUFFER_SIZE];
//#pragma HLS ARRAY_PARTITION variable=frameBuffer complete dim=1
	myType currentClosest;
	int currentObjIdx;

	unsigned objType[OBJ_NUM];
#pragma HLS ARRAY_PARTITION variable=objType complete dim=1
	memcpy(objType, objTypeIn, sizeof(unsigned) * OBJ_NUM);

	/*
	 * LIGHTING AND MATERIALS
	 */

	Light lights[LIGHTS_NUM];
//#pragma HLS ARRAY_PARTITION variable=lights complete dim=1
	for (unsigned i = 0; i < LIGHTS_NUM; ++i)
	{
#pragma HLS PIPELINE
		lights[i].position = lightPositionIn[i];
		lights[i].color = lightColorIn[i];
	}

	Material materials[OBJ_NUM];
//#pragma HLS ARRAY_PARTITION variable=materials complete dim=1
	for (unsigned i = 0; i < OBJ_NUM; ++i)
	{
#pragma HLS PIPELINE
		materials[i].k = materialCoeffIn[i];
		materials[i].ambientColor = materialColorsIn[i * 3 + 0];
		materials[i].diffuseColor = materialColorsIn[i * 3 + 1];
		materials[i].specularColor = materialColorsIn[i * 3 + 2];
	}

	/*
	 *
	 */

	for (int h = 0; h < HEIGHT; ++h)
	{
#pragma HLS DATAFLOW
		InnerLoop(camera,
					posShift,
					objTransform,
					objInvTransform,
					objType,
					HEIGHT - 1 - h,
					lights,
					materials,
					frameBuffer);

		//TODO: Change output color to RGBA (4x8b) unsigned -> will reduce copy time by 3
		memcpy(outColor + FRAME_BUFFER_SIZE * h, frameBuffer, sizeof(pixelColorType) * FRAME_BUFFER_SIZE);
	}

	return 0;

}
