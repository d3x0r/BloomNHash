

# Object Storage Hash Principles


## Building related sources

I used the source and header from here for a few small utility things.
https://github.com/d3x0r/micro-C-Boost-Types (rename sack_ucb_typelib.* to sack.* )


## Flowering Hash Model

Consider a root stem, at the end of the stem, there are a bunch of petals, each of these
is a value to consider in the graph; the petals are ordered from least to most in an array
indexed by a virtual binary tree, with the first index in the middle of the array. When the 
number of values at the end of a stem exceeds
a certain amount, those that all have a similar value create a new stem, and move those leaves
that apply to that stem to the new location; This operation is only the idea, not required, the
existing values could remain where they are, just creating a stem, on insertion, when there
is no room in this directory tables; and putting only new values in the new location(s).

![Flower Model Illustration](/d3x0r/dc8a5638fa75520376911b05faf9fc88/raw/floweringModel.png)

## Implementation Details....

The number of hard stems(branches) is `HASH_MODULOUS_BITS` (HMB)  from which `HASH_MODULOUS` (HM) is derived, as
`1<<HASH_MODULOUS_BITS`, which is the number of ways that it will fork
to new hash blocks.  Values stored in the new leaves (could) have the low bits that went into 
indexing the hash removed, or a sliding window-mask based on the level in the tree could be used
to non-destructively read the value for the next hash block.

First search all values at the current node, and on failing
follow the stem that is the group of values that are similar to this value... something like `N`
is the value, and `N & ( (HASH_MODULOUS-1) << (TREE_LEVEL*HASH_MODULOUS_BITS)` and ` >> (TREE_LEVEL*HASH_MODULOUS_BITS)`
at the next level for the next index... (each level before already used the previous HMB)

The Value nodes on the leaf of each stem.  A subset of all values stored in the leaves of 
a hash node... 


When operating in a 'dense' mode that the most data entries are filled, it is O((log(N))^2/2).
If key entries only exist at the leaves it's O(1) ; where 1 is 3-4 ( `LogBaseHashModulous(N/KEY_DATA_ENTRIES)` ) hash table lookups, and `Log2(KEY_DATA_ENTRIES)` comparisons in a sorted array/binary trie.


## Flat Binary Tree Operations and Notes

Conventions...
  - The bottom/leaves of the tree are level 0; this lets every tree, no matter the depth have a common base.
  - The size of the tree is ((1<<(levels+1))-2).
    - the tree size is 1 for a 1 level tree, 3 for a 2 level tree, 7 for a 3 level tree..
    - it is perfectly split at 0 and +/-N from the 0 centerline.

An advantage of indexing by in-order, is that in the case of collision, it's just a simple slide; ordering with
the tree root index at 0, and it's leaves as 1-2, etc, requires a lot of shuffling in leaves because near spaces
are disconnected spatially in memory.

In the following table, a portion of a tree, one can notice that each level shares a portion of bits in the 
index.  The lowest bits are filled with 1's depending on how deep the tree is... 
  
|Lvl | 0 | 1 | 2 | 3 | 4 | N... |
|---|-------|--|--|--|--|---|
| (lvl num)  | 1 | 2 | 3 | 4 | 5 | ( N of M ) level refers to this number |
| 0 | 00000 | | | | | |
| 1 | | 00001 | | | | |
| 2 | 00010 | | | | | |
| 3 | | | 00011 | | | |
| 4 | 00100 | | | | | |
| 5 | | 00101 | | | | |
| 6 | 00110 | | | | | |
| 7 | | | | 00111 | | |
| 8 | 01000 | | | | | |
| 9 | | 01001 | | | | |
| 10| 01010 | | | | | |
| 11| | | 01011 | | | |
| 12| 01100 | | | | | |
| 13| | 01101 | | | | |
| 14| 01110 | | | | | |
| 15| | | | | 01111 | |
| 16| 10000 | | | | | |
| 17| | 10001 | | | | |
| 18| 10010 | | | | | |
| 19| | | 10011 | | | |
| 20| 10100 | | | | | |
| 21| | 10101 | | | | |
| 22| 10110 | | | | | |
| 23| | | | 10111 | | |
| 24| 11000 | | | | | |
| 25| | 11001 | | | | |
| 26| 11010 | | | | | |
| 27| | | 11011 | | | |
| 28| 11100 | | | | | |
| 29| | 11101 | | | | |
| 30| 11110 | | | | | |
| 31| | | | | | 011111 |

