/* $Id$ */

/** @file oldpool.h */

#ifndef OLDPOOL_H
#define OLDPOOL_H

/* The function that is called after a new block is added
     start_item is the first item of the new made block */
typedef void OldMemoryPoolNewBlock(uint start_item);
/* The function that is called before a block is cleaned up */
typedef void OldMemoryPoolCleanBlock(uint start_item, uint end_item);

/**
 * Stuff for dynamic vehicles. Use the wrappers to access the OldMemoryPool
 *  please try to avoid manual calls!
 */
struct OldMemoryPoolBase {
	void CleanPool();
	bool AddBlockToPool();
	bool AddBlockIfNeeded(uint index);

protected:
	OldMemoryPoolBase(const char *name, uint max_blocks, uint block_size_bits, uint item_size,
				OldMemoryPoolNewBlock *new_block_proc, OldMemoryPoolCleanBlock *clean_block_proc) :
		name(name), max_blocks(max_blocks), block_size_bits(block_size_bits), item_size(item_size),
		new_block_proc(new_block_proc), clean_block_proc(clean_block_proc), current_blocks(0),
		total_items(0), blocks(NULL) {}

	const char* name;     ///< Name of the pool (just for debugging)

	uint max_blocks;      ///< The max amount of blocks this pool can have
	uint block_size_bits; ///< The size of each block in bits
	uint item_size;       ///< How many bytes one block is

	/// Pointer to a function that is called after a new block is added
	OldMemoryPoolNewBlock *new_block_proc;
	/// Pointer to a function that is called to clean a block
	OldMemoryPoolCleanBlock *clean_block_proc;

	uint current_blocks;        ///< How many blocks we have in our pool
	uint total_items;           ///< How many items we now have in this pool

public:
	byte **blocks;              ///< An array of blocks (one block hold all the items)

	inline uint GetSize()
	{
		return this->total_items;
	}

	inline bool CanAllocateMoreBlocks()
	{
		return this->current_blocks < this->max_blocks;
	}

	inline uint GetBlockCount()
	{
		return this->current_blocks;
	}
};

template <typename T>
struct OldMemoryPool : public OldMemoryPoolBase {
	OldMemoryPool(const char *name, uint max_blocks, uint block_size_bits, uint item_size,
				OldMemoryPoolNewBlock *new_block_proc, OldMemoryPoolCleanBlock *clean_block_proc) :
		OldMemoryPoolBase(name, max_blocks, block_size_bits, item_size, new_block_proc, clean_block_proc) {}

	inline T *Get(uint index)
	{
		assert(index < this->GetSize());
		return (T*)(this->blocks[index >> this->block_size_bits] +
				(index & ((1 << this->block_size_bits) - 1)) * sizeof(T));
	}
};

/**
 * Those are the wrappers:
 *   CleanPool cleans the pool up, but you can use AddBlockToPool directly again
 *     (no need to call CreatePool!)
 *   AddBlockToPool adds 1 more block to the pool. Returns false if there is no
 *     more room
 */
static inline void CleanPool(OldMemoryPoolBase *array) { array->CleanPool(); }
static inline bool AddBlockToPool(OldMemoryPoolBase *array) { return array->AddBlockToPool(); }

/**
 * Adds blocks to the pool if needed (and possible) till index fits inside the pool
 *
 * @return Returns false if adding failed
 */
static inline bool AddBlockIfNeeded(OldMemoryPoolBase *array, uint index) { return array->AddBlockIfNeeded(index); }


/**
 * Generic function to initialize a new block in a pool.
 * @param start_item the first item that needs to be initialized
 */
template <typename T, OldMemoryPool<T> *Tpool>
static void PoolNewBlock(uint start_item)
{
	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 *  TODO - This is just a temporary stage, this will be removed. */
	for (T *t = Tpool->Get(start_item); t != NULL; t = (t->index + 1U < Tpool->GetSize()) ? Tpool->Get(t->index + 1U) : NULL) {
		t->index = start_item++;
		t->PreInit();
	}
}

/**
 * Generic function to free a new block in a pool.
 * This function uses QuickFree that is intended to only free memory that would be lost if the pool is freed.
 * @param start_item the first item that needs to be cleaned
 * @param end_item   the last item that needs to be cleaned
 */
template <typename T, OldMemoryPool<T> *Tpool>
static void PoolCleanBlock(uint start_item, uint end_item)
{
	for (uint i = start_item; i <= end_item; i++) {
		T *t = Tpool->Get(i);
		if (t->IsValid()) {
			t->QuickFree();
		}
	}
}



#define OLD_POOL_ENUM(name, type, block_size_bits, max_blocks) \
	enum { \
		name##_POOL_BLOCK_SIZE_BITS = block_size_bits, \
		name##_POOL_MAX_BLOCKS      = max_blocks \
	};


#define OLD_POOL_ACCESSORS(name, type) \
	static inline type* Get##name(uint index) { return _##name##_pool.Get(index);  } \
	static inline uint Get##name##PoolSize()  { return _##name##_pool.GetSize(); }


#define DECLARE_OLD_POOL(name, type, block_size_bits, max_blocks) \
	OLD_POOL_ENUM(name, type, block_size_bits, max_blocks) \
	extern OldMemoryPool<type> _##name##_pool; \
	OLD_POOL_ACCESSORS(name, type)


#define DEFINE_OLD_POOL(name, type, new_block_proc, clean_block_proc) \
	OldMemoryPool<type> _##name##_pool( \
		#name, name##_POOL_MAX_BLOCKS, name##_POOL_BLOCK_SIZE_BITS, sizeof(type), \
		new_block_proc, clean_block_proc);

#define DEFINE_OLD_POOL_GENERIC(name, type) \
	OldMemoryPool<type> _##name##_pool( \
		#name, name##_POOL_MAX_BLOCKS, name##_POOL_BLOCK_SIZE_BITS, sizeof(type), \
		PoolNewBlock<type, &_##name##_pool>, PoolCleanBlock<type, &_##name##_pool>);


#define STATIC_OLD_POOL(name, type, block_size_bits, max_blocks, new_block_proc, clean_block_proc) \
	OLD_POOL_ENUM(name, type, block_size_bits, max_blocks) \
	static DEFINE_OLD_POOL(name, type, new_block_proc, clean_block_proc) \
	OLD_POOL_ACCESSORS(name, type)

#endif /* OLDPOOL_H */
