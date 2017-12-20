#ifndef TYPEDEFS__H_
#define TYPEDEFS__H_

#define USE_FIXEDPOINT

#ifdef USE_FIXEDPOINT

#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_math.h"

#define FIXEDPOINT_WIDTH 32
#define FIXEDPOINT_INTEGER_BITS 22

#define PRAGMA_SUB(x) _Pragma (#x)
#define DO_PRAGMA(x) PRAGMA_SUB(x)

#define OUTER_LOOP_UNROLL_FACTOR 1

//typedef ap_fixed<FIXEDPOINT_WIDTH, FIXEDPOINT_INTEGER_BITS, AP_RND> myType;
//typedef half myType;
typedef float myType;

#define CORE_BIAS (myType(0.0001))
#define MAX_DISTANCE 1000000

//typedef int pixelColorType;
typedef unsigned pixelColorType;

#else

#include <cmath>
typedef float myType;

typedef short int pixelColorType;

#endif

enum ObjectType{
	INVALID = -1,
	SPHERE = 1,
	PLANE,
};

/*
 * SCENE DEFINITION
 */
#define OBJ_NUM 3
#define WIDTH 1920
#define HEIGHT 1080
#define FRAME_BUFFER_SIZE (WIDTH)
#define LIGHTS_NUM 2
#define SAMPLES_PER_PIXEL 1
#define SAMPLING_FACTOR (myType(1.0) / myType(SAMPLES_PER_PIXEL))

#endif
