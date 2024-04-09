#include "resources.h"
#include <stdio.h>

int main(int argc, const char* argv[]) {
	xincbin_data_t embedded = XINCBIN_GET(embedded);
	printf("%.*s\n", embedded.size, embedded.data);
	return 0;
}
