#ifndef BMACRO_H
#define BMACRO_H

#define BCONTAINER_OF(ptr, type, member) \
	((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))

#define BCOUNT_OF(X) (sizeof(X) / sizeof(X[0]))

#endif
