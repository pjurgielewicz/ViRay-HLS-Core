#ifndef INSTRUCTION__H_
#define INSTRUCTION__H_

#include "../Common/typedefs.h"

enum InstructionType{
	NOP,
	LC, // LOAD CONSTANT as ap_fixed<17,11>
	LV, LVI, // LOAD VECTOR FROM THE MEMORY
	MOV,
	ADD,
	SHR,
	MUL, DOT, ADOT, MADD,

	PRE_S,
	PRE_D,

	JMP, JMP_IF, JMP_IFN
};

// Local register names
enum LocalVars {
//-- VECTORS
	V4_0 = 0,

	SPECIAL = 9,
	NORMAL = 10, LOCAL_HIT = 11,
	RAY_ORIGIN=12, RAY_ORIGIN_TRANSFORMED,
	RAY_DIRECTION, RAY_DIRECTION_TRANSFORMED
};

// Represents a single instruction
struct Instruction {
	ap_uint<128> codedData;
};

//////////////////////

struct InstructionExtracted
{
	ap_uint<4> it;					// instruction type 								-> 4 bits
	ap_uint<5> wr;					// write register									-> 5 bits
	ap_uint<2> idx0, idx1;			// input1, input2 idxs (x, y, z, w)					-> 2 x 2 bits

	ap_int<8> r0;					// input0											-> 8 bits (1 (sign) + 7)
	ap_int<8> r1;					// input1											-> 8 bits (1 (sign) + 7)

	ap_fixed<19, 13, AP_RND> val;	//													-> 19 bits

	InstructionExtracted operator=(const Instruction& inst)
	{
#pragma HLS INLINE
#pragma HLS PIPELINE
		it = (inst.codedData >> 28);
		wr = (inst.codedData >> 23) & 0x1F; // idxWr does not matter anymore
		idx0 = (inst.codedData >> 21) & 0x3;
		idx1 = (inst.codedData >> 19) & 0x3;

		r0 = (inst.codedData & 0xFF00) >> 8;
		r1 = inst.codedData & 0xFF;

		val.setBits(inst.codedData & 0x7FFFF);

		return *this;
	}

	InstructionExtracted operator=(const unsigned& instCode)
	{
//#pragma HLS FUNCTION_INSTANTIATE variable=instCode
#pragma HLS INLINE off
//#pragma HLS PIPELINE
#pragma HLS LATENCY min=2 max=2

		it = (instCode >> 28);
		wr = (instCode >> 23) & 0x1F; // idxWr does not matter anymore
		idx0 = (instCode >> 21) & 0x3;
		idx1 = (instCode >> 19) & 0x3;

		r0 = (instCode & 0xFF00) >> 8;
		r1 = instCode & 0xFF;

		val.setBits(instCode & 0x7FFFF);

		return *this;
	}
};

#endif