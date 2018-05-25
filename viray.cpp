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
#pragma HLS INLINE //?
//#pragma HLS PIPELINE II=8
	myType samplePoint[2] = {posShift[0] + /*myType(0.5) +*/ c,
					 	 	 posShift[1] + /*myType(0.5) +*/ r};
	ray = camera.GetCameraRayForPixel(samplePoint);
}

myType CubeTest(const Ray& transformedRay, unsigned char& face)
{
//#pragma HLS INLINE
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
#pragma HLS PIPELINE
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

myType GetFresnelReflectionCoeff(const myType& cosRefl, const myType& relativeEta, const myType& invRelativeEtaSqrORExtendedAbsorptionCoeff, bool isConductor)
{
#pragma HLS INLINE
//#pragma HLS PIPELINE II=4

	myType f1(1.0), f2(1.0); // Handling total internal reflection in advance (eta < 1.0)

	if (!isConductor)
	{
		myType invRelativeEtaSqr = invRelativeEtaSqrORExtendedAbsorptionCoeff;
		myType cosRefrSqr = myType(1.0) - invRelativeEtaSqr * (myType(1.0) - cosRefl * cosRefl);
		if (cosRefrSqr >= myType(0.0))
		{
//#if defined(FAST_INV_SQRT_ENABLE) && defined(USE_FLOAT)
//			myType invCosRefr = ViRayUtils::InvSqrt(cosRefrSqr);
//			myType k = cosRefl * invCosRefr;
//
//			f1 = (k - relativeEta) / (k + relativeEta);
//			f2 = (relativeEta * k - myType(1.0)) / (relativeEta * k + myType(1.0));
//#else
#if defined(AGGRESSIVE_AREA_OPTIMIZATION_ENABLE) && !defined(UC_OPERATION)
			myType cosRefr = hls::sqrt(cosRefrSqr);
#else
			myType cosRefr = ViRayUtils::Sqrt(cosRefrSqr);
#endif

			f1 = (relativeEta * cosRefl - cosRefr) / (relativeEta * cosRefl + cosRefr);		// parallel
			f2 = (cosRefl - relativeEta * cosRefr) / (relativeEta * cosRefr + cosRefl);		// perpendicular
//#endif
			f1 *= f1;																		// parallel^2
			f2 *= f2;																		// perpendicular^2
		}
	}
	else
	{
		myType etak = invRelativeEtaSqrORExtendedAbsorptionCoeff;
		myType cosSqr = cosRefl * cosRefl;
		myType doubleEtaCos = myType(2.0) * relativeEta * cosRefl;

		// APPROX FORMULA FROM PHYSICALLY BASED RENDERING
		f1 = (etak * cosSqr - doubleEtaCos + myType(1.0)) / (etak * cosSqr + doubleEtaCos + myType(1.0));	// parallel^2
		f2 = (etak - doubleEtaCos + cosSqr) / (etak + doubleEtaCos + cosSqr);								// perpendicular^2
	}

	/*
	 * ASSUMING EQUAL CONTRIBUTION OF EACH KIND OF POLARIZATION <-> UNPOLARIZED LIGHT
	 */
	return myType(0.5) * (f1 + f2);
}

//myType GetOrenNayarDiffuseCoeff(const myType& cosR, const myType& cosI)
//{
//#pragma HLS INLINE
////#pragma HLS PIPELINE II=4
//	myType sinRS 	= myType(1.0) - cosR * cosR;
//	myType sinIS 	= myType(1.0) - cosI * cosI;
//
//	myType sinRsinI = ViRayUtils::Sqrt(sinRS * sinIS);
//
//	myType minInv 	= myType(1.0) / ((cosR > cosI) ? cosR : cosI);
//
//	myType f 		= (cosR * cosI + sinRsinI) * sinRsinI * minInv;
//	f 				= f > myType(0.0) ? f : myType(0.0);
//
//	return f;
//}

#define AGGRESSIVE_DOT(x,y) (myTypeNC(x[0]) * myTypeNC(y[0]) + myTypeNC(x[1]) * myTypeNC(y[1]) + myTypeNC(x[2]) * myTypeNC(y[2]))

myType GetOrenNayarDiffuseCoeff(const vec3& wi, const vec3& wo, const vec3& n, const myTypeNC& cosThetai, const myTypeNC& cosThetao)
{
#pragma HLS PIPELINE II=4
//#pragma HLS INLINE

//	vec3 ri = wi - n * cosThetai;
//	vec3 ro = wo - n * cosThetao;
//
//	myTypeNC riSqr = AGGRESSIVE_DOT(ri, ri); //ri * ri;
//	myTypeNC roSqr = AGGRESSIVE_DOT(ro, ro);//ro * ro;
//
////	vec3 localX = (vec3(myType(0.00424), myType(1.0), myType(0.00764))^n)^wi;//.Normalize();
////	myTypeNC localXSqr = AGGRESSIVE_DOT(localX, localX);//localX * localX;
//
//	vec3 localX = (vec3(myType(0.004239838), myType(0.999961829), myType(0.007639708))^n);//.Normalize();
//	const myTypeNC localXSqr = myTypeNC(1.0);
//
//	myTypeNC riLocalX = AGGRESSIVE_DOT(ri, localX);//ri * localX;
//	myTypeNC roLocalX = AGGRESSIVE_DOT(ro, localX);//ro * localX;
//
//#if defined(AGGRESSIVE_AREA_OPTIMIZATION_ENABLE) && !defined(UC_OPERATION)
//	myTypeNC sinPhiTerm = hls::sqrt((riSqr * localXSqr - riLocalX * riLocalX) * (roSqr * localXSqr - roLocalX * roLocalX) );
//#else
//	myTypeNC sinPhiTerm = ViRayUtils::Sqrt((riSqr * localXSqr - riLocalX * riLocalX) * (roSqr * localXSqr - roLocalX * roLocalX) );
//#endif
//
//	myTypeNC minInv   = myTypeNC(1.0) / (localXSqr * ((ViRayUtils::Abs(cosThetai) > ViRayUtils::Abs(cosThetao)) ? ViRayUtils::Abs(cosThetai) : ViRayUtils::Abs(cosThetao)));
//
//	myTypeNC phiTerm = riLocalX * roLocalX + sinPhiTerm;
//
//	return (phiTerm > myTypeNC(0.0)) ? phiTerm * minInv : myTypeNC(0.0);

	myTypeNC ri[3], ro[3], localX[3];
	const myTypeNC dummyVec[3] = {myTypeNC(0.004239838), myTypeNC(0.999961829), myTypeNC(0.007639708)};

	for (unsigned i = 0; i < 3; ++i)
	{
		ri[i] = myTypeNC(wi[i]) - myTypeNC(n[i]) * myTypeNC(cosThetai);
		ro[i] = myTypeNC(wo[i]) - myTypeNC(n[i]) * myTypeNC(cosThetao);
	}

	myTypeNC riSqr = AGGRESSIVE_DOT(ri, ri);
	myTypeNC roSqr = AGGRESSIVE_DOT(ro, ro);

	localX[0] = dummyVec[1] * myTypeNC(n[2]) - dummyVec[2] * myTypeNC(n[1]);
	localX[1] = -dummyVec[0] * myTypeNC(n[2]) + dummyVec[2] * myTypeNC(n[0]);
	localX[2] = dummyVec[0] * myTypeNC(n[1]) - dummyVec[1] * myTypeNC(n[0]);

	myTypeNC riLocalX = AGGRESSIVE_DOT(ri, localX);
	myTypeNC roLocalX = AGGRESSIVE_DOT(ro, localX);

	myTypeNC sinPhiTerm = hls::sqrt((riSqr - riLocalX * riLocalX) * (roSqr - roLocalX * roLocalX) );

	myTypeNC minInv = myTypeNC(1.0) / ((ViRayUtils::Abs(cosThetai) > ViRayUtils::Abs(cosThetao)) ? ViRayUtils::Abs(cosThetai) : ViRayUtils::Abs(cosThetao));
	myTypeNC phiTerm = riLocalX * roLocalX + sinPhiTerm;

	return (phiTerm > myTypeNC(0.0)) ? phiTerm * minInv : myTypeNC(0.0);
}

myType GetTorranceSparrowGeometricCoeff(const vec3& normal, const vec3& toViewer, const vec3& toLight, const myType& cosR, const myType& cosI, myType& nhalfDot)
{
#pragma HLS INLINE
//#pragma HLS PIPELINE II=4
	vec3 half = (toViewer + toLight).Normalize();
	myType halfDotInv = myType(1.0) / (half * toViewer);

	nhalfDot = ViRayUtils::Clamp(normal * half, myType(0.0), myType(1.0));

	myType tmp = myType(2.0) * halfDotInv * nhalfDot;
	myType f1 = tmp * cosR;
	myType f2 = tmp * cosI;

	myType f = (f1 < f2) ? f1 : f2;
	f = (f < myType(1.0)) ? f : myType(1.0);

	return f / (myType(4.0) * cosR * cosI);
}

void PerformHits(const Ray& transformedRay,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
				const vec3& objOrientation,
#endif
				unsigned objType, ShadeRec& sr)
{
//#pragma HLS INLINE
#pragma HLS PIPELINE II=1
	myType res = MAX_DISTANCE;
	myType aInv = myType(1.0);
	unsigned char faceIdx(0);

	vec3 abc;
	bool xInverse(false), yInverse(false), zInverse(false);
	Ray transformedRayByObjectDirection(transformedRay);

#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
	/*
	 * SWAPPING COORDINATES IN ORDER TO ORIENT OBJECT
	 * ALONG CHOSEN AXIS - IT IS BETTER THAN NOTHING...
	 */
	if (objOrientation[0] != myType(0.0))
	{
		if (objOrientation[0] < myType(0.0))
		{
			transformedRayByObjectDirection.direction[0] 	= -transformedRayByObjectDirection.direction[0];
			transformedRayByObjectDirection.origin[0] 		= -transformedRayByObjectDirection.origin[0];
			xInverse = true;
		}
		ViRayUtils::Swap(transformedRayByObjectDirection.direction[0], 	transformedRayByObjectDirection.direction[1]);
		ViRayUtils::Swap(transformedRayByObjectDirection.origin[0], 	transformedRayByObjectDirection.origin[1]);
	}
	else if (objOrientation[1] < myType(0.0))
	{
		transformedRayByObjectDirection.direction[1] 		= -transformedRayByObjectDirection.direction[1];
		transformedRayByObjectDirection.origin[1] 			= -transformedRayByObjectDirection.origin[1];
		yInverse = true;
	}
	else if (objOrientation[2] != myType(0.0))
	{
		if (objOrientation[2] < myType(0.0))
		{
			transformedRayByObjectDirection.direction[2] 	= -transformedRayByObjectDirection.direction[2];
			transformedRayByObjectDirection.origin[2] 		= -transformedRayByObjectDirection.origin[2];
			zInverse = true;
		}
		ViRayUtils::Swap(transformedRayByObjectDirection.direction[2], 	transformedRayByObjectDirection.direction[1]);
		ViRayUtils::Swap(transformedRayByObjectDirection.origin[2], 	transformedRayByObjectDirection.origin[1]);
	}
#endif

//	if (objType == SPHERE 	||
//		objType == CYLINDER ||
//		objType == CONE		||
//		objType == PLANE	||
//		objType == DISK		||
//		objType == SQUARE)
	switch(objType)
	{
#if defined(SPHERE_OBJECT_ENABLE) || defined(CYLINDER_OBJECT_ENABLE) || defined(CONE_OBJECT_ENABLE) || defined(PLANE_OBJECT_ENABLE) || defined(DISK_OBJECT_ENABLE) || defined(SQUARE_OBJECT_ENABLE)
	case SPHERE:
	case CYLINDER:
	case CONE:
	case PLANE:
	case DISK:
	case SQUARE:
	{
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
	break;
#endif
#if defined(CUBE_OBJECT_ENABLE) && !defined(SIMPLE_OBJECT_TRANSFORM_ENABLE)
	case CUBE:
		res = CubeTest(transformedRayByObjectDirection, faceIdx);
		break;
#endif
	default:
		res = MAX_DISTANCE;
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
		sr.localNormal = vec3(sr.localHitPoint[0], myType(1.0) - sr.localHitPoint[1], sr.localHitPoint[2]);
#else
		/*
		 * IF ORIENTATION IS INVERTED, INVERT ASYMETRIC (1.0 -> -1.0) TERM IN LOCAL NORMAL CALCULATION
		 */
		if 		(objOrientation[0] != myType(0.0))	sr.localNormal = vec3((xInverse ? myType(-1.0) : myType(1.0)) - sr.localHitPoint[0], sr.localHitPoint[1], sr.localHitPoint[2]);
		else if (objOrientation[1] != myType(0.0)) 	sr.localNormal = vec3(sr.localHitPoint[0], (yInverse ? myType(-1.0) : myType(1.0)) - sr.localHitPoint[1], sr.localHitPoint[2]);
		else										sr.localNormal = vec3(sr.localHitPoint[0], sr.localHitPoint[1], (zInverse ? myType(-1.0) : myType(1.0)) - sr.localHitPoint[2]);
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
#if  defined(CUBE_OBJECT_ENABLE) && !defined(SIMPLE_OBJECT_TRANSFORM_ENABLE)
	case CUBE:
		sr.localNormal = GetCubeNormal(faceIdx);
		break;
#endif
	default:
		break;
	}
}

void PerformShadowHits(const Ray& transformedRay,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
						const vec3& objOrientation,
#endif
						unsigned objType, ShadeRec& sr)
{
//#pragma HLS INLINE
#pragma HLS PIPELINE II=1
	myType res = MAX_DISTANCE;
	myType aInv = myType(1.0);
	unsigned char faceIdx(0);

	vec3 abc;
	bool xInverse(false), yInverse(false), zInverse(false);
	Ray transformedRayByObjectDirection(transformedRay);

#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
	/*
	 * SWAPPING COORDINATES IN ORDER TO ORIENT OBJECT
	 * ALONG CHOSEN AXIS - IT IS BETTER THAN NOTHING...
	 */
	if (objOrientation[0] != myType(0.0))
	{
		if (objOrientation[0] < myType(0.0))
		{
			transformedRayByObjectDirection.direction[0] 	= -transformedRayByObjectDirection.direction[0];
			transformedRayByObjectDirection.origin[0] 		= -transformedRayByObjectDirection.origin[0];
			xInverse = true;
		}
		ViRayUtils::Swap(transformedRayByObjectDirection.direction[0], 	transformedRayByObjectDirection.direction[1]);
		ViRayUtils::Swap(transformedRayByObjectDirection.origin[0], 	transformedRayByObjectDirection.origin[1]);
	}
	else if (objOrientation[1] < myType(0.0))
	{
		transformedRayByObjectDirection.direction[1] 		= -transformedRayByObjectDirection.direction[1];
		transformedRayByObjectDirection.origin[1] 			= -transformedRayByObjectDirection.origin[1];
		yInverse = true;
	}
	else if (objOrientation[2] != myType(0.0))
	{
		if (objOrientation[2] < myType(0.0))
		{
			transformedRayByObjectDirection.direction[2] 	= -transformedRayByObjectDirection.direction[2];
			transformedRayByObjectDirection.origin[2] 		= -transformedRayByObjectDirection.origin[2];
			zInverse = true;
		}
		ViRayUtils::Swap(transformedRayByObjectDirection.direction[2], 	transformedRayByObjectDirection.direction[1]);
		ViRayUtils::Swap(transformedRayByObjectDirection.origin[2], 	transformedRayByObjectDirection.origin[1]);
	}
#endif

//	if (objType == SPHERE 	||
//		objType == CYLINDER ||
//		objType == CONE		||
//		objType == PLANE	||
//		objType == DISK		||
//		objType == SQUARE)
	switch(objType)
	{
#if defined(SPHERE_OBJECT_ENABLE) || defined(CYLINDER_OBJECT_ENABLE) || defined(CONE_OBJECT_ENABLE) || defined(PLANE_OBJECT_ENABLE) || defined(DISK_OBJECT_ENABLE) || defined(SQUARE_OBJECT_ENABLE)
	case SPHERE:
	case CYLINDER:
	case CONE:
	case PLANE:
	case DISK:
	case SQUARE:
	{
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
	break;
#endif
#if defined(CUBE_OBJECT_ENABLE) && !defined(SIMPLE_OBJECT_TRANSFORM_ENABLE)
	case CUBE:
		res = CubeTest(transformedRayByObjectDirection, faceIdx);
		break;
#endif
	default:
		res = MAX_DISTANCE;
	}

	sr.localHitPoint = transformedRay.origin + transformedRay.direction * res;

#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
	sr.localNormal = vec3(myType(0.0), myType(1.0), myType(0.0));
#else
	sr.localNormal = objOrientation;
#endif

	switch(objType)
	{
//#ifdef SPHERE_OBJECT_ENABLE
//	case SPHERE:
//		sr.localNormal = sr.localHitPoint;
//		break;
//#endif
//#ifdef CYLINDER_OBJECT_ENABLE
//	case CYLINDER:
//#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
//		sr.localNormal = vec3(sr.localHitPoint[0], myType(0.0), sr.localHitPoint[2]);
//#else
//		if 		(objOrientation[0] != myType(0.0)) 	sr.localNormal = vec3(myType(0.0), sr.localHitPoint[1], sr.localHitPoint[2]);
//		else if (objOrientation[1] != myType(0.0)) 	sr.localNormal = vec3(sr.localHitPoint[0], myType(0.0), sr.localHitPoint[2]);
//		else 										sr.localNormal = vec3(sr.localHitPoint[0], sr.localHitPoint[1], myType(0.0));
//#endif
//		break;
//#endif
//#ifdef CONE_OBJECT_ENABLE
//	case CONE:
//#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
//		sr.localNormal = vec3(sr.localHitPoint[0], myType(1.0) - sr.localHitPoint[1], sr.localHitPoint[2]);
//#else
//		/*
//		 * IF ORIENTATION IS INVERTED, INVERT ASYMETRIC (1.0 -> -1.0) TERM IN LOCAL NORMAL CALCULATION
//		 */
//		if 		(objOrientation[0] != myType(0.0))	sr.localNormal = vec3((xInverse ? myType(-1.0) : myType(1.0)) - sr.localHitPoint[0], sr.localHitPoint[1], sr.localHitPoint[2]);
//		else if (objOrientation[1] != myType(0.0)) 	sr.localNormal = vec3(sr.localHitPoint[0], (yInverse ? myType(-1.0) : myType(1.0)) - sr.localHitPoint[1], sr.localHitPoint[2]);
//		else										sr.localNormal = vec3(sr.localHitPoint[0], sr.localHitPoint[1], (zInverse ? myType(-1.0) : myType(1.0)) - sr.localHitPoint[2]);
//#endif
//		break;
//#endif
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

#ifdef RENDER_DATAFLOW_ENABLE
void RenderSceneInnerLoop(const CCamera& camera,
							const myType* posShift,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
							const mat4* objTransform,
							const mat4* objTransformInv,
#else
							const SimpleTransform* objTransform,
#endif
							const unsigned* objType,

							const Light* lights,
							const CMaterial* materials,

							const float_union* textureData,

							pixelColorType* frameBuffer)
{
	const vec3 clearColor(myType(0.0));
	vec3 colorAccum;

	static unsigned short h = HEIGHT - 1;
	unsigned short w = 0;

#ifdef TEST_CUSTOM_CLEAR_COLOR
	bool isVaccum;
#endif

	RenderPixelLoop: for (unsigned short fbPos = 0; fbPos < FRAME_ROW_BUFFER_SIZE; ++fbPos, ++w)
	{
DO_PRAGMA(HLS PIPELINE II=DESIRED_INNER_LOOP_II)

		if (w == WIDTH)
		{
			w -= WIDTH;
			h -= 1;
		}

		colorAccum = vec3(myType(0.0));

		Ray ray, transformedRay, reflectedRay;
		ShadeRec closestSr, closestReflectedSr;

		CreatePrimaryRay(camera, posShift, h, w, ray);

		myType currentReflectivity(1.0);

#ifdef TEST_CUSTOM_CLEAR_COLOR
		isVaccum = false;
#endif

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

#ifdef TEST_CUSTOM_CLEAR_COLOR
			if (depth == 0 && !closestSr.isHit) isVaccum = true;
#endif

			if (!closestSr.isHit) break;

			myType ndir = closestSr.normal * ray.direction;
			myType ndir2min = myType(-2.0) * ndir;

			myType fresnelReflectionCoeff = GetFresnelReflectionCoeff( -ndir,
																		materials[closestSr.objIdx].GetMaterialDescription().fresnelData[1],
																		materials[closestSr.objIdx].GetMaterialDescription().fresnelData[2],
																		materials[closestSr.objIdx].GetMaterialDescription().isConductor);


			vec3 depthColor = Shade(closestSr, ray,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
									objTransform, objTransformInv,
#else
									objTransform,
#endif
									objType, lights, materials, textureData, ndir2min, -ray.direction, fresnelReflectionCoeff);

			colorAccum += depthColor * currentReflectivity;

			/*
			 * NEXT DEPTH STEP PREPARATION
			 */
#ifdef FRESNEL_REFLECTION_ENABLE
			myType reflectivity = (materials[closestSr.objIdx].GetMaterialDescription().useFresnelReflection) ? fresnelReflectionCoeff : materials[closestSr.objIdx].GetMaterialDescription().ks;
#else
			myType reflectivity = materials[closestSr.objIdx].GetMaterialDescription().ks;
#endif
			currentReflectivity *= reflectivity;

			// RAY ALREADY USED TO COMPUTE REFLECTIVITY -> REFLECT
			ray = Ray(closestSr.hitPoint, (-ray.direction).Reflect(closestSr.normal));
		}

#ifdef TEST_CUSTOM_CLEAR_COLOR
		if (isVaccum)
		{
			colorAccum = CLEAR_COLOR;
		}
#endif

		SaveColorToBuffer(colorAccum,
#ifdef PIXEL_COLOR_CONVERSION_ENABLE
							w,
#endif
							frameBuffer[fbPos]);
	}

	// Go to next row in the next call or reset row (in SELF_RESTART MODE)
	if (h != 0)
	{
		--h;
	}
	else
	{
		h = HEIGHT - 1;
	}
}

void RenderSceneOuterLoop(const CCamera& camera,
							const myType* posShift,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
							const mat4* objTransform,
							const mat4* objTransformInv,
#else
							const SimpleTransform* objTransform,
#endif
							const unsigned* objType,

							const Light* lights,
							const CMaterial* materials,

							const float_union* textureData,

							pixelColorType* frameBuffer,
							pixelColorType* outColor)
{
#pragma HLS INLINE
	for (unsigned short verticalPart = 0; verticalPart < VERTICAL_PARTS_OF_FRAME; ++verticalPart)
	{
#pragma HLS DATAFLOW
		RenderSceneInnerLoop(	camera,
								posShift,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
								objTransform,
								objTransformInv,
#else
								objTransform,
#endif
								objType,

								lights,
								materials,
								textureData,
								frameBuffer);

		memcpy(outColor + FRAME_ROW_BUFFER_SIZE * verticalPart, frameBuffer, sizeof(pixelColorType) * FRAME_ROW_BUFFER_SIZE);
	}

}
#else
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
					const CMaterial* materials,

					const float_union* textureData,

					pixelColorType* outColor)
{
	unsigned short w = 0;
	unsigned short h = HEIGHT - 1;

	vec3 colorAccum;
	pixelColorType codedColor;

	RenderPixelLoop: for (unsigned px = 0; px < NUM_OF_PIXELS; ++px, ++w)
	{
DO_PRAGMA(HLS PIPELINE II=DESIRED_INNER_LOOP_II)
		if (w == WIDTH)
		{
			w = (unsigned short)(0);
			h -= 1;
		}

		colorAccum = vec3(myType(0.0));

		Ray ray, transformedRay, reflectedRay;
		ShadeRec closestSr, closestReflectedSr;

		CreatePrimaryRay(camera, posShift, h, w, ray);

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

			myType fresnelReflectionCoeff = GetFresnelReflectionCoeff( -ndir,
																		materials[closestSr.objIdx].GetMaterialDescription().fresnelData[1],
																		materials[closestSr.objIdx].GetMaterialDescription().fresnelData[2],
																		materials[closestSr.objIdx].GetMaterialDescription().isConductor);


			vec3 depthColor = Shade(closestSr, ray,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
									objTransform, objTransformInv,
#else
									objTransform,
#endif
									objType, lights, materials, textureData, ndir2min, -ray.direction, fresnelReflectionCoeff);

			colorAccum += depthColor * currentReflectivity;

			/*
			 * NEXT DEPTH STEP PREPARATION
			 */
#ifdef FRESNEL_REFLECTION_ENABLE
			myType reflectivity = (materials[closestSr.objIdx].GetMaterialDescription().useFresnelReflection) ? fresnelReflectionCoeff : materials[closestSr.objIdx].GetMaterialDescription().ks;
#else
			myType reflectivity = materials[closestSr.objIdx].GetMaterialDescription().ks;
#endif
			currentReflectivity *= reflectivity;

			// RAY ALREADY USED TO COMPUTE REFLECTIVITY -> REFLECT
			ray = Ray(closestSr.hitPoint, (-ray.direction).Reflect(closestSr.normal));
		}

		SaveColorToBuffer(colorAccum,
#ifdef PIXEL_COLOR_CONVERSION_ENABLE
							w,
#endif
							codedColor);

		memcpy(outColor + px, &codedColor, sizeof(pixelColorType));
	}
}
#endif

void SaveColorToBuffer(vec3 color,
#ifdef PIXEL_COLOR_CONVERSION_ENABLE
						unsigned short horizontalPos,
#endif
						pixelColorType& colorOut)
{
#pragma HLS INLINE
//#pragma HLS PIPELINE II=8

	pixelColorType tempColor = 0;

	for (unsigned char i = 0; i < 3; ++i)
	{
#pragma HLS UNROLL

		if (color[i] > myType(1.0))	color[i] = myType(1.0);
		tempColor += (( unsigned(color[i] * myType(255.0)) & 0xFF ) << ((2 - i) * 8));
	}


#ifdef PIXEL_COLOR_CONVERSION_ENABLE
	if (horizontalPos & 0x1) // Odd pixel -> time for GR
	{
		tempColor = ((tempColor & 0x0000FF00) + ((tempColor >> 16) & 0x00FF));
	}
	colorOut = tempColor & 0xFFFF;
#else
	colorOut = tempColor & 0xFFFFFF;
#endif
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

			const float_union* textureData,

			const myType ndir2min,
			const vec3& toViewer,

			myType& fresnelCoeff)
{
#pragma HLS INLINE
//#pragma HLS PIPELINE II=4

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
	if (materials[closestSr.objIdx].GetTextureDescription().textureType == ViRay::CMaterial::CONSTANT) ambientColor = materials[closestSr.objIdx].GetMaterialDescription().ambientColor;

	for (unsigned char l = 1; l < LIGHTS_NUM; ++l)
	{
#pragma HLS PIPELINE
		vec3 objToLight = lights[l].position - closestSr.hitPoint;
		myType d2 = objToLight * objToLight;
		myType dInv = ViRayUtils::InvSqrt(d2);

		myType d2Inv = dInv * dInv;
		vec3 dirToLight = objToLight * dInv;

		myType dot = closestSr.normal * dirToLight;

#ifdef OREN_NAYAR_DIFFUSE_MODEL_ENABLE
		// k[0] := A	A == 1 && B == 0 -> Lambertian
		// k[1] := B
		myType OrenNayarCoeff = materials[closestSr.objIdx].GetMaterialDescription().k[0] + materials[closestSr.objIdx].GetMaterialDescription().k[1] * GetOrenNayarDiffuseCoeff(toViewer, dirToLight, closestSr.normal, ndir2min * myType(0.5), dot);//GetOrenNayarDiffuseCoeff(ndir2min * myType(0.5), dot);
#endif

		myType nhalfDot;
		myType TorranceSparrowGCoeff = GetTorranceSparrowGeometricCoeff(closestSr.normal, toViewer, dirToLight, ndir2min * myType(0.5), dot, nhalfDot);
		myType TorranceSparrowDCoeff = ViRayUtils::NaturalPow(nhalfDot, materials[closestSr.objIdx].GetMaterialDescription().specExp) * (materials[closestSr.objIdx].GetMaterialDescription().specExp + myType(2.0)) * INV_TWOPI;
#ifdef TORRANCE_SPARROW_SPECULAR_MODEL_ENABLE
		myType TorranceSparrowFCoeff = GetFresnelReflectionCoeff(nhalfDot,
																	materials[closestSr.objIdx].GetMaterialDescription().fresnelData[1],
																	materials[closestSr.objIdx].GetMaterialDescription().fresnelData[2],
																	materials[closestSr.objIdx].GetMaterialDescription().isConductor);//fresnelCoeff;
		myType specularCoeff = (materials[closestSr.objIdx].GetMaterialDescription().useTorranceSparrowSpecularReflection) ? myType(TorranceSparrowDCoeff * TorranceSparrowGCoeff * TorranceSparrowFCoeff) : TorranceSparrowDCoeff;
		fresnelCoeff = (materials[closestSr.objIdx].GetMaterialDescription().useTorranceSparrowSpecularReflection) ? TorranceSparrowFCoeff : fresnelCoeff;
#else
		myType specularCoeff = TorranceSparrowDCoeff;		// USE BLINN-PHONG MODEL INSTEAD
#endif

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
//		myType specularDot = dirToLight * ray.direction + ndir2min * dot; // TODO: DO NOT CARE ABOUT < DOT > CLAMPING
//		specularDot = (( specularDot > myType(0.0) ) ? specularDot : myType(0.0));
//		// SHOULD NEVER EXCEED 1
//		if ( specularDot > myType(1.0) ) cout << "SD: " << specularDot << ", ";

		myType lightToObjLightDirDot = -(dirToLight * lights[l].dir);
		myType lightSpotCoeff = (lightToObjLightDirDot - lights[l].coeff[0]) * lights[l].coeff[1];
		lightSpotCoeff = ViRayUtils::Clamp(lightSpotCoeff, myType(0.0), myType(1.0));

		// DIFFUSE + SPECULAR
		// (HLS::POW() -> SUBOPTIMAL QoR)
		// COLORS SHOULD BE ATTENUATED BEFOREHAND (AT MICROCONTROLLER/TESTBENCH STAGE)

		vec3 baseColor =
#ifdef DIFFUSE_COLOR_ENABLE
							diffuseColor * INV_PI
#else
						   vec3(myType(0.0))
#endif

#ifdef OREN_NAYAR_DIFFUSE_MODEL_ENABLE
							* OrenNayarCoeff
#endif
							+
#ifdef SPECULAR_HIGHLIGHT_ENABLE
#ifdef CUSTOM_COLOR_SPECULAR_HIGHLIGHTS_ENABLE
							materials[closestSr.objIdx].GetMaterialDescription().specularColor * specularCoeff
#else
							// MOST MATERIALS EXHIBIT PROPERTY THAT SPECULAR HIGHLIGHT COLOR IS THE SAME AS THE COLOR OF THE LIGHT SOURCE (SCRATCHAPIXEL)
							// HOWEVER THIS CHANGES FOR CONDUCTORS
							// THIS PROPERTY CAN BE ALSO EMULATED BY USING VEC3(1.0) AS THE SPECULAR HIGHLIGHTS COLOR WHEN CUSTOM_COLOR_SPECULAR_HIGHLIGHTS_ENABLE IS ON
							// BUT THIS FLEXIBILITY AFFECTS AREA
							vec3(specularCoeff)
#endif
#else
						   vec3(myType(0.0))
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
		PerformShadowHits(transformedRay, objTransform[n].orientation, objType[n], sr);
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
#pragma HLS PIPELINE II=1
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
//#pragma HLS ALLOCATION instances=TransformRay limit=4 function
//#pragma HLS ALLOCATION instances=PerformHits limit=4 function
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
#pragma HLS INLINE
#ifdef USE_FIXEDPOINT
	return (val > myType(0.0)) ? myType(val) : myType(-val);
#elif defined(USE_FLOAT)
	return hls::fabs(val);
#else
	return hls::abs(val);
#endif
}

#ifndef UC_OPERATION
half ViRayUtils::Abs(half val)
{
#pragma HLS INLINE
#ifdef USE_FIXEDPOINT
	return (val > myType(0.0)) ? myType(val) : myType(-val);
#elif defined(USE_FLOAT)
	return hls::fabs(val);
#else
	return hls::abs(val);
#endif
}
#endif

myTypeNC ViRayUtils::Acos(myTypeNC x)
{
#ifdef FAST_ACOS_ENABLE
#pragma HLS INLINE
//#pragma HLS PIPELINE II=4
	/*
	* LUT - BASED ACOS IMPLEMENTATION
	*/
	const unsigned LUT_SIZE(32);
	const myTypeNC acosLUT[LUT_SIZE + 2] = { 	1.5708, 1.53954, 1.50826, 1.47691,
											1.44547, 1.4139, 1.38218, 1.35026,
											1.31812, 1.2857, 1.25297, 1.21989,
											1.1864, 1.15245, 1.11798, 1.08292,
											1.0472, 1.01072, 0.97339, 0.935085,
											0.895665, 0.854958, 0.812756, 0.768794,
											0.722734, 0.67413, 0.622369, 0.566564,
											0.50536, 0.436469, 0.355421, 0.250656,
											0.0, 0.0 };	// one additional - just in case

	myTypeNC lutIdxF 		= ViRayUtils::Abs(x) * LUT_SIZE;
#ifdef USE_FIXEDPOINT
	myTypeNC mix			= lutIdxF & LOW_BITS;
	lutIdxF					= lutIdxF & HIGH_BITS;
#else
	myTypeNC mix 			= hls::modf(lutIdxF, &lutIdxF);
#endif
	unsigned lutIdx 	= lutIdxF;
	unsigned nextLutIdx = lutIdxF + 1;

	// linear interpolation
	myTypeNC calcAngle = (myTypeNC(1.0) - mix) * acosLUT[lutIdx] + mix * acosLUT[nextLutIdx];

	return ((x < myTypeNC(0.0)) ? myTypeNC(myTypeNC(PI) - calcAngle) : calcAngle);
#else
#pragma HLS INLINE
	return hls::acos(x);
#endif
}

myTypeNC ViRayUtils::AtanUtility(myTypeNC z)
{
#pragma HLS INLINE
    const myTypeNC n1(0.97239411);
    const myTypeNC n2(-0.19194795);

    return (n1 + n2 * z * z) * z;
}

myTypeNC ViRayUtils::Atan2(myTypeNC y, myTypeNC x)
{
#pragma HLS INLINE
//#pragma HLS PIPELINE II=4

#ifdef FAST_ATAN2_ENABLE
	/*
	* BASED ON:
	*
	* https://www.dsprelated.com/showarticle/1052.php
	*
	*/
	myTypeNC result(0.0);
#ifdef USE_FLOAT

	const myTypeNC n1(0.97239411);
	const myTypeNC n2(-0.19194795);

	if (x != myTypeNC(0.0))
	{
#if defined(AGGRESSIVE_AREA_OPTIMIZATION_ENABLE) && !defined(UC_OPERATION)
		half_union tXSign, tYSign, tOffset;
		const unsigned short offsetConst1(0x8000u), offsetConst2(15);
#else
		float_union tXSign, tYSign, tOffset;
		const unsigned offsetConst1(0x80000000u), offsetConst2(31);
#endif
		tXSign.fp_num = x;
		tYSign.fp_num = y;

		if (ViRayUtils::Abs(x) >= ViRayUtils::Abs(y))
		{
			tOffset.fp_num = myTypeNC(PI);
			// Add or subtract PI based on y's sign.
			tOffset.raw_bits |= tYSign.raw_bits & offsetConst1;
			// No offset if x is positive, so multiply by 0 or based on x's sign.
			tOffset.raw_bits *= tXSign.raw_bits >> offsetConst2;

			result = tOffset.fp_num;

			myTypeNC z = y / x;
			result += (n1 + n2 * z * z) * z;
		}
		else // Use atan(y/x) = pi/2 - atan(x/y) if |y/x| > 1.
		{
			tOffset.fp_num = myTypeNC(HALF_PI);
			// Add or subtract PI/2 based on y's sign.
			tOffset.raw_bits |= tYSign.raw_bits & offsetConst1;

			result = tOffset.fp_num;

			myTypeNC z = x / y;
			result -= (n1 + n2 * z * z) * z;
		}
	}
	else if (y > myTypeNC(0.0))
	{
		result = myTypeNC(HALF_PI);
	}
	else if (y < myTypeNC(0.0))
	{
		result = -myTypeNC(HALF_PI);
	}
#else

    if (x != myTypeNC(0.0))
    {
        if (Abs(x) > Abs(y))
        {
            const myTypeNC z = y / x;
            if (x > myTypeNC(0.0))
            {
                // atan2(y,x) = atan(y/x) if x > 0
                result = AtanUtility(z);
            }
            else if (y >= myTypeNC(0.0))
            {
                // atan2(y,x) = atan(y/x) + PI if x < 0, y >= 0
            	result = AtanUtility(z) + myTypeNC(PI);
            }
            else
            {
                // atan2(y,x) = atan(y/x) - PI if x < 0, y < 0
            	result = AtanUtility(z) - myTypeNC(PI);
            }
        }
        else // Use property atan(y/x) = PI/2 - atan(x/y) if |y/x| > 1.
        {
            const myTypeNC z = x / y;
            if (y > myTypeNC(0.0))
            {
                // atan2(y,x) = PI/2 - atan(x/y) if |y/x| > 1, y > 0
            	result = -AtanUtility(z) + myTypeNC(HALF_PI);
            }
            else
            {
                // atan2(y,x) = -PI/2 - atan(x/y) if |y/x| > 1, y < 0
            	result = -AtanUtility(z) - myTypeNC(HALF_PI);
            }
        }
    }
    else
    {
        if (y > myTypeNC(0.0)) // x = 0, y > 0
        {
        	result = myTypeNC(HALF_PI);
        }
        else if (y < myTypeNC(0.0)) // x = 0, y < 0
        {
        	result = -myTypeNC(HALF_PI);
        }
    }

#endif
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
#pragma HLS INLINE
	return N / D;
#endif

#else
#pragma HLS INLINE
	return N / D;
#endif
}

myType ViRayUtils::GeomObjectSolve(const vec3& abc, const Ray& transformedRay, myType& aInv)
{
#pragma HLS INLINE
//#pragma HLS PIPELINE
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
	float x2;
	float_union y;
	const float threehalfs = myType(1.5);

	x2 = val * myType(0.5);
	y.fp_num = val;
	i = y.raw_bits;

	i = 0x5f375a86 - (i >> 1);												// 'MAGICK' number

	y.raw_bits = i;

	for (unsigned char k = 0; k < FAST_INV_SQRT_ORDER; ++k)
	{
		y.fp_num = y.fp_num * (threehalfs - (x2 * y.fp_num * y.fp_num));   // Newton-Raphson iteration
	}

	return y.fp_num;
#else
#ifdef USE_FIXEDPOINT
	return hls::sqrt(float(myType(1.0) / val));
#else
	return hls::sqrt(myType(1.0) / val);
#endif
#endif

#else

#ifdef USE_FIXEDPOINT
	return hls::sqrt(float(myType(1.0) / val));
#else
	return hls::sqrt(myType(1.0) / val);
#endif
#endif

}

myType ViRayUtils::NaturalPow(myType val, unsigned char n)
{
	/*
	* N_max = 2^MAX_POWER_LOOP_ITER (FF OPENGL - like)
	*/
#pragma HLS PIPELINE II=4
	myType x = val;
	myType y(1.0);

	if (n == 0) return myType(1.0);

	PoweringLoop: for (unsigned char i = 0; i < MAX_POWER_LOOP_ITER; ++i)
	{
#pragma HLS PIPELINE
		if (n == 1) continue;

		else if (n & 0x1)
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
#pragma HLS INLINE
#ifdef FAST_INV_SQRT_ENABLE
	return val * InvSqrt(val);
#else
#ifdef USE_FIXEDPOINT
	return hls::sqrt(float(val));
#else
	return hls::sqrt(val);
#endif
#endif
}

void ViRayUtils::Swap(myType& val1, myType& val2)
{
#pragma HLS INLINE
	myType tmp = val1;
	val1 = val2;
	val2 = tmp;
}

};
