#include "viray.h"

#include "iostream"
using namespace std;

namespace ViRay
{

void RenderScene(const CCamera& camera,
					const myType* posShift,
					const mat4* objTransform,
					const mat4* objTransformInv,
					const unsigned* objType,

					const Light* lights,
					const Material* materials,

					pixelColorType* frameBuffer,
					pixelColorType* outColor)
{
#pragma HLS INLINE
	for (int h = 0; h < HEIGHT; ++h)
	{
#pragma HLS DATAFLOW
		InnerLoop(camera,
				posShift,
				objTransform,
				objTransformInv,
				objType,
				HEIGHT - 1 - h,
				lights,
				materials,
				frameBuffer);
		memcpy(outColor + FRAME_BUFFER_SIZE * h, frameBuffer, sizeof(pixelColorType) * FRAME_BUFFER_SIZE);
	}
}

// ****************   THE REAL CORE OF THE PROCESSING   ****************
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

	const vec3 clearColor(myType(0.0), myType(0.0), myType(0.0));

	InnerLoop: for (int w = 0; w < WIDTH; ++w)
	{
#pragma HLS PIPELINE
		vec3 colorAccum(myType(0.0), myType(0.0), myType(0.0));

		SamplingLoop: for (unsigned sampleIdx = 0; sampleIdx < SAMPLES_PER_PIXEL; ++sampleIdx)
		{
	//#pragma HLS DATAFLOW
	#pragma HLS PIPELINE off
	DO_PRAGMA(HLS UNROLL factor=OUTER_LOOP_UNROLL_FACTOR)

			CRay ray, transformedRay;
			ShadeRec sr, closestSr;

			mat4 transform, transformInv;
			vec3 color(0, 0, 0);

			CreateRay(camera, posShift, h, w, ray);

			ObjectsLoop: for (unsigned n = 0; n < OBJ_NUM; ++n)
			{
	#pragma HLS PIPELINE
				AssignMatrix(objTransformInv, transformInv, n);
				TransformRay(transformInv, ray, transformedRay);
				PerformHits(transformedRay, objType[n], sr);
				UpdateClosestObject(sr, n, closestSr);
			}
	#ifndef __SYNTHESIS__
			if (closestSr.objIdx != -1) // THIS IF IS A BOTTLENECK - MAYBE IT CAN BE GOT RID OF
	#endif
			{
				AssignMatrix(objTransform, transform, closestSr.objIdx);
				AssignMatrix(objTransformInv, transformInv, closestSr.objIdx);

				closestSr.hitPoint = transform.Transform(closestSr.localHitPoint);
				closestSr.normal = transformInv.TransposeTransformDir(closestSr.normal).Normalize();  //closestSr.normal.Normalize();

				color = materials[closestSr.objIdx].ambientColor.CompWiseMul(lights[0].color);

				for (unsigned l = 1; l < LIGHTS_NUM; ++l)
				{
	#pragma HLS PIPELINE
					vec3 objToLight = lights[l].position - closestSr.hitPoint;
					myType d2 = objToLight * objToLight;

					vec3 dirToLight = objToLight / (hls::sqrt(d2));

					myType dot = closestSr.normal * dirToLight;
					ShadeRec shadowSr;

					CRay shadowRay(closestSr.hitPoint, dirToLight);

					// TODO: PASS A COPY OF TRANSFORM_INV, O
					ShadowLoop: for (unsigned n = 0; n < OBJ_NUM; ++n)
					{
	#pragma HLS PIPELINE
						if ( n == closestSr.objIdx ) continue;
						mat4 transformInv;

						AssignMatrix(objTransformInv, transformInv, n);
						TransformRay(transformInv, shadowRay, transformedRay);

						PerformShadowHits(transformedRay, objType[n], sr);

						//// TODO: FIX BUGS IN SHADOWS
						//// TODO: FIX ACCESS TO TRANSFORMS (BREAKS DESIGN)
						UpdateClosestObjectShadow(sr, objTransform[n], n, shadowRay, d2, shadowSr);
					}

					myType specularDot = -ray.direction * dirToLight.Reflect(closestSr.normal);

					if (dot > myType(0.0))
					{
						if (shadowSr.objIdx == -1)
						{
							color += 	materials[closestSr.objIdx].diffuseColor.CompWiseMul(lights[l].color) *
										dot * materials[closestSr.objIdx].k[1];

							color += 	materials[closestSr.objIdx].specularColor.CompWiseMul(lights[l].color) *
										NaturalPow(dot, materials[closestSr.objIdx].k[2]);

	//						cout << h << " x " << w << " : " << shadowSr.objIdx << " vs " << closestSr.objIdx << endl;

						}
	//#ifndef __SYNTHESIS__
	//					else color = vec3(0.8, 0.5, 0.0); // DEBUG ONLY
	//#endif
					}
				}
			}

	//		 TODO: The final result will consist of RGBA value (32 bit)
//			(closestSr.objIdx != -1) ? SaveColorToBuffer(color, frameBuffer[w]) : SaveColorToBuffer(clearColor, frameBuffer[w]);
			(closestSr.objIdx != -1) ? colorAccum += color : colorAccum += clearColor;
	//	***	DEBUG ***
	//		frameBuffer[w] = pixelColorType(closestSr.objIdx);
	//		cout << (closestSr.objIdx + 1 )<< " ";
	//		if (w == WIDTH - 1) cout << endl;

		}
		SaveColorToBuffer(colorAccum * SAMPLING_FACTOR, frameBuffer[w]);
	}
}
// ****************   !THE REAL CORE OF THE PROCESSING   ****************

