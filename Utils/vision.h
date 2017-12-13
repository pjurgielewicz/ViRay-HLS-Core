#ifndef VISION__H_
#define VISION__H_

#include "vec4.h"

class CRay
{
public:
	vec4 origin;
	vec4 direction;

	CRay(const vec4 &origin = vec4(0.0, 0.0, 0.0),
		 const vec4 &direction = vec4(0.0, 0.0, -1.0)) :origin( origin ), direction( direction ) {}

	CRay( const CRay &ray ) : origin( ray.origin ), direction( ray.direction ) {}
};

class CCamera
{
public:
	CCamera(const vec4 &eyePosition = vec4(0.0, 0.0, 0.0),
			const vec4 &lookAt = vec4(0.0, 0.0, -1.0),
			const vec4 &up = vec4(0.0, 1.0, 0.0), unsigned refHRes = 640, unsigned refVRes = 480) :
				eyePosition(eyePosition), lookAt(lookAt), up(up), refHRes(refHRes), refVRes(refVRes), viewDistance(100.0)
	{
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

	CRay GetCameraRayForPixel(const vec4 &p) const
	{
//#pragma HLS INLINE
		vec4 direction = u * p.data[0] + v * p.data[1] - w * viewDistance;
		return CRay(eyePosition, direction/*.Normalize()*/);
	}

	myType GetRefHRes() const { return refHRes; }
	myType GetRefVRes() const { return refVRes; }
protected:
	vec4 eyePosition;
	vec4 lookAt;
	vec4 up;
	vec4 u, v, w;

	myType refHRes;
	myType refVRes;

	myType viewDistance;
};

#endif
