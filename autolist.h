#ifndef AUTOLIST_H
#define AUTOLIST_H

/**
 * @file
 * @brief A list of items collected from all compilation units.
 */

#include <stddef.h>

#define AUTOLIST_ENTRY(LIST_NAME, ITEM_TYPE, ITEM_NAME) \
	AUTOLIST_ENTRY_EX(LIST_NAME, ITEM_TYPE, ITEM_NAME, ITEM_NAME)

#define AUTOLIST_ENTRY_EX(LIST_NAME, ITEM_TYPE, ITEM_NAME, VAR_NAME) \
	extern ITEM_TYPE VAR_NAME; \
	AUTOLIST_ADD_ENTRY(LIST_NAME, ITEM_NAME, VAR_NAME) \
	ITEM_TYPE VAR_NAME

#define AUTOLIST_ADD_ENTRY(LIST_NAME, ITEM_NAME, VAR_NAME) \
	const autolist_entry_t AUTOLIST__CONCAT4(LIST_NAME, _, ITEM_NAME, _entry) = { \
		.name = #ITEM_NAME, \
		.name_length = sizeof(AUTOLIST__STRINGIFY(ITEM_NAME)) - 1, \
		.value_addr = (void*)&VAR_NAME, \
		.value_size = sizeof(VAR_NAME), \
	}; \
	AUTOLIST__SECTION_BEGIN(LIST_NAME) \
	const autolist_entry_t* const AUTOLIST__CONCAT4(LIST_NAME, _, ITEM_NAME, _info_ptr) = \
		&AUTOLIST__CONCAT4(LIST_NAME, _, ITEM_NAME, _entry); \
	AUTOLIST__SECTION_END(AUTOLIST__CONCAT4(LIST_NAME, _, ITEM_NAME, _info_ptr))

#define AUTOLIST_FOREACH(ITR, LIST_NAME) \
	for ( \
		const autolist_entry_t* const* autolist__itr = AUTOLIST_BEGIN(LIST_NAME); \
		autolist__itr != AUTOLIST_END(LIST_NAME); \
		++autolist__itr \
	) \
		for (const autolist_entry_t* ITR = *autolist__itr; ITR != NULL; ITR = NULL)

#define AUTOLIST__CONCAT3(A, B, C) AUTOLIST__CONCAT(AUTOLIST__CONCAT(A, B), C)
#define AUTOLIST__CONCAT4(A, B, C, D) AUTOLIST__CONCAT(AUTOLIST__CONCAT(A, B), AUTOLIST__CONCAT(C, D))
#define AUTOLIST__CONCAT(A, B) AUTOLIST__CONCAT_(A, B)
#define AUTOLIST__CONCAT_(A, B) A##B
#define AUTOLIST__STRINGIFY(X) AUTOLIST__STRINGIFY_(X)
#define AUTOLIST__STRINGIFY_(X) #X

#if defined(_MSC_VER)
#	define AUTOLIST__SECTION_BEGIN(NAME) \
	__pragma(data_seg(push)); \
	__pragma(section(AUTOLIST__STRINGIFY(AUTOLIST__CONCAT(NAME, $data)), read)); \
	__declspec(allocate(AUTOLIST__STRINGIFY(AUTOLIST__CONCAT(NAME, $data))))
#elif defined(__APPLE__)
#	define AUTOLIST__SECTION_BEGIN(NAME) __attribute__((retain, used, section("__DATA,autolist_" AUTOLIST__STRINGIFY(NAME))))
#elif defined(__unix__)
#	define AUTOLIST__SECTION_BEGIN(NAME) __attribute__((retain, used, section("autolist_" AUTOLIST__STRINGIFY(NAME))))
#else
#	error Unsupported compiler
#endif

#if defined(_MSC_VER)
#	define AUTOLIST__SECTION_END(INFO_PTR) \
	__pragma(data_seg(pop)); \
	__pragma(comment(linker, "/INCLUDE:" AUTOLIST__STRINGIFY(INFO_PTR)));
