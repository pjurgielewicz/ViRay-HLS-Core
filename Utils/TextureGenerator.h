#ifndef TEXTUREGENERATOR__H_
#define TEXTUREGENERATOR__H_

#include "../Common/typedefs.h"
#include "../Utils/material.h"

/*
 * Custom random numbers generator
 */
namespace Random{
	/*
	 * Check for sources in the code there:
	 * https://bitbucket.org/rtMasters/raytracer/src/a1193bdd879904324fe85c2998bc721c75d88581/src/Common/Random.cpp?at=master&fileviewer=file-view-default
	 */

	static unsigned mwc1616_x = 1;
	static unsigned mwc1616_y = 2;
	static unsigned maxRand   = 4294967295;				// ???

	static void SetSeed(unsigned seed)
	{
		mwc1616_x = seed | 1;   /* Can't seed with 0 */
		mwc1616_y = seed | 2;
	}

	static unsigned Rand()
	{
		/*
		 * http://www.helsbreth.org/random/rng_mwc1616.html
		 */
		mwc1616_x = 18000 * (mwc1616_x & 0xffff) + (mwc1616_x >> 16);
		mwc1616_y = 30903 * (mwc1616_y & 0xffff) + (mwc1616_y >> 16);

		return (mwc1616_x << 16) + (mwc1616_y & 0xffff);
	}

	static int RandInt(int lo, int hi) // [lo, ..., hi)
	{
		return ( Rand() % (hi - lo) + lo );
	}

	static myType RandReal(myType lo, myType hi)
	{
		return (((double) ( Rand() % maxRand ) / (double) (maxRand)) * (hi - lo) + lo );
	}
};



class CTextureGenerator{

private:
	/*
	 * 2D noise class
	 * Based on tutorial: http://lodev.org/cgtutor/randomnoise.html
	 */

	class C2DNoiseGenerator{
	public:
		C2DNoiseGenerator(float_union* outputBitmap, unsigned short width, unsigned short height, unsigned char octaves = 1, unsigned seed = 1784301) :
			outputBitmap(outputBitmap), width(width), height(height)
		{
#ifndef UC_OPERATION
			noise = new float_union[width * height];
#else
			noise = (float_union*)UC_TEMP_NOISE_TEXTURE_POOL;
#endif
			this->octaves = (octaves >= 1) ? octaves : 1;

			GenerateNoise();

			for (unsigned short x = 0; x < width; ++x)
			{
				for (unsigned short y = 0; y < height; ++y)
				{
					outputBitmap[x * height + y].fp_num = Turbulence(x, y);
				}
			}
		}
		~C2DNoiseGenerator()
		{
#ifndef UC_OPERATION
			if (noise)
			{
				delete[] noise;
			}
#endif
		}
	private:
		void GenerateNoise()
		{
			for (unsigned short x = 0; x < width; ++x)
			{
				for (unsigned short y = 0; y < height; ++y)
				{
					noise[x * height + y].fp_num = Random::RandReal(0.0, 1.0);
				}
			}
		}

		float SmoothNoise(float x, float y)
		{
			float fx = x - (unsigned short)x;
			float fy = y - (unsigned short)y;

			int x1 = ((unsigned short)x + width) % width;
			int y1 = ((unsigned short)y + height) % height;

			int x2 = ((unsigned short)x + width - 1) % width;
			int y2 = ((unsigned short)y + height - 1) % height;

			float res = fx * fy * noise[x1 * height + y1].fp_num;
			res += ((float)1.0 - fx) * fy * noise[x2 * height + y1].fp_num;
			res += fx * ((float)1.0 - fy) * noise[x1 * height + y2].fp_num;
			res += ((float)1.0 - fx) * ((float)1.0 - fy) * noise[x2 * height + y2].fp_num;

			return res;
		}