So, from any given index, we can determine the height from the leaves... LR# is left-right decision bit N, where N is Lvl above.
The notation of 'of N' 

| level | High Bits |  Low Bits | 
|---|:---|---:|
| 1 of 5 | `LR4 LR3 LR2 LR1 ___`  |  `____0` |
| 2 of 5 | `LR4 LR3 LR2 ___ ___`  |  `___01` |
| 3 of 5 | `LR4 LR3 ___ ___ ___`  |  `__011` |
| 4 of 5 | `LR4 ___ ___ ___ ___`  |  `_0111` |
| 5 of 5 | `___ ___ ___ ___ ___` |  `01111` |
| -  |   |  |
| N=8 | `___ ___ ___ ___ ...`  |  `011111111` |
| 1 of 9 | `LR8 LR7 LR6 LR5 LR4 LR3 LR2 LR1 ___`  |  `________0` |
| 7 of 9 | `LR8 LR7 ___ ___ ___ ___ ___ ___ ___`  |  `__0111111` |

So, also, from any given level on a tree, we can go up to its parent...

```c
   int levelBitNumber = findLeast0(N);
   int levelBit = 1 << levelBitNumber;
   N |= levelBit;        // set this least 0 to 1.
   N &= ~(levelBit<<1);  // set the next bit to 0.
```

What about going back down the tree?

```c
   int levelBitNumber = findLeast0(N); // find my level.
   int levelBit = 1 << levelBitNumber; // compute the effective value.
   N &= ~(levelBit>>1); // clear this to move towards the leaves 1 layer
   // 'levelBit' bit will, by definition always be 0 initially.
   N |= (direction)? levelBit:0; // direction is left or right; 'right' or 'greater' being +1.
```

Can we go across?

```
   int levelBitNumber = findLeast0(N); // find my level.
   int levelBit = 1 << levelBitNumber; // compute the effective value.
   N ^= (levelBit<<1); // if this was left, go to right; and vice versa.
	// change the LR decision of the previous level.
```


Or really, more generally?

```
   int levelBitNumber = findLeast0(N); // find my level.
   int levelBit = 1 << levelBitNumber; // compute the effective value.
   int levelMask = levelBit;
   for( n = 0; n < levels_to_enumerate; n++ ) {
	levelMask |= levelMask<<1;
   }
   for( n = 0; n < treeSize( levels_to_enumerate ); n++ ) {
	N &= ~levelMask;
	N = n << (levelBitNumber+1);
   }
```


### Inerstion into flat binary tree

Because values are stored in their final location, it is possible that multiple inserts will end up
at the same lowest point, and require shuffling values to make room for this value in-order. 

A disadvantage of this tree method lies here; to know where to insert, one has to know the total depth of the tree.
The first index is a zero bit followed by N 1 bits, where N 

#### Idea 1 - Track the used nodes

A bit per non-leaf node can be used to determine whether a node is full or not-full.  On Insertion,
the current location shifted right 1 is the current Node Allocation Bit (NAB);  If the final insertion
point is a leaf node, then it's peer node ( current location ^ my level bit ) should be checked for content,
if they are both occupied, set the parent's full bit, and step to parent.

If the parent's peer is also full, set full on it's parent, step to parent, repeat; Eventually the root's
node will be marked as full, and the tree is saturated.