#elif defined(__APPLE__)
#	define AUTOLIST__SECTION_END(INFO_PTR)
#elif defined(__unix__)
#	define AUTOLIST__SECTION_END(INFO_PTR)
#endif

typedef struct {
	const char* name;
	size_t name_length;
	void* value_addr;
	size_t value_size;
} autolist_entry_t;

#if defined(_MSC_VER)
#	define AUTOLIST_DECLARE(NAME) \
	extern const autolist_entry_t* const AUTOLIST__CONCAT3(autolist_, NAME, _begin); \
	extern const autolist_entry_t* const AUTOLIST__CONCAT3(autolist_, NAME, _end);
#	define AUTOLIST_IMPL(NAME) \
	__pragma(section(AUTOLIST__STRINGIFY(AUTOLIST__CONCAT(NAME, $begin)), read)); \
	__pragma(section(AUTOLIST__STRINGIFY(AUTOLIST__CONCAT(NAME, $data)), read)); \
	__pragma(section(AUTOLIST__STRINGIFY(AUTOLIST__CONCAT(NAME, $end)), read)); \
	__declspec(allocate(AUTOLIST__STRINGIFY(AUTOLIST__CONCAT(NAME, $begin)))) \
		extern const autolist_entry_t* const AUTOLIST__CONCAT3(autolist_, NAME, _begin) = NULL; \
	__declspec(allocate(AUTOLIST__STRINGIFY(AUTOLIST__CONCAT(NAME, $end)))) \
		extern const autolist_entry_t* const AUTOLIST__CONCAT3(autolist_, NAME, _end) = NULL;
#elif defined(__APPLE__)
#	define AUTOLIST_DECLARE(NAME) \
	extern const autolist_entry_t* const AUTOLIST__CONCAT(__start_, NAME) \
	__asm("section$start$__DATA$autolist_" AUTOLIST__STRINGIFY(NAME)); \
	extern const autolist_entry_t* const AUTOLIST__CONCAT(__stop_, NAME) \
	__asm("section$end$__DATA$autolist_" AUTOLIST__STRINGIFY(NAME));
#	define AUTOLIST_IMPL(NAME) \
	__attribute__((retain, used, section("__DATA,autolist_" AUTOLIST__STRINGIFY(NAME)))) \
		const autolist_entry_t* const AUTOLIST__CONCAT3(autolist_, NAME, __dummy) = NULL;
#elif defined(__unix__)
#	define AUTOLIST_DECLARE(NAME) \
	extern const autolist_entry_t* const AUTOLIST__CONCAT(__start_autolist_, NAME); \
	extern const autolist_entry_t* const AUTOLIST__CONCAT(__stop_autolist_, NAME);
#	define AUTOLIST_IMPL(NAME) \
	__attribute__((retain, used, section("autolist_" AUTOLIST__STRINGIFY(NAME)))) \
		const autolist_entry_t* const AUTOLIST__CONCAT3(autolist_, NAME, __dummy) = NULL;
#endif

#define AUTOLIST_DEFINE(NAME) \
	AUTOLIST_DECLARE(NAME) \
	AUTOLIST_IMPL(NAME)

#if defined(_MSC_VER)
#	define AUTOLIST_BEGIN(NAME) (&AUTOLIST__CONCAT3(autolist_, NAME, _begin) + 1)
#	define AUTOLIST_END(NAME) (&AUTOLIST__CONCAT3(autolist_, NAME, _end))
#elif defined(__unix__) || defined(__APPLE__)
#	define AUTOLIST_BEGIN(NAME) (&AUTOLIST__CONCAT(__start_autolist_, NAME))
#	define AUTOLIST_END(NAME) (&AUTOLIST__CONCAT(__stop_autolist_, NAME))
#endif

#endif