		float Turbulence(float x, float y)
		{
			unsigned short ampl = 1;
			float invAmpl = 1.0;
			float norm = 0.0;

			float res = 0.0;

			for (unsigned char i = 0; i < octaves; ++i)
			{
				res += ampl * SmoothNoise(x * invAmpl, y * invAmpl);
				norm += invAmpl;

				ampl <<= 1;
				invAmpl = (float)1.0/ampl;
			}
			return res/(norm * (ampl >> 1));
		}

		unsigned short width;
		unsigned short height;
		unsigned char octaves;
		float_union* outputBitmap;
		float_union* noise;
	};

public:
	enum TextureGeneratorType{
		RGB_DOTS,
		CHECKERBOARD,
		XOR,
		WOOD,
		MARBLE,
	};

public:
	
	CTextureGenerator(	unsigned short textureWidth, unsigned short textureHeight, TextureGeneratorType type,
						myType param1 = 1.0, myType param2 = 1.0, unsigned seed = 1784301, unsigned char octaves = 1, myType turbPower = 1.0) :
		width(textureWidth), height(textureHeight), type(type),
		seed(seed), octaves(octaves), turbPower(turbPower), param1(param1), param2(param2)
	{
		Random::SetSeed(seed);

#ifndef UC_OPERATION
		bitmap = new float_union[width * height];
#else
		bitmap = (float_union*)UC_TEMP_TEXTURE_POOL;
#endif

		GenerateTexture();
	}
	~CTextureGenerator()
	{
#ifndef UC_OPERATION
		if (bitmap)
		{
			delete[] bitmap;
		}
#endif
	}
	
	unsigned CopyBitmapInto(float_union* copyBuffer) const
	{
		memcpy(copyBuffer, bitmap, sizeof(float_union) * width * height);
		return width * height;
	}

	ViRay::CMaterial::TextureType GetTextureType() const { return textureContentType; }

	unsigned short GetTextureWidth()  const { return width; }
	unsigned short GetTextureHeight() const { return height; }

private:

	void GenerateTexture()
	{
		switch(type)
		{
		case RGB_DOTS:
			GenerateRGBDots();
			textureContentType = ViRay::CMaterial::PIXEL_MAP;
			break;
		case CHECKERBOARD:
			GenerateCheckerboard();
			textureContentType = ViRay::CMaterial::BITMAP_MASK;
			break;
		case XOR:
			GenerateXOR();
			textureContentType = ViRay::CMaterial::BITMAP_MASK;
			break;
		case WOOD:
			GenerateWood();
			textureContentType = ViRay::CMaterial::BITMAP_MASK;
			break;
		case MARBLE:
			GenerateMarble();
			textureContentType = ViRay::CMaterial::BITMAP_MASK;
			break;
		}
	}

	void GenerateRGBDots()
	{
		unsigned char shift = 0;
		for (unsigned short w = 0; w < width; ++w)
		{
			for (unsigned short h = 0; h < height; ++h)
			{
				bitmap[w * height + h].raw_bits = (230 << (shift * 8));
				shift = (shift == 2) ? 0 : shift + 1;
			}
		}
	}
	void GenerateCheckerboard()
	{
		for (unsigned short w = 0; w < width; ++w)
		{
			unsigned short xc = w >> ((unsigned)param1);
			for (unsigned short h = 0; h < height; ++h)
			{
				unsigned short yc = h >> ((unsigned)param2);

				unsigned short s = xc + yc;

				bitmap[w * height + h].fp_num = (s & 0x1) ? myType(1.0) : myType(0.0);
			}
		}
	}

	void GenerateXOR()
	{
		/*
		 * Based on:
		 * http://lodev.org/cgtutor/xortexture.html
		 */
		for (unsigned short w = 0; w < width; ++w)
		{
			for (unsigned short h = 0; h < height; ++h)
			{
				unsigned x = w ^ h;
//				unsigned val = 0;
//				for (unsigned char i = 0; i < 3; ++i)
//				{
//					val += (x << (i * 8));
//				}
//				bitmap[w * height + h].raw_bits = val;
				bitmap[w * height + h].fp_num = x / 255.0;
			}
		}
	}

