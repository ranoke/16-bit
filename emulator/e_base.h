#pragma once

#pragma pack(push)

/*
 * ARCH:
 *   - bus 24
 *   - RAM 4096
 *   - registers 8
 * ARITHMETIC:
 *   - INC regA ; increment by one
 *   - IDIV regA regB destReg ; sign division destReg=regA/regB
 *   - IMUL regA regB destReg ; sign multiplication destReg=regA*regB
 * LOGICAL:
 *   - AND regA regB destReg ; destReg=regA & regB
 *   - XOR regA regB destReg ; addition by module 2: destReg=regA # regB
 *   - SHR regA regB destReg ; logic shift right destReg=regA >> regB
 * FLOW CONTROL:
 *   - JMA regA regB offSet ; no sign if (regA> regB) PC=PC+1+offSet
 *   - JMBE regA regB offSet ; no sign if (regA<= regB) PC=PC+1+offSet
 * FLAGS:
 *   - CF
 *   - ADC regA regB destReg ; addition with CF: destReg=regA+regB+CF
 *   - SBB regA regB destReg ; subtraction with CF: destReg=regA-regB-СF
 *   - CMP regA regB ; cmp regA regB and set flags
 *																           СF SF ZF
 *																regA < regB 1  1  0
 *																regA = regB 0  0  1
 *																regA > regB 0  0  0
 * ADDRESATION:
 *   - Direct (using operands)
 */

#include <map>
#include <regex>
#include <bitset>
#include <sstream>
#include <algorithm>
#include <unordered_map>

#include <cstdio>
#include <cstdint>
#include <cassert>

#define RAM_SIZE 4096
#define REGISTERS_COUNT 9

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define CONCAT(x, y) x ## y
#define LOG(fmt, ...) printf(__FILE__ ": " fmt "\n", __VA_ARGS__)
#define ASSERT assert
#define IN_RANGE(val, min, max) (min < val && val < max)
#define IN_RANGE_E(val, min, max) (min <= val && val <= max)

#define BITS_12_MASK (0b1111'1111'1111)
#define BITS_16_MASK (0b1111'1111'1111'1111)
#define BITS_24_MASK (0b1111'1111'1111'1111'1111'1111)
#define BITS_27_MASK (0b0111'1111'1111'1111'1111'1111'1111)
#define BITS_MASKED_COPY(B, M) ((B) & (M))
#define BITS_MASKED_COPY_DEST_UNCHENGED(OUT, B, M) (((B) & (M)) | ((OUT) & ~(M)))

using u16 = uint16_t;
using i16 = int16_t;
using u32 = uint32_t;
using i32 = int32_t;
using i8 = int8_t;
using u8 = uint8_t;

enum Status
{
	SUCCESS,
	FAILURE,
	FILE_NOT_FOUND,
	INVALID_FILE
};

enum ECommand
{
	E_ADD,    // destReg = regA + regB
	E_NAND,   // destReg = !(regA && regB)
	E_LW,     // destReg = load from memory by OPERAND/LABEL
	E_SW,     // save to memory from regR by OPERAND/LABEL
	E_BEQ,    // if regR == regA goto ProgramCounter + 1 + shiftamount, in PC is saved addr of current instruction
	E_JALR,   // saves PC+1 into regR. in PC is saved addr of current instruction. Goto regA addr. if regA and regB is same register then first write PC + 1 and then goto PC + 1.
	E_HALT,   // PC + 1 and then STOP. Show message about emu stop
	E_NOOP,   // no operation
	E_INC,
	E_IDIV,
	E_IMUL,
	E_AND,
	E_XOR,
	E_SHR,
	E_JMA,
	E_JMBE,
	E_ADC,
	E_SBB,
	E_CMP,
	__ECOMMAND_MAX,
	__ECOMMAND_LAST = __ECOMMAND_MAX - 1
};

enum EArgsType
{
	EARGS_INVAL,
	EARGS_NONE,
	EARGS_A,
	EARGS_A_B,
	EARGS_A_B_R,
	EARGS_A_B_OFFSET
};

struct EOpcodeDesc
{
	ECommand opcode_;
	EArgsType args_type_;
	std::string_view asm_name_;
};

