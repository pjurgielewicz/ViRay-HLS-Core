#ifndef TYPEDEFS__H_
#define TYPEDEFS__H_

#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_math.h"

#define PRAGMA_SUB(x) _Pragma (#x)
#define DO_PRAGMA(x) PRAGMA_SUB(x)

#define OUTER_LOOP_UNROLL_FACTOR 1
#define DESIRED_INNER_LOOP_II 6

//#define USE_FIXEDPOINT

#ifdef USE_FIXEDPOINT

#define FIXEDPOINT_WIDTH 32
#define FIXEDPOINT_INTEGER_BITS 22
typedef ap_fixed<FIXEDPOINT_WIDTH, FIXEDPOINT_INTEGER_BITS, AP_RND> myType;

#define CORE_BIAS (myType(0.01))
#define MAX_DISTANCE 10000

#else
//typedef half myType;
typedef float myType;

#define CORE_BIAS (myType(0.0001))
#define MAX_DISTANCE myType(1000000)

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
};

#define USE_SPHERE_OBJECT
#define USE_PLANE_OBJECT
#define USE_DISK_OBJECT
#define USE_SQUARE_OBJECT

/*
 * ENABLE/DISABLE
 */

#define PRIMARY_COLOR_ENABLE
#define SHADOW_ENABLE
#define REFLECTION_ENABLE

/*
 * SCENE 'RANGE' DEFINITION
 */
#define OBJ_NUM 10
#define WIDTH 1920
#define HEIGHT 1080
#define FRAME_BUFFER_SIZE (WIDTH)
#define LIGHTS_NUM 2
#define SAMPLES_PER_PIXEL 1
#define SAMPLING_FACTOR (myType(1.0) / myType(SAMPLES_PER_PIXEL))

#endif
