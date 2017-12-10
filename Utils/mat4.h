#ifndef MAT4__H_
#define MAT4__H_

#include "vec4.h"

struct mat4
{
	myType data[12]; // is 16 really needed? Nope...

	mat4()
	{
#pragma HLS ARRAY_PARTITION variable=data complete dim=1
	}

	mat4 operator=(const mat4& mat)
	{
		for (unsigned i = 0; i < 12; ++i) data[i] = mat.data[i];
		return *this;
	}

	mat4 operator*(const myType m) const
	{
		mat4 temp;
		for (unsigned i = 0; i < 12; ++i)
		{
			temp.data[i] = data[i] * m;
		}
		return temp;
	}

	mat4 operator*(const mat4& m) const
	{
		mat4 temp;
		myType s;// = (myType)0.0;

		for (unsigned i = 0; i < 3; ++i)
		{
			for (unsigned j = 0; j < 4; ++j)
			{
				s = 0.0;
				for (unsigned k = 0; k < 4; ++k)
				{
					if (k != 3)
					{
						s += data[i + k * 3] * data[k + j * 3];
					}
					else if (k == j)
					{
						s += data[i + k * 3];
					}
				}
				temp.data[i + j * 3] = s;
			}
		}

		return temp;
	}

	vec4 operator*(const vec4& v) const
	{
		vec4 temp;
		temp.data[0] = data[0] * v.data[0] + data[3] * v.data[1] + data[6] * v.data[2] + data[9];
		temp.data[1] = data[1] * v.data[0] + data[4] * v.data[1] + data[7] * v.data[2] + data[10];
		temp.data[2] = data[2] * v.data[0] + data[5] * v.data[1] + data[8] * v.data[2] + data[11];
		return temp;
	}

	vec4 Transform(const vec4& v) const
	{
		return *this * v;
	}

	vec4 TransformDir(const vec4& v) const
	{
		vec4 temp;
		temp.data[0] = data[0] * v.data[0] + data[3] * v.data[1] + data[6] * v.data[2];
		temp.data[1] = data[1] * v.data[0] + data[4] * v.data[1] + data[7] * v.data[2];
		temp.data[2] = data[2] * v.data[0] + data[5] * v.data[1] + data[8] * v.data[2];
		return temp;
	}

	vec4 TransposeTransformDir(const vec4& v) const
	{
		vec4 temp;
		temp.data[0] = data[0] * v.data[0] + data[1] * v.data[1] + data[2] * v.data[2];
		temp.data[1] = data[3] * v.data[0] + data[4] * v.data[1] + data[5] * v.data[2];
		temp.data[2] = data[6] * v.data[0] + data[7] * v.data[1] + data[8] * v.data[2];
		return temp;
	}

#define D3(n1, n2, n3) (data[n1]*data[n2]*data[n3])
#define D2(n1, n2) (data[n1]*data[n2])
	myType Det() const
	{
		return D3(0, 4, 8) + D3(3, 7, 2) + D3(6, 1, 5) - (D3(6, 4, 2) + D3(3, 1, 8) + D3(0, 7, 5));
	}

	mat4 Inverse() const
	{
		myType det = Det();
		if (det == (myType)0.0) return *this;

		mat4 temp;
		myType invDet = (myType)1.0/det;

		temp.data[0] = D2(4, 8) - D2(7, 5);
		temp.data[1] = D2(7, 2) - D2(1, 8);
		temp.data[2] = D2(1, 5) - D2(4, 2);

		temp.data[3] = D2(6, 5) - D2(3, 8);
		temp.data[4] = D2(0, 8) - D2(6, 2);
		temp.data[5] = D2(3, 2) - D2(0, 5);

		temp.data[6] = D2(3, 7) - D2(6, 4);
		temp.data[7] = D2(6, 1) - D2(0, 7);
		temp.data[8] = D2(0, 4) - D2(3, 1);

		temp.data[9] = D3(9, 7, 5) + D3(6, 4, 11) + D3(3, 10, 8) - (D3(3, 7, 11) + D3(6, 10, 5) + D3(9, 4, 8));
		temp.data[10] = D3(0, 7, 11) + D3(6, 10, 2) + D3(9, 1, 8) - (D3(9, 7, 2) + D3(6, 1, 11) + D3(0, 10, 8));
		temp.data[11] = D3(9, 4, 2) + D3(3, 1, 11) + D3(0, 10, 5) - (D3(0, 4, 11) + D3(3, 10, 2) + D3(9, 1, 5));

		return temp * invDet;
	}
#undef D3
#undef D2

	void IdentityMatrix()
	{
		for (unsigned i = 0; i < 12; ++i)
		{
			data[i] = (i % 4 == 0)?1.0:0.0;
		}
	}

	void TranslationMatrix(const vec4& v)
	{
		for (unsigned i = 0; i < 9; ++i)
		{
			data[i] = (i % 4 == 0)?1.0:0.0;
		}
		data[9] = v.data[0];	data[10] = v.data[1];	data[11] = v.data[2];
	}

	vec4 ExtractTranslationVactor()const
	{
		vec4 tmp;
		tmp.data[0] = data[9];	tmp.data[1] = data[10];	tmp.data[2] = data[11];
		return tmp;
	}

	void ScaleMatrix(const vec4& v)
	{
		for (unsigned i = 0; i < 4; ++i)
		{
			for (unsigned j = 0; j < 3; ++j)
			{
				if (i == j) data[j + i * 3] = v.data[j];
				else data[j + i * 3] = 0.0;
			}
		}
	}

	void RotationMatrix(myType rad, const vec4& v)
	{
		myType mag, s, c;
		myType xx, yy, zz, xy, yz, zx, xs, ys, zs, one_c;
		vec4 rotVec;

#ifdef USE_FIXEDPOINT
		s = myType(hls::sin((float)rad));
		c = myType(hls::cos((float)rad));
#else
		s = myType(std::sin(rad));
		c = myType(std::cos(rad));
#endif
		mag = v.Magnitude();

		// Identity matrix
		if (mag < (myType)0.001) {
			this->IdentityMatrix();
			return;
		}

		// Rotation matrix is normalized
		rotVec = v / mag;

		xx = rotVec.data[0] * rotVec.data[0];
		yy = rotVec.data[1] * rotVec.data[1];
		zz = rotVec.data[2] * rotVec.data[2];
		xy = rotVec.data[0] * rotVec.data[1];
		yz = rotVec.data[1] * rotVec.data[2];
		zx = rotVec.data[2] * rotVec.data[0];
		xs = rotVec.data[0] * s;
		ys = rotVec.data[1] * s;
		zs = rotVec.data[2] * s;
		one_c = (myType)1.0 - c;

#define M(row,col)  data[col*3+row]

	M(0,0) = (one_c * xx) + c;	M(0,1) = (one_c * xy) - zs; M(0,2) = (one_c * zx) + ys;	M(0,3) = 0.0;
	M(1,0) = (one_c * xy) + zs; M(1,1) = (one_c * yy) + c;	M(1,2) = (one_c * yz) - xs;	M(1,3) = 0.0;
	M(2,0) = (one_c * zx) - ys; M(2,1) = (one_c * yz) + xs; M(2,2) = (one_c * zz) + c;  M(2,3) = 0.0;
//	M(3,0) = 0.0;				M(3,1) = 0.0;				M(3,2) = 0.0;				M(3,3) = 1.0;

#undef M
	}
};

#endif