This also helps discovery of the free node on insertion.   While finding a location for the new value, when a node
is found to be full, then a step cursor is created at the peer node to the current.  While the current is further
stepped down the tree, the edge counter follows the closest left/right edge depending on wheter is value is more
or less than the current index.  If the final current index lands on a leaf node that is already full, the nodes
from there to the edge are shifted towards the edge, and the new node is inserted in the discovered location.  When 
shifting, the edge node may have to bubble up to its closest parent.

The node shift may be done in one large memory shift.

This becomes O(1) to find the free node, and O(N/2) to shift the memory; although it is a very small factor, a very
large tree can find great expense in shifting half the nodes.  The Fixup of the leave end nodes is like O(log(N)/2),
it won't bubble up to the top of the tree, but must go up by LogN(1) steps...

#### Idea 0 - Scan For Free

The first cut idea for finding a free node on collision, was to scan from the current index to the left until a node is
found and to the right (or simultaneously, preferring the closer free node).  This is O(N) potentially.






## Example Iteration to find a value

For a tree with some `M` unique keys in it...

The first comparison is Log2(valueEntriesPerNode)
the next comparison is LogN(M) + Log2(valueEntriesPerNode) * LevelOfTree
and LevelOfTree is LogN(M)



```

If the first hash stem has 100 entries, and 2 branches.

The first comparison is 7 checks (binary search 0-127).
When that fails, the next level is 2 100 entry blocks, of which 
1 will be checked.  (200 of 300), another 7 checks will have to be done.

14 checks is 16384 unique values in a well balanced binary tree.

When this fails, the next block is another 200, half of which is checked
(there is also another half that we definatly aren't checking on the other side too)
...


If the first hash branch has 4 entries, and 4 branches.

The first comparison is 2 checks (binary search 0-3).
When that fails, the next level is 4 4 entry blocks, of which 
1 will be checked.  (8 of 36), another 2 checks will have to be done.

4 checks is 16 unique values in a well balanced binary tree.

When this fails, the next block is another 4*4, 1/16th of which is checked
(there is also another half that we definatly aren't checking on the other side too)

4      2           2
16     2+2         4
64     2+2+2       6
256    2+2+2+2     8
1024   2+2+2+2+2   10


...


If the first hash branch has 100 entries, and 32 branches.

The first comparison is 7 checks (binary search 0-127).
When that fails, the next level is 32 100 entry blocks, of which 
1 of which will be checked.  (200 of 3300), another 7 checks will have to be done.

the next level, 12800 (15000 total) blocks are potential

14 checks is 16384 unique values in a well balanced binary tree.


This is better only at 25% efficiency...





```


## Structures and Givens

For a given page size... , these are "BLOCKS" a BLOCKINDEX refers to a 4096 block of memory.
It doesn't have a specific type, just an alignment.  The goal is to pack everything into 
pages of memory.


```
#define BLOCK_SIZE 4096

// BLOCKINDEX  This is some pointer type to another hash node.

typedef uintptr_t BLOCKINDEX;
// uint64_t on x64, uint32 on x86
// can just store a simple integer number.

```

A certain dimension on the branching should be chosen... this is how wide the sub-indexing should be
this could, in theory, be 2, and be a binary tree, with pools of values along the path.

```

#define HASH_MODULUS_BITS  5
#define HASH_MODULUS       ( 1 << HASH_MODULUS_BITS )
// 5 bits is 32 values.
// 1 bits is 2 values; this paged key lookup could just add data to nodes in a binary tree.

```




Hash nodes look like this... 

This is the number of leaf entries per block.... Application 
specific information may be of use here; 