static constexpr EOpcodeDesc opcode_descriptions[] = {
	{ E_ADD,  EARGS_A_B_R, "add"  },
	{ E_NAND, EARGS_A_B_R, "nand" },
	{ E_LW,   EARGS_A_B_OFFSET, "lw" },
	{ E_SW,   EARGS_A_B_OFFSET, "sw" },
	{ E_BEQ,  EARGS_A_B_OFFSET, "beq"  },
	{ E_JALR, EARGS_A_B_R, "jalr" },
	{ E_HALT, EARGS_NONE,  "halt" },
	{ E_NOOP, EARGS_NONE,  "noop" },
	{ E_INC,  EARGS_A, "inc"  },
	{ E_IDIV, EARGS_A_B_R, "idiv" },
	{ E_IMUL, EARGS_A_B_R, "imul" },
	{ E_AND,  EARGS_A_B_R, "and"  },
	{ E_XOR,  EARGS_A_B_R, "xor"  },
	{ E_SHR,  EARGS_A_B_R, "shr"  },
	{ E_JMA,  EARGS_A_B_OFFSET, "jma"  },
	{ E_JMBE, EARGS_A_B_OFFSET, "jmbe" },
	{ E_ADC,  EARGS_A_B_R, "adc"  },
	{ E_SBB,  EARGS_A_B_R, "sbb"  },
	{ E_CMP,  EARGS_A_B, "cmp"  }
};

using ERegister = u16;
struct EBusBase
{
	[[nodiscard]] u32
		get_value()
	{
		return BITS_MASKED_COPY(data, BITS_27_MASK);
	}

	void
		set_value(u32 v)
	{
		data = BITS_MASKED_COPY(v, BITS_27_MASK);
		u32 overflow = BITS_MASKED_COPY(v, 0b1111'1100'0000'0000'0000'0000'0000'0000);
	}

	u32 operator++(int)
	{
		u32 inc_val = get_value() + 1;
		set_value(inc_val);
		return inc_val;
	}

	u32 data = 0;
};


struct EInstruction : EBusBase
{
	[[ no_discard ]] u32
		get_opcode()
	{
		return BITS_MASKED_COPY(get_value(), 0b11111 << 22) >> 22;
	}

	[[ no_discard ]] std::pair<u32, bool>
		get_reg_a()
	{
		return { BITS_MASKED_COPY(get_value(), 0b1111 << 17) >> 17, BITS_MASKED_COPY(get_value(), 0b1 << 21) >> 21 };
	}

	[[ no_discard ]] std::pair<u32, bool>
		get_reg_b()
	{
		return { BITS_MASKED_COPY(get_value(), 0b1111 << 12) >> 12, BITS_MASKED_COPY(get_value(), 0b1 << 16) >> 16 };
	}

	[[ no_discard ]] u32
		get_reg_r()
	{
		return BITS_MASKED_COPY(get_value(), 0b1111);
	}

	[[ no_discard ]] u32
		get_operand()
	{
		return BITS_MASKED_COPY(get_value(), BITS_12_MASK);
	}

	static EInstruction create_ra_rb_rr(u32 opcode, u32 ra, u32 rb, u32 rr, u32 ra_direct = 0, u32 rb_direct = 0)
	{
		ASSERT(IN_RANGE_E(opcode, 0, 0b11111));
		ASSERT(IN_RANGE_E(ra, 0, 0b1111));
		ASSERT(IN_RANGE_E(rb, 0, 0b1111));
		ASSERT(IN_RANGE_E(rr, 0, 0b1111));
		ASSERT(IN_RANGE_E(ra_direct, 0, 0b1));
		ASSERT(IN_RANGE_E(rb_direct, 0, 0b1));

		u32 to_set = 0;

		ra_direct = ra_direct << 21;
		to_set ^= ra_direct;

		rb_direct = rb_direct << 16;
		to_set ^= rb_direct;

		//LOG("Exec: %s", std::bitset<32>(to_set).to_string().c_str());

		//set_opcode
		opcode = opcode << 22;
		to_set ^= opcode;

		//LOG("Exec: %s", std::bitset<32>(to_set).to_string().c_str());

		//set_reg_a
		ra = ra << 17;
		to_set ^= ra;

		//set_reg_r
		rb = rb << 12;
		to_set ^= rb;

		//set_reg_r
		to_set ^= rr;

		EInstruction ret = {};
		ret.set_value(to_set);
		return ret;
	}

