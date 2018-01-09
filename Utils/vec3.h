#ifndef VEC3__H_
#define VEC3__H_

#include "../Common/typedefs.h"

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
//#ifndef USE_FIXEDPOINT
	vec3(myType x, myType y, myType z)
	{
		data[0] = x;
		data[1] = y;
		data[2] = z;
	}
//#endif
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
		return hls::sqrt(myType((*this) * (*this)));
	}

	vec3 Normalize() const
	{
//#pragma HLS INLINE
		return (*this) / hls::sqrt(myType((*this) * (*this)));
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
	
};

#endif
