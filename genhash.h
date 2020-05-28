//#define USE_SACK_PORTABILITY
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#endif

#if defined( USE_SACK_PORTABILITY )
#  include "sack.h"

#  define fa(b)   Release(b)
#  define aa(a,s) HeapAllocateAligned(NULL,s,a)

#else
#if __GNUC__
#  define __USE_ISOC11
#    ifndef _WIN32
#      include <features.h>
#    endif
#  endif
#  define _ISOC11_SOURCE
#  include <stdio.h>
#  include <stdlib.h>
#  include <stdint.h>
#  include <string.h>
#  include <malloc.h>
#  include <limits.h>
#  ifdef _WIN32
#    include <Windows.h>
#  else
//   these are aserts that should never happen.
#    if defined( __EMSCRIPTEN__ ) || defined( __ARM__ )
#      define DebugBreak()
#    else
#      define DebugBreak()  __asm__("int $3\n" )
#    endif
#  endif

//----------- Static Includes ---------------------------------------
// included from github.com/d3x0r/sack include/sack_types.h
// is really all from the amalgamtate, so from various include/*.h

// define support for various compiler's packed structure specifications

// and MSC_VER requires a #pragma packed around the structures..
#  define PREFIX_PACKED
#  ifdef __WATCOMC__
#    undef PREFIX_PACKED
#    define PREFIX_PACKED _Packed
#  endif
#  if defined( __GNUC__ )
#    define PACKED __attribute__((packed))
#  else
#    define PACKED
#  endif

#if defined( _WIN32 )
#  define aa(a,s)  _aligned_malloc( s,a )
#  define fa(b)    _aligned_free(b)
#else
#  define fa(b)    free(b)
#  ifdef _ISOC11_SOURCE
#    define aa(a,s)  aligned_alloc(a,s)
#  else
#    error no definition for aligned_alloc or _alligned_malloc could be ciphered.
#  endif
#endif

#ifdef New
#  undef New
#  undef NewPlus
#endif

#define NewPlus(type,extra)   ((type*)aa( 4096,(sizeof(type)+((extra)+4095))&~4095))
#define New(type)              (type*)aa( 4096,(sizeof(type)+4095)&~4095)

//----------- FLAG SETS (single bit fields) -----------------
/* the default type to use for flag sets - flag sets are arrays of bits
 which can be set/read with/as integer values an index.
 All of the fields in a maskset are the same width */
#  define FLAGSETTYPE uintmax_t
 /* the number of bits a specific type is.
	Example
	int bit_size_int = FLAGTYPEBITS( int ); */
#  define FLAGTYPEBITS(t) (sizeof(t)*CHAR_BIT)
	/* how many bits to add to make sure we round to the next greater index if even 1 bit overflows */
#  define FLAGROUND(t) (FLAGTYPEBITS(t)-1)
/* the index of the FLAGSETTYPE which contains the bit in question */
#  define FLAGTYPE_INDEX(t,n)  (((n)+FLAGROUND(t))/FLAGTYPEBITS(t))
/* how big the flag set is in count of FLAGSETTYPEs required in a row ( size of the array of FLAGSETTYPE that contains n bits) */
#  define FLAGSETSIZE(t,n) (FLAGTYPE_INDEX(t,n) * sizeof( FLAGSETTYPE ) )
// declare a set of flags...
#  define FLAGSET(v,n)   FLAGSETTYPE (v)[((n)+FLAGROUND(FLAGSETTYPE))/FLAGTYPEBITS(FLAGSETTYPE)]
// set a single flag index
#  define SETFLAG(v,n)   ( ( (v)[(n)/FLAGTYPEBITS((v)[0])] |= (FLAGSETTYPE)1 << ( (n) & FLAGROUND((v)[0]) )),1)
// clear a single flag index
#  define RESETFLAG(v,n) ( ( (v)[(n)/FLAGTYPEBITS((v)[0])] &= ~( (FLAGSETTYPE)1 << ( (n) & FLAGROUND((v)[0]) ) ) ),0)
// test if a flags is set
//  result is 0 or not; the value returned is the bit shifted within the word, and not always '1'
#  define TESTFLAG(v,n)  ( (v)[(n)/FLAGTYPEBITS((v)[0])] & ( (FLAGSETTYPE)1 << ( (n) & FLAGROUND((v)[0]) ) ) )
// reverse a flag from 1 to 0 and vice versa
// return value is undefined... and is a whole bunch of flags from some offset...
// if you want ot toggle and flag and test the result, use TESTGOGGLEFLAG() instead.
#  define TOGGLEFLAG(v,n)   ( (v)[(n)/FLAGTYPEBITS((v)[0])] ^= (FLAGSETTYPE)1 << ( (n) & FLAGROUND((v)[0]) ))
// Toggle a bit, return the state of the bit after toggling.
#  define TESTTOGGLEFLAG(v,n)  ( TOGGLEFLAG(v,n), TESTFLAG(v,n) )


