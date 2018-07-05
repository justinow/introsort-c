# introsort-c
Implementation of introsort in C99. Comprised of:
- Quicksort with explicit stack instead of recursion, and ignoring small partitions
- Binary heapsort with Floyd's optimisation, for stack depth > 2log2(n)
- Final shellsort pass on skipped small partitions (small gaps only)
