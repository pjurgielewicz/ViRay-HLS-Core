#ifndef VIRAY__H_
#define VIRAY__H_

#include "Common/typedefs.h"
#include "Utils/mat4.h"
#include "Utils/vec3.h"
#include "Utils/vision.h"
#include "Utils/material.h"

namespace ViRay
{
	/*
	 * ViRay Utility Functions
	 */
	namespace ViRayUtils
	{
		/*
		 * Custom absolute value solution to help working with deifferent data types.
		 * return |val|.
		 */
		myType Abs(myType val);

		/*
		 * Get arcus cosinus value for parameter x.
		 * If FAST_ACOS_ENABLE is specified use LUT-based approximation with linear approximation between points.
		 * This reduces FPGA LUT utilization by roughly 4k.
		 * Call hls::acos() otherwise.
		 */
		myType Acos(myType x);

		/*
		 * Get arcus tangens value for parameters y and x.
		 * If FAST_ATAN2_ENABLE is specified use polynomial approximation between y/x : [-1, 1].
		 * The idea & code is based on: https://www.dsprelated.com/showarticle/1052.php
		 * This approximate solution reduces FPGA LUT utilization by roughly 13k.
		 * Call hls::atan2() otherwise.
		 */
		myType Atan2(myType y, myType x);

		/*
		 * Restrict val within [min, max] range
		 */
		myType Clamp(myType val, myType min = myType(0.0), myType max = myType(1.0));

		/*
		 * Divide N by D
		 * if FAST_DIVISION_ENABLE is specified and floating point numbers (float, half) are used
		 * get approximate value of division using Newton-Raphson method.
		 * Start values are sourced from: https://en.wikipedia.org/wiki/Division_algorithm#Newton%E2%80%93Raphson_division
		 * Number of Newton-Raphson iterations can be controlled by: FAST_DIVISION_ORDER
		 * Most useful in C++ simulation since it reduces time but does not affect area positively.
		 * Perform N/D directly otherwise.
		 */
		myType Divide(myType N, myType D);

		/*
		 * Get distance from the transformedRay.origin along transformedRay.direction to the unit (local y: |y| < 1) quadric described by the abc values
		 * abc are the coefficients of the quadratic equation: ax^2 + bx + c = 0
		 * a = abc[0], b = abc[1], c = abc[2]
		 * To save resources it returns aInv = 1.0 / a by reference which is used for ray-plane hit detection
		 * If the object is not hit return MAX_DISTANCE
		 */
		myType GeomObjectSolve(const vec3& abc, const Ray& transformedRay, myType& aInv);

		/*
		 * Get inverse square root of val
		 * If FAST_INV_SQRT_ENABLE is specified and floating point numbers (float) are used
		 * get approximate value using Quake-Newton-Raphson method: https://en.wikipedia.org/wiki/Fast_inverse_square_root#Overview_of_the_code
		 * Number of Newton-Raphson iterations can be controlled by: FAST_INV_SQRT_ORDER
		 * Most useful in C++ simulation since it reduces time but does not affect area positively.
		 * Perform hls::sqrt(myType(1.0) / val) otherwise.
		 */
		myType InvSqrt(myType val);

		/*
		 * Get val to the power of n using powering by squaring algorithm
		 * Number of iteration (thus maximum n) is controlled by: MAX_POWER_LOOP_ITER
		 */
		myType NaturalPow(myType val, unsigned char n);

		/*
		 * If it is possible to use fast implementation of ViRayUtils::InvSqrt() call val * ViRayUtils::InvSqrt(val)
		 * otherwise hls::sqrt(val);
		 */
		myType Sqrt(myType val);

		/*
		 * Custom swap function solution
		 */
		void Swap(myType& val1, myType& val2);

	}

	/*
	 * Structure that holds all necessary information about the hit
	 * that is used then at the shading stage
	 */
	struct ShadeRec{
		vec3 localNormal;
		vec3 normal;

		vec3 localHitPoint;
		vec3 hitPoint;

		vec3 orientation;

		myType distanceSqr;

		unsigned char objIdx;

		bool isHit;

