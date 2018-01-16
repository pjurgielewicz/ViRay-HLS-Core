#ifndef VEC3__H_
#define VEC3__H_

#include <iostream>
#include "../Common/typedefs.h"
#include "../viray.h"

namespace ViRay{
	namespace ViRayUtils{
		myType InvSqrt(myType val);
		myType Sqrt(myType val);
	};
};

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

	vec3(myType x, myType y, myType z)
	{
		data[0] = x;
		data[1] = y;
		data[2] = z;
	}

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

	vec3 operator+=(const vec3& v)
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

	vec3 operator-=(const vec3& v)
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

	vec3 operator*=(myType s)
	{
		data[0] *= s;
		data[1] *= s;
		data[2] *= s;
		return *this;
	}

	vec3 CompWiseMul(const vec3& v) const
	{
#pragma HLS INLINE
		vec3 temp;
		temp[0] = data[0] * v[0];
		temp[1] = data[1] * v[1];
		temp[2] = data[2] * v[2];

		return temp;
	}

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

	vec3 operator^(const vec3& v) const
	{
		vec3 temp;
		temp[0] = data[1] * v[2] - data[2] * v[1];
		temp[1] = data[2] * v[0] - data[0] * v[2];
		temp[2] = data[0] * v[1] - data[1] * v[0];
		return temp;
	}

	myType Magnitude() const
	{
		return ViRay::ViRayUtils::Sqrt((*this) * (*this));
	}

	vec3 Normalize() const
	{
//#pragma HLS INLINE

#if defined(FAST_INV_SQRT_ENABLE) && defined(USE_FLOAT)
		return (*this) * ViRay::ViRayUtils::InvSqrt(((*this) * (*this)));
#else
		return (*this) * ViRay::ViRayUtils::Sqrt(myType(1.0) / ((*this) * (*this)));
#endif
	}

	vec3 Reflect(const vec3& normal) const
	{
		return (normal * (normal * (*this)) * (myType)2.0) - (*this);
	}

	const myType& operator[](int i) const
	{
		return data[i];
	}

	myType& operator[](int i)
	{
		return data[i];
	}
	
	friend std::ostream& operator<<(std::ostream& cout, const vec3& v)
	{
		for (unsigned i = 0; i < 3; ++i)
		{
			cout << v[i] << " ";
		}
		return cout;
	}

};

#endif
