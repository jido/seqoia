/*

Copyright (c) 2021, Dominic Szablewski - https://phoboslab.org
SPDX-License-Identifier: MIT


clang fuzzing harness for qoi_decode

Compile and run with: 
	clang -fsanitize=address,fuzzer -g -O0 sqoafuzz.c && ./a.out

*/


#define SQOA_IMPLEMENTATION
#include "seqoia.h"
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	int w, h;
	if (size < 4) {
		return 0;
	}

	sqoa_desc desc;
	void* decoded = sqoa_decode((void*)(data + 4), (int)(size - 4), &desc, *((int *)data));
	if (decoded != NULL) {
		free(decoded);
	}
	return 0;
}
