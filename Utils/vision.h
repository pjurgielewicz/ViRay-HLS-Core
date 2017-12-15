#ifndef VISION__H_
#define VISION__H_

#include "vec3.h"

class CRay
{
public:
	vec3 origin;
	vec3 direction;

	CRay(const vec3 &origin = vec3(0.0, 0.0, 0.0),
		 const vec3 &direction = vec3(0.0, 0.0, -1.0)) : origin( origin ), direction( direction ) {}

	CRay( const CRay &ray ) : origin( ray.origin ), direction( ray.direction ) {}
};

class CCamera
{
public:
	CCamera(myType hDepthRatio, myType vDepthRatio,
			unsigned refHRes = 640, unsigned refVRes = 480,
			const vec3 &eyePosition = vec3(0.0, 0.0, 0.0),
			const vec3 &lookAt = vec3(0.0, 0.0, -1.0),
			const vec3 &up = vec3(0.0, 1.0, 0.0)) :
				refHRes(refHRes), refVRes(refVRes),
				eyePosition(eyePosition), lookAt(lookAt), up(up)
	{
		hFactor = hDepthRatio / (myType(refHRes));
		vFactor = vDepthRatio / (myType(refVRes));

		ComputeUVW();
	}

	void ComputeUVW()
	{
		// DON'T HAVE TO NORMALIZE AS LONG AS INPUT VECTORS ARE UNIT VECTORS
		w = eyePosition - lookAt;
//		w = w.Normalize();
		u = up ^ w;
//		u = u.Normalize();
		v = w ^ u;
	}

	CRay GetCameraRayForPixel(const myType* p) const
	{
//#pragma HLS INLINE
		vec3 direction = u * p[0] * hFactor +
						 v * p[1] * vFactor -
						 w;
		return CRay(eyePosition, direction/*.Normalize()*/);
	}

	myType GetRefHRes() const { return refHRes; }
	myType GetRefVRes() const { return refVRes; }
protected:
	vec3 eyePosition;
	vec3 lookAt;
	vec3 up;
	vec3 u, v, w;

	myType refHRes;
	myType refVRes;

	myType hFactor, vFactor;
};

#endif