	static EInstruction create_ra_rb_offset(u32 opcode, u32 ra, u32 rb, u32 offset, u32 ra_direct = 0, u32 rb_direct = 0)
	{
		ASSERT(IN_RANGE_E(offset, 0, BITS_12_MASK));
		ASSERT(IN_RANGE_E(opcode, 0, 0b11111));
		ASSERT(IN_RANGE_E(ra, 0, 0b1111));
		ASSERT(IN_RANGE_E(rb, 0, 0b1111));
		ASSERT(IN_RANGE_E(ra_direct, 0, 0b1));
		ASSERT(IN_RANGE_E(rb_direct, 0, 0b1));

		u32 to_set = 0;

		ra_direct = ra_direct << 21;
		to_set ^= ra_direct;

		rb_direct = rb_direct << 16;
		to_set ^= rb_direct;

		//set_opcode
		opcode = opcode << 22;
		to_set ^= opcode;

		//set_reg_a
		ra = ra << 17;
		to_set ^= ra;

		//set_reg_r
		rb = rb << 12;
		to_set ^= rb;

		to_set ^= offset;

		LOG("Curr: %s", std::bitset<32>(to_set).to_string().c_str());

		EInstruction ret = {};
		ret.set_value(to_set);
		return ret;
	}

	/*
	//                     rr_direct-     ----operand---
	//                            |      |||| |||| ||||
	//                operand     |      |||| |||| ||||
	//                ||| ||      |      |||| |||| ||||
	u32 data =      0b000'0000'0000'0000'0000'0000'0000';
	//                      || |||  ||||           ||||
	//                      |regA   regB           regR
	//              ra_direct-
	*/
};

struct EFlags
{
	u8 СF_;
	u8 SF_;
	u8 ZF_;
};


struct EState
{
	EInstruction command_register_;
	ERegister program_counter_;

	ERegister r_[REGISTERS_COUNT];
	EFlags f_;

	EInstruction ram_[RAM_SIZE / sizeof(EInstruction)] = {};

