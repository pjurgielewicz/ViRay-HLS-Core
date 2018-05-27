#ifndef TYPEDEFS__H_
#define TYPEDEFS__H_

//#define UC_OPERATION												// Switch between HLS and SDK environments

#ifndef UC_OPERATION

#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_math.h"

#else

#include <cmath>

#endif

#define PRAGMA_SUB(x) _Pragma (#x)
#define DO_PRAGMA(x) PRAGMA_SUB(x)

#define DESIRED_INNER_LOOP_II 8											// How many clock cylces are required to output the new pixel

//#define USE_FIXEDPOINT

#ifdef USE_FIXEDPOINT

#define FIXEDPOINT_WIDTH 		32
#define FIXEDPOINT_INTEGER_BITS 16
#define HIGH_BITS 				((0xFFFFFFFF >> (FIXEDPOINT_WIDTH - FIXEDPOINT_INTEGER_BITS)) << (FIXEDPOINT_WIDTH - FIXEDPOINT_INTEGER_BITS) )
#define LOW_BITS				(0xFFFFFFFF - HIGH_BITS)
typedef ap_fixed<FIXEDPOINT_WIDTH, FIXEDPOINT_INTEGER_BITS, AP_RND> myType;

#define CORE_BIAS (myType(0.001))
#define MAX_DISTANCE (myType(1000000))

#else

#define USE_FLOAT

#ifdef USE_FLOAT

typedef float myType;

#else

typedef half myType;

#endif

#define CORE_BIAS 		(myType(0.001))									// Small positive number to test against computed distance
#define MAX_DISTANCE 	(myType(1000000))								// Ray tracer's 'infinity' distance

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
} float_union;															// Usage: texture storage & fast inverse square root

typedef unsigned pixelColorType;										// Pixel buffer type

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
};																		// Globally defined object types

/*
 * ENABLE/DISABLE
 */

#define SIMPLE_OBJECT_TRANSFORM_ENABLE									// If on matrix transforms are off, only axis aligned transformations are allowed

//#define SELF_RESTART_ENABLE												// If on core enters infinite loop of rendering right after loading textures, all other parameters can change between frames - reduces # of clock cycles (TEXT_PAGE_SIZE)

#define PIXEL_COLOR_CONVERSION_ENABLE									// Convert RGB pixel color to 422 standard (to use with limited bandwidth ADV7511)

#define RENDER_DATAFLOW_ENABLE											// DATAFLOW seems to require less logic around rendering loop however this construct uses extra cycles for each start of RenderSceneInnerLoop and final buffer dump
																		// The amount of theses extra cycles can be optimized by FRAME_ROWS_IN_BUFFER
																		// If disabled use plain RenderScene with PIPELINE only
																		// HINT: In Vivado after synthesis the difference in area consumption is almost negligible so PIPELINE is preferred over DATAFLOW
																		// HINT2: Implementation says that PIPELINE solution is almost not feasible (high implementation effort)

//#define AGGRESSIVE_AREA_OPTIMIZATION_ENABLE								// To use or not to use half's for some less computational critical tasks
#if defined(AGGRESSIVE_AREA_OPTIMIZATION_ENABLE) && !defined(UC_OPERATION)
typedef half myTypeNC;
typedef union {
    half fp_num;
    unsigned short raw_bits;
    struct {
    	unsigned mant : 10;
    	unsigned bexp : 5;
    	unsigned sign : 1;
    };
} half_union;
#else
typedef myType myTypeNC;
#endif

#define SPHERE_OBJECT_ENABLE											// Start of allowed object togglers
#define PLANE_OBJECT_ENABLE
#define DISK_OBJECT_ENABLE
#define SQUARE_OBJECT_ENABLE
#define CYLINDER_OBJECT_ENABLE
//#define CUBE_OBJECT_ENABLE
#define CONE_OBJECT_ENABLE

#define AMBIENT_COLOR_ENABLE											// Ambient color: on/off
#define DIFFUSE_COLOR_ENABLE											// Diffuse color: on/off
#define SPECULAR_HIGHLIGHT_ENABLE										// Specular highlights: on/off
#define CUSTOM_COLOR_SPECULAR_HIGHLIGHTS_ENABLE							// If on use assigned specular highlight color and the color of the light otherwise
#define INTERNAL_SHADING_ENABLE											// Surface normal inversion when pointing out of ray direction: on/off
#define SHADOW_ENABLE													// Shadows rendering: on/off
#define SELF_SHADOW_ENABLE												// Can an object cast shadows on itself: on/off
#define FRESNEL_REFLECTION_ENABLE										// Allow to compute reflection amount based on angle of incidence and relative index of refraction: on/off
#define OREN_NAYAR_DIFFUSE_MODEL_ENABLE									// Use roughness-based statistical model to compute amount of diffused light which is seamlessly able to blend to the Lambert model of diffuse reflection
#define TORRANCE_SPARROW_SPECULAR_MODEL_ENABLE							// Enable possibility to use Torrance-Sparrow specular reflection model

