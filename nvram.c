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
 * We will refer to variables that have been augmented with this capability as
 * NVRAM variables - for "Non-Volatile RAM", as unlike normal variables in C,
 * their value is kept across program runs.
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
 * We can take this technique a step further and have the variables
 * automatically saved to disk at program exit, we can do this by using the
 * "atexit" function, which registers a callback to called when the program
 * exists normally. This has the added benefit that if an "assert" fails, or a
 * signal is not caught, then the data will not be saved as it is presumably
 * corrupt. The programmer now only has to worry about calling an 
 * initialization function (defined as "nvram_initialize" below).
 * 
 * This technique has various portability problems and version problems, which
 * can be remedied but require a deeper understanding of a specific toolchain,
 * discipline from the programmer, or extra code not shown here.
 *
 * Limitations:
 * - The method of course does not work for dynamically allocated variables,
 *   only for variables of a static storage duration. It must be known at
 *   compile time the size of the data structures to be stored.
 *
 * Consistency Problems:
 * - The program below may fail only do a partial update or load, these
 *   consistency problems can be solved in code, but are not needed for a
 *   simple demonstration.
 *
 * Portability Problems:
 * - The alignment between variables stored in a section needs to be
 *   controlled.
 * - Only fixed width types can be used (portably)
 * - The endianess of variables can differ between machines, however it can be
 *   checked for, correcting for it when it differs is a different matter
 *   however.
 * - The order in which variables are stored by the linker is not specified,
 *   although for GCC there is the option "-fno-toplevel-reorder", which will
 *   place variables (as well as functions) in their section in the order in
 *   which they appear. 
 * - If Non-Volatile variables are declared in multiple files then the order
 *   object files are passed to the linker could change the format of the file
 *   stored on disk.
 * - Most of these problems can be solved by only storing a single struct
 *   per section, with "__attribute__((packed))", however this makes the
 *   variables more awkward to use. 
 * 
 * Version Incompatibility Problems:
 * - If a variables data storage type is changed, or if a variable is added
 *   or removed, this will invalidate any previously stored files.
 * - A checksum of the default values can be calculated and stored, along with
 *   the size of the section used to store the NVRAM variables, and perhaps a
 *   version number. If either of these does not matched the stored data the
 *   data would have to be regenerated.
 *
 * https://gcc.gnu.org/onlinedocs/gcc-4.0.4/gcc/Type-Attributes.html
 * https://stackoverflow.com/questions/16751378
 * https://gcc.gnu.org/ml/gcc-help/2013-10/msg00047.html
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

/* ======= NVRAM Setup ===================================================== */

const  char *nvram_name = "nvram.blk";          /**< file to store NVRAM variables in */
extern char __start_nvram;                      /**< start of section 'nvram' */
extern char __stop_nvram;                       /**< end   of section 'nvram' */
#define NVRAM volatile __attribute__((section("nvram"))) __attribute__ ((aligned (8))) /**< used to put a variable in 'NVRAM' */

/* ======= NVRAM Setup ===================================================== */

/* ======= NVRAM Variables ================================================= */

       NVRAM uint64_t nv_format   = 0xFF4e5652414d00FFuLL; /**< file format _and_ endianess specifier */
       NVRAM uint64_t nv_version  = 1u;        /**< data version number */
static NVRAM int32_t  nv_a = 0;                /**< example NVRAM variable 'a' */
static NVRAM int32_t  nv_b = 0;                /**< example NVRAM variable 'b' */
static NVRAM int32_t  nv_c = 0;                /**< example NVRAM variable 'c' */
static NVRAM uint64_t nv_count = 0;            /**< this variable is incremented each time the program is run */

/* ======= NVRAM Variables ================================================= */

/* ======= Utility Functions =============================================== */

/**< Transfer a block of memory to (read = false) or from (read = true) disk  */
static int block(char *buffer, size_t length, const char *name, bool read)
{
	FILE *l = NULL;
	size_t r = 0;
	assert(buffer);
	assert(name);

	errno = 0;
	l = fopen(name, read ? "rb" : "wb");
	if (!l) {
		fprintf(stderr, "block %s from '%s' failed: %s\n", 
				read ? "load" : "save", 
				name, 
				strerror(errno));
		return -1;
	}
	errno = 0;
	r = read ? fread(buffer, 1, length, l) : fwrite(buffer, 1, length, l);
	if (r != length) {
		fprintf(stderr, "warning - partial block operation "
				" (%zu/%zu bytes %s): %s\n", 
				r, 
				length, 
				read ? "read" : "wrote", 
				strerror(errno));
		fclose(l);
		return -1;
	}
	fclose(l);
	return 0;
}

/**< function to register with atexit, this saves the block to disk */
static void nvram_save(void)
{
	fprintf(stderr, "saving nvram to '%s'\n", nvram_name);
	if (block(&__start_nvram, &__stop_nvram - &__start_nvram, nvram_name, false)) {
		fprintf(stderr, "nvram block save failed: '%s'\n", nvram_name);
	}
}

/**< register save call back atexit and load in NVRAM variables
 * @return 0< fatal error, 0 = okay, 1 = warning */
static int nvram_initialize(void)
{
	int r = 0;
	uint64_t format = nv_format;
	uint64_t version = nv_version;

	if (block(&__start_nvram, &__stop_nvram - &__start_nvram, nvram_name, true))
		r = 1;

	if (format != nv_format) {
		fprintf(stderr, "file format/endianess incompatibility: expected %"PRIx64 " - actual %"PRIx64"\n", format, nv_version);
		return -1;
	}
	if (version != nv_version) {
		fprintf(stderr, "version incompatibility: expected %"PRIx64 " - actual %"PRIx64"\n", version, nv_version);
		return -1;
	}

	if (atexit(nvram_save)) {
		fputs("atexit: failed to register nvram_save\n", stderr);
		return -1;
	}
	return r;
}

/* ======= Utility Functions =============================================== */

/* ======= Test Program ==================================================== */
/* A simple test program for the techniques described above, it prints the
 * default values for NVRAM variables, initializes the NVRAM and registers the
 * save callback, and allows the user to update values which will be saved to
 * disk on exit. */
int main(void)
{
	/* default values can be accessed before nvram_initialize is called */
	printf("default a:   %d\n", (int)nv_a);
	printf("default b:   %d\n", (int)nv_b);
	printf("default c:   %d\n", (int)nv_c);

	/* loads in variables off disk and registers nvram_initialize */
	if(nvram_initialize() < 0)
		return -1;

	printf("count:       %u\n", (unsigned)(nv_count++));
	printf("loaded a:    %d\n", (int)nv_a);
	printf("loaded b:    %d\n", (int)nv_b);
	printf("loaded c:    %d\n", (int)nv_c);

	/* accept some user input and do some calculations */
	fputs("a new value: ", stdout);
	scanf("%" SCNd32, &nv_a);
	fputs("b new value: ", stdout);
	scanf("%" SCNd32, &nv_b);
	nv_c = nv_a + nv_b;
	printf("c = a + b\nc = %d\n", (int)nv_c);

	/* We do not have to worry about calling nvram_save, atexit will */

	return 0;
}
/* ======= Test Program ==================================================== */
