#ifndef CACHESIM_HPP
#define CACHESIM_HPP

#include <cinttypes>
#include <stdlib.h>
#include <string.h>

#define ADDRESS_SIZE 64
#define MAX_4BIT 16 

struct cache_stats_t {
    uint64_t accesses;
    uint64_t reads;
    uint64_t read_misses;
    uint64_t read_misses_combined;
    uint64_t writes;
    uint64_t write_misses;
    uint64_t write_misses_combined;
    uint64_t misses;
    uint64_t hit_time;
    uint64_t miss_penalty;
    double   miss_rate;
    double   avg_access_time;
    uint64_t storage_overhead;
    double   storage_overhead_ratio;
};

typedef struct clock {
	uint8_t time_lru;
	uint8_t time_other:4;
} logclock_t;

typedef struct cache_entry {
	uint64_t address;
	uint64_t tag;
	uint8_t dirty:1;
	uint8_t valid1:1;
	uint8_t valid2:1;
	logclock_t clock_data;
} cache_entry_t;

struct cache_t {
	uint64_t total_data_storage; //c
	char     block_type;         //st
	char     replacement_policy; //r
	uint64_t blocks_per_set;     //s
	uint64_t cacheline_size;     //b
	uint64_t victim_blocks;      //v
	// derived values
	uint64_t block_offset_size;
	uint64_t cache_line_overhead;
	uint64_t index_size;
	uint64_t tag_size;
	uint64_t total_storage;
	uint64_t total_overhead_bits;
	uint64_t total_sets;
	uint64_t tag_length;
	cache_entry_t **cache;
	cache_entry_t *victim_cache;
	uint64_t *nmru_reg;
};

void cache_access(char rw, uint64_t address, cache_stats_t* p_stats);
void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, char st, char r);
void complete_cache(cache_stats_t *p_stats);

static const uint64_t DEFAULT_C = 15;   /* 32KB Cache */
static const uint64_t DEFAULT_B = 5;    /* 32-byte blocks */
static const uint64_t DEFAULT_S = 3;    /* 8 blocks per set */
static const uint64_t DEFAULT_V = 2;    /* 4 victim blocks */

static const char     BLOCKING = 'B';
static const char     SUBBLOCKING = 'S';
static const char     DEFAULT_ST = BLOCKING;

static const char     LRU = 'L';
static const char     NMRU_FIFO = 'N';
static const char     DEFAULT_R = LRU;

/** Argument to cache_access rw. Indicates a load */
static const char     READ = 'r';
/** Argument to cache_access rw. Indicates a store */
static const char     WRITE = 'w';

#endif /* CACHESIM_HPP */
