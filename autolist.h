#ifndef AUTOLIST_H
#define AUTOLIST_H

#include <stddef.h>

#define AUTOLIST_ENTRY(LIST_NAME, ITEM_TYPE, ITEM_NAME) \
	extern ITEM_TYPE ITEM_NAME; \
	const autolist_entry_t autolist__##ITEM_NAME##_entry = { \
		.name = #ITEM_NAME, \
		.name_length = sizeof(#ITEM_NAME) - 1, \
		.value_addr = (void*)&ITEM_NAME, \
		.value_size = sizeof(ITEM_NAME), \
	}; \
	AUTOLIST__SECTION_BEGIN(LIST_NAME) \
	const autolist_entry_t* const autolist__##ITEM_NAME##_info_ptr = &autolist__##ITEM_NAME##_entry; \
	AUTOLIST__SECTION_END \
	ITEM_TYPE ITEM_NAME

#if defined(_MSC_VER)
#	define AUTOLIST__CONCAT2(A, B) A##B
#	define AUTOLIST__CONCAT(A, B) AUTOLIST__CONCAT2(A, B)
#	define AUTOLIST__STR2(X) #X
#	define AUTOLIST__STR(X) AUTOLIST__STR2(X)
#	define AUTOLIST__SECTION_BEGIN(NAME) \
	__pragma(data_seg(push)); \
	__pragma(section(AUTOLIST__STR(AUTOLIST__CONCAT(NAME, $data)), read)); \
	__declspec(allocate(AUTOLIST__STR(AUTOLIST__CONCAT(NAME, $data))))
#elif defined(__APPLE__)
#	define AUTOLIST__SECTION_BEGIN(NAME) __attribute__((used, section("__DATA,autolist_" #NAME)))
#elif defined(__unix__)
#	define AUTOLIST__SECTION_BEGIN(NAME) __attribute__((used, section("autolist_" #NAME)))
#else
#	error Unsupported compiler
#endif

#if defined(_MSC_VER)
#	define AUTOLIST__SECTION_END __pragma(data_seg(pop));
#elif defined(__APPLE__)
#	define AUTOLIST__SECTION_END
#elif defined(__unix__)
#	define AUTOLIST__SECTION_END
#endif

typedef struct {
	const char* name;
	size_t name_length;
	void* value_addr;
	size_t value_size;
} autolist_entry_t;

#if defined(_MSC_VER)
#	define AUTOLIST_DECLARE(NAME) \
	__pragma(section(AUTOLIST__STR(AUTOLIST__CONCAT(NAME, $begin)), read)); \
	__pragma(section(AUTOLIST__STR(AUTOLIST__CONCAT(NAME, $data)), read)); \
	__pragma(section(AUTOLIST__STR(AUTOLIST__CONCAT(NAME, $end)), read)); \
	__declspec(allocate(AUTOLIST__STR(AUTOLIST__CONCAT(NAME, $begin)))) extern const autolist_entry_t* const autolist_##NAME##_begin = NULL; \
	__declspec(allocate(AUTOLIST__STR(AUTOLIST__CONCAT(NAME, $end)))) extern const autolist_entry_t* const autolist_##NAME##_end = NULL;
#elif defined(__APPLE__)
#	define AUTOLIST_DECLARE(NAME) \
	extern const autolist_entry_t* const __start_##NAME __asm("section$start$__DATA$autolist_" #NAME); \
	extern const autolist_entry_t* const __stop_##NAME __asm("section$end$__DATA$autolist_" #NAME); \
	__attribute__((used, section("__DATA,autolist_" #NAME))) const autolist_entry_t* const autolist_##NAME##__dummy = NULL;
#elif defined(__unix__)
#	define AUTOLIST_DECLARE(NAME) \
	extern const autolist_entry_t* const __start_autolist_##NAME; \
	extern const autolist_entry_t* const __stop_autolist_##NAME; \
	__attribute__((used, section(#NAME))) const autolist_entry_t* const autolist_##NAME##__dummy = NULL;
#endif

#if defined(_MSC_VER)
#	define AUTOLIST_BEGIN(NAME) (&autolist_##NAME##_begin + 1)
#	define AUTOLIST_END(NAME) (&autolist_##NAME##_end)
#elif defined(__unix__) || defined(__APPLE__)
#	define AUTOLIST_BEGIN(NAME) (&__start_autolist_##NAME)
#	define AUTOLIST_END(NAME) (&__stop_autolist_##NAME)
#endif

#endif
