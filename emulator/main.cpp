#include "e_base.h"
#include "e_asm.h"

i32
main(
	i32 argc,
	char **argv)
{
	// if (argc < 2)
	// 	 return -1;

	EAsmCompillerData compiller_data = {};
	emu_asm(compiller_data, R"(
		lw r0 $first 0
		inc r0
		halt
		$first .fill dec 10
)");

	EState state = {};

	std::memcpy(state.ram_, compiller_data.compilled_code, RAM_SIZE);

	emu_execute(state);

	emu_show_state(state);
	// const char* image_filepath = "image.img"; // argv[2];
	// if (emu_load_image(state, image_filepath) != SUCCESS)
	// {
	// 	LOG("Failed to read image: %s", image_filepath);
	// 	ASSERT(false);
	// 	return -1;
	// }


	return 0;
}