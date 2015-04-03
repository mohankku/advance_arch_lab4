#include "cachesim.hpp"

static cache_t cache_metadata;
static uint64_t logical_clock;

/**
 * Subroutine for initializing the cache. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @c The total number of bytes for data storage is 2^C
 * @b The size of a single cache line in bytes is 2^B
 * @s The number of blocks in each set is 2^S
 * @v The number of blocks in the victim cache is 2^V
 * @st The storage policy, BLOCKING or SUBBLOCKING (refer to project description for details)
 * @r The replacement policy, LRU or NMRU_FIFO (refer to project description for details)
 */
void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, char st, char r) {
	uint64_t overhead_bits = 0;
	uint64_t i;

	// convert the inputs to actual size
	cache_metadata.total_data_storage = 1 << c;
	cache_metadata.block_type = st;
	cache_metadata.replacement_policy = r;
	cache_metadata.blocks_per_set = 1 << s;
	cache_metadata.cacheline_size = 1 << b;
	cache_metadata.victim_blocks = 1 << v;

	cache_metadata.total_sets =  (1 << (c - b - s));

	cache_metadata.block_offset_size = b;
	cache_metadata.index_size = (c - b - s);
	cache_metadata.tag_size = ADDRESS_SIZE - (b + (c - b - s));

	// calculate the overheads and derived values
	// add the dirty bit to overhead	
	overhead_bits++;
	overhead_bits += ((cache_metadata.block_type == BLOCKING) ? 1 : 2);
	overhead_bits += ((cache_metadata.replacement_policy == LRU) ? 8 : 4);
	overhead_bits += cache_metadata.tag_size;
	cache_metadata.cache_line_overhead = overhead_bits/8;
	overhead_bits = overhead_bits *
		            (cache_metadata.total_data_storage / cache_metadata.cacheline_size);

	// convert to bytes
	overhead_bits = overhead_bits / 8;
	cache_metadata.total_storage = cache_metadata.total_data_storage + overhead_bits;

	cache_metadata.cache = (cache_entry_t **) malloc(sizeof(cache_entry_t *) * cache_metadata.total_sets);

	for (i=0; i<cache_metadata.total_sets; i++) {
		cache_metadata.cache[i] = (cache_entry_t *) malloc((sizeof(cache_entry_t)) *
				                          cache_metadata.blocks_per_set);
		memset(cache_metadata.cache[i], 0, (sizeof(cache_entry_t) * cache_metadata.blocks_per_set));
	}

	cache_metadata.victim_cache = (cache_entry_t *) malloc(((sizeof(cache_entry_t)) *
				                           cache_metadata.victim_blocks));
	memset(cache_metadata.victim_cache, 0, (sizeof(cache_entry_t) * cache_metadata.victim_blocks));

	if (cache_metadata.block_type == NRMU) {
		cache_metadata.nrmu_reg = (uint64_t *) malloc(sizeof(uint64_t) * cache_metadata.total_sets);
	}

	for (i=0; i<cache_metadata.total_sets; i++) {
		cache_metadata.nrmu_reg[i] = -1;
	}
}

uint64_t victim_to_update () {
	uint64_t entry, lru = 0, lru_entry = 0;

	// look for LRU
	for (entry=0; entry<cache_metadata.victim_blocks; entry++) {
		if (!cache_metadata.victim_cache[entry]->valid1) {
			return entry;
		}
		if (lru > cache_metadata.victim_cache[entry]->clock_data.time_lru) {
			lru = cache_metadata.victim_cache[entry]->clock_data.time_lru;
			lru_entry = entry;
		}
	}
	return lru_entry;
}

uint64_t lru_entry_to_update (uint64_t index) {
	uint64_t entry, lru = 0, lru_entry = 0;

	// look for LRU
	for (entry=0; entry<cache_metadata.blocks_per_set; entry++) {
		if (cache_metadata.block_type == BLOCKING) {
			if (!cache_metadata.cache[index+entry]->valid1) {
				return entry;
			}
		} else {
			// subblocking
			if ((!cache_metadata.cache[index+entry]->valid1) &&
			    (!cache_metadata.cache[index+entry]->valid2)) {
				return entry;
			}
		}
		if (lru > cache_metadata.cache[index+entry]->clock_data.time_lru) {
			lru = cache_metadata.cache[index+entry]->clock_data.time_lru;
			lru_entry = entry;
		}
	}
	return lru_entry;
}