```



// this measures the directory entries.
// the block size, minus ...
//   2 block indexes at the tail, for a pointer to the actual name/data block, and a count of used names.
//   HASH_MODULOUS block indexes - for the loctions of the next hash blocks...
//   (and 3 or 7 bytes padding)
#  define KEY_DATA_ENTRIES ( \
                ( BLOCK_SIZE - ( 2*sizeof(BLOCKINDEX) + HASH_MODULOUS*sizeof(BLOCKINDEX)) )  \
                / sizeof( struct key_data_entry) \
          )

// brute force a compile time shift and compare to get the number of bits... this will be a small-ish number so
// this is generally good enough (up to 512 entries, which is 8 bytes per entry in 4096 bytes with 0 other bytes
// in the block used... 
#  define KEY_DATA_ENTRIES_BITS (( KEY_DATA_ENTRIES >> 1 )?( KEY_DATA_ENTRIES >> 2 )?( KEY_DATA_ENTRIES >> 3 )?( KEY_DATA_ENTRIES >> 4 )?( KEY_DATA_ENTRIES >> 5 )?( KEY_DATA_ENTRIES >> 6 )?( KEY_DATA_ENTRIES >> 7 )?( KEY_DATA_ENTRIES >> 8 )?9:8:7:6:5:4:3:2:1 )

// BlockPositionIndex (A byte index into a small array of blocks)
typedef uint16_t FPI;

PREFIX_PACKED struct key_data_entry
{
	// the offset MAY be aligned for larger integer comparisons.
	FPI key_data_offset;  // small integer, max of N directoryEntries * maxKeyLength bytes.
	                      // this offset is applied to flower_hash_lookup_block.key_data_first_block

	FPI key_data_end; // another short integer that is the lenght of the key data (position of end, length,...)
	                  // this offset is applied to flower_hash_lookup_block.key_data_first_block

	uintptr_t stored_data; // the data this results with; some user pointer sized value.

	// a disk storage system might store the start of the data block and also
        // the size of the data block.  
	//BLOCKINDEX first_block;  // first block of data of the file
	//VFS_DISK_DATATYPE filesize;  // how big the file is
} PACKED;



PREFIX_PACKED struct flower_hash_lookup_block
{
	// pointers to more data, indexed by hashing the key with
	// low byte & (HASH_MODULOUS-1).
	struct flower_hash_lookup_block*  next_block[HASH_MODULOUS];

	// these are values to be checked in this node.
	struct key_data_entry             entries[KEY_DATA_ENTRIES];

	// this is where my data for these values to check is stored
	uint8_t*                          key_data_first_block;

	// (optional) depending on the insersion method; this is the count of used entries above.
	// use can also be tracked with NULL/NOT-NULL value in the `entries[n].stored_data` 
	uint8_t                           used_key_data;

	// need a entry that is the parent; to know the depth of 'here'...(could be a few depth bits?)
	struct flower_hash_lookup_block * parent;
	

	// -- this could represent the above data in just a few bits...
	struct {
		unsigned used_key_data : (KEY_DATA_ENTRIES_BITS);
		unsigned depth : 2; // 0 is root (raw key)
		                    // 1 is first level (only drop first HASH_MODULOUS_BITS)
		                    // 2 is any other level (drop byte, and 5 bits)
	} info;

} PACKED;


struct key_data_block {
	uint8_t data[];
} PACKED;

```


## Great, Things are allocatable, now what?


*sigh* it all looked so simple in retrospect; and even as I'm defining this, the proposal is to use a 
insertion tree with AVL balancer instead of insertion sort.
This will deposit values in the tree in the appropriate places, more likely than pushing into a packed-stack
and sorting that way.


`_os_GetNewDirectory` gets a new object storage directory entry.  This takes a name (key), and can store the value when it creates the entry.
So for these purposes should be renamed to `FlowerHashInsertNode( key, value );`

There is sort of a balance between time and space; and the tree's operation could simply have a boolean
behavior to select this dynamically; however, these are the options on node capacity exceeded (page fully allocated).
 1) On insert, failing to find a empty entry, check the `next_block` index to see if `next_block[key % HASH_MODULOUS]` has a value already; if it does, go to that node, and check again; if it does NOT, create a new, empty hash node, and fill the first entry with this new value.
 2) On insert, failing to find a empty entry, count the most used hash index in the current entries, create a new, empty hash block, move all entries that have that hash index into the new block, add the new entry to the new block, link the new block as the `next_entries[key % HASH_MODULOUS]` index.

