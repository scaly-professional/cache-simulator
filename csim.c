/*
 * Eric Gan (ehgan)
 * @file csim.c
 * This file reads in traces from an input file with inputs for:
 * s: 2^s is the number of sets
 * E: number of lines per set
 * b: 2^b bytes per cache block
 * Code contains useful helper functions and is based on three structs that
 * separate the structure of a cache into chunks: Line: which contains the
 * relevant information per line Set: The array of lines Cache: The array of
 * sets
 */

// Include statements/libraries for use in code
#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Structures that break up a cache into sections
// Cache is an array of sets
// Set is array of lines
// Relevant information broken up into chunks for easy and understandable access
typedef struct {
    long dirtyBit;   /*dirty bit for if line has dirty bit set*/
    long tagBit;     /*tag bit for comparisons (hit or miss)*/
    long lruCounter; /*counter to keep track of LRU line*/
} Line;

typedef struct {
    long num;      /*number of lines being used in set*/
    long capacity; /*max number of lines possible in set*/
    Line **lines;  /*array of lines*/
} Set;

typedef struct {
    long s;         /*number of sets*/
    long E;         /*number of lines per set*/
    long opCounter; /*global op counter to keep track of LRU*/
    Set **sets;     /*array of sets*/
} Cache;

// Instantiation of initial line
Line *newLine(long dirtyBit, long tagBit, long lruCount) {
    Line *line = (Line *)malloc(sizeof(Line));
    line->dirtyBit = dirtyBit;
    line->tagBit = tagBit;
    line->lruCounter = lruCount;
    return line;
}

// Instantiation of initial set
Set *newSet(long capacity) {
    Set *set = (Set *)malloc(sizeof(Set));
    set->num = 0;
    set->capacity = capacity;
    set->lines = (Line **)malloc(capacity * sizeof(Line *));
    for (int i = 0; i < capacity; i++) {
        set->lines[i] = newLine(0, 0, 0);
    }
    return set;
}

// Instantiation of initial cache
Cache *createCache(long s, long E) {
    Cache *cache = (Cache *)malloc(sizeof(Cache));
    cache->s = (1L << s);
    cache->E = E;
    cache->opCounter = 0;
    cache->sets = (Set **)malloc((cache->s) * sizeof(Set *));
    for (int i = 0; i < cache->s; i++) {
        cache->sets[i] = newSet(E);
    }
    return cache;
}

// Simple check if set isn't using any lines (helps with understandability)
bool setEmpty(Set *set) {
    return (set->num == 0);
}

// Index of LRU line in set to know which line to evict
int indexLRU(Set *set) {
    long min = -1;
    int index = -1;
    // Iterate through all lines with valid bit set
    // find the line with min LRU counter
    for (int i = 0; i < set->num; i++) {
        if (min < 0) {
            min = set->lines[i]->lruCounter;
            index = i;
        } else {
            if ((set->lines[i]->lruCounter) < min) {
                min = set->lines[i]->lruCounter;
                index = i;
            }
        }
    }
    return index;
}

