#include "viray.h"

#include "iostream"
using namespace std;

namespace ViRay
{

void RenderScene(const CCamera& camera,
					const myType* posShift,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
			const mat4* objTransform,
			const mat4* objTransformInv,
#else
			const SimpleTransform* objTransform,
#endif
					const unsigned* objType,

					const Light* lights,
					const Material* materials,

					pixelColorType* frameBuffer,
					pixelColorType* outColor)
{
#pragma HLS INLINE
	for (unsigned short h = 0; h < HEIGHT; ++h)
	{
#pragma HLS DATAFLOW
		InnerLoop(	camera,
					posShift,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
					objTransform,
					objTransformInv,
#else
					objTransform,
#endif
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
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
			const mat4* objTransform,
			const mat4* objTransformInv,
#else
			const SimpleTransform* objTransform,
#endif
				const unsigned* objType,
				unsigned short h,

				const Light* lights,
				const Material* materials,

				pixelColorType* frameBuffer)
{
//#pragma HLS INLINE

	const vec3 clearColor(myType(0.0), myType(0.0), myType(0.0));
	vec3 colorAccum;

	InnerLoop: for (unsigned short w = 0; w < WIDTH; ++w)
	{
DO_PRAGMA(HLS PIPELINE II=DESIRED_INNER_LOOP_II)
DO_PRAGMA(HLS UNROLL factor=INNER_LOOP_UNROLL_FACTOR)

		colorAccum = vec3(myType(0.0), myType(0.0), myType(0.0));

		CRay ray, transformedRay, reflectedRay;
		ShadeRec closestSr, closestReflectedSr;

		CreateRay(camera, posShift, h, w, ray);
		VisibilityTest(ray,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
						objTransform, objTransformInv,
#else
						objTransform,
#endif
						objType, closestSr);

		/*
		 * NO NEED TO CHECK WHETHER ANY OBJECT WAS HIT
		 * IT IS BEING DONE AT THE COLOR ACCUMULATION STAGE
		 */

#ifdef REFLECTION_ENABLE

		/*
		 * REFLECTION PASS (DEPTH = 1)
		 */

		reflectedRay = CRay(closestSr.hitPoint, (-ray.direction).Reflect(closestSr.normal));
		VisibilityTest(reflectedRay,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
						objTransform, objTransformInv,
#else
						objTransform,
#endif
						objType, closestReflectedSr);
#endif
		/*
		 * SHADING
		 */

		ShadingBlock: {
//#pragma HLS LOOP_MERGE

#ifdef PRIMARY_COLOR_ENABLE
		colorAccum += Shade(closestSr, ray,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
						objTransform, objTransformInv,
#else
						objTransform,
#endif
						objType, lights, materials);
#endif
#ifdef REFLECTION_ENABLE

#ifdef FRESNEL_REFLECTION_ENABLE
		myType reflectivity = (materials[closestSr.objIdx].fresnelData[0] != myType(0.0)) ? GetFresnelReflectionCoeff(ray.direction,
																														closestSr.normal,
																														materials[closestSr.objIdx].fresnelData[1],
																														materials[closestSr.objIdx].fresnelData[2]) : materials[closestSr.objIdx].k[1];
#else
		myType reflectivity = materials[closestSr.objIdx].k[1];
#endif

		colorAccum += Shade(closestReflectedSr, reflectedRay,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
							objTransform, objTransformInv,
#else
							objTransform,
#endif
							objType, lights, materials) *
							reflectivity *
							closestSr.isHit;
#endif

		}
		SaveColorToBuffer(colorAccum, frameBuffer[w]);
	}
}

myType GetFresnelReflectionCoeff(const vec3& rayDirection, const vec3 surfaceNormal, const myType& relativeEta, const myType& invRelativeEtaSqr)
{
#pragma HLS INLINE

	myType cosRefl = -(rayDirection * surfaceNormal);
	myType cosRefrSqr = myType(1.0) - invRelativeEtaSqr * (myType(1.0) - cosRefl * cosRefl);

#if defined(FAST_INV_SQRT_ENABLE) && defined(USE_FLOAT)
	myType invCosRefr = ViRayUtils::InvSqrt(cosRefrSqr);
	myType k = cosRefl * invCosRefr;

	myType f1 = (k - relativeEta) / (k + relativeEta);
	myType f2 = (relativeEta * k - myType(1.0)) / (relativeEta * k + myType(1.0));
#else
	myType cosRefr = ViRayUtils::Sqrt(cosRefrSqr);

	myType f1 = (relativeEta * cosRefl - cosRefr) / (relativeEta * cosRefl + cosRefr);
	myType f2 = (relativeEta * cosRefr - cosRefl) / (relativeEta * cosRefr + cosRefl);
#endif

	/*
	 * ASSUMING EQUAL CONTRIBUTION OF EACH KIND OF POLARIZATION - UNPOLARIZED LIGHT
	 */

	myType fCoeff = myType(0.5) * (f1 * f1 + f2 * f2);

	/*
	 * ACCOUNT FOR TOTAL INTERNAL REFLECTION
	 * ETA < 1.0
	 */
	return (cosRefrSqr < myType(0.0)) ? myType(1.0) : fCoeff;
}

void VisibilityTest(const CRay& ray,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
					const mat4* objTransform,
					const mat4* objTransformInv,
#else
					const SimpleTransform* objTransform,
#endif
					const unsigned* objType,
					ShadeRec& closestSr)
{
#pragma HLS INLINE
	ShadeRec sr;
	CRay transformedRay;

	for (unsigned char n = 0; n < OBJ_NUM; ++n)
	{
#pragma HLS PIPELINE
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
		TransformRay(objTransformInv[n], ray, transformedRay);
		PerformHits(transformedRay, objType[n], sr);
#else
		TransformRay(objTransform[n], ray, transformedRay);
		PerformHits(transformedRay, objTransform[n].orientation, objType[n], sr);
#endif
		UpdateClosestObject(sr, objTransform[n], n, ray, closestSr);
	}
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
	closestSr.normal = objTransformInv[closestSr.objIdx].TransposeTransformDir(closestSr.localNormal).Normalize();
#else
	closestSr.normal = closestSr.localNormal.CompWiseMul(objTransform[closestSr.objIdx].invScale).Normalize();
#endif

	/*
	 * ALLOW TO SHADE INTERNAL SURFACES
	 */
	if (closestSr.normal * ray.direction > myType(0.0)) closestSr.normal = -closestSr.normal;
}

void ShadowVisibilityTest(	const CRay& shadowRay,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
							const mat4* objTransform,
							const mat4* objTransformInv,
#else
							const SimpleTransform* objTransform,
#endif
							const unsigned* objType,
							const ShadeRec& closestSr,
							myType d2,
							ShadeRec& shadowSr)
{
#pragma HLS INLINE
	CRay transformedRay;
	ShadeRec sr;

	ShadowLoop: for (unsigned char n = 0; n < OBJ_NUM; ++n)
	{
#pragma HLS PIPELINE
		if ( n == closestSr.objIdx ) continue;

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
		TransformRay(objTransformInv[n], shadowRay, transformedRay);
		PerformHits(transformedRay, objType[n], sr);
#else
		TransformRay(objTransform[n], shadowRay, transformedRay);
		PerformHits(transformedRay, objTransform[n].orientation, objType[n], sr);
#endif
		UpdateClosestObjectShadow(sr, objTransform[n], shadowRay, d2, shadowSr);
	}
}

vec3 Shade(	const ShadeRec& closestSr,
			const CRay& ray,

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
			const mat4* objTransform,
			const mat4* objTransformInv,
#else
			const SimpleTransform* objTransform,
#endif
			const unsigned* objType,

			const Light* lights,
			const Material* materials)
{
#pragma HLS INLINE
#pragma HLS PIPELINE

	vec3 colorOut(myType(0.0), myType(0.0), myType(0.0));
	CRay transformedRay;
	ShadeRec sr;

	for (unsigned char l = 1; l < LIGHTS_NUM; ++l)
	{
#pragma HLS PIPELINE
		vec3 objToLight = lights[l].position - closestSr.hitPoint;
		myType d2 = objToLight * objToLight;
		myType dInv = ViRayUtils::InvSqrt(d2);

		myType d2Inv = dInv * dInv;
		vec3 dirToLight = objToLight * dInv;

		myType dot = closestSr.normal * dirToLight;
		dot = ViRayUtils::Clamp(dot, myType(0.0), myType(1.0));

		ShadeRec shadowSr;
		shadowSr.objIdx = int(1);

#ifdef DIRECT_SHADOW_ENABLE
		CRay shadowRay(closestSr.hitPoint, dirToLight);
		ShadowVisibilityTest(shadowRay,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
								objTransform, objTransformInv,
#else
								objTransform,
#endif
								objType, closestSr, d2, shadowSr);
#endif

		myType specularDot = -(ray.direction * dirToLight.Reflect(closestSr.normal));
		specularDot = (( specularDot > myType(0.0) ) ? specularDot : myType(0.0));
		// SHOULD NEVER EXCEED 1
		if ( specularDot > myType(1.0) ) cout << "SD: " << specularDot << ", ";

		myType lightToObjLightDirDot = -(dirToLight * lights[l].dir);
		myType lightSpotCoeff = (lightToObjLightDirDot - lights[l].coeff[0]) * lights[l].coeff[1];
		lightSpotCoeff = ViRayUtils::Clamp(lightSpotCoeff, myType(0.0), myType(1.0));

		// DIFFUSE + SPECULAR
		// (HLS::POW() -> SUBOPTIMAL QoR)
		// Coeffs (k) can be moved to the color at the stage of preparation
		// COLORS SHOULD BE ATTENUATED BEFOREHAND (AT MICROCONTROLLER STAGE)
		vec3 baseColor =
#ifdef DIFFUSE_COLOR_ENABLE
							materials[closestSr.objIdx].diffuseColor  // * materials[closestSr.objIdx].k[0]
#else
						   colorOut(myType(0.0), myType(0.0), myType(0.0))
#endif
							+
#ifdef SPECULAR_HIGHLIGHT_ENABLE
							materials[closestSr.objIdx].specularColor * ViRayUtils::NaturalPow(specularDot, materials[closestSr.objIdx].k[2])// * materials[closestSr.objIdx].k[1]
#else
						   colorOut(myType(0.0), myType(0.0), myType(0.0))
#endif
							;

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
	return (colorOut
#ifdef AMBIENT_COLOR_ENABLE
			+ materials[closestSr.objIdx].ambientColor.CompWiseMul(lights[0].color)
#endif
			) * closestSr.isHit;
}

// ****************   !THE REAL CORE OF THE PROCESSING   ****************

void CreateRay(const CCamera& camera, const myType* posShift, unsigned short r, unsigned short c, CRay& ray)
{
#pragma HLS PIPELINE
	myType samplePoint[2] = {posShift[0] + myType(0.5) + c,
					 	 	 posShift[1] + myType(0.5) + r};
	ray = camera.GetCameraRayForPixel(samplePoint);
}

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
void TransformRay(const mat4& mat, const CRay& ray, CRay& transformedRay)
{
#pragma HLS PIPELINE
#pragma HLS INLINE

	/*
	 * RAY DIRECTION DOES NOT NEED TO BE NORMALIZED
	 * SINCE INTERSECTION TESTS MORPHED NATURALLY TO THEIR
	 * GENERAL FORMS
	 */
	transformedRay.direction 	= mat.TransformDir(ray.direction);//.Normalize();
	transformedRay.origin 		= mat.Transform(ray.origin);
}
#else
void TransformRay(const SimpleTransform& transform, const CRay& ray, CRay& transformedRay, bool isInverse)
{
	if (isInverse)
	{
		transformedRay.direction = transform.TransformDirInv(ray.direction);
		transformedRay.origin = transform.TransformInv(ray.origin);
	}
	else
	{
		transformedRay.direction = transform.TransformDir(ray.direction);
		transformedRay.origin = transform.Transform(ray.origin);
	}
}
#endif


#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
myType PlaneTest(const CRay& transformedRay)
{
#pragma HLS PIPELINE
// XZ - plane pointing +Y
	myType t = ViRayUtils::Divide(-transformedRay.origin[1], (transformedRay.direction[1] /*+ CORE_BIAS*/)); // BIAS removes zero division problem

	if (t > CORE_BIAS)
	{
		return t;
	}
	return myType(MAX_DISTANCE);
}
#else
myType PlaneTest(const CRay& transformedRay, const vec3& orientation)
{
#pragma HLS PIPELINE

	myType t = ViRayUtils::Divide(-(transformedRay.origin * orientation), (transformedRay.direction * orientation));

	if (t > CORE_BIAS)
	{
		return t;
	}
	return myType(MAX_DISTANCE);
}
#endif



myType CubeTest(const CRay& transformedRay, unsigned char& face)
{
#pragma HLS PIPELINE
	vec3 t_min, t_max, abc;

	for (unsigned char i = 0; i < 3; ++i)
	{
#pragma HLS UNROLL
#pragma HLS PIPELINE
		abc[i] = myType(1.0) / transformedRay.direction[i];

		myType pos = (transformedRay.direction[i] >= myType(0.0)) ? myType(1.0) : myType(-1.0);

		// POTENTAILLY OPTIMIZING RESOURCE USAGE
		// SEE ORIGINAL CONTENT IN: RTFTGU
		t_min[i] = (-pos - transformedRay.origin[i]) * abc[i];
		t_max[i] = ( pos - transformedRay.origin[i]) * abc[i];
	}

	myType t0, t1, t(MAX_DISTANCE);
	unsigned char faceIn, faceOut;
	if (t_min[0] > t_min[1])
	{
		t0 = t_min[0];
		faceIn = (abc[0] >= myType(0.0)) ? 0 : 3;
	}
	else
	{
		t0 = t_min[1];
		faceIn = (abc[1] >= myType(0.0)) ? 1 : 4;
	}
	if (t_min[2] > t0)
	{
		t0 = t_min[2];
		faceIn = (abc[2] >= myType(0.0)) ? 2 : 5;
	}
//////////////
	if (t_max[0] < t_max[1])
	{
		t1 = t_max[0];
		faceOut = (abc[0] >= myType(0.0)) ? 3 : 0;
	}
	else
	{
		t1 = t_max[1];
		faceOut = (abc[1] >= myType(0.0)) ? 4 : 1;
	}
	if (t_max[2] < t0)
	{
		t1 = t_max[2];
		faceOut = (abc[2] >= myType(0.0)) ? 5 : 2;
	}

	if (t0 < t1 && t1 > CORE_BIAS)
	{
		if (t0 > CORE_BIAS)
		{
			t = t0;
			face = faceIn;
		}
		else
		{
			t = t1;
			face = faceOut;
		}
	}
	return t;
}

vec3 GetCubeNormal(const unsigned char& faceIdx)
{
	switch(faceIdx)
	{
	case 0:
		return vec3(myType(-1.0), myType(0.0), myType(0.0));
	case 1:
		return vec3(myType(0.0), myType(-1.0), myType(0.0));
	case 2:
		return vec3(myType(0.0), myType(0.0), myType(-1.0));
	case 3:
		return vec3(myType(1.0), myType(0.0), myType(0.0));
	case 4:
		return vec3(myType(0.0), myType(1.0), myType(0.0));
	default: // case 5
		return vec3(myType(0.0), myType(0.0), myType(1.0));
	}
}

void PerformHits(const CRay& transformedRay,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
				const vec3& objOrientation,
#endif
				unsigned objType, ShadeRec& sr)
{
//#pragma HLS INLINE
#pragma HLS PIPELINE
	myType res = MAX_DISTANCE;
	unsigned char faceIdx(0);

	vec3 abc;

#if defined(SPHERE_OBJECT_ENABLE) || defined(CYLINDER_OBJECT_ENABLE) || defined(CONE_OBJECT_ENABLE)
	if (objType == SPHERE 	||
		objType == CYLINDER ||
		objType == CONE)
	{
		switch(objType)
		{
		/*
		 * DUE TO THE LACK OF STABILITY OF COMPUTATIONS
		 * ALL FACTORS NEED TO BE COMPUTED FROM SCRATCH
		 */
#ifdef SPHERE_OBJECT_ENABLE
		case SPHERE:
			abc[0] = transformedRay.direction * transformedRay.direction;
			abc[1] = transformedRay.direction * transformedRay.origin;
			abc[2] = transformedRay.origin * transformedRay.origin - myType(1.0);
			break;
#endif
#ifdef CYLINDER_OBJECT_ENABLE
		case CYLINDER:
			abc[0] = transformedRay.direction[0] * transformedRay.direction[0] +
			 	 	 transformedRay.direction[2] * transformedRay.direction[2];
			abc[1] = transformedRay.direction[0] * transformedRay.origin[0] +
					 transformedRay.direction[2] * transformedRay.origin[2];
			abc[2] = transformedRay.origin[0] * transformedRay.origin[0] +
					 transformedRay.origin[2] * transformedRay.origin[2] - myType(1.0);
			break;
#endif
#ifdef CONE_OBJECT_ENABLE
		case CONE:
			abc[0] = transformedRay.direction[0] * transformedRay.direction[0] -
					 transformedRay.direction[1] * transformedRay.direction[1] +
					 transformedRay.direction[2] * transformedRay.direction[2];
			abc[1] = transformedRay.direction[0] * transformedRay.origin[0] -
					 transformedRay.direction[1] * (transformedRay.origin[1] - myType(1.0)) +
					 transformedRay.direction[2] * transformedRay.origin[2];
			abc[2] = transformedRay.origin[0] * transformedRay.origin[0] -
					(transformedRay.origin[1] - myType(1.0)) * (transformedRay.origin[1] - myType(1.0)) +
					 transformedRay.origin[2] * transformedRay.origin[2];
			break;
#endif
		default:
			break;
		}
		res = ViRayUtils::QuadraticObjectSolve(abc, transformedRay);
	}
	else
#endif
#if defined(PLANE_OBJECT_ENABLE) || defined(DISK_OBJECT_ENABLE) || defined(SQUARE_OBJECT_ENABLE)
	if (objType == PLANE 	||
		objType == DISK 	||
		objType == SQUARE)
	{
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
		res = PlaneTest(transformedRay);
#else
		res = PlaneTest(transformedRay, objOrientation);
#endif
	}
	else
#endif
#if defined(CUBE_OBJECT_ENABLE)
	if (objType == CUBE)
	{
		res = CubeTest(transformedRay, faceIdx);
	}
#endif
	// JUST IN CASE IF-ELSE BLOCK TERMINATION
	{
		// WHATEVER
		;
	}

	sr.localHitPoint = transformedRay.origin + transformedRay.direction * res;
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
	sr.localNormal = vec3(myType(0.0), myType(1.0), myType(0.0));
#else
	sr.localNormal = objOrientation;
#endif

	switch(objType)
	{
#ifdef SPHERE_OBJECT_ENABLE
	case SPHERE:
		sr.localNormal = sr.localHitPoint;
		break;
#endif
#ifdef CYLINDER_OBJECT_ENABLE
	case CYLINDER:
		sr.localNormal = vec3(sr.localHitPoint[0], myType(0.0), sr.localHitPoint[2]);
		break;
#endif
#ifdef CONE_OBJECT_ENABLE
	case CONE:
		sr.localNormal = vec3(sr.localHitPoint[0], -sr.localHitPoint[1] + myType(1.0), sr.localHitPoint[2]);
		break;
#endif
#ifdef DISK_OBJECT_ENABLE
	case DISK:
		if (sr.localHitPoint * sr.localHitPoint > myType(1.0)) 	res = MAX_DISTANCE;
		break;
#endif
#ifdef SQUARE_OBJECT_ENABLE
	case SQUARE:
		if (ViRayUtils::Abs(sr.localHitPoint[0]) > myType(1.0) ||
			ViRayUtils::Abs(sr.localHitPoint[2]) > myType(1.0))		res = MAX_DISTANCE;


		break;
#endif
#ifdef CUBE_OBJECT_ENABLE
	case CUBE:
		sr.localNormal = GetCubeNormal(faceIdx);
		break;
#endif
	default:
		break;
	}

	// IT IS REQUIRED FOR SHADOW PASS ANALYSIS
	sr.localHitPoint = transformedRay.origin + transformedRay.direction * res;
	// the most important at this point
	sr.localDistance = res;
}

void UpdateClosestObject(ShadeRec& current,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
		const mat4& transform,
#else
		const SimpleTransform& transform,
#endif
		const unsigned char& n, const CRay& ray, ShadeRec& best)
{
#pragma HLS INLINE
#pragma HLS PIPELINE
	current.hitPoint = transform.Transform(current.localHitPoint);

	vec3 fromRayToObject = current.hitPoint - ray.origin;
	myType realDistanceSqr = fromRayToObject * fromRayToObject;

	if (realDistanceSqr < best.distanceSqr)
	{
		best.localDistance 	= current.localDistance;
		best.distanceSqr 	= realDistanceSqr;

		best.localHitPoint 	= current.localHitPoint;
		best.hitPoint 		= current.hitPoint;

		best.localNormal 	= current.localNormal;

		best.isHit 	= true;
		best.objIdx = n;
	}
}

void UpdateClosestObjectShadow(const ShadeRec& current,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
								const mat4& transform,
#else
								const SimpleTransform& transform,
#endif
								const CRay& shadowRay, myType distanceToLightSqr, ShadeRec& best)
{
#pragma HLS INLINE
#pragma HLS PIPELINE

	vec3 shadowPointInWorldSpace = transform.Transform(current.localHitPoint);
	vec3 fromObjToOccluder = shadowPointInWorldSpace - shadowRay.origin;

	myType distSqr = fromObjToOccluder * fromObjToOccluder;

	if (distSqr < distanceToLightSqr)
	{
		best.objIdx = (unsigned char)0;
	}
}

void SaveColorToBuffer(vec3 color, pixelColorType& colorOut)
{
#pragma HLS INLINE
#pragma HLS PIPELINE

	pixelColorType tempColor = 0;

	for (unsigned i = 0; i < 3; ++i)
	{
#pragma HLS UNROLL
		if (color[i] > myType(1.0))	color[i] = myType(1.0);

		tempColor += (( unsigned(color[i] * myType(255.0)) & 0xFF ) << ((3 - i) * 8));
	}
	colorOut = tempColor;
}

myType ViRayUtils::QuadraticObjectSolve(const vec3& abc, const CRay& transformedRay)
{
	myType aInv = ViRayUtils::Divide(myType(1.0), abc[0]);

	myType d2 = abc[1] * abc[1] - abc[0] * abc[2];

	if (d2 >= myType(0.0))
	{
		myType d = ViRayUtils::Sqrt(d2);

		myType t1 = (-abc[1] - d) * aInv;

		myType localHitPointY1 = transformedRay.origin[1] + t1 * transformedRay.direction[1];
		if (t1 > CORE_BIAS && ViRayUtils::Abs(localHitPointY1) < myType(1.0))
		{
			return t1;
		}

		myType t2 = (-abc[1] + d) * aInv;

		myType localHitPointY2 = transformedRay.origin[1] + t2 * transformedRay.direction[1];
		if (t2 > CORE_BIAS && ViRayUtils::Abs(localHitPointY2) < myType(1.0))
		{
			return t2;
		}
	}

	return myType(MAX_DISTANCE);
}

myType ViRayUtils::NaturalPow(myType valIn, unsigned char n)
{
	/*
	 * N_max = 128 (OPENGL - like)
	 */
#pragma HLS INLINE
	myType x = valIn;
	myType y(1.0);

	if (n == 0) return myType(1.0);

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
#pragma HLS INLINE
	if (val > max) return max;
	if (val < min) return min;
	return val;
}

myType ViRayUtils::InvSqrt(myType val)
{
#ifdef FAST_INV_SQRT_ENABLE

#ifdef USE_FLOAT
		long i;
		float x2, y;
		const float threehalfs = myType(1.5);

		x2 = val * myType(0.5);
		y  = val;
		i  = * ( long * ) &y;                       // evil floating point bit level hacking
//		i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
		i  = 0x5f375a86 - ( i >> 1 );               // what the fuck?

		y  = * ( float * ) &i;

		for (unsigned k = 0; k < FAST_INV_SQRT_ORDER; ++k)
		{
			y  = y * ( threehalfs - ( x2 * y * y ) );   // NR iteration
		}

		return y;
#else
		return hls::sqrt(myType(1.0) / val);
#endif

#else
		return hls::sqrt(myType(1.0) / val);
#endif

}

myType ViRayUtils::Sqrt(myType val)
{
#ifdef FAST_INV_SQRT_ENABLE
	return val * InvSqrt(val);
#else
	return hls::sqrt(myType(val));
#endif
}

myType ViRayUtils::Divide(myType N, myType D)
{
//#pragma HLS INLINE off
#ifdef FAST_DIVISION_ENABLE

#ifdef USE_FLOAT
	myType_union Nunion, Dunion;

	Nunion.fp_num = N;
	Dunion.fp_num = D;

	if (Dunion.sign)
	{
		Dunion.sign = 0;
		Nunion.sign = !Nunion.sign;
	}

	unsigned expDiff = Dunion.bexp - 126;
	Dunion.bexp = 126;
	Nunion.bexp -= expDiff;

	myType xi = myType(48.0 / 17.0) - myType(32.0 / 17.0) * Dunion.fp_num;

    for (unsigned i = 0; i < FAST_DIVISION_ORDER; ++i)
    {
        xi = xi * (float(2.0) - Dunion.fp_num * xi);
    }

    return xi * Nunion.fp_num;

#else
	return N / D;
#endif

#else
	return N / D;
#endif
}

myType ViRayUtils::Abs(myType val)
{
#ifdef USE_FIXEDPOINT
	return (val > myType(0.0)) ? myType(val) : myType(-val);
#elif defined(USE_FLOAT)
	return hls::fabs(val);
#else
	return hls::abs(val);
#endif
}

};