#endif

// -- COMPILE TIME OPTIONS --

// FLOWER_HASH_ENABLE_LIVE_USER_DATA
//    uses the space in the user entry to store the address user's pointer
//    which points at this entry.  This allows mutable trees and essentially
//    'static' access.
//    - could alternatively just store the user's data as requested, and then
//      the external API wouldn't be aware of internal mutations.

// TODO : IMPLEMENT
// #define FLOWER_HASH_ENABLE_LIVE_USER_DATA


// prefered page sizing
#define BLOCK_SIZE 4096

// depending on the average key size, this could be sized smaller
// the default 7 byte keys used to test are only 1659 bytes (near half a page)
#define KEY_BLOCK_SIZE BLOCK_SIZE
// alignment might differ...
#define KEY_BLOCK_ALIGN BLOCK_SIZE

#define HASH_MASK_BITS  5
// compute this so it's the mask of low bits.
#define HASH_MASK       ( ( 1 << HASH_MASK_BITS ) -1 )
// 5 bits is 32 values.
// 1 bits is 2 values; this paged key lookup could just add data to nodes in a binary tree.



//-----------------------------------------------------------------------


#define largestBitOn_(bias,n,more) ((( ((1+bias)<sizeof(n)*CHAR_BIT) && ((n) >> (1+bias)) )   \
                                      ?( ((2+bias)<sizeof(n)*CHAR_BIT) && ((n) >> (2+bias)) ) \
                                      ?( ((3+bias)<sizeof(n)*CHAR_BIT) && ((n) >> (3+bias)) ) \
                                      ?( ((4+bias)<sizeof(n)*CHAR_BIT) && ((n) >> (4+bias)) ) \
                                      ?( ((5+bias)<sizeof(n)*CHAR_BIT) && ((n) >> (5+bias)) ) \
                                      ?( ((6+bias)<sizeof(n)*CHAR_BIT) && ((n) >> (6+bias)) ) \
                                      ?( ((7+bias)<sizeof(n)*CHAR_BIT) && ((n) >> (7+bias)) ) \
                                      ?( ((8+bias)<sizeof(n)*CHAR_BIT) && ((n) >> (8+bias)) ) \
                                      ? (more)                \
                                      :(8+bias):(7+bias):(6+bias):(5+bias):(4+bias):(3+bias):(2+bias):(1+bias) ))

#define largestBitOn__(bias,n,more)  largestBitOn_(bias+0,n,largestBitOn_(bias+8,n,largestBitOn_(bias+16,n,largestBitOn_(bias+24,n,more ) ) ) )
// this really supports much too big of a type.
#define largestBitOn(n)  largestBitOn__(0,n,largestBitOn__(32,n,65) )

#define bitsForValue(n)  largestBitOn(n)


#define testZero_(n,bias,depth,else)     ( (!(n&(1<<(0+bias))))?((depth-1)-bias):(else))
#define firstZero__(n,bias,depth,else)   testZero_(n,bias,0,depth, testZero_(n,bias,1,depth,testZero_(n,bias,2,depth, testZero_(n,bias,3,depth,else )) ) )
#define firstZero8_(n,bias,depth,else)   firstZero__( n,0,depth, firstZero__( n, 4, depth, else ) )
#define firstZero16_(n,bias,depth,else)  firstZero8_( n,bias,depth, firstZero8_( n, 8+bias, depth, else ) )
#define firstZero32_(n,bias,depth,else)  firstZero16_( n,bias,depth, firstZero16_( n, 16+bias, depth, else ) )
#define firstZero64_(n,bias,depth,else)  firstZero32_( n,bias,depth, firstZero32_( n, 32+bias, depth, else ) )

