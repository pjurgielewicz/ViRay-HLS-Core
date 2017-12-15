#ifndef VEC4__H_
#define VEC4__H_

#include "../Common/typedefs.h"

struct vec4
{
//	vec4()
//	{
//		data[0] = data[1] = data[2] = 0.0;
//	}

	myType data[4];
	vec4()
	{
#pragma HLS ARRAY_PARTITION variable=data complete dim=1
//#pragma HLS ARRAY_PARTITION variable=data cyclic factor=2 dim=1

	}
//#ifndef USE_FIXEDPOINT
	vec4(myType x, myType y, myType z, myType w = 0.0)
	{
		data[0] = x;
		data[1] = y;
		data[2] = z;
		data[3] = w;
	}
//#endif
	vec4 operator=(const vec4& v)
	{
		this->data[0] = v.data[0];
		this->data[1] = v.data[1];
		this->data[2] = v.data[2];
		this->data[3] = v.data[3];
		return *this;
	}

	vec4 operator+(const vec4& v) const
	{
		vec4 temp;
		temp.data[0] = this->data[0] + v.data[0];
		temp.data[1] = this->data[1] + v.data[1];
		temp.data[2] = this->data[2] + v.data[2];
		temp.data[3] = this->data[3] + v.data[3];
		return temp;
	}

	vec4 operator+=(const vec4& v)
	{
		this->data[0] += v.data[0];
		this->data[1] += v.data[1];
		this->data[2] += v.data[2];
		this->data[3] += v.data[3];
		return *this;
	}

	vec4 operator-(const vec4& v) const
	{
#pragma HLS INLINE
		vec4 temp;
		temp.data[0] = this->data[0] - v.data[0];
		temp.data[1] = this->data[1] - v.data[1];
		temp.data[2] = this->data[2] - v.data[2];
		temp.data[3] = this->data[3] - v.data[3];
		return temp;
	}

	vec4 operator-=(const vec4& v)
	{
		this->data[0] -= v.data[0];
		this->data[1] -= v.data[1];
		this->data[2] -= v.data[2];
		this->data[3] -= v.data[3];
		return *this;
	}

	vec4 operator-() const
	{
//#pragma HLS INLINE
		vec4 temp;
		temp.data[0] = -this->data[0];
		temp.data[1] = -this->data[1];
		temp.data[2] = -this->data[2];
		temp.data[3] = -this->data[3];
		return temp;
	}

	vec4 operator*(myType s) const
	{
		vec4 temp;
		temp.data[0] = this->data[0] * s;
		temp.data[1] = this->data[1] * s;
		temp.data[2] = this->data[2] * s;
		temp.data[3] = this->data[3] * s;
		return temp;
	}

	vec4 CompWiseMul(const vec4& v) const
	{
#pragma HLS INLINE
		vec4 temp;
		temp.data[0] = this->data[0] * v.data[0];
		temp.data[1] = this->data[1] * v.data[1];
		temp.data[2] = this->data[2] * v.data[2];
		temp.data[3] = this->data[3] * v.data[3];

		return temp;
	}

	myType Dot3(const vec4& v) const
	{
//#pragma HLS INLINE
		return this->data[0] * v.data[0] + this->data[1] * v.data[1] + this->data[2] * v.data[2];
	}

//	myType operator*(const vec4& v) const
//	{
//		return this->data[0] * v.data[0] + this->data[1] * v.data[1] + this->data[2] * v.data[2] + this->data[3] * v.data[3];
//	}

	vec4 operator/(myType s) const
	{
		return (*this) * ((myType)1.0/s);
	}

	vec4 operator^(const vec4& v) const
	{
		vec4 temp;
		temp.data[0] = this->data[1] * v.data[2] - this->data[2] * v.data[1];
		temp.data[1] = this->data[2] * v.data[0] - this->data[0] * v.data[2];
		temp.data[2] = this->data[0] * v.data[1] - this->data[1] * v.data[0];
		return temp;
	}

	myType Magnitude() const
	{
#ifdef USE_FIXEDPOINT
		return hls::sqrt((*this).Dot3(*this));
#else
		return std::sqrt((*this).Dot3(*this));
#endif
	}

	vec4 Normalize() const
	{
#ifdef USE_FIXEDPOINT
		return (*this) / hls::sqrt((*this).Dot3(*this));

//		return (*this) * (myType(1.0) / hls::sqrt((*this).Dot3(*this)));
#else
		return (*this) / std::sqrt((*this).Dot3(*this));
#endif
	}

//	void DecodeFromStream(const myType* stream)
//	{
//		for (unsigned i = 0; i < 4; ++i)
//			data[i] = *(stream + i);
//	}

	vec4 Reflect(const vec4& normal) const
	{
		return (normal * (normal.Dot3(*this)) * (myType)2.0) - (*this);
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
