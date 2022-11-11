#pragma once

#include "utest.h"

#include "e_base.h"
#include "e_asm.h"

UTEST(emu, emu_lw_add_halt) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		lw r0 $a 0
		lw r1 $b 0
		add r0 r1 r2
		add $a $b r3
		halt
		$a .fill dec 1
		$b .fill dec 2
	)");
	EState state = {};
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ERegister reg_state[9] = { 1,2,3,3,0,0,0,0,0 };

	ASSERT_TRUE(memcmp(state.r_, reg_state, sizeof(state.r_)) == 0);
	ASSERT_TRUE(state.halt_ && state.program_counter_ == 5);
}

UTEST(emu, emu_inc) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		lw r0 $a 0
		inc r0
		inc $a
		halt
		$a .fill dec 1
	)");
	EState state = {};
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ERegister reg_state[9] = { 2 };

	ASSERT_TRUE(memcmp(state.r_, reg_state, sizeof(state.r_)) == 0);
	ASSERT_TRUE(state.ram_[4].get_value() == 2);
	ASSERT_TRUE(state.halt_ && state.program_counter_ == 4);
}

UTEST(emu, emu_halt) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		halt
	)");
	EState state = {};
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);
	ASSERT_TRUE(state.halt_ && state.program_counter_ == 1);
}

UTEST(emu, emu_sw) {
	{
		EAsmCompillerData compiller_data = {};
		emu_asm(compiller_data, R"(
		sw $save r0 0
		halt
		$save .fill dec 0
	)");
		EState state = {};
		state.r_[0] = 10;
		std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
		emu_execute(state);

		ASSERT_TRUE(state.ram_[2].get_value() == 10);
	}
	// TODO
	//{
	//	EAsmCompillerData compiller_data = {};
	//	emu_asm(compiller_data, R"(
	//	sw r1 r0 0
	//	halt
	//	$save .fill dec 0
	//)");
	//	EState state = {};
	//	state.r_[0] = 10;
	//	state.r_[1] = 2;
	//	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	//	emu_execute(state);

	//	ASSERT_TRUE(state.ram_[2].get_value() == 10);
	//}
}

UTEST(emu, emu_nand) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		nand r0 r1 r0
		halt
	)");
	EState state = {};
	state.r_[0] = 2;
	state.r_[1] = 3;
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ERegister reg_state[9] = { };

	ASSERT_TRUE(memcmp(state.r_, reg_state, sizeof(state.r_)) == 0);
}

UTEST(emu, emu_beq) {
	ASSERT_TRUE(0);
}

UTEST(emu, emu_jalr) {
	ASSERT_TRUE(0);
}

UTEST(emu, emu_noop) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		noop
		noop
		halt
	)");
	EState state = {};
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ASSERT_TRUE(state.halt_ && state.program_counter_ == 3);
}

UTEST(emu, emu_idiv) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		idiv r0 r1 r2
		halt
	)");
	EState state = {};
	state.r_[0] = 5;
	state.r_[1] = 2;
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ERegister reg_state[9] = { 5, 2, 2 };
	ASSERT_TRUE(memcmp(state.r_, reg_state, sizeof(state.r_)) == 0);
}

UTEST(emu, emu_imul) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		imul r0 r1 r2
		halt
	)");
	EState state = {};
	state.r_[0] = 5;
	state.r_[1] = 2;
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ERegister reg_state[9] = { 5, 2, 10 };
	ASSERT_TRUE(memcmp(state.r_, reg_state, sizeof(state.r_)) == 0);
}

UTEST(emu, emu_and) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		and r0 r1 r2
		halt
	)");
	EState state = {};
	state.r_[0] = 5;
	state.r_[1] = 2;
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ERegister reg_state[9] = { 5, 2, 5 & 2 };
	ASSERT_TRUE(memcmp(state.r_, reg_state, sizeof(state.r_)) == 0);
}

UTEST(emu, emu_xor) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		xor r0 r1 r2
		halt
	)");
	EState state = {};
	state.r_[0] = 5;
	state.r_[1] = 2;
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ERegister reg_state[9] = { 5, 2, 5 ^ 2 };
	ASSERT_TRUE(memcmp(state.r_, reg_state, sizeof(state.r_)) == 0);
}

UTEST(emu, emu_shr) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		shr r0 r1 r2
		halt
	)");
	EState state = {};
	state.r_[0] = 5;
	state.r_[1] = 2;
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ERegister reg_state[9] = { 5, 2, 5 >> 2 };
	ASSERT_TRUE(memcmp(state.r_, reg_state, sizeof(state.r_)) == 0);
}

UTEST(emu, emu_jma) {
	ASSERT_TRUE(0);
}

UTEST(emu, emu_jmbe) {
	ASSERT_TRUE(0);
}

UTEST(emu, emu_adc) {
	EAsmCompillerData compiller_data = {};

	// TODO somehow adc opcode  is 
	emu_asm(compiller_data, R"(
		adc r0 r1 r2
		halt
	)");
	EState state = { .f_ = { .СF_ = 1 } };
	state.r_[0] = 5;
	state.r_[1] = 2;
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ERegister reg_state[9] = { 5, 2, 8 };
	ASSERT_TRUE(memcmp(state.r_, reg_state, sizeof(state.r_)) == 0);
}

UTEST(emu, emu_sbb) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		sbb r0 r1 r2
		halt
	)");
	EState state = { .f_ = {.СF_ = 1 } };
	state.r_[0] = 5;
	state.r_[1] = 2;
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ERegister reg_state[9] = { 5, 2, 2 };
	ASSERT_TRUE(memcmp(state.r_, reg_state, sizeof(state.r_)) == 0);
}

UTEST(emu, emu_cmp) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		cmp r0 r1
		halt
	)");
	EState state = {};
	state.r_[0] = 5;
	state.r_[1] = 5;
	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);
	emu_execute(state);

	ASSERT_TRUE(state.f_.ZF_);
}

UTEST(emu, emu_fill_and_labels) {
	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		$first  .fill dec 1
		$second .fill dec 2
		$third  .fill dec 3
	)");

	ASSERT_TRUE(compiller_data.compilled_code[0].get_value() == 1);
	ASSERT_TRUE(compiller_data.compilled_code[1].get_value() == 2);
	ASSERT_TRUE(compiller_data.compilled_code[2].get_value() == 3);

	ASSERT_TRUE(compiller_data.labels_.size() == 3);
	ASSERT_TRUE(compiller_data.labels_.find("$first") != compiller_data.labels_.end());
	ASSERT_TRUE(compiller_data.labels_.find("$second") != compiller_data.labels_.end());
	ASSERT_TRUE(compiller_data.labels_.find("$third") != compiller_data.labels_.end());
}

