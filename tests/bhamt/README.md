# bhamt

Generic type-safe hash trie: an arena-friendly associative map.

Based on: https://nullprogram.com/blog/2023/09/30/

Compared to [bhash](../bhash), a zero-initialized table is valid,
entry pointers are stable, and there is no resizing or removal.
It is designed for arena allocation: symbol tables, scoped environments,
deduplication and interning.
