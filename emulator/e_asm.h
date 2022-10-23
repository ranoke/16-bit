#pragma once
#include "e_base.h"

constexpr EOpcodeDesc FIND_OPCODE_BY_NAME(const std::string_view name)
{
	for (size_t i = 0; i < ARRAY_SIZE(opcode_descriptions); i++)
	{
		if (opcode_descriptions[i].asm_name_ == name)
			return opcode_descriptions[i];
	}
	ASSERT(false && "Not found!");
}

static const std::map<std::string, EOpcodeDesc> instr_to_opcode = {
	{ "add"  , FIND_OPCODE_BY_NAME("add") },
	{ "nand" , FIND_OPCODE_BY_NAME("nand") },
	{ "lw"   , FIND_OPCODE_BY_NAME("lw") },
	{ "sw"   , FIND_OPCODE_BY_NAME("sw") },
	{ "beq"  , FIND_OPCODE_BY_NAME("beq") },
	{ "jalr" , FIND_OPCODE_BY_NAME("jalr") },
	{ "halt" , FIND_OPCODE_BY_NAME("halt") },
	{ "noop" , FIND_OPCODE_BY_NAME("noop") },
	{ "inc"  , FIND_OPCODE_BY_NAME("inc") },
	{ "idiv" , FIND_OPCODE_BY_NAME("idiv") },
	{ "imul" , FIND_OPCODE_BY_NAME("imul") },
	{ "and"  , FIND_OPCODE_BY_NAME("and") },
	{ "xor"  , FIND_OPCODE_BY_NAME("xor") },
	{ "shr"  , FIND_OPCODE_BY_NAME("shr") },
	{ "jma"  , FIND_OPCODE_BY_NAME("jma") },
	{ "jmbe" , FIND_OPCODE_BY_NAME("jmbe") },
	{ "adc"  , FIND_OPCODE_BY_NAME("adc") },
	{ "sbb"  , FIND_OPCODE_BY_NAME("sbb") },
	{ "cmp"  , FIND_OPCODE_BY_NAME("cmp") },
	{ ".fill", {}}
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

struct EAsmCompillerData
{
	std::map<std::string, u32> labels_;
	EInstruction compilled_code[RAM_SIZE / sizeof(EInstruction)] = {};
};

[[ no_discard ]] std::vector<std::string>
str_split(
	std::string s,
	std::string delimiter)
{
	size_t pos_start = 0, pos_end, delim_len = delimiter.length();
	std::string token;
	std::vector<std::string> res;

	while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
		token = s.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
		res.push_back(token);
	}

	res.push_back(s.substr(pos_start));
	return res;
}

[[no_discard]] std::string
str_concat(
	const std::vector<std::string>& strings,
	const std::string& endl = "\n")
{
	std::string ret;
	for (const auto& s : strings)
		ret += s + endl;
	return ret;
}

void
str_strip(
	std::string& str)
{
	if (str.length() != 0)
	{
		auto w = std::string(" ");
		auto n = std::string("\n");
		auto r = std::string("\t");
		auto t = std::string("\r");
		auto v = std::string(1, str.front());
		while ((v == w) || (v == t) || (v == r) || (v == n))
		{
			str.erase(str.begin());
			v = std::string(1, str.front());
		}
		v = std::string(1, str.back());
		while ((v == w) || (v == t) || (v == r) || (v == n))
		{
			str.erase(str.end() - 1);
			v = std::string(1, str.back());
		}
	}
}

void
str_replace(
	std::string& in,
	const std::regex& re,
	const std::string& replace_with)
{
	while (std::regex_search(in, re)) {
		in = std::regex_replace(in, re, replace_with);
	}
}

[[ no_discard ]] u32
emu_asm_offset(
	const std::string& s
)
{
	return std::stoi(s);
}

