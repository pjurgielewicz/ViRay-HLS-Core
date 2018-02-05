#ifndef VISION__H_
#define VISION__H_

#include "vec3.h"

/*
 * Basic struct to keep origin and direction of rays
 */
struct Ray
{
	vec3 origin;
	vec3 direction;

	Ray(const vec3 &origin = vec3(0.0, 0.0, 0.0),
		 const vec3 &direction = vec3(0.0, 0.0, -1.0)) : origin( origin ), direction( direction ) {}

	Ray( const Ray &r ) : origin( r.origin ), direction( r.direction ) {}
};



/*
 * CCamera defines how the world is seen by the observer
 * - position of the observer
 * - viewing direction
 * - space distortion & zoom
 */
class CCamera
{
public:
	CCamera(myType hDepthRatio, myType vDepthRatio,
			unsigned refHRes = 1920, unsigned refVRes = 1080,
			const vec3 &eyePosition = vec3(0.0, 0.0, 0.0),
			const vec3 &lookAtDir = vec3(0.0, 0.0, -1.0),
			const vec3 &up = vec3(0.0, 1.0, 0.0)) :
				refHRes(refHRes), refVRes(refVRes),
				eyePosition(eyePosition), lookAtDir(lookAtDir), up(up)
	{
		hFactor = hDepthRatio / (myType(refHRes));
		vFactor = vDepthRatio / (myType(refVRes));

		ComputeUVW();
	}

	/*
	 * Build camera coordinate system
	 */
	void ComputeUVW()
	{
		// DON'T HAVE TO NORMALIZE AS LONG AS INPUT VECTORS ARE UNIT VECTORS
		w = -lookAtDir;		// camera z
//		w = w.Normalize();
		u = up ^ w;			// camera x
//		u = u.Normalize();
		v = w ^ u;			// camera y
	}

	/*
	 * Get the ray that corresponds to the current camera frame and the position of the pixel
	 */
	Ray GetCameraRayForPixel(const myType* p) const
	{
//#pragma HLS INLINE
		vec3 direction = u * p[0] * hFactor +
						 v * p[1] * vFactor -
						 w;

		/*
		 * ALTHOUGH NORMALIZATION IS NOT MANDATORY WHEN CHECKING FOR DISTANCE FROM OBJECT,
		 * IT IS USED DURING SHADING STAGE (FOR DOT PRODUCT)
		 */

		return Ray(eyePosition, direction.Normalize());
	}

	myType GetRefHRes() const { return refHRes; }
	myType GetRefVRes() const { return refVRes; }
protected:
	vec3 eyePosition;
	vec3 lookAtDir;
	vec3 up;
	vec3 u, v, w;

	myType refHRes;
	myType refVRes;

	myType hFactor, vFactor;
};

#endif
