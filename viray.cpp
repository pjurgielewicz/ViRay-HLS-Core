#include "viray.h"

#include "iostream"
using namespace std;

namespace ViRay
{

/*
 * ***************************************  VIRAY CORE FUNCTIONS ***************************************
 */

void CreatePrimaryRay(const CCamera& camera, const myType* posShift, unsigned short r, unsigned short c, Ray& ray)
{
#pragma HLS PIPELINE
	myType samplePoint[2] = {posShift[0] + /*myType(0.5) +*/ c,
					 	 	 posShift[1] + /*myType(0.5) +*/ r};
	ray = camera.GetCameraRayForPixel(samplePoint);
}

myType CubeTest(const Ray& transformedRay, unsigned char& face)
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

myType GetFresnelReflectionCoeff(const myType& cosRefl, const myType& relativeEta, const myType& invRelativeEtaSqr)
{
#pragma HLS INLINE

//	myType cosRefl = -(rayDirection * surfaceNormal);
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

void PerformHits(const Ray& transformedRay,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
				const vec3& objOrientation,
#endif
				unsigned objType, ShadeRec& sr)
{
//#pragma HLS INLINE
#pragma HLS PIPELINE
	myType res = MAX_DISTANCE;
	myType aInv = myType(1.0);
	unsigned char faceIdx(0);

	vec3 abc;
	Ray transformedRayByObjectDirection(transformedRay);

#if defined(SPHERE_OBJECT_ENABLE) || defined(CYLINDER_OBJECT_ENABLE) || defined(CONE_OBJECT_ENABLE) || defined(PLANE_OBJECT_ENABLE) || defined(DISK_OBJECT_ENABLE) || defined(SQUARE_OBJECT_ENABLE)
	if (objType == SPHERE 	||
		objType == CYLINDER ||
		objType == CONE		||
		objType == PLANE	||
		objType == DISK		||
		objType == SQUARE)
	{
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
		/*
		 * SWAPPING COORDINATES IN ORDER TO ORIENT OBJECT
		 * ALONG CHOSEN AXIS - IT IS BETTER THAN NOTHING...
		 */
		if (objOrientation[0] != myType(0.0))
		{
			ViRayUtils::Swap(transformedRayByObjectDirection.direction[0], 	transformedRayByObjectDirection.direction[1]);
			ViRayUtils::Swap(transformedRayByObjectDirection.origin[0], 	transformedRayByObjectDirection.origin[1]);
		}
		else if (objOrientation[2] != myType(0.0))
		{
			ViRayUtils::Swap(transformedRayByObjectDirection.direction[2], 	transformedRayByObjectDirection.direction[1]);
			ViRayUtils::Swap(transformedRayByObjectDirection.origin[2], 	transformedRayByObjectDirection.origin[1]);
		}
#endif

		myType dxdx = transformedRayByObjectDirection.direction[0] * transformedRayByObjectDirection.direction[0];
		myType dydy = transformedRayByObjectDirection.direction[1] * transformedRayByObjectDirection.direction[1];
		myType dzdz = transformedRayByObjectDirection.direction[2] * transformedRayByObjectDirection.direction[2];

		myType dxox = transformedRayByObjectDirection.direction[0] * transformedRayByObjectDirection.origin[0];
		myType dyoy = transformedRayByObjectDirection.direction[1] * transformedRayByObjectDirection.origin[1];
		myType dzoz = transformedRayByObjectDirection.direction[2] * transformedRayByObjectDirection.origin[2];

		myType oxox = transformedRayByObjectDirection.origin[0] * transformedRayByObjectDirection.origin[0];
		myType oyoy = transformedRayByObjectDirection.origin[1] * transformedRayByObjectDirection.origin[1];
		myType ozoz = transformedRayByObjectDirection.origin[2] * transformedRayByObjectDirection.origin[2];

		myType dxdxdzdz = dxdx + dzdz;
		myType dxoxdzoz = dxox + dzoz;
		myType oxoxozoz = oxox + ozoz;

		switch(objType)
		{
		/*
		 * DUE TO THE LACK OF STABILITY OF COMPUTATIONS
		 * ALL FACTORS NEED TO BE COMPUTED FROM SCRATCH
		 */
#ifdef SPHERE_OBJECT_ENABLE
		case SPHERE:
			abc[0] = dxdxdzdz + dydy;
			abc[1] = dxoxdzoz + dyoy;
			abc[2] = oxoxozoz + oyoy - myType(1.0);

			break;
#endif
#ifdef CYLINDER_OBJECT_ENABLE
		case CYLINDER:
			abc[0] = dxdxdzdz;
			abc[1] = dxoxdzoz;
			abc[2] = oxoxozoz - myType(1.0);

			break;
#endif
#ifdef CONE_OBJECT_ENABLE
		case CONE:
			abc[0] = dxdxdzdz - dydy;
			abc[1] = dxoxdzoz - dyoy + transformedRayByObjectDirection.direction[1];
			abc[2] = oxoxozoz - oyoy + myType(2.0) * transformedRayByObjectDirection.origin[1] - myType(1.0);

			break;
#endif
#if	defined(PLANE_OBJECT_ENABLE) || defined (DISK_OBJECT_ENABLE) || defined(SQUARE_OBJECT_ENABLE)
			/*
			 * REUSING THE RESULT OF 1/A THAT IS BEING DONE EITHER WAY
			 * ONE HIT FUNCTION FOR ALL SIMPLE SHAPES
			 */
		case PLANE:
		case DISK:
		case SQUARE:
			abc[0] = transformedRayByObjectDirection.direction[1];
			abc[1] = myType(0.0);
			abc[2] = myType(0.0);
			break;
#endif
		default:
			break;
		}
		res = ViRayUtils::GeomObjectSolve(abc, transformedRayByObjectDirection, aInv);
#if	defined(PLANE_OBJECT_ENABLE) || defined (DISK_OBJECT_ENABLE) || defined(SQUARE_OBJECT_ENABLE)
		if (objType == PLANE	||
			objType == DISK		||
			objType == SQUARE)
		{
			res = -transformedRayByObjectDirection.origin[1] * aInv;
			res = (res > CORE_BIAS) ? res : MAX_DISTANCE;
		}
#endif
	}
	else
	{
#endif
#if defined(CUBE_OBJECT_ENABLE)
		if (objType == CUBE)
		{
			res = CubeTest(transformedRay, faceIdx);
		}
#endif
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
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
		sr.localNormal = vec3(sr.localHitPoint[0], myType(0.0), sr.localHitPoint[2]);
#else
		if 		(objOrientation[0] != myType(0.0)) 	sr.localNormal = vec3(myType(0.0), sr.localHitPoint[1], sr.localHitPoint[2]);
		else if (objOrientation[1] != myType(0.0)) 	sr.localNormal = vec3(sr.localHitPoint[0], myType(0.0), sr.localHitPoint[2]);
		else 										sr.localNormal = vec3(sr.localHitPoint[0], sr.localHitPoint[1], myType(0.0));
#endif
		break;

#endif
#ifdef CONE_OBJECT_ENABLE
	case CONE:
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
		sr.localNormal = vec3(sr.localHitPoint[0], myType(1.0) -sr.localHitPoint[1], sr.localHitPoint[2]);
#else
		if 		(objOrientation[0] != myType(0.0))	sr.localNormal = vec3(myType(1.0) - sr.localHitPoint[0], sr.localHitPoint[1], sr.localHitPoint[2]);
		else if (objOrientation[1] != myType(0.0)) 	sr.localNormal = vec3(sr.localHitPoint[0], myType(1.0) - sr.localHitPoint[1], sr.localHitPoint[2]);
		else										sr.localNormal = vec3(sr.localHitPoint[0], sr.localHitPoint[1], myType(1.0) - sr.localHitPoint[2]);
#endif
		break;
#endif
#ifdef DISK_OBJECT_ENABLE
	case DISK:
		if (sr.localHitPoint * sr.localHitPoint > myType(1.0)) 		sr.localHitPoint = vec3(MAX_DISTANCE);//res = MAX_DISTANCE;
		break;
#endif
#ifdef SQUARE_OBJECT_ENABLE
	case SQUARE:
		if (ViRayUtils::Abs(sr.localHitPoint[0]) > myType(1.0) ||
			ViRayUtils::Abs(sr.localHitPoint[1]) > myType(1.0) ||
			ViRayUtils::Abs(sr.localHitPoint[2]) > myType(1.0))		sr.localHitPoint = vec3(MAX_DISTANCE);//res = MAX_DISTANCE;
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
}

void RenderSceneInnerLoop(const CCamera& camera,
							const myType* posShift,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
							const mat4* objTransform,
							const mat4* objTransformInv,
#else
							const SimpleTransform* objTransform, const SimpleTransform* objTransformCopy,
#endif
							const unsigned* objType,
							unsigned short h,

							const Light* lights,
							const CMaterial* materials,

							const float_union textureData[MAX_TEXTURE_NUM][TEXT_WIDTH * TEXT_HEIGHT],

							pixelColorType* frameBuffer)
{
//#pragma HLS INLINE

	const vec3 clearColor(myType(0.0));
	vec3 colorAccum;

	InnerLoop: for (unsigned short w = 0; w < WIDTH; ++w)
	{
DO_PRAGMA(HLS PIPELINE II=DESIRED_INNER_LOOP_II)
DO_PRAGMA(HLS UNROLL factor=INNER_LOOP_UNROLL_FACTOR)

		colorAccum = vec3(myType(0.0));

		Ray ray, transformedRay, reflectedRay;
		ShadeRec closestSr, closestReflectedSr;

		CreatePrimaryRay(camera, posShift, h, w, ray);

#if defined(DEEP_RAYTRACING_ENABLE)
		myType currentReflectivity(1.0);
		for (unsigned char depth = 0; depth < RAYTRACING_DEPTH; ++depth)
		{
			closestSr = ShadeRec();

			VisibilityTest(ray,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
							objTransform, objTransformInv,
#else
							objTransform,
#endif
							objType, closestSr);

			if (!closestSr.isHit) break;

			myType ndir = closestSr.normal * ray.direction;
			myType ndir2min = myType(-2.0) * ndir;

			vec3 depthColor = Shade(closestSr, ray,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
									objTransform, objTransformInv,
#else
									objTransform,
#endif
									objType, lights, materials, ndir2min);

			colorAccum += depthColor * currentReflectivity;

			/*
			 * NEXT DEPTH STEP PREPARATION
			 */
#ifdef FRESNEL_REFLECTION_ENABLE
			myType reflectivity = (materials[closestSr.objIdx].fresnelData[0] != myType(0.0)) ? GetFresnelReflectionCoeff(/*ray.direction,
																															closestSr.normal,*/ -ndir,
																															materials[closestSr.objIdx].fresnelData[1],
																															materials[closestSr.objIdx].fresnelData[2]) : materials[closestSr.objIdx].k[1];
#else
			myType reflectivity = materials[closestSr.objIdx].k[1];
#endif
			currentReflectivity *= reflectivity;

			// RAY ALREADY USED TO COMPUTE REFLECTIVITY -> REFLECT
			ray = Ray(closestSr.hitPoint, (-ray.direction).Reflect(closestSr.normal));
		}

#else

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

		myType ndir = closestSr.normal * ray.direction;
		myType ndir2min = myType(-2.0) * ndir;

#ifdef REFLECTION_ENABLE

		/*
		 * REFLECTION PASS (DEPTH = 1)
		 */

//		reflectedRay = Ray(closestSr.hitPoint, (-ray.direction).Reflect(closestSr.normal));
		reflectedRay = Ray(closestSr.hitPoint, closestSr.normal * ndir2min + ray.direction);
		VisibilityTest(reflectedRay,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
						objTransform, objTransformInv,
#else
						objTransformCopy,
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
						objType, lights, materials, textureData, ndir2min);
#endif

#ifdef REFLECTION_ENABLE
		myType ndir2minRefl = (closestReflectedSr.normal * reflectedRay.direction) * myType(-2.0);
#ifdef FRESNEL_REFLECTION_ENABLE
		myType reflectivity = (materials[closestSr.objIdx].fresnelData[0] != myType(0.0)) ? GetFresnelReflectionCoeff(/*ray.direction,
																														closestSr.normal,*/
																														-ndir,
																														materials[closestSr.objIdx].fresnelData[1],
																														materials[closestSr.objIdx].fresnelData[2]) : materials[closestSr.objIdx].k[1];
#else
		myType reflectivity = materials[closestSr.objIdx].k[1];
#endif
		colorAccum += (closestSr.isHit) ? Shade(closestReflectedSr, reflectedRay,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
							objTransform, objTransformInv,
#else
							objTransformCopy,
#endif
							objType, lights, materials, textureData, ndir2minRefl) * reflectivity : vec3(myType(0.0));
#endif
		}

#endif

		SaveColorToBuffer(colorAccum, frameBuffer[w]);
	}
}

void RenderSceneOuterLoop(const CCamera& camera,
							const myType* posShift,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
							const mat4* objTransform,
							const mat4* objTransformInv,
#else
							const SimpleTransform* objTransform, const SimpleTransform* objTransformCopy,
#endif
							const unsigned* objType,

							const Light* lights,
							const CMaterial* materials,

							const float_union textureData[MAX_TEXTURE_NUM][TEXT_WIDTH * TEXT_HEIGHT],

							pixelColorType* frameBuffer,
							pixelColorType* outColor)
{
#pragma HLS INLINE
	for (unsigned short h = 0; h < HEIGHT; ++h)
	{
#pragma HLS DATAFLOW
		RenderSceneInnerLoop(	camera,
								posShift,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
								objTransform,
								objTransformInv,
#else
								objTransform, objTransformCopy,
#endif
								objType,
								HEIGHT - 1 - h,
								lights,
								materials,
								textureData,
								frameBuffer);
		memcpy(outColor + FRAME_ROW_BUFFER_SIZE * h, frameBuffer, sizeof(pixelColorType) * FRAME_ROW_BUFFER_SIZE);
	}
}

void SaveColorToBuffer(vec3 color, pixelColorType& colorOut)
{
#pragma HLS INLINE
#pragma HLS PIPELINE

	pixelColorType tempColor = 0;

	for (unsigned char i = 0; i < 3; ++i)
	{
#pragma HLS UNROLL
		if (color[i] > myType(1.0))	color[i] = myType(1.0);

		tempColor += (( unsigned(color[i] * myType(255.0)) & 0xFF ) << ((2 - i) * 8));
	}
	colorOut = tempColor;
}

vec3 Shade(	const ShadeRec& closestSr,
			const Ray& ray,

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
			const mat4* objTransform,
			const mat4* objTransformInv,
#else
			const SimpleTransform* objTransform,
#endif
			const unsigned* objType,

			const Light* lights,
			const CMaterial* materials,

			const float_union textureData[MAX_TEXTURE_NUM][TEXT_WIDTH * TEXT_HEIGHT],

			const myType ndir2min)
{
#pragma HLS INLINE
#pragma HLS PIPELINE

	vec3 colorOut(myType(0.0), myType(0.0), myType(0.0));
	Ray transformedRay;
	ShadeRec sr;

	vec3 diffuseColor = materials[closestSr.objIdx].GetDiffuseColor(closestSr.localHitPoint,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
																	closestSr.orientation,
#endif
																	textureData);

	/*
	 * WHAT IS THE REAL DIFFERENCE ???
	 */
//	vec3 ambientColor = (materials[closestSr.objIdx].textureType == ViRay::Material::CONSTANT ? materials[closestSr.objIdx].ambientColor : diffuseColor );
	vec3 ambientColor = diffuseColor;
	if (materials[closestSr.objIdx].textureType == ViRay::CMaterial::CONSTANT) ambientColor = materials[closestSr.objIdx].ambientColor;

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

#ifdef SHADOW_ENABLE
		Ray shadowRay(closestSr.hitPoint, dirToLight);
		ShadowVisibilityTest(shadowRay,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
								objTransform, objTransformInv,
#else
								objTransform,
#endif
								objType, closestSr, d2, shadowSr);
#endif

//		myType specularDot = -(ray.direction * dirToLight.Reflect(closestSr.normal));
		myType specularDot = dirToLight * ray.direction + ndir2min * dot; // TODO: DO NOT CARE ABOUT < DOT > CLAMPING
		specularDot = (( specularDot > myType(0.0) ) ? specularDot : myType(0.0));
		// SHOULD NEVER EXCEED 1
		if ( specularDot > myType(1.0) ) cout << "SD: " << specularDot << ", ";

		myType lightToObjLightDirDot = -(dirToLight * lights[l].dir);
		myType lightSpotCoeff = (lightToObjLightDirDot - lights[l].coeff[0]) * lights[l].coeff[1];
		lightSpotCoeff = ViRayUtils::Clamp(lightSpotCoeff, myType(0.0), myType(1.0));

		// DIFFUSE + SPECULAR
		// (HLS::POW() -> SUBOPTIMAL QoR)
		// COLORS SHOULD BE ATTENUATED BEFOREHAND (AT MICROCONTROLLER/TESTBENCH STAGE)

		vec3 baseColor =
#ifdef DIFFUSE_COLOR_ENABLE
							diffuseColor  // * materials[closestSr.objIdx].k[0]
#else
						   vec3(myType(0.0), myType(0.0), myType(0.0))
#endif
							+
#ifdef SPECULAR_HIGHLIGHT_ENABLE
							materials[closestSr.objIdx].specularColor * ViRayUtils::NaturalPow(specularDot, materials[closestSr.objIdx].k[2])// * materials[closestSr.objIdx].k[1]
#else
						   vec3(myType(0.0), myType(0.0), myType(0.0))
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
		colorOut += (shadowSr.objIdx) ? baseColor.CompWiseMul(lights[l].color) * d2Inv * lightSpotCoeff * dot : vec3(myType(0.0));

	}
	return (closestSr.isHit) ?
								(colorOut
#ifdef AMBIENT_COLOR_ENABLE
								+ /*materials[closestSr.objIdx].*/ambientColor.CompWiseMul(lights[0].color)
#endif
								) : vec3(myType(0.0));
}

void ShadowVisibilityTest(	const Ray& shadowRay,
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
	Ray transformedRay;
	ShadeRec sr;

	ShadowLoop: for (unsigned char n = 0; n < OBJ_NUM; ++n)
	{
#pragma HLS PIPELINE
#ifndef SELF_SHADOW_ENABLE
		if ( n == closestSr.objIdx ) continue;
#endif

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

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
void TransformRay(const mat4& mat, const Ray& ray, Ray& transformedRay)
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
void TransformRay(const SimpleTransform& transform, const Ray& ray, Ray& transformedRay)
{
	transformedRay.direction 	= transform.TransformDirInv(ray.direction);
	transformedRay.origin 		= transform.TransformInv(ray.origin);
}
#endif

void UpdateClosestObject(ShadeRec& current,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
		const mat4& transform,
#else
		const SimpleTransform& transform,
#endif
		const unsigned char& n, const Ray& ray, ShadeRec& best)
{
#pragma HLS INLINE
#pragma HLS PIPELINE
	current.hitPoint = transform.Transform(current.localHitPoint);

	vec3 fromRayToObject = current.hitPoint - ray.origin;
	myType realDistanceSqr = fromRayToObject * fromRayToObject;

	if (realDistanceSqr < best.distanceSqr)
	{
		best.distanceSqr 	= realDistanceSqr;

		best.localHitPoint 	= current.localHitPoint;
		best.hitPoint 		= current.hitPoint;

		best.localNormal 	= current.localNormal;

#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
		best.orientation	= transform.orientation;
#endif
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
								const Ray& shadowRay, myType distanceToLightSqr, ShadeRec& best)
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

void VisibilityTest(const Ray& ray,
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
	Ray transformedRay;

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
#ifdef INTERNAL_SHADING_ENABLE
	if (closestSr.normal * ray.direction > myType(0.0)) closestSr.normal = -closestSr.normal;
#endif

}

/*
 * *************************************** VIRAY::VIRAYUTILS ***************************************
 */

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

myType ViRayUtils::Acos(myType x)
{
#ifdef FAST_ACOS_ENABLE
#pragma HLS PIPELINE
	/*
	* LUT - BASED ACOS IMPLEMENTATION
	*/
	const unsigned LUT_SIZE = 32;
	const myType acosLUT[LUT_SIZE + 2] = { 1.5708, 1.53954, 1.50826, 1.47691,
		1.44547, 1.4139, 1.38218, 1.35026,
		1.31812, 1.2857, 1.25297, 1.21989,
		1.1864, 1.15245, 1.11798, 1.08292,
		1.0472, 1.01072, 0.97339, 0.935085,
		0.895665, 0.854958, 0.812756, 0.768794,
		0.722734, 0.67413, 0.622369, 0.566564,
		0.50536, 0.436469, 0.355421, 0.250656,
		0.0, 0.0 };	// one additional - just in case

	myType lutIdxF = hls::fabs(x) * LUT_SIZE;
	myType mix = hls::modf(lutIdxF, &lutIdxF);
	unsigned lutIdx = lutIdxF;
	unsigned nextLutIdx = lutIdx + 1;

	// linear interpolation
	myType calcAngle = (myType(1.0) - mix) * acosLUT[lutIdx] + mix * acosLUT[nextLutIdx];

	return ((x < myType(0.0)) ? (PI - calcAngle) : calcAngle);
#else
#pragma HLS INLINE
	return hls::acos(x);
#endif
}

myType ViRayUtils::Atan2(myType y, myType x)
{
#ifdef FAST_ATAN2_ENABLE
#pragma HLS PIPELINE
	/*
	* BASED ON:
	*
	* https://www.dsprelated.com/showarticle/1052.php
	*
	*
	*/

	const myType n1 = myType(0.97239411);
	const myType n2 = myType(-0.19194795);
	myType result = 0.0f;

	if (x != myType(0.0))
	{
		float_union tXSign, tYSign, tOffset;
		tXSign.fp_num = x;
		tYSign.fp_num = y;

		if (hls::fabs(x) >= hls::fabs(y))
		{
			tOffset.fp_num = PI;
			// Add or subtract PI based on y's sign.
			tOffset.raw_bits |= tYSign.raw_bits & 0x80000000u;
			// No offset if x is positive, so multiply by 0 or based on x's sign.
			tOffset.raw_bits *= tXSign.raw_bits >> 31;
			result = tOffset.fp_num;
			const myType z = y / x;
			result += (n1 + n2 * z * z) * z;
		}
		else // Use atan(y/x) = pi/2 - atan(x/y) if |y/x| > 1.
		{
			tOffset.fp_num = HALF_PI;
			// Add or subtract PI/2 based on y's sign.
			tOffset.raw_bits |= tYSign.raw_bits & 0x80000000u;
			result = tOffset.fp_num;
			const myType z = x / y;
			result -= (n1 + n2 * z * z) * z;
		}
	}
	else if (y > myType(0.0))
	{
		result = HALF_PI;
	}
	else if (y < myType(0.0))
	{
		result = -HALF_PI;
	}
	return result;
#else
	return hls::atan2(y, x);
#endif
}

myType ViRayUtils::Clamp(myType val, myType min, myType max)
{
#pragma HLS INLINE
	if (val > max) return max;
	if (val < min) return min;
	return val;
}

myType ViRayUtils::Divide(myType N, myType D)
{
	//#pragma HLS INLINE off
#ifdef FAST_DIVISION_ENABLE

#ifdef USE_FLOAT
	float_union Nunion, Dunion;

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
		xi = xi * (myType(2.0) - Dunion.fp_num * xi);
	}

	return xi * Nunion.fp_num;

#else
	return N / D;
#endif

#else
	return N / D;
#endif
}

myType ViRayUtils::GeomObjectSolve(const vec3& abc, const Ray& transformedRay, myType& aInv)
{
	aInv = ViRayUtils::Divide(myType(1.0), abc[0]);

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

myType ViRayUtils::InvSqrt(myType val)
{
#ifdef FAST_INV_SQRT_ENABLE

#ifdef USE_FLOAT
	long i;
	float x2, y;
	const float threehalfs = myType(1.5);

	x2 = val * myType(0.5);
	y = val;
	i = *(long *)&y;                       // evil floating point bit level hacking
										   //		i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
	i = 0x5f375a86 - (i >> 1);               // what the fuck?

	y = *(float *)&i;

	for (unsigned char k = 0; k < FAST_INV_SQRT_ORDER; ++k)
	{
		y = y * (threehalfs - (x2 * y * y));   // NR iteration
	}

	return y;
#else
	return hls::sqrt(myType(1.0) / val);
#endif

#else
	return hls::sqrt(myType(1.0) / val);
#endif

}

myType ViRayUtils::NaturalPow(myType val, unsigned char n)
{
	/*
	* N_max = 128 (FF OPENGL - like)
	*/
#pragma HLS INLINE
	myType x = val;
	myType y(1.0);

	if (n == 0) return myType(1.0);

	PoweringLoop: for (unsigned char i = 0; i < MAX_POWER_LOOP_ITER; ++i)
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

myType ViRayUtils::Sqrt(myType val)
{
#ifdef FAST_INV_SQRT_ENABLE
	return val * InvSqrt(val);
#else
	return hls::sqrt(myType(val));
#endif
}

void ViRayUtils::Swap(myType& val1, myType& val2)
{
	myType tmp = val1;
	val1 = val2;
	val2 = tmp;
}

};