Status
emu_asm_preprocess(
	EAsmCompillerData& compiller_data,
	std::string& code)
{
	// remove comments
	str_replace(code, std::regex(R"(\;(.*))"), "");

	// remove empty lines
	str_replace(code, std::regex(R"(^\s*$\n)"), "");

	// replace multi space with a single space
	str_replace(code, std::regex(R"([\r\t\f ]{2,})"), " ");

	// remove spaces before the text and after in a line and spaces between instruction params
	auto code_lines = str_split(code, "\n");
	for (auto& s : code_lines)
	{
		// idk from where do i get \t string ? 
		if (s == "\t")
		{
			s = "";
			continue;
		}

		if (!s.empty())
			str_strip(s);
	}


	// process labels - they start with $
	u32 ref_line = 0;
	for (auto& s : code_lines)
	{
		auto line = str_split(s, " ");
		if (std::regex_match(line[0], std::regex(R"(\$\w+)")))
		{
			compiller_data.labels_[line[0]] = ref_line;

			// remove label - we don't need it anymore
			line.erase(line.begin());
		}

		s = str_concat(line, " ");

		ref_line++;
	}

	code = str_concat(code_lines);

	// remove empty lines
	str_replace(code, std::regex(R"(^\s*$\n)"), "");
	code_lines = str_split(code, "\n");
	std::remove_if(code_lines.begin(), code_lines.end(), [](const std::string& s) {return s.empty(); });
	code = str_concat(code_lines);

	LOG("\nCode:\n\n%s\n", code.c_str());
	return SUCCESS;
}

Status
emu_asm(
	EAsmCompillerData& compiller_data,
	const std::string& asm_code)
{
	std::string code = asm_code;

	emu_asm_preprocess(compiller_data, code);

	u32 code_line = 0;
	auto instructions = str_split(code, "\n");
	for (auto& i_line : instructions)
	{
		if (i_line.empty())
		{
			LOG("Skipping one line");
			continue;
		}

		auto i = str_split(i_line, " ");
		auto opcode_str = i[0];

		auto it = instr_to_opcode.find(opcode_str);
		if (it == instr_to_opcode.end())
		{
			LOG("Failure on line: %s", i_line.c_str());
			ASSERT(false);
			return FAILURE;
		}

		const auto get_arg = [&](const std::string& arg) -> std::pair<u32, bool> {
			if (std::regex_match(arg, std::regex(R"(\$\w+)")))
			{
				// label
				return { compiller_data.labels_[arg], true };
			}
			else if (std::regex_match(arg, std::regex(R"(r[0-8])")))
			{
				// register
				return { reg_name_to_reg_index.at(arg), false};
			}
			
			LOG("Arg is not valid: %s", arg.c_str());
			ASSERT(false && "Argument is not valid!");
			return { 0, false };
		};

		// a b r
		EInstruction compilled_instruction = {};

		if (opcode_str == ".fill")
		{
			auto type = i[1];
			auto value = emu_asm_offset(i[2]);

			if (type == "dec")
				compilled_instruction.set_value(value);
			else
				ASSERT(false && "Bad value desc!");
		}
		else
		{

			u32 opcode = it->second.opcode_;
			switch (it->second.args_type_)
			{
			case EARGS_NONE: {
				compilled_instruction = EInstruction::create_ra_rb_rr(opcode, 0, 0, 0);
			} break;
			case EARGS_A: {
				auto ra = get_arg(i[1]);

				compilled_instruction = EInstruction::create_ra_rb_rr(opcode, ra.first, 0, 0, ra.second, 0);
			} break;
			case EARGS_A_B: {
				auto ra = get_arg(i[1]);
				auto rb = get_arg(i[2]);

				compilled_instruction = EInstruction::create_ra_rb_rr(opcode, ra.first, rb.first, 0, ra.second, rb.second);
			} break;
			case EARGS_A_B_R: {
				auto ra = get_arg(i[1]);
				auto rb = get_arg(i[2]);
				auto rr = get_arg(i[3]);

				compilled_instruction = EInstruction::create_ra_rb_rr(opcode, ra.first, rb.first, rr.first, ra.second, rb.second);
			} break;
			case EARGS_A_B_OFFSET: {
				auto ra = get_arg(i[1]);
				auto rb = get_arg(i[2]);
				auto offset = emu_asm_offset(i[3]);

				compilled_instruction = EInstruction::create_ra_rb_offset(opcode, ra.first, rb.first, offset, ra.second, rb.second);
			} break;
			default: {
				ASSERT(false && "Opcode Type is not found!");
			} break;
			}
		}

		compiller_data.compilled_code[code_line] = compilled_instruction;
		code_line++;

		if (code_line > ARRAY_SIZE(compiller_data.compilled_code))
			ASSERT(false && "too much code!");
	}

	LOG("Compilled: ");
	for (size_t i = 0; i < ARRAY_SIZE(compiller_data.compilled_code); i++)
	{
		LOG("%s", std::bitset<32>(compiller_data.compilled_code[i].get_value()).to_string().c_str());
	}
	

	return SUCCESS;
}