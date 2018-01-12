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
//#pragma HLS INLINE

	const vec3 clearColor(myType(0.0), myType(0.0), myType(0.0));
	vec3 colorAccum;

	InnerLoop: for (int w = 0; w < WIDTH; ++w)
	{
DO_PRAGMA(HLS PIPELINE II=DESIRED_INNER_LOOP_II)

		SamplingLoop: for (unsigned sampleIdx = 0; sampleIdx < SAMPLES_PER_PIXEL; ++sampleIdx)
		{
			if (sampleIdx == 0) colorAccum = vec3(myType(0.0), myType(0.0), myType(0.0));

//#pragma HLS DATAFLOW
#pragma HLS PIPELINE off
DO_PRAGMA(HLS UNROLL factor=OUTER_LOOP_UNROLL_FACTOR)

			CRay ray, transformedRay, reflectedRay;
			ShadeRec sr, closestSr, closestReflectedSr;

			mat4 transform, transformInv;
			vec3 color(0, 0, 0);

			CreateRay(camera, posShift, h, w, ray);

			ObjectsLoop: for (unsigned n = 0; n < OBJ_NUM; ++n)
			{
#pragma HLS PIPELINE
				TransformRay(objTransformInv[n], ray, transformedRay);
				PerformHits(transformedRay, objType[n], sr);
				UpdateClosestObject(sr, n, closestSr);
			}

			// NO NEED TO CHECK WHETHER ANY OBJECT WAS HIT
			// IT IS DONE AT THE COLOR ACCUMULATION STAGE
			transform = objTransform[closestSr.objIdx];
			transformInv = objTransformInv[closestSr.objIdx];

			closestSr.hitPoint = transform.Transform(closestSr.localHitPoint);
			closestSr.normal = transformInv.TransposeTransformDir(closestSr.normal).Normalize();

//			if (closestSr.normal * ray.direction > myType(0.0)) closestSr.normal = -closestSr.normal;

//			colorAccum = materials[closestSr.objIdx].ambientColor.CompWiseMul(lights[0].color);

#ifdef REFLECTION_ENABLE

			/*
			 * REFLECTION PASS (DEPTH = 1)
			 */

			reflectedRay = CRay(closestSr.hitPoint, (-ray.direction).Reflect(closestSr.normal));

			ReflectionObjectsLoop: for (unsigned n = 0; n < OBJ_NUM; ++n)
			{
#pragma HLS PIPELINE
				TransformRay(objTransformInv[n], reflectedRay, transformedRay);
				PerformHits(transformedRay, objType[n], sr);
				UpdateClosestObject(sr, n, closestReflectedSr);
			}

			transform = objTransform[closestReflectedSr.objIdx];
			transformInv = objTransformInv[closestReflectedSr.objIdx];

			closestReflectedSr.hitPoint = transform.Transform(closestReflectedSr.localHitPoint);
			closestReflectedSr.normal = transformInv.TransposeTransformDir(closestReflectedSr.normal).Normalize();

//			if (closestReflectedSr.normal * reflectedRay.direction > myType(0.0)) closestReflectedSr.normal = -closestReflectedSr.normal;

#endif
			/*
			 * SHADING
			 */

			ShadingBlock: {
//#pragma HLS LOOP_MERGE

#ifdef PRIMARY_COLOR_ENABLE
			colorAccum += Shade(closestSr, ray,
								objTransform, objTransformInv, objType,
								lights, materials);
#endif
#ifdef REFLECTION_ENABLE
			colorAccum += ShadeReflective(closestReflectedSr, reflectedRay,
											objTransform, objTransformInv, objType,
											lights, materials) * materials[closestSr.objIdx].k[1]; // Coeff has to be the same as for specular highlight of the reflective surface
#endif
			}
		}
		SaveColorToBuffer(colorAccum * SAMPLING_FACTOR, frameBuffer[w]);
	}
}

