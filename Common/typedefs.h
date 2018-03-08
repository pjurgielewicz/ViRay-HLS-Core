#ifndef TYPEDEFS__H_
#define TYPEDEFS__H_

#define UC_OPERATION

#ifndef UC_OPERATION

#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_math.h"

#else

#include <cmath>

#endif

#define PRAGMA_SUB(x) _Pragma (#x)
#define DO_PRAGMA(x) PRAGMA_SUB(x)

#define INNER_LOOP_UNROLL_FACTOR 1
#define DESIRED_INNER_LOOP_II 8									// How many clock cylces are required to output the new pixel

//#define USE_FIXEDPOINT

#ifdef USE_FIXEDPOINT

#define FIXEDPOINT_WIDTH 32
#define FIXEDPOINT_INTEGER_BITS 22
typedef ap_fixed<FIXEDPOINT_WIDTH, FIXEDPOINT_INTEGER_BITS, AP_RND> myType;

#define CORE_BIAS (myType(0.01))
#define MAX_DISTANCE 1000000

#else

#define USE_FLOAT

#ifdef USE_FLOAT

typedef float myType;

#else

typedef half myType;

#endif

// USED MAINLY FOR TEXTURE STORAGE AND FAST BIT CONVERSION
// SEE << fp_mul_pow2 >> example project
typedef union {
    float fp_num;
    unsigned raw_bits;
    struct {
    	unsigned mant : 23;
    	unsigned bexp : 8;
    	unsigned sign : 1;
    };
} float_union;													// Usage: texture storage & fast inverse square root


#define CORE_BIAS 		(myType(0.001))						// Small positive number to test against computed distance
#define MAX_DISTANCE 	(myType(1000000))						// Ray tracer's 'infinity' distance

#endif

typedef unsigned pixelColorType;								// Pixel buffer type

/*
 * AVAILABLE PRIMITIVES
 */
enum ObjectType{
	INVALID = -1,
	SPHERE = 1,
	PLANE,
	DISK,
	SQUARE,
	CYLINDER,
	CUBE,
	CONE,
};																// Globally defined object types

/*
 * ENABLE/DISABLE
 */

#define SIMPLE_OBJECT_TRANSFORM_ENABLE							// If on matrix transforms are off, only axis aligned transformations are allowed

//#define SELF_RESTART_ENABLE									// If on the core enters infinite loop of rendering right after loading textures, all other parameters can change between frames

#define DEEP_RAYTRACING_ENABLE								// Only in simulation mode - allows to specify how deep ray tracing recursion should be used

//#define RGB_TO_YUV422_CONVERSION_ENABLE							// Convert RGB pixel color to YUV422 standard (to use with limited bandwidth ADV7511)

#define SPHERE_OBJECT_ENABLE									// Start of allowed object togglers
#define PLANE_OBJECT_ENABLE
#define DISK_OBJECT_ENABLE
#define SQUARE_OBJECT_ENABLE
#define CYLINDER_OBJECT_ENABLE
//#define CUBE_OBJECT_ENABLE
#define CONE_OBJECT_ENABLE

#define AMBIENT_COLOR_ENABLE									// Ambient color: on/off
#define DIFFUSE_COLOR_ENABLE									// Diffuse color: on/off
#define SPECULAR_HIGHLIGHT_ENABLE								// Specular highlights: on/off
#define INTERNAL_SHADING_ENABLE									// Surface normal inversion when pointing out of ray direction: on/off
#define SHADOW_ENABLE											// Shadows rendering: on/off
#define SELF_SHADOW_ENABLE										// Can an object cast shadows on itself: on/off
#define FRESNEL_REFLECTION_ENABLE								// Allow to compute reflection amount based on angle of incidence and relative index of refraction: on/off
#define OREN_NAYAR_DIFFUSE_MODEL_ENABLE							// Use roughness-based statistical model to compute amount of diffuse light
#define TORRANCE_SPARROW_SPECULAR_MODEL_ENABLE

#define TEXTURE_ENABLE											// Texturing: on/off
//#define TEXTURE_URAM_STORAGE									// If enabled textures will be placed in the UltraRAM instead of BlockRAM
#define BILINEAR_TEXTURE_FILTERING_ENABLE						// Texture filtering: on/off
#define ADVANCED_TEXTURE_MAPPING_ENABLE							// If enabled allow user to use SPHERICAL and CYLINDRICAL texture mapping methods, when off and the user will try to use it RECTANGULAR mapping will be used anyway
//#define FAST_INV_SQRT_ENABLE									// Newton-Raphson-Quake-based implementation of inverse square root approximation: on/off
//#define FAST_DIVISION_ENABLE									// Newton-Raphson-based implementation od division: on/off
#define FAST_ATAN2_ENABLE										// Polynomial approximation of atan2(): on/off
#define FAST_ACOS_ENABLE										// LUT-based arcus cosinus implementation: on/off

/*
 * SCENE 'RANGE' DEFINITION
 */

#define WIDTH 						((unsigned short)(1920))	// Final image width
#define HEIGHT 						((unsigned short)(1080))	// Final image height

#define TEXT_PAGE_SIZE				((unsigned)(256 * 256))		// How much memory will be used for texture storage

#define FRAME_ROW_BUFFER_SIZE 		(WIDTH)						// Size of the temporary row framebuffer
#define LIGHTS_NUM 					((unsigned char)(2))		// The number of lights being processed, the 0-th light is always ambient illumination, 1...n are full lights
#define OBJ_NUM 					((unsigned char)(8))		// Maximum number of geometric objects in the scene

#ifdef DEEP_RAYTRACING_ENABLE

#define RAYTRACING_DEPTH			((unsigned char)(2))		// If DEEP_RAYTRACING_ENABLE is specified use this value to determine how deep ray tracing algorithm should go

#else

#define PRIMARY_COLOR_ENABLE									// If DEEP_RAYTRACING_ENABLE is unset show the resulting color from the primary rays on/off
#define REFLECTION_ENABLE										// If DEEP_RAYTRACING_ENABLE is unset show the resulting color from the secondary rays on/off

#endif

#define MAX_POWER_LOOP_ITER			((unsigned char)(7))		// Determines to which maximum natural power the number can be raised

#define FAST_INV_SQRT_ORDER 		((unsigned char)(2))		// How many iterations Newton-Raphson fast inverse sqrt algorithm should perform
#define FAST_DIVISION_ORDER 		((unsigned char)(2))		// How many iterations Newton-Raphson fast division algorithm should perform

#define PI 							(myType(3.141592))			// PI
#define HALF_PI						(myType(1.570796))			// 0.5 * PI
#define TWOPI						(myType(6.283184))			// 2.0 * PI
#define INV_PI						(myType(0.31831))			// 1.0 / PI
#define INV_TWOPI					(myType(0.159155))			// 1.0 / (2.0 * PI)

#ifdef UC_OPERATION
#define UC_TEMP_NOISE_TEXTURE_POOL 	((unsigned char*)0x80000000 + (0x05000000))
#define UC_TEMP_TEXTURE_POOL 		(UC_TEMP_NOISE_TEXTURE_POOL + TEXT_PAGE_SIZE * 4)
#endif

#endif