	void GenerateWood()
	{
		/*
		 * param1 : xyPeriod
		 */

		C2DNoiseGenerator noiseTexture(bitmap, width, height, octaves, seed);

		float xVal, yVal, xyDist;
		for (unsigned short w = 0; w < width; ++w)
		{
			for (unsigned short h = 0; h < height; ++h)
			{
				xVal = (float)( w - width  * 0.5 ) / (float)width;
				yVal = (float)( h - height * 0.5 ) / (float)height;

				xyDist = std::sqrt(xVal * xVal + yVal * yVal) + turbPower * bitmap[w * height + h].fp_num;
				bitmap[w * height + h].fp_num = std::fabs(std::sin(xyDist * param1 * TWOPI));
			}
		}
	}

	void GenerateMarble()
	{
		/*
		 * param1 : xPeriod
		 * param2 : yPeriod
		 */

		float xyRes;

		C2DNoiseGenerator noiseTexture(bitmap, width, height, octaves, seed);

		for (unsigned short w = 0; w < width; ++w)
		{
			for (unsigned short h = 0; h < height; ++h)
			{
				xyRes = w * param1 / width + h * param2 / height + turbPower * bitmap[w * height + h].fp_num;
				bitmap[w * height + h].fp_num = std::fabs(std::sin(xyRes * PI));
			}
		}
	}
private:
	/*
	 *
	 */
	unsigned short width;
	unsigned short height;
	TextureGeneratorType type;
	unsigned seed;
	unsigned char octaves;
	float turbPower;
	float param1, param2;

	float_union* bitmap;

	ViRay::CMaterial::TextureType textureContentType;
};

/*
 * Simplify texture copy to the texture page and its mapping onto an object
 */
class CTextureHelper{
public:
	CTextureHelper(float_union* textureData, unsigned* textureDescriptionData, unsigned* textureBaseAddress) :
		baseAddr(0), textureData(textureData), textureDescriptionData(textureDescriptionData), textureBaseAddress(textureBaseAddress)
	{

	}

	/*
	 * Given CTextureGenerator instance save its pixels into the texture page and return its address.
	 * Optionally bind this texture to the object
	 */
	unsigned SaveTexture(const CTextureGenerator& texture, unsigned char textureMapping, unsigned& descriptionCode, bool bindImmediately = false, unsigned objIdx = 0)
	{
		unsigned addressToReturn = baseAddr;

		baseAddr += texture.CopyBitmapInto(textureData + baseAddr);

		descriptionCode = CTextureHelper::GetTextureDescriptionCode(texture.GetTextureType(), textureMapping, texture.GetTextureWidth(), texture.GetTextureHeight());

		if (bindImmediately)
		{
			BindTextureToObject(objIdx, addressToReturn, descriptionCode);
		}

		return addressToReturn;
	}

	/*
	 * Knowing texture address inside the texture page and mapping options enclosed in textureDescriptionCode
	 * bind this texture to the object with objIdx
	 */
	void BindTextureToObject(unsigned objIdx, unsigned textureAddr, unsigned textureDescriptionCode)
	{
		textureBaseAddress[objIdx] 		= textureAddr;
		textureDescriptionData[objIdx]  = textureDescriptionCode;
	}
	static unsigned GetTextureDescriptionCode(	unsigned char textureType = ViRay::CMaterial::CONSTANT,
											unsigned char textureMapping = ViRay::CMaterial::PLANAR,
											unsigned short textureWidth = 128,
											unsigned short textureHeight = 128)
	{
		// 6 MSBs are unused
		return 	((textureType & 0x7) << 23) +
				((textureMapping & 0x7) << 20) +
				((textureWidth & 0x3FF) << 10) +
				((textureHeight) & 0x3FF);
	}

private:


	unsigned* textureBaseAddress;
	unsigned* textureDescriptionData;
	unsigned baseAddr;
	float_union* textureData;
};

#endif