vec3 Shade(	const ShadeRec& closestSr,
			const CRay& ray,

			const mat4* objTransform,
			const mat4* objTransformInv,
			const unsigned* objType,

			const Light* lights,
			const Material* materials)
{
#pragma HLS INLINE

	vec3 colorOut(myType(0.0), myType(0.0), myType(0.0));
	CRay transformedRay;
	ShadeRec sr;

	for (unsigned l = 1; l < LIGHTS_NUM; ++l)
	{
#pragma HLS PIPELINE
		vec3 objToLight = lights[l].position - closestSr.hitPoint;
		myType d2 = objToLight * objToLight;

		myType d2Inv = myType(1.0) / d2;
		vec3 dirToLight = objToLight * (hls::sqrt(d2Inv));

		myType dot = closestSr.normal * dirToLight;
		ShadeRec shadowSr;
		shadowSr.objIdx = int(1);

#ifdef SHADOW_ENABLE
		CRay shadowRay(closestSr.hitPoint, dirToLight);

		// TODO: PASS A COPY OF TRANSFORM_INV, O
		ShadowLoop: for (unsigned n = 0; n < OBJ_NUM; ++n)
		{
#pragma HLS PIPELINE
			if ( n == closestSr.objIdx ) continue;

			TransformRay(objTransformInv[n], shadowRay, transformedRay);
			PerformHits(transformedRay, objType[n], sr);
			UpdateClosestObjectShadow(sr, objTransform[n], n, shadowRay, d2, shadowSr);
		}
#endif

		myType specularDot = -ray.direction * dirToLight.Reflect(closestSr.normal);
		specularDot = (( specularDot > myType(0.0) ) ? specularDot : myType(0.0));
		// SHOULD NEVER EXCEED 1
		if ( specularDot > myType(1.0) ) cout << "SD: " << specularDot << ", ";

		myType lightToObjLightDirDot = -(dirToLight * lights[l].dir);
		myType lightSpotCoeff = (lightToObjLightDirDot - lights[l].coeff[0]) * lights[l].innerMinusOuterInv;
		lightSpotCoeff = ViRayUtils::Clamp(lightSpotCoeff, myType(0.0), myType(1.0));

		dot = ViRayUtils::Clamp(dot, myType(0.0), myType(1.0));

		// DIFFUSE + SPECULAR
		// (HLS::POW() -> SUBOPTIMAL QoR)
		// Coeffs (k) can be moved to the color at the stage of preparation
		vec3 baseColor = materials[closestSr.objIdx].diffuseColor /** dot*/ * materials[closestSr.objIdx].k[0] +
						 materials[closestSr.objIdx].specularColor * ViRayUtils::NaturalPow(specularDot, materials[closestSr.objIdx].k[2]) * materials[closestSr.objIdx].k[1];

		/*
		 * APPLYING COLOR MODIFIERS:
		 * - LIGHT COLOR
		 * - SHADOW
		 * - DISTANCE TO LIGHT SQR
		 * - SPOT LIGHT
		 * - NORMAL LIGHT DIRECTION DOT
		 */
		colorOut += baseColor.CompWiseMul(lights[l].color) * shadowSr.objIdx * d2Inv * lightSpotCoeff * dot;

	}
	return ((closestSr.isHit) ? (colorOut + materials[closestSr.objIdx].ambientColor.CompWiseMul(lights[0].color)) : vec3(myType(0.0), myType(0.0), myType(0.0)));
}

vec3 ShadeReflective(	const ShadeRec& closestSr,
						const CRay& ray,

						const mat4* objTransform,
						const mat4* objTransformInv,
						const unsigned* objType,

						const Light* lights,
						const Material* materials)
{
#pragma HLS INLINE

	vec3 colorOut(myType(0.0), myType(0.0), myType(0.0));
	CRay transformedRay;
	ShadeRec sr;

	for (unsigned l = 1; l < LIGHTS_NUM; ++l)
	{
#pragma HLS PIPELINE
		vec3 objToLight = lights[l].position - closestSr.hitPoint;
		myType d2 = objToLight * objToLight;

		myType d2Inv = myType(1.0) / d2;
		vec3 dirToLight = objToLight * (hls::sqrt(d2Inv));

		myType dot = closestSr.normal * dirToLight;
		ShadeRec shadowSr;
		shadowSr.objIdx = int(1);

		CRay shadowRay(closestSr.hitPoint, dirToLight);

		// TODO: PASS A COPY OF TRANSFORM_INV, O
		ShadowLoop: for (unsigned n = 0; n < OBJ_NUM; ++n)
		{
#pragma HLS PIPELINE
			if ( n == closestSr.objIdx ) continue;

			TransformRay(objTransformInv[n], shadowRay, transformedRay);
			PerformHits(transformedRay, objType[n], sr);
			UpdateClosestObjectShadow(sr, objTransform[n], n, shadowRay, d2, shadowSr);
		}

		myType specularDot = -ray.direction * dirToLight.Reflect(closestSr.normal);
		specularDot = (( specularDot > myType(0.0) ) ? specularDot : myType(0.0));
		// SHOULD NEVER EXCEED 1
		if ( specularDot > myType(1.0) ) cout << "SD: " << specularDot << ", ";

		myType lightToObjLightDirDot = -(dirToLight * lights[l].dir);
		myType lightSpotCoeff = (lightToObjLightDirDot - lights[l].coeff[0]) * lights[l].innerMinusOuterInv;
		lightSpotCoeff = ViRayUtils::Clamp(lightSpotCoeff, myType(0.0), myType(1.0));

		/*
		 * THE MAIN DIFFERENCE BETWEEN REGULAR SHADING:
		 * EITHER OBJECT IS FACING THE LIGHT OR NOT
		 * IT RESULTS FROM THE EQUAL CROSS SECTION OF INCIDENT AND REFLECTED LIGHT BEAM
		 * SEE RAY TRACING FROM THE GROUND UP
		 */
		dot = ((dot > myType(0.0)) ? myType(1.0) : myType(0.0));

		// DIFFUSE + SPECULAR
		// (HLS::POW() -> SUBOPTIMAL QoR)
		// Coeffs (k) can be moved to the color at the stage of preparation
		vec3 baseColor = materials[closestSr.objIdx].diffuseColor /** dot*/ * materials[closestSr.objIdx].k[0] +
						 materials[closestSr.objIdx].specularColor * ViRayUtils::NaturalPow(specularDot, materials[closestSr.objIdx].k[2]) * materials[closestSr.objIdx].k[1];

		/*
		 * APPLYING COLOR MODIFIERS:
		 * - LIGHT COLOR
		 * - SHADOW
		 * - DISTANCE TO LIGHT SQR
		 * - SPOT LIGHT
		 * - NORMAL LIGHT DIRECTION DOT
		 */
		colorOut += baseColor.CompWiseMul(lights[l].color) * shadowSr.objIdx * d2Inv * lightSpotCoeff * dot;

	}
	return ((closestSr.isHit) ? (colorOut + materials[closestSr.objIdx].ambientColor.CompWiseMul(lights[0].color)) : vec3(myType(0.0), myType(0.0), myType(0.0)));
}

