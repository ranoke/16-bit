#include <map>
#include <regex>
#include <bitset>
#include <sstream>
#include <unordered_map>

#include <cstdio>
#include <cstdint>
#include <cassert>

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
 *																СF SF ZF
 *																regA < regB 1 1 0
 *																regA = regB 0 0 1
 *																regA > regB 0 0 0
 * ADDRESATION:
 *   - Direct (using operands)
 */

#define RAM_SIZE 4096

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define CONCAT(x, y) x ## y
#define LOG(fmt, ...) printf(__FILE__ ": " fmt "\n", __VA_ARGS__)
#define ASSERT assert

using u16 = uint16_t;
using i16 = int16_t;
using u32 = uint32_t;
using i32 = int32_t;
using i8  = int8_t;
using u8  = uint8_t;

enum Status
{
	SUCCESS,
	FAILURE,
	FILE_NOT_FOUND,
	INVALID_FILE,
};

enum ECommand
{
	E_ADD,
	E_NAND,
	E_LW,
	E_SW,
	E_BEQ,
	E_JALR,
	E_HALT,
	E_NOOP,
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
	E_STORE,
	E_LOAD,
	__ECOMMAND_MAX,
	__ECOMMAND_LAST = __ECOMMAND_MAX - 1
};

template<typename T>
struct Pair
{
	T x;
	T y;
};

#pragma pack(push)
struct ERegister
{
	[[nodiscard]] u32
	get_value()
	{
		return ((value[0] << 24)
			  | (value[1] << 16)
			  | (value[2] << 8)) >> 8;  
	}

	void
	set_value(u32 v)
	{
		value[0]     = 0b0000'0000'1111'1111'0000'0000'0000'0000 & v;
		value[1]     = 0b0000'0000'0000'0000'1111'1111'0000'0000 & v;
		value[2]     = 0b0000'0000'0000'0000'0000'0000'1111'1111 & v;
		u32 overflow = 0b1111'1111'0000'0000'0000'0000'0000'0000 & v;
	}

	u32 operator++(int)
	{
		u32 inc_val = get_value() + 1;
		set_value(inc_val);
		return inc_val;
	}

	u8 value[3] = { 0 };
};
#pragma pack(pop)

using EOperand = ERegister;

#pragma pack(push)
struct EInstuction
{
	[[nodiscard]] u32
		get_opcode()
	{
		return 0b1111'1000'0000'0000'0000'0000'0000'0000 & data;
	}

	[[nodiscard]] u32
		get_reg_a()
	{
		return 0b0000'0111'0000'0000'0000'0000'0000'0000 & data;
	}

	[[nodiscard]] u32
		get_reg_b()
	{
		return 0b0000'0000'1110'0000'0000'0000'0000'0000 & data;
	}

	[[nodiscard]] u32
		get_dest()
	{
		return 0b0000'0000'0001'1100'0000'0000'0000'0000 & data;
	}

	void 
		set_opcode(u32 val)
	{
		ASSERT(0 > val || val > 0b1111);
		val = val << 27;
		data ^= val;
	}

	void
		set_reg_a(u32 val)
	{
		ASSERT(0 > val || val > 0b111);
		val = val << 24;
		data ^= val;
	}

	void
		set_reg_b(u32 val)
	{
		ASSERT(0 > val || val > 0b111);
		val = val << 21;
		data ^= val;
	}

	void
		set_reg_dest(u32 val)
	{
		ASSERT(0 > val || val > 0b111);
		val = val << 18;
		data ^= val;
	}

	/*
	//          operand    regB
	//           |||| |    |||
	u32 data = 0b0000'0000'0000'0000'0000'0000'0000'0000;
	//                 |||    | ||       
	//                 regA   regR-dest   
	*/

	u32 data = 0b0000'0000'0000'0000'0000'0000'0000'0000;
};
#pragma pack(pop)

#pragma pack(push)
struct EState
{
	EInstuction command_register_;
	ERegister   program_counter_;

	ERegister r_[8];

	// union {
	 	i8 ram_[RAM_SIZE];
	// 	EInstuction iram_[RAM_SIZE / sizeof(EInstuction)];
	// };

		bool halt_; // TODO - think maybe it should be a flag ?
	
};
#pragma pack(pop)

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
	EState &state,
	const char *filepath)
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
	EState &state)
{
	const auto iarr = (EInstuction*)state.ram_;
	state.command_register_ = iarr[state.program_counter_.get_value()];

	return SUCCESS;
}

