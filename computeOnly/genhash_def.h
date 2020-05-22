

// FilePositionIndex (A byte index into a small array of blocks)
typedef uint16_t FPI;

#ifdef _MSC_VER
#  pragma pack (push, 1)
#endif

PREFIX_PACKED struct key_data_entry
{
	// the offset MAY be aligned for larger integer comparisons.
	//FPI key_data_offset;  // small integer, max of N directoryEntries * maxKeyLength bytes.
	//                      // this offset is applied to flower_hash_lookup_block.key_data_first_block

	uint16_t stored_data; // the data this results with; some user pointer sized value.
#ifdef FLOWER_HASH_ENABLE_LIVE_USER_DATA
	uintptr_t** user_data_; // Store the user's reference so it can be updated when this entry moves.
#endif
	// a disk storage system might store the start of the data block and also
        // the size of the data block.  
	//uintptr_t first_block;  // first block of data of the file
	//VFS_DISK_DATATYPE filesize;  // how big the file is
} PACKED;


// Current configuration has 237 entries and is 4084 byes long.
// that leads to 128 bits to track free space   (64+32+16+8+4+2+1) 127
PREFIX_PACKED struct flower_hash_lookup_block
{
	// pointers to more data, indexed by hashing the key with
	// low byte & (HASH_MASK-1).
	struct flower_hash_lookup_block*  next_block[HASH_MASK+1];
	// need a entry that is the parent; to know the depth of 'here'...(could be a few depth bits?)
	struct flower_hash_lookup_block* parent;
	uintmax_t baseOffset;

	// specify buffer from outside, and just return index into existing buffer.
	// if an external buffer is specified, the base pointer is stored in the root hash node
	// as 'entries'.
	// This represents the maximum count that can be allocated from that buffer.
	// internal entries_ stores the allocated index into this.
	uintptr_t userData;  
	// size_t userDataSize;  

	// this is the user data in the structure.
	struct key_data_entry            *entries;

	// this is used to track nodes that are full, for tracking free locations.
	FLAGSET( used_key_data, 1<<(KEY_DATA_ENTRIES_BITS-1) );

	// on delete (which would imply reuse of internal space ability)
	// this tracks entries which have been free'd before usedEntries counter.
	FLAGSET( freeEntries, KEY_DATA_ENTRIES );

	// -- this could represent the above data in just a few bits...
	struct smallDataFields{
		// keeping the last stored key value is faster than scanning to the end; even though the entry count is only hundreds.
		// this is an offset into key_data_first_block; it is only good for +65535 byes. (237 x 276-byte keys)
		unsigned used_key_data : 16;
		// this is the size of data tracked in entries[]
		unsigned userRecordSize : 16;
		// this allocates existing entries.
		unsigned usedEntries : 12; 
		// this is the count of how many key data blocks are allocated together...
		// (16 * KEY_BLOCK_SIZE) max key data
		unsigned key_data_blocks : 4;
		// immutable affects the insertion behavior, such that when no space is found in the entry tree
		// entries will not be moved in order to make room.
		unsigned immutable : 1;
		// When a directory block is entirely full, and convertible is enabled,
		// a new directory block will be created, and linked to the existing hash block's
		// hash talbe for the most commonly used value.  All of the entries matching that value
		// will be moved into the new block; making new entries available in the parent block.
		// If immutable and convertible are specified, the conversion will happen on a collision
		// which will modify entires in the existin block. 
		unsigned  convertible : 1;
		// This flag affects the search order, whether the hash table or key entries in this block
		// are read first.  
		// If the hash table is read first, convertible must also be used, otherwise entries
		// 
		unsigned  dense : 1;
		// this interprets baseOffset as a function pointer to be called later...
		// 
		unsigned  externalAllocator : 1;
	} info;

	// these are values to be checked in this node.
	FPI                               key_data_offset[KEY_DATA_ENTRIES];
	// store key data...
	uint8_t* key_data_first_block;
	// could alternatively concat key data to this block.
	//uint8_t key_data[];
	struct key_data_entry entries_[1];// internal storage, when external is not specified.
} PACKED;

struct key_data_block {
	uint8_t data[BLOCK_SIZE];
} PACKED;

#  ifdef _MSC_VER
#    pragma pack (pop)
#  endif