// ****************   !THE REAL CORE OF THE PROCESSING   ****************

void CreateRay(const CCamera& camera, const myType* posShift, unsigned r, unsigned c, CRay& ray)
{
#pragma HLS PIPELINE
// TODO: Remove 'halfs' addition

	myType samplePoint[2] = {posShift[0] + myType(0.5) + c,
					 	 	 posShift[1] + myType(0.5) + r};
	ray = camera.GetCameraRayForPixel(samplePoint);
}

void TransformRay(const mat4& mat, const CRay& ray, CRay& transformedRay)
{
#pragma HLS PIPELINE
#pragma HLS INLINE

	transformedRay.direction = mat.TransformDir(ray.direction).Normalize();
	transformedRay.origin = mat.Transform(ray.origin);
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
	myType res = MAX_DISTANCE;

	switch(objType)
	{
#ifdef USE_SPHERE_OBJECT
	case SPHERE:
		res = SphereTest(transformedRay);
		sr.normal = transformedRay.origin + transformedRay.direction * res;
		break;
#endif
#if defined(USE_PLANE_OBJECT) || defined(USE_DISK_OBJECT) || defined(USE_SQUARE_OBJECT)
	case PLANE:
	case DISK:
	case SQUARE:
		res = PlaneTest(transformedRay);
		sr.normal = vec3(myType(0.0), myType(1.0), myType(0.0));
		break;
#endif
	default:
		sr.normal = vec3(myType(-1.0), myType(-1.0), myType(-1.0));
		break;
	}

	sr.localHitPoint = transformedRay.origin + transformedRay.direction * res;

#if defined(USE_DISK_OBJECT) || defined(USE_SQUARE_OBJECT)
	switch(objType)
	{
#ifdef USE_DISK_OBJECT
	case DISK:
		if (sr.localHitPoint * sr.localHitPoint > myType(1.0)) 	res = MAX_DISTANCE;
		break;
#endif
#ifdef USE_SQUARE_OBJECT
	case SQUARE:
		if (hls::fabs(sr.localHitPoint[0]) > myType(0.5) ||
			hls::fabs(sr.localHitPoint[2]) > myType(0.5))		res = MAX_DISTANCE;
		break;
#endif
	default: // PLANE
		break;
	}

	// IT IS REQUIRED FOR SHADOW PASS ANALYSIS
	sr.localHitPoint = transformedRay.origin + transformedRay.direction * res;
#endif

	// the most important at this point
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

		best.isHit = true;
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
		best.objIdx = 0;
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

myType ViRayUtils::NaturalPow(myType valIn, unsigned n)
{
//#pragma HLS INLINE
	myType x = valIn;
	myType y(1.0);

	if (n > 128) n = 128;
	else if (n == 0) return myType(1.0);

	PoweringLoop: for (int i = 0; i < 7; ++i)
	{
#pragma HLS PIPELINE
		if (n == 1) continue;

		if (n & 0x1)
		{
			y = x * y;
		}
		x *= x;
		n >>= 1;
	}
	return x * y;
}

myType ViRayUtils::Clamp(myType val, myType min, myType max)
{
	if (val > max) return max;
	if (val < min) return min;
	return val;
}

};
