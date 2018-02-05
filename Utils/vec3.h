#ifndef VEC3__H_
#define VEC3__H_

#include "../Common/typedefs.h"

//#include "typedefs.h"

#ifndef UC_OPERATION
#include <iostream>
#include "../viray.h"
#endif

namespace ViRay{
	namespace ViRayUtils{
		myType InvSqrt(myType val);
		myType Sqrt(myType val);
	};
};

/*
 * Simple 3D vector implementation
 */

struct vec3
{
//	vec3()
//	{
//		data[0] = data[1] = data[2] = 0.0;
//	}

	myType data[3];
	vec3()
	{
#pragma HLS ARRAY_PARTITION variable=data complete dim=1
	}

	/*
	 * Scalar assignment constructor
	 */
	vec3(myType x, myType y, myType z)
	{
		data[0] = x;
		data[1] = y;
		data[2] = z;
	}

	/*
	 * Scalar assignment constructor - every coordinate will have value of s
	 */
	vec3(myType s)
	{
		data[0] = s;
		data[1] = s;
		data[2] = s;
	}

	vec3 operator=(const vec3& v)
	{
		data[0] = v[0];
		data[1] = v[1];
		data[2] = v[2];
		return *this;
	}

	vec3 operator+(const vec3& v) const
	{
		vec3 temp;
		temp[0] = data[0] + v[0];
		temp[1] = data[1] + v[1];
		temp[2] = data[2] + v[2];
		return temp;
	}

	vec3& operator+=(const vec3& v)
	{
		data[0] += v[0];
		data[1] += v[1];
		data[2] += v[2];
		return *this;
	}

	vec3 operator-(const vec3& v) const
	{
#pragma HLS INLINE
		vec3 temp;
		temp[0] = data[0] - v[0];
		temp[1] = data[1] - v[1];
		temp[2] = data[2] - v[2];
		return temp;
	}

	vec3& operator-=(const vec3& v)
	{
		data[0] -= v[0];
		data[1] -= v[1];
		data[2] -= v[2];
		return *this;
	}

	vec3 operator-() const
	{
		vec3 temp;
		temp[0] = -data[0];
		temp[1] = -data[1];
		temp[2] = -data[2];
		return temp;
	}

	vec3 operator*(myType s) const
	{
		vec3 temp;
		temp[0] = data[0] * s;
		temp[1] = data[1] * s;
		temp[2] = data[2] * s;
		return temp;
	}

	vec3& operator*=(myType s)
	{
		data[0] *= s;
		data[1] *= s;
		data[2] *= s;
		return *this;
	}

	/*
	 * Component-by-component vector multiplication
	 * Useful for color determination
	 */
	vec3 CompWiseMul(const vec3& v) const
	{
#pragma HLS INLINE
		vec3 temp;
		temp[0] = data[0] * v[0];
		temp[1] = data[1] * v[1];
		temp[2] = data[2] * v[2];

		return temp;
	}

	/*
	 * 3D vector dot product
	 */
	myType operator*(const vec3& v) const
	{
		return 	data[0] * v[0] + 
				data[1] * v[1] + 
				data[2] * v[2];
	}

	vec3 operator/(myType s) const
	{
		return (*this) * ((myType)1.0/s);
	}

	/*
	 * 3D vector cross product
	 */
	vec3 operator^(const vec3& v) const
	{
		vec3 temp;
		temp[0] = data[1] * v[2] - data[2] * v[1];
		temp[1] = data[2] * v[0] - data[0] * v[2];
		temp[2] = data[0] * v[1] - data[1] * v[0];
		return temp;
	}

	/*
	 * Get the magnitude of the vector
	 */
	myType Magnitude() const
	{
#ifndef UC_OPERATION
		return ViRay::ViRayUtils::Sqrt((*this) * (*this));
#else
		return std::sqrt((*this) * (*this));
#endif
	}

	/*
	 * Normalize vector. It uses appropriate implementation based on user FAST_INV_SQRT_ENABLE choice
	 */
	vec3 Normalize() const
	{
//#pragma HLS INLINE

#ifndef UC_OPERATION

#if defined(FAST_INV_SQRT_ENABLE) && defined(USE_FLOAT)
		return (*this) * ViRay::ViRayUtils::InvSqrt(((*this) * (*this)));
#else
		return (*this) * ViRay::ViRayUtils::Sqrt(myType(1.0) / ((*this) * (*this)));
#endif

#else
		return (*this) * std::sqrt(myType(1.0) / ((*this) * (*this)));
#endif
	}

	/*
	 * Get a new vector which is the reflection of current vector from the surface described by the normal vector
	 */
	vec3 Reflect(const vec3& normal) const
	{
		return (normal * (normal * (*this)) * (myType)2.0) - (*this);
	}

	/*
	 * Get vector i-th component
	 */
	const myType& operator[](int i) const
	{
		return data[i];
	}

	/*
	 * Get vector i-th component
	 */
	myType& operator[](int i)
	{
		return data[i];
	}
#ifndef UC_OPERATION
	/*
	 * Flush vector content to the std::cout stream
	 */
	friend std::ostream& operator<<(std::ostream& cout, const vec3& v)
	{
		for (unsigned i = 0; i < 3; ++i)
		{
			cout << v[i] << " ";
		}
		return cout;
	}
#endif
};

#endif
