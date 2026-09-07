#!/bin/sh
# Doxygen input filter for markdown: turn [x.h](x.h) into bare x.h so
# Doxygen auto-links documented headers to their page and leaves the
# rest as plain text (see FILTER_PATTERNS in Doxyfile).
sed -E 's/\[([A-Za-z0-9_]+\.h)\]\(\1\)/\1/g' "$1"
