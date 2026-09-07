#include "resources.h"
#include "../../btest.h"
#include <string.h>

static btest_suite_t xincbin_ = {
	.name = "xincbin",
};

BTEST(xincbin_, embedded_text) {
	xincbin_data_t embedded = XINCBIN_GET(embedded);
	BTEST_ASSERT(embedded.data != NULL);
	BTEST_ASSERT(embedded.size > 0);
	BTEST_EXPECT(strncmp((const char*)embedded.data, "Lorem ipsum", 11) == 0);

	// Resources are implicitly null-terminated without counting the
	// terminator in the size
	BTEST_EXPECT_EQUAL("%d", embedded.data[embedded.size], 0);
	BTEST_EXPECT_EQUAL("%zu", strlen((const char*)embedded.data), (size_t)embedded.size);
}

BTEST(xincbin_, repeated_retrieval) {
	// Repeated retrievals return the same resource
	xincbin_data_t embedded = XINCBIN_GET(embedded);
	xincbin_data_t embedded2 = XINCBIN_GET(embedded);
	BTEST_EXPECT(embedded2.data == embedded.data);
	BTEST_EXPECT_EQUAL("%u", embedded2.size, embedded.size);
}
