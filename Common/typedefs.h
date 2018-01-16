#ifndef TYPEDEFS__H_
#define TYPEDEFS__H_

#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_math.h"

#define PRAGMA_SUB(x) _Pragma (#x)
#define DO_PRAGMA(x) PRAGMA_SUB(x)

#define INNER_LOOP_UNROLL_FACTOR 1
#define DESIRED_INNER_LOOP_II 16

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

// SEE << fp_mul_pow2 >> example project
typedef union {
    myType fp_num;
//    unsigned raw_bits;
    struct {
    	unsigned mant : 23;
    	unsigned bexp : 8;
    	unsigned sign : 1;
    };
} myType_union;

#else

typedef half myType;

#endif


#define CORE_BIAS 		(myType(0.0001))
#define MAX_DISTANCE 	(myType(1000000))

#endif

typedef unsigned pixelColorType;

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
};

/*
 * ENABLE/DISABLE
 */

#define SPHERE_OBJECT_ENABLE
#define PLANE_OBJECT_ENABLE
#define DISK_OBJECT_ENABLE
#define SQUARE_OBJECT_ENABLE
#define CYLINDER_OBJECT_ENABLE
#define CUBE_OBJECT_ENABLE
#define CONE_OBJECT_ENABLE

#define AMBIENT_COLOR_ENABLE
#define DIFFUSE_COLOR_ENABLE
#define SPECULAR_HIGHLIGHT_ENABLE
#define PRIMARY_COLOR_ENABLE
#define DIRECT_SHADOW_ENABLE
#define REFLECTION_SHADOW_ENABLE
#define REFLECTION_ENABLE
#define FRESNEL_REFLECTION_ENABLE
#define FAST_INV_SQRT_ENABLE
//#define FAST_DIVISION_ENABLE

/*
 * SCENE 'RANGE' DEFINITION
 */

#define WIDTH 						((unsigned short)(1920))
#define HEIGHT 						((unsigned short)(1080))
#define FRAME_BUFFER_SIZE (WIDTH)
#define LIGHTS_NUM 					((unsigned char)(2))
#define OBJ_NUM 					((unsigned char)(10))

#define FAST_INV_SQRT_ORDER 		((unsigned char)(2))
#define FAST_DIVISION_ORDER 		((unsigned char)(2))

#define PI 							(myType(3.141592))

#endif
