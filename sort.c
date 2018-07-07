/**
 * Stable sort
 * Uses introsort, comprised of:
 * - Quicksort with explicit stack instead of recursion, and ignoring small partitions
 * - Binary heapsort with Floyd's optimisation, for stack depth > 2log2(n)
 * - Final shellsort pass on skipped small partitions (small gaps only)
 */

#include "sort.h"

// typedef comparator function pointer
typedef int (*comparator_fn_t)(const void *, const void*);

// Real men use manual stacks
typedef struct StackNode { char *low, *high; } StackNode;

// Defines:
// - manual indexing
// - stack size constant
// - stack empty condition (for readability)
#define index(x)        (x) * data_size
#define STACK_SIZE      (sizeof(size_t) * CHAR_BIT)
#define STACK_NOT_EMPTY (stack < top)

// integer log base 2
#if defined __GNUC__

static inline int _log2(size_t x) { return 63 - __builtin_clzll(x); }

#elif defined _WIN32

static inline int _log2(size_t x)
{
    int i = 0;
     _BitScanReverse(&i, x);
    return i;
}

#elif defined _WIN64

static inline int _log2(size_t x)
{
    int i = 0;
    _BitScanReverse64(&i, x);
    return i;
}

#endif

#define WORD_ALIGNED 1
static inline void swap(void *restrict _a, void *restrict _b, size_t data_size)
{
#if WORD_ALIGNED
    long *a = (long *)_a;
    long *b = (long *)_b;
    
    while((data_size--) / sizeof(long) > 0)
    {
        long temp = *a;
        *a++ = *b;
        *b++ = temp;
    }
    
#else
    char *a = (char *)_a;
    char *b = (char *)_b;
    
    while(data_size --> 0)
    {
        char temp = *a;
        *a++ = *b;
        *b++ = temp;
    }
#endif
}

extern void sort(void *_array, size_t length, size_t data_size, comparator_fn_t comparator)
{
    if(length == 0) return;
    
    char *array = (char *)_array;
    const size_t max_thresh = data_size << 2;
    const int max_depth = _log2(length) << 1;
    
    // Temporary storage used by both heapsort and shellsort
    // MSVC doesn't support VLAs - use malloca instead
    #if WINDOWS
    void *temp = _malloca(data_size);
    #else
    char temp[data_size];
    #endif
    
    if(length > 16)
    {
        char *low = array, *high = array + index(length - 1);
        StackNode stack[STACK_SIZE] = { 0 };
        StackNode *top = stack + 1;

        int depth = 0;
        while(STACK_NOT_EMPTY)
        {
            // Exceeded max depth: do heapsort on this partition
            if(depth > max_depth)
            {
                size_t part_length = (size_t)((high - low) / data_size) - 1;
                if(part_length > 0)
                {
                    size_t i, j, k = part_length >> 1;
                    
                    // heapification
                    do
                    {
                        i = k;
                        j = (i << 1) + 2;
                        memcpy(temp, low + index(i), data_size);
                        
                        while(j <= part_length)
                        {
                            if(j < part_length) j += (comparator(low + index(j), low + index(j + 1)) < 0);
                            if(comparator(low + index(j), temp) <= 0) break;
                            memcpy(low + index(i), low + index(j), data_size);
                            i = j;
                            j = (i << 1) + 2;
                        }
                        
                        memcpy(low + index(i), temp, data_size);
                    }
                    while(k --> 0);
                    
                    // heapsort actual
                    do
                    {
                        i = part_length;
                        j = 0;
                        memcpy(temp, low + index(part_length), data_size);

                        // Floyd's optimisation:
                        // Not checking low[j] <= temp saves nlog2(n) comparisons
                        while(j < part_length)
                        {
                            if(j < part_length - 1) j += (comparator(low + index(j), low + index(j + 1)) < 0);
                            memcpy(low + index(i), low + index(j), data_size);
                            i = j;
                            j = (i << 1) + 2; 
                        }

                        // Compensate for Floyd's optimisation by sifting down temp
                        // This adds O(n) comparisons and moves
                        while(i > 1)
                        {
                            j = (i - 2) >> 1;
                            if(comparator(temp, low + index(j)) <= 0) break;
                            memcpy(low + index(i), low + index(j), data_size);
                            i = j;
                        }

                        memcpy(low + index(i), temp, data_size);
                    }
                    while(part_length --> 0);
                }
                
                // pop next partition from stack
                --top;
                --depth;
                low = top->low;
                high = top->high;
				continue;
            }
            
            // 3-way "Dutch national flag" partition
            char *mid = low + data_size * ((high - low) / data_size >> 1);
            if(comparator(mid, low) < 0)  swap(mid, low,  data_size);
            if(comparator(mid, high) > 0) swap(mid, high, data_size);
            else goto skip;
            if(comparator(mid, low) < 0)  swap(mid, low,  data_size);

            skip:;
            char *left  = low  + data_size, *right = high - data_size;
            
            // sort this partition
            do
            {
                while(comparator(left,  mid) < 0)  left += data_size;
                while(comparator(mid, right) < 0) right -= data_size;

                if(left < right)
                {
                    swap(left, right, data_size);
                    if(mid == left) mid = right;
                    else if(mid == right) mid = left;
                    left  += data_size;
                    right -= data_size;
                }
                else if(left == right)
                {
                    left  += data_size;
                    right -= data_size;
                    break;
                }
            }
            while(left <= right);

            // Prepare the next iteration
            // Push larger partition and sort the other; unless one or both
            // smaller than threshold, then leave it to the final shellsort.
            //
            // Both below threshold
            if((size_t)(right - low) <= max_thresh && (size_t)(high - left) <= max_thresh)
            {
                --top;
                --depth;
                low  = top->low;
                high = top->high;
            }
            
            // Left below threshold
            else if((size_t)(right - low) <= max_thresh && (size_t)(high - left) > max_thresh)
            {
                low = left;
            }
            
            // Right below threshold
            else if((size_t)(right - low) > max_thresh && (size_t)(high - left) <= max_thresh)
            {
                high = right;
            }
            
            // Both above threshold
            else
            {
                // Push big left, sort smaller right
                if((right - low) > (high - left))
                {
                    top->low  = low;
                    top->high = right;
                    low = left;
                    ++top;
                    ++depth;
                }
                    
                // Push big right, sort smaller left
                else
                {
                    top->low  = left;
                    top->high = high;
                    high = right;
                    ++top;
                    ++depth;
                }
            }
        }
    }
    
    // Clean up the leftovers with shellsort
    // Already mostly sorted; use only small gaps
    const size_t gaps[2] = { 1ul, 4ul };
    
    int i = 1;
    do
    {   
        if(length < 4) continue;
        
        for(size_t j = gaps[i], k = j; j < length; k = ++j)
        {
            memcpy(temp, array + index(k), data_size);
            
            while(k >= gaps[i] && comparator(array + index(k - gaps[i]), temp) > 0)
            {
                memcpy(array + index(k), array + index(k - gaps[i]), data_size);
                k -= gaps[i];
            }
            
            memcpy(array + index(k), temp, data_size);
        }
    }
    while(i --> 0);
    
    #if WINDOWS
    _freea(temp);
    #endif
}