#define firstZero(n,depth) firstZero32_(n,0,depth,depth)


// this measures the directory entries.
// the block size, minus ...
//   2 block indexes at the tail, for a pointer to the actual name/data block, and a count of used names.
//   HASH_MASK block indexes - for the loctions of the next hash blocks...
//   (and 3 or 7 bytes padding)

#  define KEY_DATA_ENTRIES ( \
                ( BLOCK_SIZE \
                   - ( 8*sizeof(uintptr_t) + (HASH_MASK+1)*sizeof(uintptr_t)) \
                   - ( 8 * 4 ) \
                )  \
                / ( sizeof( struct key_data_entry) + sizeof( FPI ) ) \
          )


// brute force a compile time shift and compare to get the number of bits... this will be a small-ish number so
// this is generally good enough (up to 512 entries, which is 8 bytes per entry in 4096 bytes with 0 other bytes
// in the block used... 
#  define KEY_DATA_ENTRIES_BITS bitsForValue(KEY_DATA_ENTRIES)

// BlockPositionIndex (A byte index into a small array of blocks)
typedef uint16_t FPI;

#ifdef _MSC_VER
#  pragma pack (push, 1)
#endif

PREFIX_PACKED struct key_data_entry
{
	// the offset MAY be aligned for larger integer comparisons.
	//FPI key_data_offset;  // small integer, max of N directoryEntries * maxKeyLength bytes.
	//                      // this offset is applied to flower_hash_lookup_block.key_data_first_block

	uintptr_t stored_data; // the data this results with; some user pointer sized value.
#ifdef FLOWER_HASH_ENABLE_LIVE_USER_DATA
	uintptr_t** user_data_; // Store the user's reference so it can be updated when this entry moves.
#endif
	// a disk storage system might store the start of the data block and also
        // the size of the data block.  
	//uintptr_t first_block;  // first block of data of the file
	//VFS_DISK_DATATYPE filesize;  // how big the file is
} PACKED;

#ifdef _MSC_VER
#  pragma pack (pop)
#endif


// basically a yardstick that converts a coordinate at a depth to an index.
// first element is 0, second level is 1,2, etc...
// linear map; items are not themselves in order
#define treeRuler(x,depth) (((depth)<=0)?1:( ( x*(1 << ( depth))+ (1 << ( (depth) -1 ))-1 ) )


// basically a yardstick that converts max size of tree
#define treeSize(depth) (((depth)<=0)?1:(1<<(depth)))



// return the in-order index of the tree.
// x is 0 or 1.  level is 0-depth.  depth is the max size of the tree (so it knows what index is 1/2)
// this is in-place coordinate based...
// item 0 level 0 is 50% through the array
// item 0 level 1 is 25% and item 1 level 1 is 75% through the array...
// item 0 level (max) is first element of the array.
// items are ordered in-place in the array.
#define treeEnt(x,level,depth)  ( ( (x)*(1 << ( (depth)-(level))) + (1 << ( (depth) - (level) -1 ))-1 ) )

// function treeEnt( x,level,depth) { return ( x*(1 << ( depth-level)) + (1 << ( depth - level -1 ))-1 ) }

/*
function testZero_(n,bias,depth,els) { return  ( (!(n&(1<<(bias))))?((depth-1)-bias):(els())) }
function firstZero__(n,bias,depth,els) { return testZero_(n,0+bias,depth,()=>testZero_(n,1+bias,depth,()=>testZero_(n,2+bias,depth,()=>testZero_(n,3+bias,depth,els))))}
function firstZero8_(n,bias,depth,els) { return firstZero__(n,0+bias,depth,()=>firstZero__(n,4+bias,depth,els)) }
function firstZero16_(n,bias,depth,els) { return firstZero8_(n,0+bias,depth,()=>firstZero8_(n,8+bias,depth,els)) }
function firstZero32_(n,bias,depth,els) { return firstZero16_(n,0+bias,depth,()=>firstZero16_(n,16+bias,depth,els)) }
function firstZero(n,depth) { return firstZero32_(n,0,depth,()=>depth) }
*/


#  ifdef _MSC_VER
#    pragma pack (push, 1)
#  endif


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
		// when adding a duplicate key, return existing entry instead of adding the new entry
		// repeatedly.  (locate can currently only find one of them (order uncertain) anyway)
		unsigned  no_dup : 1;
		// this interprets baseOffset as a function pointer to be called later...
		// 
		unsigned  externalAllocator : 1;
		unsigned  used : 12;
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

