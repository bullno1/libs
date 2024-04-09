#ifndef REGLIST_H
#define REGLIST_H

#include <stddef.h>

#define REGLIST_ENTRY(LIST_NAME, ITEM_TYPE, ITEM_NAME) \
	extern ITEM_TYPE ITEM_NAME; \
	const reglist_entry_t reglist__##ITEM_NAME##_entry = { \
		.name = #ITEM_NAME, \
		.name_length = sizeof(#ITEM_NAME) - 1, \
		.value_addr = &ITEM_NAME, \
		.value_size = sizeof(ITEM_NAME), \
	}; \
	REGLIST__SECTION_BEGIN(LIST_NAME) \
	const reglist_entry_t* const reglist__##ITEM_NAME##_info_ptr = &reglist__##ITEM_NAME##_entry; \
	REGLIST__SECTION_END \
	ITEM_TYPE ITEM_NAME

#if defined(_MSC_VER)
#	define REGLIST__CONCAT2(A, B) A##B
#	define REGLIST__CONCAT(A, B) REGLIST__CONCAT2(A, B)
#	define REGLIST__STR2(X) #X
#	define REGLIST__STR(X) REGLIST__STR2(X)
#	define REGLIST__SECTION_BEGIN(NAME) \
	__pragma(data_seg(push)); \
	__pragma(section(REGLIST__STR(REGLIST__CONCAT(NAME, $data)), read)); \
	__declspec(allocate(REGLIST__STR(REGLIST__CONCAT(NAME, $data))))
#elif defined(__APPLE__)
#	define REGLIST__SECTION_BEGIN(NAME) __attribute__((used, section("__DATA,reglist_" #NAME)))
#elif defined(__unix__)
#	define REGLIST__SECTION_BEGIN(NAME) __attribute__((used, section("reglist_" #NAME)))
#else
#	error Unsupported compiler
#endif

#if defined(_MSC_VER)
#	define REGLIST__SECTION_END __pragma(data_seg(pop));
#elif defined(__APPLE__)
#	define REGLIST__SECTION_END
#elif defined(__unix__)
#	define REGLIST__SECTION_END
#endif

typedef struct {
	const char* name;
	size_t name_length;
	void* value_addr;
	size_t value_size;
} reglist_entry_t;

#if defined(_MSC_VER)
#	define REGLIST_DECLARE(NAME) \
	__pragma(section(REGLIST__STR(REGLIST__CONCAT(NAME, $begin)), read)); \
	__pragma(section(REGLIST__STR(REGLIST__CONCAT(NAME, $data)), read)); \
	__pragma(section(REGLIST__STR(REGLIST__CONCAT(NAME, $end)), read)); \
	__declspec(allocate(REGLIST__STR(REGLIST__CONCAT(NAME, $begin)))) extern const reglist_entry_t* const reglist_##NAME##_begin = NULL; \
	__declspec(allocate(REGLIST__STR(REGLIST__CONCAT(NAME, $end)))) extern const reglist_entry_t* const reglist_##NAME##_end = NULL;
#elif defined(__APPLE__)
#	define REGLIST_DECLARE(NAME) \
	extern const reglist_entry_t* const __start_##NAME __asm("section$start$__DATA$reglist_" #NAME); \
	extern const reglist_entry_t* const __stop_##NAME __asm("section$end$__DATA$reglist_" #NAME); \
	__attribute__((used, section("__DATA,reglist_" #NAME))) const reglist_entry_t* const reglist_##NAME##__dummy = NULL;
#elif defined(__unix__)
#	define REGLIST_DECLARE(NAME) \
	extern const reglist_entry_t* const __start_reglist_##NAME; \
	extern const reglist_entry_t* const __stop_reglist_##NAME; \
	__attribute__((used, section(#NAME))) const reglist_entry_t* const reglist_##NAME##__dummy = NULL;
#endif

#if defined(_MSC_VER)
#	define REGLIST_BEGIN(NAME) (&reglist_##NAME##_begin + 1)
#	define REGLIST_END(NAME) (&reglist_##NAME##_end)
#elif defined(__unix__) || defined(__APPLE__)
#	define REGLIST_BEGIN(NAME) (&__start_reglist_##NAME)
#	define REGLIST_END(NAME) (&__stop_reglist_##NAME)
#endif

#endif