// Load or store call of line with tagBit and b
// Update results
// Load (dirtyBit = 0), Store (dirtyBit = 1)
void addToSet(csim_stats_t *results, Cache *cache, Set *set, long dirtyBit,
              long tagBit, long b) {
    bool isIn = false;
    // Separate into cases of empty set, set with space left in it, and full set
    // If no lines in use in set, add in the input line and update result
    if (setEmpty(set)) {
        free(set->lines[0]);
        set->lines[0] = newLine(dirtyBit, tagBit, cache->opCounter);
        set->num += 1;
        results->misses += 1;
        if (dirtyBit > 0) {
            results->dirty_bytes += (1L << b);
        }
    }
    // If there is space in set
    else if (set->num != set->capacity) {
        // Check if there is a hit, and update dirty bit if needed
        for (int i = 0; i < set->num; i++) {
            if (tagBit == set->lines[i]->tagBit) {
                set->lines[i]->lruCounter = cache->opCounter;
                if (set->lines[i]->dirtyBit == 0) {
                    if (dirtyBit > 0) {
                        set->lines[i]->dirtyBit = dirtyBit;
                        results->dirty_bytes += (1L << b);
                    }
                }
                isIn = true;
            }
        }
        if (isIn) {
            results->hits += 1;
        }
        // If miss, add into set
        else {
            free(set->lines[set->num]);
            set->lines[set->num] = newLine(dirtyBit, tagBit, cache->opCounter);
            results->misses += 1;
            if (dirtyBit > 0) {
                results->dirty_bytes += (1L << b);
            }
            set->num += 1;
        }
    }
    // If set is full
    // Possible need to address case of evict
    else {
        // Check if there is a hit, and update dirty bit if needed
        for (int i = 0; i < set->num; i++) {
            if (tagBit == set->lines[i]->tagBit) {
                set->lines[i]->lruCounter = cache->opCounter;
                if (set->lines[i]->dirtyBit == 0) {
                    if (dirtyBit > 0) {
                        set->lines[i]->dirtyBit = dirtyBit;
                        results->dirty_bytes += (1L << b);
                    }
                }
                isIn = true;
            }
        }
        if (isIn) {
            results->hits += 1;
        }
        // If not hit, have to deal with eviction case
        // Update results
        else {
            results->misses += 1;
            results->evictions += 1;
            int evictIndex = indexLRU(set);
            if (set->lines[evictIndex]->dirtyBit > 0) {
                results->dirty_bytes -= (1L << b);
                results->dirty_evictions += (1L << b);
            }
            if (dirtyBit > 0) {
                results->dirty_bytes += (1L << b);
            }
            set->lines[evictIndex]->dirtyBit = dirtyBit;
            set->lines[evictIndex]->tagBit = tagBit;
            set->lines[evictIndex]->lruCounter = cache->opCounter;
        }
    }
    cache->opCounter += 1;
}

int main(int argc, char *argv[]) {

    // Chars and variables needed to store inputs from getopt
    extern char *optarg;
    extern int optind, opterr, optopt;

    int verbose = 0;
    int s = 0;
    int E = 0;
    int b = 0;
    char *t = "";
    char arg;

    char *optstring = "vs:E:b:t:";

    // getopt to read in inputs
    while ((arg = getopt(argc, argv, optstring)) != -1) {
        switch (arg) {
        case 'v':
            verbose = 1;
            break;
        case 's':
            s = atof(optarg);
            break;
        case 'E':
            E = atof(optarg);
            break;
        case 'b':
            b = atof(optarg);
            break;
        case 't':
            t = optarg;
            break;
        default:
            exit(1);
        }
    }

    // Create results for analysis
    // Set everything initially to 0 and increment accordingly
    csim_stats_t *results = malloc(sizeof(csim_stats_t));
    results->hits = 0;
    results->misses = 0;
    results->evictions = 0;
    results->dirty_bytes = 0;
    results->dirty_evictions = 0;
    Cache *cache = createCache(s, E);
    FILE *trace = fopen(t, "r");
    if (trace == NULL) {
        printf("Could not open trace file");
        return 0;
    }
    char op;
    unsigned long addr;
    int size;

    // Reading in the trace files line by line
    while (fscanf(trace, "%c %lx, %d\n", &op, &addr, &size) > 0) {
        // Get the set bits and tag bits from the input file
        unsigned long address = addr;
        unsigned long set;
        // If s = 0,there is only one set, so there are no set bits
        // so we don't do bit manipulation to find the set, because there is
        // only one set anyway.
        if (s == 0) {
            set = 0;
        } else {
            set = address >> b << (64 - s) >> (64 - s);
        }
        unsigned long tag = address >> (b + s);

        // If the operation is Store, dirty bit is set
        // If load, don't worry about setting dirty bit
        if (op == 'S') {
            addToSet(results, cache, cache->sets[set], 1, tag, b);
        } else {
            addToSet(results, cache, cache->sets[set], 0, tag, b);
        }
    }
    fclose(trace);

    // verbose mode
    if (verbose == 1) {
        printf("Hits: %lx\n", results->hits);
        printf("Misses: %lx\n", results->misses);
        printf("Evictions: %lx\n", results->evictions);
        printf("Dirty Bytes: %lx\n", results->dirty_bytes);
        printf("Dirty Evictions: %lx\n", results->dirty_evictions);
    }
    printSummary(results);

    // Freeing malloc'd memory
    for (int i = 0; i < cache->s; i++) {
        for (int j = 0; j < cache->sets[i]->capacity; j++) {
            free(cache->sets[i]->lines[j]);
        }
        free(cache->sets[i]->lines);
        free(cache->sets[i]);
    }
    free(cache->sets);
    free(cache);
    free(results);
    return 0;
}