enum insertFlowerHashEntryOptions {
	IFHEO_IMMUTABLE = 1,  // the tree won't change; if an overflow happens, follow into hash table; implies dense in that aspect. 
	IFHEO_CONVERTIBLE = 2,
	IFHEO_DENSE = 4, // follow hash last
	IFHEO_NO_DUPLICATES = 8, // return existing entry instead of a new; do not fault insert.
};

// Allocates an empty has tree root.
//
// The Flower Hash tree will never modify the input key data; but will
// internally copy the data as required.
//
// Example Usage:
// {
//    struct flower_hash_lookup_block* symbolHash = InitFlowerHash( IFHEO_IMMUTABLE | IFHEO_DENSE );
//    uintptr_t*  userDataPointer;
//    AddFlowerHashEntry( symbolHash, (uint8_t const*)"MyKey", myKeyLen, &userDataPointer );
//    userDataPointer[0] = (uintptr_t)"My Key's Value";
//
//
struct flower_hash_lookup_block* InitFlowerHash( enum insertFlowerHashEntryOptions options );

// Create a Flower Hash which returns records from a user defined structure.
// (this allows many more petals per flower stem)
struct flower_hash_lookup_block* CreateFlowerHashMap( enum insertFlowerHashEntryOptions options, void *userDataBuffer, size_t userRecordSize, int maxRecords );

// Create a Flower Hash which requests more storage as the hash expands.
//   This will call the provided callback function with a number of records to allocate.
//   the callback shall return a pointer to a region of memory large enough to store specifyed
//   count of records of userRecordSize size; it may return NULL to abort the insertion.
//
// Example Usage:
// void* returnBuffer( uintptr_t userData, int count ) {
//    struct callbackData *data = (struct callbackData*)userData;
//    return malloc( count * sizeof( myDataStructure ) );
// }
//
// {
//    struct flower_hash_lookup_block *myHash;
//    static struct callbackData {
//       int noData;
//    } data;
//    CreateFlowerHash( 0, sizeof( myDataStructure ), returnBuffer, (uintptr_t)&data );
//
struct flower_hash_lookup_block* CreateFlowerHash( enum insertFlowerHashEntryOptions options, size_t userRecordSize, void* (getStorage)(uintptr_t userData, int count ), uintptr_t userData );



// Destroy all associated resources for the specified hash tree
//
void DestroyFlowerHash( struct flower_hash_lookup_block* hash );


// adds the key to the tree.
// If the key already exists, it will blindly re-add it as if
// it is 'less than' the existing value (will not be greater than 0)
// 
// A compile time option `FLOWER_HASH_ENABLE_LIVE_USER_DATA` can be defined
// which will record the user's pointer, and update the pointer, if the tree
// mutates; this option will use more memory per entry and reduce the number 
// of entries per page that can be stored by about 50%. (TODO)
//
// If the tree has 'immutable' enabled, the resulting address can be kept
// as a constant; the address of their user data will always be the same.
// Otherwise, if not immutable, this address is guaranteed only valid until 
// the next Add (or Delete).
//
// Example Usage:
// { 
//    uintptr_t* storage; 
//    AddFlowerHashEntry( root, "key", 3, &storage );
//    storage[0] = myValue;
//
void AddFlowerHashEntry( struct flower_hash_lookup_block *hash, uint8_t const* key, size_t keylen, uintptr_t**dataStorage );

// readonly operation, if the key does not exist, results NULL.
//
// The result is the address of the userdata value in the tree (rather than a copy of that value).
//
// If the tree has 'immutable' enabled, entries will not move, and this address will be
// constant; otherwise this address is only guaranteed valid until the next Add (or Delete).
// 
// Example Usage:
// {
//    uintptr_t* value = LookupFlowerHashEntry( root, "key", 3 );
//    (*value)    // is the storage spot in the tree.
// 
uintptr_t* FlowerHashShallowLookup( struct flower_hash_lookup_block *hash, uint8_t const* key, size_t keylen );

// unimplemented
void DeleteFlowerHashEntry( struct flower_hash_lookup_block *hash, uint8_t const*key, size_t keylen );