If the entries are moved, values only exist in leaf nodes, and all of the `entries` space in the hash node are empty (wasted space); however, when finding values, only a max of `Log2(KEY_DATA_ENTRIES)` comparisons have to be done, and a few table lookups.
If the entries remain in place, the hash entries are denesly filled (less wasted space), however, the values in each current node have to be checked for `Log2(KEY_DATA_ENTRIES)` per tree level `Log-Base-HASH_MODULOUS(N)`;



[Convert Directory (needs line number)](/d3x0r/dc8a5638fa75520376911b05faf9fc88#file-genhash_sparse-c-L851)


### Save Key Data in a slab of data associated with the directory....

[SaveKeyData()](https://gist.github.com/d3x0r/dc8a5638fa75520376911b05faf9fc88#file-genhash_sparse-c-L97)


# Find and key's value restoration


This does a lookup by key, and returns true/false whether it had a value.
(Can be modified to fill in a pointer to a result that the caller can use, with the userdata stored in the entry).

[flowerHashLookup](https://gist.github.com/d3x0r/dc8a5638fa75520376911b05faf9fc88#file-genhash_sparse-c-L127)


Tree Ruler... [Described here](https://github.com/d3x0r/VoxelPhysicsLibrary#the-ruler).


### Move leaf node of right shift up, if required

![Tree Axis Index Progression](/d3x0r/dc8a5638fa75520376911b05faf9fc88/raw/treeIndexing.png)


[upLeftSide](https://gist.github.com/d3x0r/dc8a5638fa75520376911b05faf9fc88#file-genhash_sparse-c-L384)
[upRightSide](https://gist.github.com/d3x0r/dc8a5638fa75520376911b05faf9fc88#file-genhash_sparse-c-L278)


## Go up the tree anywhere

So this operation is pretty simple; and GCC on modern hardware optmizes this to like 0 ticks.
see https://gist.github.com/d3x0r/5d1d24536886e795531578c74059349e regarding optimization of
this function.

```js

// JS - external C sources have same function
// levels could be passed; this will iterate N outside of the tree 
// if it is passed the entry 0,0 index.
//  https://docs.google.com/spreadsheets/d/1PsS9-UjS_6PzLnOBhIoeRy0ju3P3Zpgx0qQi6uEUe-o/edit?usp=sharing
function upTree( N ) {
	for( n = 0; n < 32; n++ ) if( !(N&(1<<n)) ) break;
	return (N & ~((0x3)<<n)) | ( 0x01 << n )
}
/* this is 3-5x faster  (IN JS) it's fastest do do the above for loop,
   optimized to a bit-scan instruction; or use like gcc __builtin_ffs(~N)  */
function upTreeL( N ) {
	const n = Math.floor(Math.log(N))+1;
	return (N & ~((0x2)<<n)) | ( 0x01 << n )
}


```


## Add a hash entry

[AddHashEntry](https://gist.github.com/d3x0r/dc8a5638fa75520376911b05faf9fc88#file-genhash_sparse-c-L382)


## Notes


Sometimes, the decoration for defining a packed structure is at the front of the declaration
and sometimes it is at the end of the declaration, so both locations have a symbol which can be
defined as non-blank for the appropriate compilers...

 - `PACKED` is GCC __attribute__((packed))
 - `PREFIX_PACKED` is openWatcom/visual studio __declspec(packed) or _Packed(?) keyword?
 - `SUS_GT` is 'signed-unsigned greater-than'.. which does range checkes where integer/unsigned values would be always true



## Video Debug Demonstration

[Youtube Video](https://youtu.be/P6knC4bnIEg)