#define TEXTURE_ENABLE													// Texturing: on/off
//#define TEXTURE_URAM_STORAGE											// If enabled textures will be placed in the UltraRAM instead of BlockRAM
#define BILINEAR_TEXTURE_FILTERING_ENABLE								// Texture filtering: on/off
#define ADVANCED_TEXTURE_MAPPING_ENABLE									// If enabled allow user to use SPHERICAL and CYLINDRICAL texture mapping methods, when off and the user will try to use it RECTANGULAR mapping will be used anyway
//#define FAST_INV_SQRT_ENABLE											// Newton-Raphson-Quake-based implementation of inverse square root approximation: on/off
//#define FAST_DIVISION_ENABLE											// Newton-Raphson-based implementation od division: on/off
#define FAST_ATAN2_ENABLE												// Polynomial approximation of atan2(): on/off
#define FAST_ACOS_ENABLE												// LUT-based arcus cosinus implementation: on/off

/*
 * SCENE 'RANGE' DEFINITION
 */

#define WIDTH 						((unsigned short)(1920))			// Final image width
#define HEIGHT 						((unsigned short)(1080))			// Final image height
#define WIDTH_INV					(myType(1.0) / myType(WIDTH))
#define HEIGHT_INV					(myType(1.0) / myType(HEIGHT))

#define FRAME_ROWS_IN_BUFFER		((unsigned short)(20))				// Optimized for loosing as least clock cycles as possible for RenderInnerLoop start while being the divider of HEIGHT
#define VERTICAL_PARTS_OF_FRAME		(HEIGHT / FRAME_ROWS_IN_BUFFER)		// How many iterations of RenderOuterLoop there will be
#define FRAME_ROW_BUFFER_SIZE 		(WIDTH * FRAME_ROWS_IN_BUFFER)		// Size of the temporary row framebuffer

#define NUM_OF_PIXELS				(unsigned(WIDTH) * unsigned(HEIGHT))


#define TEXT_PAGE_SIZE				((unsigned)(256 * 256))				// How much memory will be used for texture storage

#define LIGHTS_NUM 					((unsigned char)(2))				// The number of lights being processed, the 0-th light is always ambient illumination, 1...n are full lights
#define OBJ_NUM 					((unsigned char)(8))				// Maximum number of geometric objects in the scene

#define RAYTRACING_DEPTH			((unsigned char)(2))				// Use this value to determine how deep ray tracing algorithm should go

#define MAX_POWER_LOOP_ITER			((unsigned char)(10))				// Determines to which maximum natural power the number can be raised (2^MAX_POWER_LOOP_ITER)

#define FAST_INV_SQRT_ORDER 		((unsigned char)(2))				// How many iterations Newton-Raphson fast inverse sqrt algorithm should perform
#define FAST_DIVISION_ORDER 		((unsigned char)(2))				// How many iterations Newton-Raphson fast division algorithm should perform

#define PI 							(myType(3.141592))					// PI
#define HALF_PI						(myType(1.570796))					// 0.5 * PI
#define TWOPI						(myType(6.283184))					// 2.0 * PI
#define INV_PI						(myType(0.31831))					// 1.0 / PI
#define INV_TWOPI					(myType(0.159155))					// 1.0 / (2.0 * PI)

//#define TEST_CUSTOM_CLEAR_COLOR
#ifdef TEST_CUSTOM_CLEAR_COLOR
#define CLEAR_COLOR vec3(myType(1.0))
#endif

#ifdef UC_OPERATION
#define UC_TEMP_NOISE_TEXTURE_POOL 	((unsigned char*)0x80000000 + (0x05000000))
#define UC_TEMP_TEXTURE_POOL 		(UC_TEMP_NOISE_TEXTURE_POOL + TEXT_PAGE_SIZE * 4)
//#define UC_DIAGNOSTIC_VERBOSE
//#define UC_DIAGNOSTIC_DEBUG
#endif

#endif
