/**@file nvram.c
 * @license MIT
 * @author Richard James Howe
 * @copyright Richard James Howe (2017)
 * @brief Demonstration of Non-Volatile, file backed, variables with minimal
 * configuration and code.
 *
 * The purpose of this code is to demonstrate a technique for declaring
 * ordinary C variables that are backed by a file, and which are automatically
 * saved to disk when the program terminates normally. The method is common in
 * embedded software development where the variables are saved and loaded to
 * EEPROM in a microcontroller.
 *
 * This code is written in C, but requires a few extensions to be present in
 * the compiler you are using. For this demonstration GCC
 * <https://gcc.gnu.org/> will be used and referred to (Clang
 * <https://clang.llvm.org/> can also be used, without any modification). This
 * makes the code technically non-portable, but nearly every compiler supports
 * some method for achieving the same results.
 *
 * The idea is simple, by locating variables in a specific section of memory,
 * that section of memory can be loaded or saved en mass from or to disk. This
 * requires that the compiler allow you to create a new, custom section, that
 * the compiler also allows you to place variables in that section (preferably
 * with a compiler specific attribute in a variable declaration) and that you
 * can some how work out the start and end location a section. GCC provides all
 * of these mechanisms.
 *
 * A variable can be placed into a section with the section attribute, for
 * example if we want to place an integer 'a', into a section called "nvram":
 *
 * 	__attribute__((section("nvram"))) int a = 0;
 *
 * Instead of using this syntax, we can define a macro and use this instead:
 *
 *	#define NVRAM __attribute__((section("nvram"))
 *	NVRAM int a = 0;
 *
 * By defining a variable and placing it in a section the toolchain will also
 * create two variables, "__start_nvram" and "__stop_nvram" which can be used
 * to find out the start and end of NVRAM respectively.
 *
 * We can take this technique a step further, currently the programmer has to
 * worry about the Non-Volatile variables being saved to disk.
 *
 * 
 * This technique has various portability problems and version problems, which
 * can be remedied but require a deeper understanding of a specific toolchain,
 * discipline from the programmer, or extra code not shown here.
 *
 * Portability problems:
 * - The alignment between variables stored in a section needs to be
 *   controlled.
 * - Only fixed width types can be used.
 * - The endianess of variables can differ between machines, however it can be
 *   checked for.
 * - The order in which variables are stored by the linker is not specified.
 * - If Non-Volatile variables are declared in multiple files then the order
 *   object files are passed to the linker could change the format of the file
 *   stored on disk
 * 
 * Version incompatibility problems:
 * - If a variables data storage type is changed, or if a variable is added
 *   or removed, this will invalidate any previously stored files.
 * - A checksum of the default values can be calculated and stored, along with
 *   the size of the section used to store the NVRAM variables, and perhaps a
 *   version number. If either of these does not matched the stored data the
 *   data would have to be regenerated.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

const char *nvram_name = "nvram.blk"; /**< file to store NVRAM variables in */
extern char __start_nvram; /**< placed by GCC denotes start of section 'nvram' */
extern char __stop_nvram;  /**< placed by GCC denotes end   of section 'nvram' */
#define NVRAM __attribute__((section("nvram")))

NVRAM uint16_t endianess = 0x1234;
NVRAM int32_t nv_a = 0;
NVRAM int32_t nv_b = 0;
NVRAM uint8_t nv_buffer[32] = { 0 };

static int block(char *buffer, size_t length, const char *name, bool read)
{
	FILE *l = NULL;
	size_t r = 0;
	assert(buffer);
	assert(name);

	errno = 0;
       	l = fopen(name, read ? "rb" : "wb");
	if(!l) {
		fprintf(stderr, "block %s from '%s' failed: %s\n", read ? "load" : "save", name, strerror(errno));
		return -1;
	}
	errno = 0;
	r = read ? fread(buffer, 1, length, l) : fwrite(buffer, 1, length, l);
	if(r != length) { 
		fprintf(stderr, "warning - partial block operation (%zu/%zu bytes %s): %s\n", r, length, read ? "read" : "wrote", strerror(errno));
		fclose(l);
		return -1;
	}
	fclose(l);
	return 0;
}

static void nvram_save(void)
{
	if(block(&__start_nvram, &__stop_nvram - &__start_nvram, nvram_name, false)) {
		fprintf(stderr, "nvram block save failed: '%s' will not be updated\n", nvram_name);
	}
}

static int nvram_initialize(void) 
{
	if(atexit(nvram_save)) {
		fputs("atexit: failed to register nvram_save\n", stderr);
		return -1;
	}
	if(block(&__start_nvram, &__stop_nvram - &__start_nvram, nvram_name, true)) {
		fputs("nvram block load failed: default values will be used\n", stderr);
		return -1;
	}
	return 0;
}

int main(void)
{
	nvram_initialize();

	printf("a: %d\n", (int)nv_a);
	printf("b: %d\n", (int)nv_b);

	fputs("Enter new value for 'a': ", stdout);
	scanf("%"SCNd32, &nv_a);
	fputs("Enter new value for 'b': ", stdout);
	scanf("%"SCNd32, &nv_b);

	return 0;
}