uint64_t nmru_entry_to_update (uint64_t index) {
	uint64_t entry, lru = 0, lru_entry = 0;
	uint64_t found = 0;

	// look for NRMU
	for (entry=0; entry<cache_metadata.blocks_per_set; entry++) {
		if (cache_metadata.block_type == BLOCKING) {
			if (!cache_metadata.cache[index+entry]->valid1) {
				return entry;
			}
		} else {
			// subblocking
			if ((!cache_metadata.cache[index+entry]->valid1) &&
			    (!cache_metadata.cache[index+entry]->valid2)) {
				return entry;
			}
		}
		if ((!found) && (cache_metadata.nrmu_reg[index] != entry) {
			found = 1;
			lru_entry = entry;
		}
	}
	return lru_entry;
}

uint64_t nmru_push_entry (uint64_t index, uint64_t entry) {
	uint64_t i;
	cache_entry_t temp;

	for (i = entry; entry<cache_metadata.blocks_per_set-1; i++) {
			*cache_metadata.cache[index+i] = *cache_metadata.cache[index+i+1];
			if (cache_metadata.cache[index+i+1]->valid1) {
				cache_metadata.cache[index+i+1]->valid1 = 0;
				if (cache_metadata.nrmu_reg[index] == i+1) {
					cache_metadata.nrmu_reg[index] = i;
				}
			} else {
			    return i+1;
			}
	}
	return i;
}

void read_write(char rw, uint64_t address, cache_stats_t* p_stats, uint64_t tag, 
				uint64_t index, uint64_t block_offset) {
	uint64_t i, entry_to_evict, vict_entry, temp_tag;
	cache_entry_t temp;
	uint64_t vict_tag = address >> (cache_metadata.block_offset_size);
	uint8_t found = 0;

	++logical_clock;

	// First search in the cache. If found, return.
	for (i=0; i<cache_metadata.blocks_per_set; i++) {
		if (cache_metadata.block_type == BLOCKING) {
			if ((cache_metadata.cache[index+i]->tag == tag) &&
				(cache_metadata.cache[index+i]->valid1)) {
				found = 1;
			}
		} else {
			// sub blocking
			if (block_offset < (cache_metadata.cacheline_size/2)) {
				if ((cache_metadata.cache[index+i]->tag == tag) &&
					(cache_metadata.cache[index+i]->valid1)) {
					found = 1;
				}
			} else if (block_offset >= (cache_metadata.cacheline_size/2)) {
				if ((cache_metadata.cache[index+i]->tag == tag) &&
					(cache_metadata.cache[index+i]->valid2)) {
					found = 1;
				}
			}
		}
		if (found) {
			// data found. update Stats and return.
			if (cache_metadata.replacement_policy == LRU) {
				cache_metadata.cache[index+i]->clock_data.time_lru = logical_clock;
			} else {
				cache_metadata.nrmu_reg[index] = i;
			}
			if (rw == WRITE) {
				cache_metadata.cache[index+i]->dirty = 1;
			}
			return;
		}
	}

	if (cache_metadata.replacement_policy == LRU) {
		entry_to_evict = lru_entry_to_update(index);
	} else {
		// NRMU FIFO
		entry_to_evict = nmru_entry_to_update(index);
	}

	// data not found in cache. Look in victim cache
	for (i=0; i<cache_metadata.victim_blocks; i++) {
		if (cache_metadata.block_type == BLOCKING) {
			if ((cache_metadata.victim_cache[i]->tag == vict_tag) &&
				(cache_metadata.cache[i]->valid1)) {
				found = 1;
			}
		} else {
			// sub blocking
			if (block_offset < (cache_metadata.cacheline_size/2)) {
				if ((cache_metadata.cache[index+i]->tag == tag) &&
					(cache_metadata.cache[index+i]->valid1)) {
					found = 1;
				} else {
					if ((cache_metadata.cache[index+i]->tag == tag) &&
						(cache_metadata.cache[index+i]->valid2)) {
						found_other_half = 1;
					}
				}
			} else if (block_offset >= (cache_metadata.cacheline_size/2)) {
				if ((cache_metadata.cache[index+i]->tag == tag) &&
					(cache_metadata.cache[index+i]->valid2)) {
					found = 1;
				} else {
					if ((cache_metadata.cache[index+i]->tag == tag) &&
						(cache_metadata.cache[index+i]->valid1)) {
						found_other_half = 1;
					}
				}
			}
		}
		if (found || found_other_half) {
			// found entry. swap entry
			temp = *cache_metadata.cache[index+entry_to_evict];
			if (cache_metadata.replacement_policy != LRU) {
				entry_to_evict = nrmu_push_entry(index, entry_to_evict);
			}
			*cache_metadata.cache[index+entry_to_evict] = cache_metadata.victim.cache[i];
			cache_metadata.cache[index+entry_to_evict]->tag = tag;
			cache_metadata.victim.cache[i] = temp;
			temp_tag = temp.address >> (cache_metadata.block_offset_size);
			cache_metadata.victim.cache[i].tag = temp_tag;

			if (cache_metadata.replacement_policy == LRU) {
				cache_metadata.cache[index+entry_to_evict]->clock_data.time_lru = logical_clock;
			} else {
				cache_metadata.nrmu_reg[index] = entry_to_evict;
			}
			if (found_other_half) {
				//load the other half from memory and mark both valid :)
				cache_metadata.cache[index+entry_to_evict]->valid1 = 1;
				cache_metadata.cache[index+entry_to_evict]->valid2 = 1;

			}
			if (rw == WRITE) {
				cache_metadata.cache[index+entry_to_evict]->dirty = 1;
			}
			return;
		}
	}

	// not in victim cache too. Move entry to victim cache first.
	vict_entry = victim_to_update();
	temp = *cache_metadata.cache[index+entry_to_evict];

	if (cache_metadata.replacement_policy != LRU) {
		entry_to_evict = nrmu_push_entry(index, entry_to_evict);
	}

	// evict victim, need to write to memory if dirty
	cache_metadata.victim.cache[vict_entry] = temp;
	temp_tag = temp.address >> (cache_metadata.block_offset_size);
	cache_metadata.victim.cache[i].tag = temp_tag;

	// update the cache entry now
	cache_metadata.cache[index+entry_to_evict]->address = address;
	cache_metadata.cache[index+entry_to_evict]->tag = tag;

	if (cache_metadata.block_type == BLOCKING) {
		cache_metadata.cache[index+entry_to_evict]->valid1 = 1;
	} else {
		if (block_offset < (cache_metadata.cacheline_size/2)) {
			cache_metadata.cache[index+entry_to_evict]->valid1 = 1;
		} else {
			cache_metadata.cache[index+entry_to_evict]->valid2 = 1;
		}
	}

	if (rw == WRITE) {
		cache_metadata.cache[index+entry_to_evict]->dirty = 1;
	} else {
		cache_metadata.cache[index+entry_to_evict]->dirty = 0;
	}

	if (cache_metadata.replacement_policy == LRU) {
		cache_metadata.cache[index+entry_to_evict]->clock_data.time_lru = logical_clock;
	} else {
		cache_metadata.nrmu_reg[index] = entry_to_evict;
	}
}

/**
 * Subroutine that simulates the cache one trace event at a time.
 * XXX: You're responsible for completing this routine
 *
 * @rw The type of event. Either READ or WRITE
 * @address  The target memory address
 * @p_stats Pointer to the statistics structure
 */
void cache_access(char rw, uint64_t address, cache_stats_t* p_stats) {
	uint64_t block_offset;
	uint64_t index;
	uint64_t tag;
	uint64_t i;

	tag = address >> (cache_metadata.block_offset_size + cache_metadata.index_size);

	index = ((1 << (cache_metadata.index_size + 1)) - 1);
	index = index << cache_metadata.block_offset_size;
	index = address & index;
	index = index >> cache_metadata.block_offset_size;

	block_offset = ((1 << (cache_metadata.block_offset_size + 1)) - 1);
	block_offset = address & block_offset;

	// retrieve block Offset, Index and Tag from the address
	read_write(rw, address, p_stats, tag, index, block_offset);
}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_cache(cache_stats_t *p_stats) {
}
