#include "resources.h"
#include <stdio.h>
#include <string.h>

int main(int argc, const char* argv[]) {
	xincbin_data_t embedded = XINCBIN_GET(embedded);
	printf("%.*s\n", embedded.size, embedded.data);
	// Resources are implicitly null-terminated without counting the
	// terminator in the size
	if (embedded.data[embedded.size] != 0) {
		fprintf(stderr, "Resource is not null-terminated\n");
		return 1;
	}
	if (strlen((const char*)embedded.data) != embedded.size) {
		fprintf(stderr, "strlen does not match size\n");
		return 1;
	}
	// Repeated retrievals return the same resource
	xincbin_data_t embedded2 = XINCBIN_GET(embedded);
	if (embedded2.data != embedded.data || embedded2.size != embedded.size) {
		fprintf(stderr, "Repeated retrieval returns a different result\n");
		return 1;
	}
	return 0;
}