		ShadeRec()
		{
			/*
			 * DO NOT TOUCH INITIALIZATION SINCE IT MAY RUIN SHADING
			 * WITH GREAT PROBABILITY
			 */

			localNormal 	= vec3(myType(0.0), myType(-1.0), myType(0.0));
			normal 			= vec3(myType(0.0), myType(-1.0), myType(0.0));

			localHitPoint 	= vec3(myType(0.0));
			hitPoint 		= vec3(myType(0.0));

			orientation		= vec3(myType(0.0));

			distanceSqr 	= myType(MAX_DISTANCE);

			objIdx 			= 0;
			isHit 			= false;
		}
	};

	/*
	 * Internal light data structure
	 */
	struct Light{
		vec3 position;
		vec3 dir;
		vec3 color;
		vec3 coeff;					// 0 - cos(outer angle), 1 - inv cone cos(angle) difference, 2 - ?
	};

	/*
	 * Structure to handle simplified object transformation: translation + scale + axis align only
	 */
	struct SimpleTransform{
		vec3 orientation;

		vec3 scale;
		vec3 invScale;
		vec3 translation;

		/*
		 * Transform vec by direct transformation
		 */
		vec3 Transform(const vec3& vec) const
		{
			return vec.CompWiseMul(scale) + translation;
		}

		/*
		 * Transform vec by inverse transformation
		 */
		vec3 TransformInv(const vec3& vec) const
		{
			return (vec - translation).CompWiseMul(invScale);
		}

		/*
		 * TODO: THESE ARE THE SAME FUNCTIONS
		 */

		/*
		 * Transform vec (direction) by inverse transformation
		 */
		vec3 TransformDirInv(const vec3& vec) const
		{
			return vec.CompWiseMul(invScale);
		}

		/*
		 * Transform vec (direction) by inverse transpose transformation
		 */
		vec3 TransposeTransformDir(const vec3& vec) const
		{
			return vec.CompWiseMul(invScale);
		}
	};

	/*
	 * ***************************************  VIRAY CORE FUNCTIONS ***************************************
	 */

	/*
	 * Knowing the camera viewing system (camera & posShift)
	 * create an initial ray for the pixel coordinate (c, r)
	 */
	void CreatePrimaryRay(const CCamera& camera, const myType* posShift, unsigned short r, unsigned short c, Ray& ray);
	
	/*
	 * Ray-unit cube object hit test.
	 * If cube object is hit return distance along transformedRay.direction and the face index.
	 * Otherwise return MAX_DISTANCE.
	 * NOTE: it is not adviced to use cube objects (CUBE_OBJECT_ENABLE) in real FPGA design unless there are spare resources, since
	 * this objects need additional 3 divisions
	 */
	myType CubeTest(const Ray& transformedRay, unsigned char& face);

	/*
	 * Knowing the cube face (obtained from CubeTest())
	 * get the corresponding normal
	 */
	vec3 GetCubeNormal(const unsigned char& faceIdx);

	/*
	 * Knowing angle between ray.direction and object normal (cosRefl)
	 * compute Fresnel reflection coefficient for the unpolarized beam of light
	 */
	myType GetFresnelReflectionCoeff(const myType& cosRefl, const myType& relativeEta, const myType& invRelativeEtaSqr);


	/*
	 * Calculate angle-dependent term in Oren-Nayar model
	 * cosR - cosine between toViewer and normal
	 * cosI - cosine between toLight and normal
	 */
	myType GetOrenNayarDiffuseCoeff(const myType& cosR, const myType& cosI);

	/*
	 * Calculate geometric term in Torrance-Sparrow model
	 * cosR - cosine between toViewer and normal
	 * cosI - cosine between toLight and normal
	 */
	myType GetTorranceSparrowGeometricCoeff(const vec3& normal, const vec3& toViewer, const vec3& toLight, const myType& cosR, const myType& cosI, myType& nhalfDot);


	/*
	 * Determine whether and where exactly (in local coordinates) the hit occurred
	 * The user can change the complexity of this function by enabling/disabling object flags (ie. SPHERE_OBJECT_ENABLE)
	 * If the objType is not recognizable the result hit is at MAX_DISTANCE along transformedRay.
	 */
	void PerformHits(const Ray& transformedRay,
#ifdef SIMPLE_OBJECT_TRANSFORM_ENABLE
						const vec3& objOrientation,
#endif
						unsigned objType, ShadeRec& sr);