void CreateRay(const CCamera& camera, const myType* posShift, unsigned r, unsigned c, CRay& ray)
{
#pragma HLS PIPELINE
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

	transformedRay.direction = mat.TransformDir(ray.direction).Normalize();
	transformedRay.origin = mat.Transform(ray.origin);

//	for (unsigned i = 0; i < 3; ++i)
//	{
//		cout << transformedRay.direction.data[i] << ", ";
//	}
//	cout << endl;
}

myType SphereTest(const CRay& transformedRay)
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

	return myType(MAX_DISTANCE);
}

myType PlaneTest(const CRay& transformedRay)
{
#pragma HLS PIPELINE
// XZ - plane pointing +Y
	myType t = -transformedRay.origin[1] * (myType(1.0) / (transformedRay.direction[1] + CORE_BIAS)); // BIAS removes zero division problem
	if (t > CORE_BIAS)
	{
		return t;
	}
	return myType(MAX_DISTANCE);
}

void PerformHits(const CRay& transformedRay, unsigned objType, ShadeRec& sr)
{
//#pragma HLS INLINE
#pragma HLS PIPELINE
	myType res = myType(MAX_DISTANCE);

	switch(objType)
	{
	case SPHERE:
		res = SphereTest(transformedRay);
		sr.normal = transformedRay.origin + transformedRay.direction * res;
		break;
	case PLANE:
		res = PlaneTest(transformedRay);
		sr.normal = vec3(myType(0.0), myType(1.0), myType(0.0));
		break;
	default:
		sr.normal = vec3(myType(-1.0), myType(-1.0), myType(-1.0));
		break;
	}

	sr.localHitPoint = transformedRay.origin + transformedRay.direction * res;
	sr.distance = res;
}

void PerformShadowHits(const CRay& transformedRay, unsigned objType, ShadeRec& sr)
{
//#pragma HLS INLINE
#pragma HLS PIPELINE
	myType res = myType(MAX_DISTANCE);

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

	sr.localHitPoint = transformedRay.origin + transformedRay.direction * res;
	sr.distance = res;
}

void UpdateClosestObject(const ShadeRec& current, int n, ShadeRec& best)
{
#pragma HLS INLINE
#pragma HLS PIPELINE
	if (current.distance < best.distance)
	{
		best.distance = current.distance;
		best.localHitPoint = current.localHitPoint;
		best.normal = current.normal;

		best.objIdx = n;
	}
}

void UpdateClosestObjectShadow(const ShadeRec& current, const mat4& transform, int n, const CRay shadowRay, myType distanceToLightSqr, ShadeRec& best)
{
#pragma HLS INLINE
#pragma HLS PIPELINE

	vec3 shadowPointInWorldSpace = transform.Transform(current.localHitPoint);
	vec3 fromObjToOccluder = shadowPointInWorldSpace - shadowRay.origin;

	myType distSqr = fromObjToOccluder * fromObjToOccluder;

	if (distSqr < distanceToLightSqr)
	{
		best.objIdx = n;
	}
}

void SaveColorToBuffer(vec3 color, pixelColorType& colorOut)
{
#pragma HLS INLINE
#pragma HLS PIPELINE

	pixelColorType tempColor = 0;

	for (unsigned i = 0; i < 3; ++i)
	{
		if (color[i] > myType(1.0))
			color[i] = myType(1.0);

		tempColor += (( unsigned(color[i] * myType(255)) & 0xFF ) << ((3 - i) * 8));
	}
	colorOut = tempColor;
}

myType NaturalPow(myType valIn, unsigned n)
{
	myType x = valIn;
	myType y(1.0);

	if (n > 128) n = 128;
	else if (n == 0) return myType(1.0);

	PoweringLoop: for (int i = 0; i < 7; ++i)
	{
#pragma HLS PIPELINE
		if (n == 0) continue;

		if (n & 0x1)
		{
			y = x * y;
		}
		x *= x;
		n >>= 1;
	}
	return x * y;
}

};