	bool halt_; // TODO - think maybe it should be a flag ?
	bool no_pc_increment_;
};


[[nodiscard]] size_t
get_file_size(
	FILE* f)
{
	fseek(f, 0L, SEEK_END);
	size_t sz = ftell(f);
	fseek(f, 0L, SEEK_SET);
	return sz;
}

Status
emu_load_image(
	EState& state,
	const char* filepath)
{
	FILE* f = fopen(filepath, "rb");
	if (!f)
	{
		LOG("Image not found!");
		ASSERT(false);
		return FILE_NOT_FOUND;
	}

	size_t filesize = get_file_size(f);
	if (filesize != ARRAY_SIZE(state.ram_))
	{
		LOG("Invalid file!");
		ASSERT(false);
		return INVALID_FILE;
	}

	fread_s(&state.ram_, ARRAY_SIZE(state.ram_), sizeof(*state.ram_), ARRAY_SIZE(state.ram_), f);
	fclose(f);

	state.command_register_ = { 0 };
	state.program_counter_ = { 0 };

	return SUCCESS;
}

inline Status
emu_load_next(
	EState& state)
{
	state.command_register_ = state.ram_[state.program_counter_];

	// LOG("%s", std::bitset<32>(state.command_register_.get_value()).to_string().c_str());

	return SUCCESS;
}

#define EARG(x) ((x).second ? (x).first : &s->r_[(x).first])

Status
emu_process(
	EState& state)
{
	const auto emu_add = [](EState* s) {
		// destReg = regA + regB
		auto i = s->command_register_;
		auto rr = i.get_reg_r();
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();

		auto arg_a = ra.second ? s->ram_[ra.first].get_value() : s->r_[ra.first];
		auto arg_b = rb.second ? s->ram_[rb.first].get_value() : s->r_[rb.first];
		auto arg_r = &s->r_[rr];

		*arg_r = arg_a + arg_b;
	};

	const auto emu_nand = [](EState* s) {
		// destReg = !(regA && regB)
		auto i = s->command_register_;

		auto rr = i.get_reg_r();
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();

		auto arg_a = ra.second ? ra.first : s->r_[ra.first];
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		auto arg_r = &s->r_[rr];

		*arg_r = ~(arg_a & arg_b);
	};

	const auto emu_lw = [](EState* s) {
		// destReg = load from memory by OPERAND/LABEL
		auto i = s->command_register_;
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();
		auto offset = i.get_operand();
		auto arg_a = ra.first;
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		s->r_[arg_a] = (ERegister)s->ram_[arg_b + offset].get_value();
	};

	const auto emu_sw = [](EState* s) {
		// save to memory from regR by OPERAND/LABEL
		auto i = s->command_register_;
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();
		auto offset = i.get_operand();
		auto arg_a = ra.first;
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		s->ram_[arg_a + offset].set_value(arg_b);
	};

	const auto emu_beq = [](EState* s) {
		// if regR == regA goto ProgramCounter + 1 + shiftamount, in PC is saved addr of current instruction
		auto i = s->command_register_;
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();
		auto offset = i.get_operand();
		auto arg_a = ra.second ? ra.first : s->r_[ra.first];
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		if (arg_a == arg_b)
			s->program_counter_ += offset;
	};

	const auto emu_jalr = [](EState* s) {
		// saves PC+1 into regR. in PC is saved addr of current instruction. 
		// Goto regA addr. if regR and regA is same register then first write PC + 1 and then goto PC + 1.
		auto i = s->command_register_;
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();
		auto offset = i.get_operand();
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		
		if (ra.second)
			s->ram_[ra.first].set_value((ERegister)(s->program_counter_ + 1));
		else
			s->ram_[s->r_[ra.first]].set_value((ERegister)(s->program_counter_ + 1));

		s->program_counter_ = arg_b;
		s->no_pc_increment_ = true;
	};

	const auto emu_halt = [](EState* s) {
		// PC + 1 and then STOP. Show message about emu stop
		s->halt_ = true;
	};
	const auto emu_nop = [](EState* s) {
		// no operation
	};

	const auto emu_inc = [](EState* s) {
		// -INC regA; increment by one
		auto i = s->command_register_;
		auto ra = i.get_reg_a();
		if (ra.second)
			s->ram_[ra.first]++;
		else
			s->r_[ra.first]++;
	};

	const auto emu_idiv = [](EState* s) {
		// * -IDIV regA regB destReg; sign division destReg = regA / regB
		auto i = s->command_register_;

		auto rr = i.get_reg_r();
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();

		auto arg_a = ra.second ? ra.first : s->r_[ra.first];
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		auto arg_r = &s->r_[rr];

		*arg_r = arg_a / arg_b;
	};

	const auto emu_imul = [](EState* s) {
		// * -IMUL regA regB destReg; sign multiplication destReg = regA * regB
		auto i = s->command_register_;
		auto rr = i.get_reg_r();
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();

		auto arg_a = ra.second ? ra.first : s->r_[ra.first];
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		auto arg_r = &s->r_[rr];

		*arg_r = arg_a * arg_b;
	};

	const auto emu_and = [](EState* s) {
		// *-AND regA regB destReg; destReg = regA & regB
		auto i = s->command_register_;
		auto rr = i.get_reg_r();
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();

		auto arg_a = ra.second ? ra.first : s->r_[ra.first];
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		auto arg_r = &s->r_[rr];

		*arg_r = arg_a & arg_b;
	};
	const auto emu_xor = [](EState* s) {
		// * -XOR regA regB destReg; addition by module 2: destReg = regA # regB
		auto i = s->command_register_;
		auto rr = i.get_reg_r();
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();

		auto arg_a = ra.second ? ra.first : s->r_[ra.first];
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		auto arg_r = &s->r_[rr];

		*arg_r = arg_a ^ arg_b;
	};
	const auto emu_shr = [](EState* s) {
		// * -SHR regA regB destReg; logic shift right destReg = regA >> regB
		auto i = s->command_register_;
		auto rr = i.get_reg_r();
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();

		auto arg_a = ra.second ? ra.first : s->r_[ra.first];
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		auto arg_r = &s->r_[rr];

		*arg_r = arg_a >> arg_b;
	};

	const auto emu_adc = [](EState* s) {
		// -ADC regA regB destReg; addition with CF : destReg = regA + regB + CF
		auto i = s->command_register_;

		auto rr = i.get_reg_r();
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();

		auto arg_a = ra.second ? ra.first : s->r_[ra.first];
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		auto arg_r = &s->r_[rr];

		*arg_r = arg_a + arg_b + s->f_.СF_;

	};

	const auto emu_sbb = [](EState* s) {
		// * -SBB regA regB destReg; subtraction with CF : destReg = regA - regB - СF
		auto i = s->command_register_;

		auto rr = i.get_reg_r();
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();

		auto arg_a = ra.second ? ra.first : s->r_[ra.first];
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];
		auto arg_r = &s->r_[rr];

		*arg_r = arg_a - arg_b - s->f_.СF_;
	};