	/*
	 * Per pixel image processing - this function triggers:
	 * - primary ray creation
	 * - hit object detection
	 * - direct shading
	 * - 1 bounce of ray
	 * - secondary object detection
	 * - shading of reflections and reflection coefficient determination
	 * - saving resulting pixel to the frame buffer
	 * The degree of speedup using HLS PIPELINE relies critically on the: DESIRED_INNER_LOOP_II
	 * In theory the code might process 1 pixel per 1 clock cycle however the resource usage becomes enormous.
	 * Thus, the user has to find the golden mean between the area and performance
	 *
	 * The user can enable/disable reflection pass using: REFLECTION_ENABLE
	 *
	 * To make all surfaces mirror-like comment: FRESNEL_REFLECTION_ENABLE then the reflection coefficient will be
	 * constant on the entire surface and equal to the specular highlight coefficient
	 *
	 * If DEEP_RAYTRACING_ENABLE is specified (only for C++ simulation) it can reach up to:
	 * RAYTRACING_DEPTH raytracing depth (1 - direct, 2 - single reflection, 3 - double reflection, ..., n - 1 reflection)
	 */
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

								const float_union* textureData,

								pixelColorType* frameBuffer);

	/*
	 * Outer per row rendering loop.
	 * Triggers RenderSceneInnerLoop() for each vertical line of the image
	 * and then bursts it into the memory.
	 * Uses HLS DATAFLOW optimization so the copy cycles are not degrading the performance
	 */
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

								const float_union* textureData,

								pixelColorType* frameBuffer,
								pixelColorType* outColor);

	/*
	 * Having internal color values transform them to the 0...255 range using unsigned char (x * 255) transformation
	 * If the initial intensity is higher than 1.0 the value will be clamped to 255.
	 * The output color is saved in the color buffer as follows:
	 * MSB
	 * |
	 * 8b : < not used >
	 * 8b : R = (unsigned char)clamp(color[0] * 255, 0, 255)
	 * 8b : G = (unsigned char)clamp(color[1] * 255, 0, 255)
	 * 8b : B = (unsigned char)clamp(color[2] * 255, 0, 255)
	 * |
	 * LSB
	 */
	void SaveColorToBuffer(vec3 color, pixelColorType& colorOut);

	/*
	 * Determine object illumination (including shadows):
	 * - ambient light (with texture correction)
	 * - diffuse light (with texture application)
	 * - specular highlights
	 */
	vec3 Shade(const ShadeRec& closestSr,
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

				const myType& fresnelCoeff);

	/*
	 * Very similar to the VisibilityTest() but uses shadow-specific functionality.
	 * It just has to determine whether there is shadow or not.
	 */
	void ShadowVisibilityTest(const Ray& shadowRay,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
								const mat4* objTransform,
								const mat4* objTransformInv,
#else
								const SimpleTransform* objTransform,
#endif
								const unsigned* objType,
								const ShadeRec& closestSr,
								myType d2,
								ShadeRec& shadowSr);

	/*
	 * Having the ray in world space transform it to the object space (inverse transformation)
	 * This operation simplifies objects description and allows to deform objects.
	 * However, this requires several inverse & direct vector transformations to obtain right results.
	 */
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
	void TransformRay(const mat4& mat, const Ray& ray, Ray& transformedRay);
#else
	void TransformRay(const SimpleTransform& transform, const Ray& ray, Ray& transformedRay);
#endif

	/*
	 * If the next object is nearer the observer update closest object reference
	 */
	void UpdateClosestObject(ShadeRec& current,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
								const mat4& transform,
#else
								const SimpleTransform& transform,
#endif
								const unsigned char& n,
								const Ray& ray,
								ShadeRec& best);

	/*
	 * If there is anything on the way towards the light make the system know about that fact
	 */
	void UpdateClosestObjectShadow(const ShadeRec& current,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
									const mat4& transform,
#else
									const SimpleTransform& transform,
#endif
									const Ray& shadowRay,
									myType distanceToLightSqr,
									ShadeRec& best);

	/*
	 * Loop over all objects in the scene for a given ray
	 * to check whether it hits something or not
	 */
	void VisibilityTest(const Ray& ray,
#ifndef SIMPLE_OBJECT_TRANSFORM_ENABLE
						const mat4* objTransform,
						const mat4* objTransformInv,
#else
						const SimpleTransform* objTransform,
#endif
						const unsigned* objType,
						ShadeRec& closestSr);
}

#endif