Status
emu_process(
	EState& state)
{
	// * -IDIV regA regB destReg; sign division destReg = regA / regB
	// * -IMUL regA regB destReg; sign multiplication destReg = regA * regB
	const auto emu_inc = [](EState* s) {
		// -INC regA; increment by one
		auto i = s->command_register_;
		auto r = i.get_reg_a();
		s->r_[r]++;
	};

	const auto emu_nop = [](EState* s) {};

	using instruction_executor_t = void(*)(EState*);
	static const std::unordered_map<u32, instruction_executor_t> cmd_executor = {
	{ E_ADD,  emu_nop },
	{ E_NAND, emu_nop },
	{ E_LW,	  emu_nop },
	{ E_SW,	  emu_nop },
	{ E_BEQ,  emu_nop },
	{ E_JALR, emu_nop },
	{ E_HALT, [](EState* s) { s->halt_ = true; } },
	{ E_NOOP, emu_nop },
	{ E_INC,  emu_inc },
	{ E_IDIV, emu_nop },
	{ E_IMUL, emu_nop },
	{ E_AND,  emu_nop },
	{ E_XOR,  emu_nop },
	{ E_SHR,  emu_nop },
	{ E_JMA,  emu_nop },
	{ E_JMBE, emu_nop },
	{ E_ADC,  emu_nop },
	{ E_SBB,  emu_nop },
	{ E_CMP,  emu_nop },
	{ E_STORE, [](EState* s) {}},
	{ E_STORE, [](EState* s) {}},
	};

	auto i = state.command_register_;
	u32 opcode = i.get_opcode();

	ASSERT(opcode > __ECOMMAND_LAST);

	cmd_executor.at(opcode)(&state);

	state.program_counter_++;

	return SUCCESS;
}

Status
emu_execute(
	EState &state)
{
	while (!state.halt_)
	{
		emu_load_next(state);
		emu_process(state);
	}

	return SUCCESS;
}

Status
emu_assembly(const std::string &asm_code)
{
	const auto VERIFY_REGEX = std::regex(R"([\s+]?(\w+)?\s+(\w+)\s+(\w+)\s+(\w+)?\s+(\w+)?\s+((\w+)|)[\s+]?)");

	u32 compilled[RAM_SIZE / sizeof(u32)] = { 0 };

	std::map<std::string, u32> labels;

	static const std::map<std::string, u32> instr_to_opcode = {
		{ "add",  E_ADD  },
		{ "nand", E_NAND },
		{ "lw",   E_LW	 },
		{ "sw",   E_SW	 },
		{ "beq",  E_BEQ  },
		{ "jalr", E_JALR },
		{ "halt", E_HALT },
		{ "nop",  E_NOOP },
		{ "inc",  E_INC  },
		{ "idiv", E_IDIV },
		{ "imul", E_IMUL },
		{ "and",  E_AND  },
		{ "xor",  E_XOR  },
		{ "shr",  E_SHR  },
		{ "jma",  E_JMA  },
		{ "jmbe", E_JMBE },
		{ "adc",  E_ADC  },
		{ "sbb",  E_SBB  },
		{ "cmp",  E_CMP  },
		{ "store", E_STORE },
		{ "load",  E_LOAD }
	};

	static const std::map<std::string, u32> reg_name_to_reg_index = {
		{ "r0", 0 },
		{ "r1", 1 },
		{ "r2", 2 },
		{ "r3", 3 },
		{ "r4", 4 },
		{ "r5", 5 },
		{ "r6", 6 },
		{ "r7", 7 }
	};
	
	std::string line;
	std::smatch match;
	std::istringstream f(asm_code);

	u32 counter = 0;
	while (std::getline(f, line))
	{
		if (line.empty() || std::regex_match(line, match, std::regex(R"((\s+|\n))")))
			continue;

		// verify by regex
		if (!std::regex_match(line, match, VERIFY_REGEX))
		{
			LOG("ERROR on line: %s\n", line.c_str());
			ASSERT(false);
			return FAILURE;
		}

		continue;

		// mark3 instruction pole1 pole2 pole3 comment
		const auto label    = match[0].str();
		const auto instr    = match[1].str();
		const auto reg_a    = match[2].str();
		const auto reg_b    = match[3].str();
		const auto reg_dest = match[4].str();
		const auto comment  = match[5].str();

		const auto opcode         = instr_to_opcode.at(instr);
		const auto reg_a_index    = reg_name_to_reg_index.at(reg_a);
		const auto reg_b_index    = reg_name_to_reg_index.at(reg_b);
		const auto reg_dest_index = reg_name_to_reg_index.at(reg_a);

		EInstuction i;
		i.set_opcode(opcode);
		i.set_reg_a(reg_a_index);
		i.set_reg_a(reg_b_index);
		i.set_reg_dest(reg_dest_index);

		if (labels.find(label) != labels.end())
			ASSERT(false && "Label already exists!");

		compilled[counter] = i.data;
		labels[label] = counter;
		counter++;
	}

	printf("Compilled: \n");
	for (size_t i = 0; i < ARRAY_SIZE(compilled); i++)
	{
		printf("%s\n", std::bitset<32>(compilled[i]).to_string().c_str());
	}

	return SUCCESS;
}

i32
main(
	i32 argc,
	char **argv)
{
	// if (argc < 2)
	// 	 return -1;

	//mark instruction pole1 pole2 pole3 comment
	//	mark1 instruction pole1 pole2 pole3 comment
	//	mark2 instruction pole1 pole2 pole3 comment
	//	mark3 instruction pole1 pole2 pole3 comment
	//	mark4 instruction pole1 pole2 pole3 comment

	emu_assembly(R"(
		mark1 load pole1 pole2 pole3 comment
		mark1 store pole1 pole2 pole3 comment
		mark1 halt pole1 pole2 pole3 comment

	)");

	EState state;
	// const char* image_filepath = "image.img"; // argv[2];
	// if (emu_load_image(state, image_filepath) != SUCCESS)
	// {
	// 	LOG("Failed to read image: %s", image_filepath);
	// 	ASSERT(false);
	// 	return -1;
	// }


	return 0;
}