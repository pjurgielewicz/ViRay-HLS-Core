#ifndef TYPEDEFS__H_
#define TYPEDEFS__H_

#define USE_FIXEDPOINT
//#define USE_SMALLER_TYPES

#ifdef USE_FIXEDPOINT

#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_math.h"

#define FIXEDPOINT_WIDTH 32
#define FIXEDPOINT_INTEGER_BITS 22
//#define FIXEDPOINT_INTEGER_BITS_HALF 11

#define PRAGMA_SUB(x) _Pragma (#x)
#define DO_PRAGMA(x) PRAGMA_SUB(x)

#define OUTER_LOOP_UNROLL_FACTOR 5

typedef ap_fixed<FIXEDPOINT_WIDTH, FIXEDPOINT_INTEGER_BITS, AP_RND> myType;
//typedef double myType;
typedef ap_fixed<25, 13, AP_RND>  myTypeReduced;

typedef ap_fixed<16, 8, AP_RND> pixelColorType;

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

#endif