	const auto emu_cmp = [](EState* s) {
		// *-CMP regA regB; cmp regA regBand set flags
		// 	*             СF SF ZF
		// 	* regA < regB 1  1  0
		// 	* regA = regB 0  0  1
		// 	* regA > regB 0  0  0
		auto i = s->command_register_;
		auto ra = i.get_reg_a();
		auto rb = i.get_reg_b();

		auto arg_a = ra.second ? ra.first : s->r_[ra.first];
		auto arg_b = rb.second ? rb.first : s->r_[rb.first];

		if (arg_a < arg_b)
			s->f_ = { .СF_ = true, .SF_ = true, .ZF_ = false };
		else if (arg_a == arg_b)
			s->f_ = { .СF_ = false, .SF_ = false, .ZF_ = true };
		else
			s->f_ = { .СF_ = false, .SF_ = false, .ZF_ = false };
	};

	using instruction_executor_t = void(*)(EState*);
	static const std::unordered_map<u32, instruction_executor_t> cmd_executor = {
		{ E_ADD,  emu_add  },
		{ E_NAND, emu_nand },
		{ E_LW,	  emu_lw   },
		{ E_SW,	  emu_sw   },
		{ E_BEQ,  emu_beq  },
		{ E_JALR, emu_jalr },
		{ E_HALT, emu_halt },
		{ E_NOOP, emu_nop  },
		{ E_INC,  emu_inc  },
		{ E_IDIV, emu_idiv },
		{ E_IMUL, emu_imul },
		{ E_AND,  emu_and  },
		{ E_XOR,  emu_xor  },
		{ E_SHR,  emu_shr  },
		{ E_JMA,  emu_nop  },
		{ E_JMBE, emu_nop  },
		{ E_ADC,  emu_adc  },
		{ E_SBB,  emu_sbb  },
		{ E_CMP,  emu_cmp  }
	};

	auto i = state.command_register_;
	u32 opcode = i.get_opcode();

	// LOG("Exec: %s", std::bitset<32>(i.get_value()).to_string().c_str());

	ASSERT(IN_RANGE_E(opcode, 0, __ECOMMAND_LAST) && "Opcode is not found!");
	ASSERT(IN_RANGE_E(state.program_counter_, 0, ARRAY_SIZE(state.ram_)) && "Error: program_counter inval");

	auto it = cmd_executor.at(opcode);

	it(&state);

	if (!state.no_pc_increment_)
		state.program_counter_++;
	else
		state.no_pc_increment_ = false;

	return SUCCESS;
}

Status
emu_execute(
	EState& state)
{
	while (!state.halt_)
	{
		emu_load_next(state);
		emu_process(state);
	}

	return SUCCESS;
}

Status
emu_show_state(
	EState& state)
{
	LOG("");
	LOG("Emulator state:");
	// print registers
	for (size_t i = 0; i < ARRAY_SIZE(state.r_); i++)
	{
		LOG("r%d = %d", i, (u32)state.r_[i]);
	}

	return SUCCESS;
}

#pragma pack(pop)