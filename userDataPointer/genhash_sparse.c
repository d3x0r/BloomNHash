
#include "genhash.h"


#include "../debugUtil/genhash_dump.c"
	

//#define FLOWER_HASH_DEBUG_MOVES
//#define FLOWER_TICK_PERF_COUNTERS

// this adds a bunch of sanity checks/ASSERT sort of debugging
#define FLOWER_DIAGNOSTIC_DEBUG

#ifdef FLOWER_TICK_PERF_COUNTERS

struct perfTickCounters {
	int moves_left;
	int moves_right;
	int lenMovedLeft[128];
	int lenMovedRight[128];

};
static struct perfTickCounters pft;
#endif

static struct flower_hash_lookup_block* convertFlowerHashBlock( struct flower_hash_lookup_block* hash );

// get the number of most significant bit that's on
int maxBit( int x ) { int n;  for( n = 0; n < 32; n++ ) if( x & ( 1 << ( 31 - n ) ) ) break; return 32 - n; }

static char* toBinary( int N ) {
	static char bits[5][12];
	static int cur;
	static int n;
	if( cur > 4 ) cur = 0;
	for( n = 0; n < 9; n++ ) {
		if( N & ( 1 << ( 8 - n ) ) )
			bits[cur][n] = '1';
		else
			bits[cur][n] = '0';
	}
	return bits[cur++];
}


static int linearToTreeIndex( int f_ ) 
{
	int b;
	int fMask;
	for( b = 0; b < KEY_DATA_ENTRIES_BITS; b++ ) {
		if( f_ < ( fMask = ( ( 1 << ( b + 1 ) ) - 1 ) ) )
			break;
	}
	int f = treeEnt( f_ - ( ( fMask ) >> 1 ), b, KEY_DATA_ENTRIES_BITS );
	return f;
}

static void* ReallocateAligned( void* p, size_t oldSize, size_t newSize, uint16_t align ){
	void* const newBlock = aa( align, newSize );
	if( p ) {
		memcpy( newBlock, p, oldSize );
		fa( p );
	}
	return newBlock;
}

// excessive keys (given N length keys) may require more storage for their space.
// not responsible for optimization penalties incurred :)
static void expand_key_data_space( struct flower_hash_lookup_block *here ) {
	here->key_data_first_block = ReallocateAligned( here->key_data_first_block
	                                              , (here->info.key_data_blocks) * KEY_BLOCK_SIZE
	                                              , (1+here->info.key_data_blocks) * KEY_BLOCK_SIZE
	                                              , KEY_BLOCK_ALIGN );
	here->info.key_data_blocks++;
}


static void squashKeyData( struct flower_hash_lookup_block* hash, int keyIndex ) {
	FPI const offset = hash->key_data_offset[keyIndex];
	uint8_t const length = hash->key_data_first_block[ offset - 1];
	if( offset & 0x8000 ) DebugBreak();
	//lprintf( "Squash is Moving over... %d   %d   %d\n", offset, hash->info.used_key_data, hash->info.used_key_data-(offset+length) );
	//LogBinary( hash->key_data_first_block + offset - 1, hash->key_data_first_block[offset - 1] );
	memmove( hash->key_data_first_block + offset - 1
		, hash->key_data_first_block + length + offset
		, hash->info.key_data_blocks * KEY_BLOCK_SIZE - ( length + offset ) );
	//lprintf( "So it's now...\n" );
	//LogBinary( hash->key_data_first_block + offset - 1, hash->key_data_first_block[offset - 1] );
	for( int n = 0; n < KEY_DATA_ENTRIES; n++ ) {
		if( hash->key_data_offset[n] > (offset+length) ) {
			hash->key_data_offset[n] -= length+1;
			if( hash->key_data_offset[n] & 0x8000 )DebugBreak();
		}
	}
	hash->info.used_key_data -= length + 1;
	if( hash->info.used_key_data & 0x8000 )DebugBreak();

}

// this results in an absolute disk position
// pass the hash_lookup_block which is gaining a new key value.
// pass the key and key length (offset in bytes as nessecary)
// dropHash will drop the least significant HASH_MASK_BITS bits from
// the least significant byte.  (leaving the remaining bits)

static FPI saveKeyData( struct flower_hash_lookup_block *hash, const char *key, uint8_t keylen ) {
	if( keylen > 8 ) DebugBreak();
	uint8_t * this_name_block = hash->key_data_first_block;
	uint8_t * start = this_name_block;
	size_t max_key_data = hash->info.key_data_blocks * KEY_BLOCK_SIZE;
	if( keylen & 0xFFFFF00 ) DebugBreak();
	//int keydatalen;
	//printf( "Save key..." );
	//LogBinary( key, keylen );
	while( 1 ) {
		if( hash->info.used_key_data > 4096 ) DebugBreak();
		this_name_block += hash->info.used_key_data;
		/* // scan strings, doesn't require an extra length counter
		keydatalen = this_name_block[0];
		if( keydatalen ) {
			// skip over previously used keys.
			this_name_block += keydatalen+1;
			continue;
		}
		*/
		if( ( max_key_data - ( this_name_block - hash->key_data_first_block ) ) < (keylen+2) ) {
			expand_key_data_space( hash );
			this_name_block = hash->key_data_first_block + ( this_name_block - start );
			max_key_data = hash->info.key_data_blocks * KEY_BLOCK_SIZE;
			start = hash->key_data_first_block;
		}
		this_name_block[0] = keylen & 0xFF;
		memcpy( this_name_block+1, key, keylen );
		this_name_block[keylen+1] = 0;
		hash->info.used_key_data += keylen + 1;
		return (FPI)((this_name_block - hash->key_data_first_block )+1);
	}
}


// returns pointer to user data value
void lookupFlowerHashKey( struct flower_hash_lookup_block **root, uint8_t const *key, size_t keylen, uintptr_t **userdata, int*pei, int*pem ) {
	if( !key ) {
		userdata[0] = NULL;
		return; // only look for things, not nothing.
	}
	int ofs = 0;

	struct flower_hash_lookup_block *hash = root[0];
	struct key_data_entry *next_entries;

	// provide 'goto top' by 'continue'; otherwise returns.
	do {
		uint8_t* keyBlock = hash->key_data_first_block;
		if( !hash->info.dense ) {
			// follow converted hash blocks...
			struct flower_hash_lookup_block* nextblock = hash->next_block[key[ofs] & HASH_MASK];
			if( nextblock ) {
				ofs += 1;
				hash = nextblock;
				continue;
			}
		}
		// look in the binary tree of keys in this block.
		int curName = treeEnt( 0, 0, KEY_DATA_ENTRIES_BITS );
		int curMask = ( 1 << KEY_DATA_ENTRIES_BITS ) - 1;
		next_entries = hash->entries;
		while( curMask )
		{
			struct key_data_entry* entry = hash->entries + curName;
			FPI entkey;
			if( !( entkey=hash->key_data_offset[curName] )) break; // no more entries to check.
			int d = (int)(keylen - keyBlock[entkey - 1]);
			curName &= ~( curMask >> 1 );
			// sort first by key length... (really only compare same-length keys)
			if( ( d == 0 ) && ( d = memcmp( key + ofs, keyBlock + entkey, keylen ) ) == 0 ) {
				if( pei ) pei[0] = curName;
				if( pem ) pem[0] = curMask;
				userdata[0] = &entry->stored_data;
				root[0] = hash;
				return;
			}
			if( d > 0 ) curName |= curMask;
			curMask >>= 1;
		}
		if( hash->info.dense ) {
			// follow converted hash blocks...
			struct flower_hash_lookup_block* nextblock = hash->next_block[key[ofs] & HASH_MASK];
			if( nextblock ) {
				ofs += 1;
				hash = nextblock;
				continue;
			}
		}
		userdata[0] = NULL;
		return;
	}
	while( 1 );
}

uintptr_t* FlowerHashShallowLookup( struct flower_hash_lookup_block* root, uint8_t const* key, size_t keylen ) {
	uintptr_t* t;
	lookupFlowerHashKey( &root, key, keylen, &t, NULL, NULL );
	return  t;
}

struct flower_hash_lookup_block *InitFlowerHash( enum insertFlowerHashEntryOptions options ) {
	struct flower_hash_lookup_block *hash = NewPlus( struct flower_hash_lookup_block, sizeof( struct key_data_entry ) * KEY_DATA_ENTRIES );
	memset( hash, 0, sizeof( *hash ) );
	expand_key_data_space( hash );
	hash->key_data_first_block[0] = 0;
	hash->info.dense = (options & IFHEO_DENSE)?1:0;
	hash->info.convertible = (options & IFHEO_CONVERTIBLE)?1:0;
	hash->info.immutable = (options & IFHEO_IMMUTABLE)?1:0;
	hash->info.no_dup = ( options & IFHEO_NO_DUPLICATES ) ? 1 : 0;
	// zero means, track as index number...
	hash->entries = hash->entries_;
	hash->info.userRecordSize = 0;// sizeof( struct key_data_entry );
	return hash;
}

struct flower_hash_lookup_block* CreateFlowerHashMap( enum insertFlowerHashEntryOptions options, void* userDataBuffer, size_t userRecordSize, int maxRecords ) {
	if( userRecordSize > ( (unsigned)( ( (int)-1 ) ) ) ) {
		printf( "Record size to allocate is larger than can be tracked: %zd", userRecordSize ); 
		return NULL;
	}
	struct flower_hash_lookup_block* hash = NewPlus( struct flower_hash_lookup_block, sizeof( struct key_data_entry ) * KEY_DATA_ENTRIES );
	memset( hash, 0, sizeof( *hash ) );
	expand_key_data_space( hash );
	hash->entries = userDataBuffer;
	hash->info.userRecordSize = (int)userRecordSize;
	hash->key_data_first_block[0] = 0;
	hash->info.dense = ( options & IFHEO_DENSE ) ? 1 : 0;
	hash->info.convertible = ( options & IFHEO_CONVERTIBLE ) ? 1 : 0;
	hash->info.immutable = ( options & IFHEO_IMMUTABLE ) ? 1 : 0;
	hash->info.no_dup = ( options & IFHEO_NO_DUPLICATES ) ? 1 : 0;
	return hash;
}


static void updateEmptiness( struct flower_hash_lookup_block* hash, int entryIndex, int entryMask ) {
	while( ( entryMask < KEY_DATA_ENTRIES ) && ( ( entryIndex > KEY_DATA_ENTRIES ) || TESTFLAG( hash->used_key_data, entryIndex >> 1 ) ) ) {
		if( entryIndex < KEY_DATA_ENTRIES && ( entryIndex & 1 ) ) {
			RESETFLAG( hash->used_key_data, entryIndex >> 1 );
			//dumpBlock( hash );
		}
		entryIndex = entryIndex & ~( entryMask << 1 ) | entryMask;
		entryMask <<= 1;
	}
}

static void updateFullness( struct flower_hash_lookup_block* hash, int entryIndex, int entryMask ) {
	//if( hash->key_data_offset[entryIndex ^ (entryMask<<1)] ) 
	{ // if other leaf is also used
		int broIndex;
		int pIndex = entryIndex;
		do {
			pIndex = ( pIndex & ~(entryMask<<1) ) | entryMask; // go to the parent
			if( pIndex < KEY_DATA_ENTRIES ) { // this is full automatically otherwise 
				if( !TESTFLAG( hash->used_key_data, pIndex >> 1 ) ) {
					SETFLAG( hash->used_key_data, pIndex >> 1 ); // set node full status.
				} 
#ifdef FLOWER_DIAGNOSTIC_DEBUG
				else {
					// this could just always be inserting into a place that IS full; and doesn't BECOME full...
					printf( "Parent should NOT be full. (if it was, " );
					DebugBreak();
					break; // parent is already full.
				}
#endif
			}
			else {

				int tmpIndex = pIndex;
				int tmpMask = entryMask;
				while( tmpMask && ( tmpIndex = tmpIndex & ~tmpMask ) ) { // go to left child of this out of bounds parent..
					if( tmpMask == 1 )  break;
					
					if( TESTFLAG( hash->used_key_data, tmpIndex >> 1 ) ) {
						break;
					}
					tmpMask >>= 1;
				}
				if( !tmpMask )
					break;
			}
			if( entryMask > KEY_DATA_ENTRIES / 2 ) break;
			entryMask <<= 1;
			broIndex = pIndex ^ ( entryMask << 1 );
			if( broIndex >= KEY_DATA_ENTRIES ) {
				int ok = 1;
				int tmpIndex = broIndex & ~entryMask;
				int tmpMask = entryMask>> 1;
				while( tmpMask && ( tmpIndex = tmpIndex & ~tmpMask ) ) { // go to left child of this out of bounds parent..
					if( tmpIndex < KEY_DATA_ENTRIES ) {
						if( tmpMask == 1 ) 
							ok = ( hash->key_data_offset[tmpIndex] != 0 );
						else
							ok = ( TESTFLAG( hash->used_key_data, tmpIndex >> 1 ) ) != 0;
						break;
					}
					tmpMask >>= 1;
				}
				if( ok ) {
					if( pIndex < KEY_DATA_ENTRIES )
						continue;
					else break;
				}
				else break;
			}
			else {
				if( ( hash->key_data_offset[broIndex]
					&& TESTFLAG( hash->used_key_data, broIndex >> 1 ) ) )
					continue;
			}
			// and then while the parent's peer also 
			if( entryMask >= (KEY_DATA_ENTRIES/2) )
				break;
			break;
		} while( 1);
	}
}


static void moveOneEntry_( struct flower_hash_lookup_block* hash, int from, int to, int update );
#define moveOneEntry(a,b,c) moveOneEntry_(a,b,c,1)

// Rotate Right.  (The right most node may need to bubble up...)
// the only way this can end up more than max, is if the initial input
// is larger than the size of the tree.
static void upRightSideTree( struct flower_hash_lookup_block* hash, int entryIndex, int entryMask ) {
	// if it's not at least this deep... it doesn't need balance.
	if( entryMask == 1 ) {
		if( !( entryIndex & 2 ) ) {
			int parent = entryIndex;
			int parentMask = entryMask;
			int bit = 0;
			while( 1 ) {
				// decision bit from previous level is off
				if( !( entryIndex & ( entryMask << 1 ) ) ) { // came from parent's left
					// came from the left side.
					parent = entryIndex /* TRUE & ~( entryMask << 1 ) */ | entryMask;
					parentMask <<= 1;
					if( parent >= KEY_DATA_ENTRIES ) {
						break; // keep going until we find a real parent...
					}
				}
				else break;
				// if node is empty... 
				if( !hash->key_data_offset[parent] ) {
					moveOneEntry( hash, entryIndex, parent );
					hash->key_data_offset[entryIndex] = 0;
					entryIndex = parent;
					entryMask = parentMask;
				}
				else break;
			}
		}
	}
	else
		if( !hash->key_data_offset[entryIndex] ) return;
	int broIndex = entryIndex ^ ( entryMask << 1 );
	if( entryMask == 1 
		&& ( ( broIndex >= KEY_DATA_ENTRIES ) 
			|| hash->key_data_offset[broIndex] ) ) { // if other leaf is also used
		updateFullness( hash, entryIndex, entryMask );
	}
}

// rotate left
// left most node might have to bubble up.
static void upLeftSideTree( struct flower_hash_lookup_block* hash, int entryIndex, int entryMask ) {
	if( ( entryIndex & 0x3 ) == 2 ) {
		while( 1 ) {
			int parent = entryIndex;
			int parentMask = entryMask;
			if( ( parent & ( parentMask << 1 ) ) ) { // came from parent's right
				parent = parent & ~( parentMask << 1 ) | parentMask;
				parentMask <<= 1;
				//  check if value is empty, and swap
				if( !hash->key_data_offset[parent] ) {
					moveOneEntry( hash, entryIndex, parent );
					hash->key_data_offset[entryIndex] = 0;
					entryIndex = parent;
					entryMask = parentMask;
					continue;
				}
			}
			break;
		}
	}
	int broIndex = entryIndex ^ ( entryMask << 1 );
	if( entryMask == 1 && ( ( broIndex >= KEY_DATA_ENTRIES )
		|| hash->key_data_offset[broIndex] ) ) { // if other leaf is also used
		updateFullness( hash, entryIndex, entryMask );
	}
}

static int getLevel( int N ) {
#ifdef __GNUC__
	int n = __builtin_ffs( ~N )-1;
#elif defined( _MSC_VER )
	unsigned long n;
	_BitScanForward( &n, ~N ); 
	return (int)(n);
#else
	int n;
	for( n = 0; n < 32; n++ ) if( !(N&(1<<n)) ) break;
#endif
	return n;	
}


static void validateBlock( struct flower_hash_lookup_block* hash ) {
	return;
	int used = hash->info.used;
	int realUsed = 0;
	FPI prior = 0;
	uint8_t *keyData = hash->key_data_first_block;
	int n;
	int m;
	for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
		for( m = n+1; m < KEY_DATA_ENTRIES; m++ ) {
			if( hash->key_data_offset[n] && hash->key_data_offset[m] ) {
				if( hash->key_data_offset[n] == hash->key_data_offset[m] ) {
					printf( "Duplicate key in %d and %d", n, m );
					DebugBreak();
				}
			}
		}
	}

	for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
		if( hash->key_data_offset[n] ) {
			int nLevel = 1 << getLevel( n );
			if( hash->key_data_offset[n] > hash->info.used_key_data )DebugBreak();
			if( ((n & ~( nLevel << 1 ) | nLevel) < KEY_DATA_ENTRIES )&&  !hash->key_data_offset[n & ~( nLevel << 1 ) | nLevel] ) {
				printf( "Parent should always be filled:%d\n", n );
				DebugBreak();
			}
			if( prior ) {
				if( keyData[prior - 1] > keyData[hash->key_data_offset[n] - 1] ) {
					DebugBreak();
				}
				if( memcmp( keyData + prior, keyData + hash->key_data_offset[n], keyData[hash->key_data_offset[n] - 1] ) > 0 ) {
					printf( "Entry Out of order:%d\n", n );
					DebugBreak();
				}
			}
			prior = hash->key_data_offset[n];
			realUsed++;
		}
	}
	if( realUsed!=used && realUsed != ( used + 1 ) && realUsed != (used-1) )
		DebugBreak();
	hash->info.used = realUsed;
}


static void moveOneEntry_( struct flower_hash_lookup_block* hash, int from, int to, int update ) {
#ifdef FLOWER_DIAGNOSTIC_DEBUG
	if( to >= KEY_DATA_ENTRIES ) DebugBreak();
#endif
#ifdef FLOWER_TICK_PERF_COUNTERS
	if( to < from ) {
		pft.lenMovedLeft[1]++;
		pft.moves_left++;
	}
	else {
		pft.lenMovedRight[1]++;
		pft.moves_right++;
	}
#endif
	if( to < 0 ) DebugBreak();
	hash->key_data_offset[to] = hash->key_data_offset[from];
	hash->entries[to] = hash->entries[from];
	if( update )
		if( to < from ) {
			upLeftSideTree( hash, to, 1 << getLevel( to ) );
		}
		else {
			upRightSideTree( hash, to, 1 << getLevel( to ) );
		}
}

static void moveTwoEntries_( struct flower_hash_lookup_block* hash, int from, int to, int update ) {
#define moveTwoEntries(a,b,c) moveTwoEntries_(a,b,c,1)
	int const to_ = to;
#ifdef FLOWER_DIAGNOSTIC_DEBUG
	if( to >= KEY_DATA_ENTRIES ) DebugBreak();
#endif
#ifdef FLOWER_TICK_PERF_COUNTERS
	if( to < from ) {
		pft.lenMovedLeft[2]++;
		pft.moves_left++;
	}
	else {
		pft.lenMovedRight[2]++;
		pft.moves_right++;
	}
#endif
	if( to < 0 ) DebugBreak();
	if( from > to ) {
		hash->key_data_offset[to] = hash->key_data_offset[from];
		hash->entries[to++] = hash->entries[from++];
		hash->key_data_offset[to] = hash->key_data_offset[from];
		hash->entries[to] = hash->entries[from];
	}
	else {
		hash->key_data_offset[++to] = hash->key_data_offset[++from];
		hash->entries[to--] = hash->entries[from--];
		hash->key_data_offset[to] = hash->key_data_offset[from];
		hash->entries[to] = hash->entries[from];
	}
	if( update )
		if( to < from ) {
			upLeftSideTree( hash, to_, 1 << getLevel( to_ ) );
		}
		else {
			upRightSideTree( hash, to_+1, 1 << getLevel( to_+1 ) );
		}
}

static void moveEntrySpan( struct flower_hash_lookup_block* hash, int from, int to, int count ) {
	int const to_ = to;
	int const count_ = count;
#ifdef FLOWER_DIAGNOSTIC_DEBUG
	if( to >= KEY_DATA_ENTRIES || ( to + count ) >= KEY_DATA_ENTRIES ) DebugBreak();
#endif
#ifdef FLOWER_TICK_PERF_COUNTERS
	if( to < from ) {
		pft.lenMovedLeft[count]++;
		pft.moves_left++;
	}
	else {
		pft.lenMovedRight[count]++;
		pft.moves_right++;
	}
#endif
	if( to < 0 ) DebugBreak();
	if( from > to ) {
		//for( ; count > 1; count -= 2, from += 2, to += 2 ) moveTwoEntries_( hash, from, to, 0 );
		//for( ; count > 0; count-- ) moveOneEntry_( hash, from++, to++, 0 );
		memmove( hash->entries + to, hash->entries + from, ( count ) * sizeof( hash->entries[0] ) );
		memmove( hash->key_data_offset + to, hash->key_data_offset + from, ( count ) * sizeof( hash->key_data_offset[0] ) );
	}
	else {
		memmove( hash->entries + to, hash->entries + from, ( count ) * sizeof( hash->entries[0] ) );
		memmove( hash->key_data_offset + to, hash->key_data_offset + from, ( count ) * sizeof( hash->key_data_offset[0] ) );
		//for( from += count, to += count; count > 0; count-- ) moveOneEntry_( hash, --from, --to, 0 );
	}
	if( to < from ) {
		upLeftSideTree( hash, to_, 1 << getLevel( to_ ) );
	}
	else {
		upRightSideTree( hash, to_+ count_ -1, 1 << getLevel( to_ + count_ - 1 ) );
	}
}

static int c;

static void insertFlowerHashEntry( struct flower_hash_lookup_block* *hash_
	, uint8_t const* key
	, size_t keylen
	, uintptr_t** userPointer
) {
	if( keylen > 255 ) { printf( "Key is too long to store.\n" ); userPointer[0] = NULL; return; }
	struct flower_hash_lookup_block* hash = hash_[0];
	int entryIndex = treeEnt( 0, 0, KEY_DATA_ENTRIES_BITS );
	int entryMask = 1 << getLevel( entryIndex ); // index of my zero.
	int edge = -1;
	int edgeMask;
	int leastEdge = KEY_DATA_ENTRIES;
	int leastMask;
	int mustLeft = 0;
	int mustLeftEnt = 0;
	int full = 0;
#ifdef HASH_DEBUG_BLOCK_DUMP_INCLUDED
	int dump = 0;
#endif
	struct flower_hash_lookup_block* next;

	full = TESTFLAG( hash->used_key_data, entryIndex >> 1 ) != 0;
	{
		c++;
#ifdef HASH_DEBUG_BLOCK_DUMP_INCLUDED
	//	if( ( c > ( 294050 ) ) )
	//		dump = 1;
#endif
		//if( c == 236 ) DebugBreak();
	}
	if( !hash->info.dense ) {
		struct flower_hash_lookup_block* next;
		// not... dense
		if( hash->parent && full )
			DebugBreak();
		do {
			if( full && ( hash->info.convertible || hash->info.dense ) ) {
				convertFlowerHashBlock( hash );
				full = TESTFLAG( hash->used_key_data, entryIndex >> 1 ) != 0;
			}
			if( next = hash->next_block[key[0] & HASH_MASK] ) {
				if( hash->parent ) {
					key++; keylen--;
				}
				hash = next;
				full = TESTFLAG( hash->used_key_data, entryIndex >> 1 ) != 0;
			}
		} while( next );
	}

	while( 1 ) {
		int d_ = 1;
		int d = 1;
		while( 1 ) {

			int offset = 0;
			// if entry is in use....
			if( mustLeftEnt || ( offset = hash->key_data_offset[entryIndex] ) ) {
				// compare the keys.
				if( d && offset ) {
					// mustLeftEnt ? -1 : 
					d = (int)( keylen - hash->key_data_first_block[offset - 1] );
					if( d == 0 ) {
						d = memcmp( key, hash->key_data_first_block + offset, keylen );
						if( hash->info.no_dup && d == 0 ) {
							// duplicate already found in tree, use existing entry.
							hash_[0] = hash;
							return;
						}
						// not at a leaf...
					}
				}

				if( !full && !hash->info.immutable )
					if( edge < 0 )
						if( TESTFLAG( hash->used_key_data, ( entryIndex >> 1 ) ) ) {
							edgeMask = entryMask;
							// already full... definitly will need a peer.
							edge = entryIndex ^ ( entryMask << 1 );
							if( edge > KEY_DATA_ENTRIES ) {
								int backLevels;
								// 1) immediately this edge is off the chart; this means the right side is also full (and all the dangling children)
								for( backLevels = 0; ( edge > KEY_DATA_ENTRIES ) && backLevels < 8; backLevels++ ) {
									edge = ( edge & ~( edgeMask << 1 ) ) | ( edgeMask );
									edgeMask <<= 1;
								}

								for( backLevels = backLevels; backLevels > 0; backLevels-- ) {
									int next = edge & ~( edgeMask >> 1 );
									if( ( ( next | ( edgeMask ) ) < KEY_DATA_ENTRIES ) ) {
										if( !TESTFLAG( hash->used_key_data, ( next | ( edgeMask ) ) >> 1 ) ) {
											edge = next | ( edgeMask );
										}
										else edge = next;
									}
									else {
										if( TESTFLAG( hash->used_key_data, ( next ) >> 1 ) ) {
											mustLeft = 1;
											edge = next | ( edgeMask );
										}
										else edge = next;
									}
									edgeMask >>= 1;
								}
							}
							if( !hash->key_data_offset[edge] ) {
								leastEdge = edge;
								leastMask = edgeMask;
							}
							// if this is FULL, and the edge is here, everything there IS FULL...
							// and we really need to go up 1 and left.
						}

				if( entryMask > 1 ) {
					// if edge is set, move the edge as we go down...
					if( !hash->info.immutable )
						if( edge >= 0 ) {
							int next = edge & ~( entryMask >> 1 ); // next left.
							if( mustLeft ) { if( next < KEY_DATA_ENTRIES ) mustLeft = 0; }
							else
								if( edge > entryIndex ) {
									next = edge & ~( entryMask >> 1 ); // next left.
									//printf( "Is Used?: %d  %d   %lld \n", next >> 1, edge >> 1, TESTFLAG( hash->used_key_data, next >> 1 ) ) ;
									if( ( next & 1 ) && TESTFLAG( hash->used_key_data, next >> 1 ) ) {
										if( !mustLeft ) next |= entryMask; else mustLeft = 0;
										if( next >= KEY_DATA_ENTRIES )
											mustLeft = 1;
#ifdef FLOWER_DIAGNOSTIC_DEBUG
										if( TESTFLAG( hash->used_key_data, next >> 1 ) ) {
											printf( "The parent of this node should already be marked full.\n" );
											DebugBreak();
										}
#endif
									}
									else {
										if( (!( next & 1 )) && hash->key_data_offset[next] ) {
											if( !mustLeft ) next |= entryMask; else mustLeft = 0;
											if( next >= KEY_DATA_ENTRIES )
												mustLeft = 1;
										}
									}
								}
								else {
									next = edge & ~( entryMask >> 1 ); // next left.
									//printf( "Is Used?: %d  %d   %lld  %lld\n", ( next | entryMask ) >> 1, edge >> 1, TESTFLAG( hash->used_key_data, ( next | entryMask ) >> 1 ), TESTFLAG( hash->used_key_data, ( next ) >> 1 ) ) ;
									if( ( next | entryMask ) < KEY_DATA_ENTRIES ) {
										if( entryMask == 2 ) {
											if( !hash->key_data_offset[next | entryMask] )
												next |= entryMask;
										}
										else {
											if( TESTFLAG( hash->used_key_data, ( next | entryMask ) >> 1 ) ) {
#ifdef FLOWER_DIAGNOSTIC_DEBUG
												if( TESTFLAG( hash->used_key_data, next >> 1 ) ) {
													printf( "The parent of this node should already be marked full.\n" );
													DebugBreak();
												}
#endif
											}
											else {
												if( entryMask != 2 || !hash->key_data_offset[next | entryMask] ) {
													if( !mustLeft ) next |= entryMask; else mustLeft = 0;
													if( next >= KEY_DATA_ENTRIES )
														mustLeft = 1;
												}
											}
										}
									} 
#ifdef FLOWER_DIAGNOSTIC_DEBUG
									else {
										if( TESTFLAG( hash->used_key_data, next >> 1 ) ) {
											printf( "The parent of this node should already be marked full.\n" );
											DebugBreak();
										}
									}
#endif
								}

							edge = next;
							edgeMask = entryMask>>1;
							//printf( "Edge follow: %d %s %d  %s\n", edge, toBinary( edge ), entryIndex, toBinary( entryIndex ) );
						}
					if( d_ ) {
						if( d ) {
							entryIndex &= ~( entryMask >> 1 );
							if( !mustLeftEnt && d > 0 ) entryIndex |= entryMask;
							if( entryIndex >= KEY_DATA_ENTRIES ) mustLeftEnt = 1; else mustLeftEnt = 0;
						}
					}
					else {

						//break;
					}
					if(! hash->key_data_offset[entryIndex] )
						d_ = d;
					entryMask >>= 1;
					if( edge >= 0 && !hash->key_data_offset[edge] )
						if( edge > entryIndex )
							if( edge < leastEdge ) {
								leastEdge = edge;
								leastMask = entryMask;
							}
							else;
						else {
							if( edge > leastEdge || ( leastEdge == KEY_DATA_ENTRIES ) ) {
								leastEdge = edge;
								leastMask = entryMask;
							}
						}
					continue;
				}

				if( mustLeftEnt && entryIndex >= KEY_DATA_ENTRIES ) {
					entryIndex &= ~2;
					if( entryIndex < (KEY_DATA_ENTRIES-1) ) {
						if( d > 0 ) {
							if( edge < 0 ) {
								edge = entryIndex;
								edgeMask = entryMask;
							}
							entryIndex++;
							entryMask <<= 1;
						}
					}
					mustLeftEnt = 0;
				}
				// this key needs to go here.
				// the value that IS here... d < 0 is needs to move left
				// if the d > 0 , then this value needs to move to the right.

				if( hash->info.immutable ) // if entries may not move, stop here, and store in hash.
					break;
				if( !full ) {
#ifdef FLOWER_DIAGNOSTIC_DEBUG
					if( edge >= (int)KEY_DATA_ENTRIES ) DebugBreak();
#endif
					//------------ 
					//  This huge block Shuffles the data in the array to the left or right 1 space
					//  to allow inserting the entryIndex in it's currently set spot.
					if( edge < entryIndex ) {
						if( edge == -1 ) {
							// collided on a leaf, but leaves to the left or right are available.
							// so definatly my other leaf is free, just rotate through the parent.
							int side = entryIndex & ( entryMask << 1 );
							int next = entryIndex;
							int nextToRight = 0;
							int p = entryIndex;
							int pMask = entryMask;
							while( pMask < ( KEY_DATA_ENTRIES / 2 ) ) {
								if( !TESTFLAG( hash->used_key_data, p >> 1 ) )break;
								if( p >= KEY_DATA_ENTRIES ) {
									side = 1; // force from-right
									break;
								}
								p = ( p | pMask ); p &= ~( pMask = ( pMask << 1 ) );
							}

							int nextMask;
							if( hash->key_data_offset[entryIndex] ) {
								while( ( next >= 0 ) && hash->key_data_offset[next - 1] ) next--;
								if( next > 0 ) {
									nextToRight = 0;
									nextMask = 1 << getLevel( next );
								}
								else {
									next = entryIndex;
									while( ( next < KEY_DATA_ENTRIES ) && hash->key_data_offset[next + 1] ) next++;
									if( next == KEY_DATA_ENTRIES ) {
#ifdef FLOWER_DIAGNOSTIC_DEBUG
										printf( "The tree is full to the left, full to the right, why isn't the root already full??" );
										DebugBreak();
#endif
									}
									else {
										nextToRight = 1;
										nextMask = 1 << getLevel( next );
									}
								}

								if( side ) {
									// I'm the greater of my parent.
									if( d > 0 ) {
										//printf( "01 Move %d to %d for %d\n", next - 0, next - 1, ( 1 + ( entryIndex - next ) ) );
										if( next < entryIndex ) {
#ifdef FLOWER_DIAGNOSTIC_DEBUG
											if( next <= 0 )DebugBreak();
#endif
											moveEntrySpan( hash, next, next - 1, 1+(entryIndex-next) );
											next = next - 1;
											nextMask = 1 << getLevel( next );
										} else {
											moveEntrySpan( hash, entryIndex, entryIndex+1, 1 + ( next - entryIndex ) );
											entryIndex = entryIndex + 1;
											entryMask = 1 << getLevel( next );
										}
									}
									else {
										//printf( "02 Move %d to %d for %d\n", next - 1, next - 2, ( 1 + ( entryIndex - next ) ) );
										if( next <= 1 ) {
											if( entryIndex > next ) {
												moveEntrySpan( hash, next, next - 1, 1+( entryIndex - next ) );
												entryIndex--;
												entryMask = 1 << getLevel( next );
												next = 0;
												nextMask = 1;
												//DebugBreak();
											}
											else {
												moveEntrySpan( hash, next, next - 1, ( next - entryIndex ) );
												entryIndex--;
												entryMask = 1 << getLevel( next );
												next = 0;
												nextMask = 1;
												//DebugBreak();
											}
										}
										else {
											if( nextToRight ) {
												moveEntrySpan( hash, entryIndex, entryIndex + 1, 1+( next - entryIndex ) );
												next = next++;
												nextMask = 1 << getLevel( next );
											}
											else {
												if( entryIndex - next ) {
													moveEntrySpan( hash, next, next - 1, ( entryIndex - next ) );
													entryIndex--;
													entryMask = 1 << getLevel( entryIndex );
												}
												else {
													moveOneEntry( hash, next, next - 1 );
												}
												next = next - 1;
												nextMask = 1 << getLevel( next );
											}
										}
									}
								}
								else {
									// I'm the lesser of my parent...
									if( d < 0 ) {
										//printf( "03 Move %d to %d for %d  %d\n", entryIndex + 1, entryIndex,  2 , ( entryIndex - next )  );
#ifdef FLOWER_DIAGNOSTIC_DEBUG
										if( entryIndex <= -1 )DebugBreak();
#endif
										if( entryIndex >= ( KEY_DATA_ENTRIES - 1 ) ) {
											moveOneEntry( hash, next, next - 1 );
											next = next - 1;
											nextMask = 1 << getLevel( next );
											entryIndex--;
											entryMask = 1 << getLevel( entryIndex );
										} else {
											if( entryIndex < next ) {
												moveEntrySpan( hash, entryIndex, entryIndex + 1, 1+(next-entryIndex) );
												next = entryIndex + 2;
												nextMask = 1 << getLevel( next );
											} else {
												moveEntrySpan( hash, next, next - 1, ( entryIndex - next ) );
												next--;
												nextMask = 1 << getLevel( next );
												entryIndex--;
												entryMask = 1 << getLevel( entryIndex );
											}
										}
									}
									else {
										//printf( "04 Move %d to %d for %d  %d\n", entryIndex+1, entryIndex+2,  1 , ( entryIndex - next ) );
#ifdef FLOWER_DIAGNOSTIC_DEBUG
										if( entryIndex <= -2 )DebugBreak();
#endif
										if( entryIndex >= (KEY_DATA_ENTRIES-1)) {
											moveTwoEntries( hash, next, next-1 );
											next--;
											nextMask = 1 << getLevel( next );
										} else {
											if( next > entryIndex ) {
												moveOneEntry( hash, entryIndex+1, entryIndex+2 );
												next = entryIndex + 2;
												nextMask = 1 << getLevel( next );
												entryIndex++;
#ifdef FLOWER_DIAGNOSTIC_DEBUG
												if( entryIndex >= KEY_DATA_ENTRIES )
													DebugBreak();
#endif
												entryMask = 1 << getLevel( entryIndex );
											}else{
												if( nextToRight ) {
													moveOneEntry( hash, next, next + 1 );
													next++;
													nextMask = 1 << getLevel( next );
												}
												else {
													moveEntrySpan( hash, next, next - 1, 1+(entryIndex - next) );
													next--;
													nextMask = 1 << getLevel( next );
												}
											}
										}
									}
								}
							}
						}
						else {
							if( leastEdge != KEY_DATA_ENTRIES ) {
								edge = leastEdge;
								edgeMask = leastMask;
							}
							//printf( "A Move %d to %d for %d  d:%d\n", edge + 1, edge, entryIndex - edge, d );
							if( d < 0 )
								entryIndex--;

#ifdef FLOWER_DIAGNOSTIC_DEBUG
							if( edge < 0 )DebugBreak();
#endif
							moveEntrySpan( hash, edge+1, edge, entryIndex-edge );
						}
					}
					else {
						//printf( "B Move %d   %d to %d for %d\n", d, entryIndex, entryIndex + 1, edge - entryIndex );
						if( edge != leastEdge && leastEdge != KEY_DATA_ENTRIES )
							if( entryIndex > edge ) {
								if( leastEdge > edge ) {
									edge = leastEdge;
									edgeMask = leastMask;
								}
							} else {
								if( leastEdge < edge ) {
									edge = leastEdge;
									edgeMask = leastMask;
								}
							}
							if( d > 0 ) {
								entryIndex++;
#ifdef FLOWER_DIAGNOSTIC_DEBUG
								if( entryIndex >= KEY_DATA_ENTRIES )
									DebugBreak();
#endif

							}
							if( edge >= KEY_DATA_ENTRIES ) {

							}
#ifdef FLOWER_DIAGNOSTIC_DEBUG
							if( entryIndex <= -1 )DebugBreak();
#endif
							moveEntrySpan( hash, entryIndex, entryIndex + 1, edge - entryIndex );
					}
#ifdef FLOWER_DIAGNOSTIC_DEBUG
					if( entryIndex >= KEY_DATA_ENTRIES ) {
						DebugBreak();
					}
#endif
					//printf( "A Store at %d %s\n", entryIndex, toBinary( entryIndex ) );
					// this insert does not change the fullness of this node
					hash->key_data_offset[entryIndex] = saveKeyData( hash, key, (uint8_t)keylen );
#ifdef FLOWER_HASH_ENABLE_LIVE_USER_DATA
					hash->entries[entryIndex].user_data_ = userPointer;
#endif
					userPointer[0] = &hash->entries[entryIndex].stored_data;
#ifdef HASH_DEBUG_BLOCK_DUMP_INCLUDED
					if( dump )
						dumpBlock( hash );
					if( dump )
						printf( "Added at %d\n", entryIndex );
					validateBlock( hash );
#endif
					//return hash->entries + entryIndex;
					hash_[0] = hash;
					return;
				}
				break;
			}
			else {
			// this entry is free, save here...
#ifdef FLOWER_DIAGNOSTIC_DEBUG
			if( entryIndex >= KEY_DATA_ENTRIES )
				DebugBreak();
#endif
			hash->key_data_offset[entryIndex] = saveKeyData( hash, key, (uint8_t)keylen );
			//printf( "B Store at %d  %s\n", entryIndex, toBinary( entryIndex ) );
			if( !( entryIndex & 1 ) ) { // filled a leaf.... 
				if( ( entryMask == 1 && ( entryIndex ^ 2 ) >= KEY_DATA_ENTRIES ) || hash->key_data_offset[entryIndex ^ ( entryMask << 1 )] ) { // if other leaf is also used
					updateFullness( hash, entryIndex, entryMask );
				}
			}
#ifdef FLOWER_HASH_ENABLE_LIVE_USER_DATA
			hash->entries[entryIndex].user_data_ = userPointer;
#endif
			userPointer[0] = &hash->entries[entryIndex].stored_data;
#ifdef HASH_DEBUG_BLOCK_DUMP_INCLUDED
			if( dump )
				dumpBlock( hash );
			if( dump )
				printf( "Added at %d\n", entryIndex );
			validateBlock( hash );
#endif
				//return hash->entries + entryIndex;
			hash_[0] = hash;
			return;
			}
		}
		if( hash->info.dense || hash->info.immutable ) {
			if( !( next = hash->next_block[key[0] & HASH_MASK] ) ) {
				if( hash->info.convertible )
					// can move entries from here to free up space when 
					// making a new block...
					next = convertFlowerHashBlock( hash );
				else {
					next = ( hash->next_block[key[0] & HASH_MASK] = InitFlowerHash( 0 ) );
					next->info = hash->info;
					next->info.used = 0;
					next->info.used_key_data = 0;
					//next->info.usedEntries = 0;
				}
				if( hash->parent ) {
					key++;
					keylen--;
				}
			}
			hash = next;
		}
		else break;
	}
	hash_[0] = hash;
}

// pull one of the nodes below me into this spot.
static void bootstrap( struct flower_hash_lookup_block* hash, int entryIndex, int entryMask ) {
	int c1, c2 = 0, c3;
	int cMask;
	if( entryMask > 1 ) {
		if( entryIndex >= 0 ) {
			c1 = -1;
			if( !hash->key_data_offset[c2 = entryIndex - 1] ) {
				for( cMask = entryMask >> 1, c2 = entryIndex & ( ~cMask ); cMask > 0; ) {
					if( !hash->key_data_offset[c2] ) {
						break;
					}
					c1 = c2;
					cMask >>= 1;
					c2 = c2 & ~( cMask ) | ( cMask << 1 );
				}
			}
			else c1 = c2;
		} else c1 = -1;
		if( ( c2 || c1<0 ) && entryIndex <= KEY_DATA_ENTRIES ) {
			if( !hash->key_data_offset[c3 = entryIndex + 1] ) {
				c2 = KEY_DATA_ENTRIES;
				for( cMask = entryMask>>1, c3 = entryIndex & ( ~cMask )|(cMask<<1); cMask > 0; ) {
					if( !hash->key_data_offset[c3] ) {
						break;
					}
					c2 = c3;
					cMask >>= 1;
					c3 = c3 & ( ~cMask );
				}
			}
			else c2 = c3;
		} else c2 = KEY_DATA_ENTRIES;;
		if( c1 >= 0 && ( ( c2 >= KEY_DATA_ENTRIES ) || ( entryIndex - c1 ) < ( c2 - entryIndex ) ) ) {
			moveOneEntry( hash, c1, entryIndex );
			hash->key_data_offset[c1] = 0;
			cMask = 1 << getLevel( c1 );
			if( c1 & 1 )
				bootstrap( hash, c1, cMask );
			else
				updateEmptiness( hash, c1 & ~( cMask << 1 ) | ( cMask ), cMask << 1 );
		}
		else if( ( ( c2 ) < KEY_DATA_ENTRIES ) && hash->key_data_offset[c2] ) {
			moveOneEntry( hash, c2, entryIndex );
			hash->key_data_offset[c2] = 0;
			cMask = 1 << getLevel( c2 );
			if( c2 & 1 )
				bootstrap( hash, c2, cMask );
			else
				updateEmptiness( hash, c2 & ~( cMask <<1) | ( cMask ), cMask<<1 );
		}else
			hash->key_data_offset[entryIndex] = 0;
	} else {
		// just a leaf node, clear emptiness and be done.
		updateEmptiness( hash, entryIndex, entryMask<<1 );
	}
}


static void deleteFlowerHashEntry( struct flower_hash_lookup_block* hash, int entryIndex, int entryMask )
{
	//if( entryIndex == 373 )DebugBreak();
	
#if 0
		// this can catch the tree in a certain (unique?) state... (copy row of dumped 'USED' bits)
	{
		char tests[][KEY_DATA_ENTRIES] = {
			"11110001000101111101111111111111111111110111011111010001000100010101111111111111111100010101000111110001010111110101111111111111110101010001000100000001011101011111111111111111111111010001010100010101111111110101111111111111111111110000000100000001000101010101111111111111110100010001000111111111111100011111111111111111111111111111110101010001000111111111111111111111000000",
		};
		int b;  // bit
		int t = 0; // test
		for( t = 0; t < sizeof( tests ) / sizeof( tests[0] ); t++ ) {
			for( b = 0; b < KEY_DATA_ENTRIES; b++ ) {
				if( tests[t][b] ) {
					if( tests[t][b] == '1' ) { if( !hash->key_data_offset[b] )break; }
					else if( tests[t][b] == '0' ) { if( hash->key_data_offset[b] )break; }
				}
			}
			if( b == KEY_DATA_ENTRIES ) {
				DebugBreak();
				break;
			}
		}
	}
#endif
	if( entryMask > 1 ) {
		squashKeyData( hash, entryIndex );
		bootstrap( hash, entryIndex, entryMask );
	}
	else {
		squashKeyData( hash, entryIndex );
		hash->key_data_offset[entryIndex] = 0;
		updateEmptiness( hash, entryIndex &~(entryMask<<1)|entryMask, entryMask<<1 );
	}
#ifdef HASH_DEBUG_BLOCK_DUMP_INCLUDED
	//dumpBlock( hash );
	validateBlock( hash );
#endif
}


void DeleteFlowerHashEntry( struct flower_hash_lookup_block* hash, uint8_t const* key, size_t keylen )
{
	uintptr_t* t;
	int entryIndex, entryMask;
	lookupFlowerHashKey( &hash, key, keylen, &t, &entryIndex, &entryMask );
	if( t ) {
		deleteFlowerHashEntry( hash, entryIndex, entryMask );
	}
}


struct flower_hash_lookup_block* convertFlowerHashBlock( struct flower_hash_lookup_block* hash ) {

	FPI nameoffset_temp = 0;
	struct flower_hash_lookup_block* new_dir_block;
	
	static int counters[HASH_MASK+1];
	uint8_t* namebuffer;
	namebuffer = hash->key_data_first_block;
	int maxc = 0;
	int imax = 0;
	int f_;
	int f;
	uint8_t* name_block = hash->key_data_first_block;
	// read name block chain into a single array

	memset( counters, 0, sizeof( counters ) );

	for( f = 0; f < KEY_DATA_ENTRIES; f++ ) {
		FPI name;
		int count;
		name = hash->key_data_offset[f];
		if( name ) {
			count = ( ++counters[namebuffer[name] & HASH_MASK] );
			if( count > maxc ) {
				imax = namebuffer[name] & HASH_MASK;
				maxc = count;
			}
		}
	}
	//printf( "--------------- Converting into hash:%d\n", imax );
	// after finding most used first byte; get a new block, and point
	// hash entry to that.
	hash->next_block[imax]
		= ( new_dir_block
			= InitFlowerHash(0) );
	new_dir_block->info = hash->info;
	new_dir_block->info.used = 0;
	new_dir_block->info.used_key_data = 0;

	maxc = 0;
	{
		struct flower_hash_lookup_block* newHash;
		uint8_t* newFirstNameBlock;
		size_t const memSize = hash->info.key_data_blocks*KEY_BLOCK_SIZE;
		newHash = new_dir_block;

		newFirstNameBlock = newHash->key_data_first_block;

		newHash->parent = hash;
		int b = 0;
		for( f_ = 0; 1; f_++ ) {
			int fMask;
			fMask = 0;
			for( ; b < KEY_DATA_ENTRIES_BITS; b++ ) {
				if( f_ < ( fMask = ( ( 1 << ( b+1) ) - 1 ) ) )
					break;
			}
			if( !fMask ) 
				break;
			f = treeEnt( f_ - ( ( fMask ) >> 1 ), b, KEY_DATA_ENTRIES_BITS );
			if( f >= KEY_DATA_ENTRIES ) 
				continue;
			//printf( "test at %d  -> %d   %d  %d   %02x  ch:%02x\n", f_, f, b, f_ - ( ( fMask ) >> 1 ), fMask, hash->key_data_offset[f] );


			FPI const name = hash->key_data_offset[f];
			if( name ) {
				int newlen = namebuffer[name - 1] - ( ( !hash->parent ) ? 0 : 1 );
				//printf( "Found %d  %02x = %02x at %d(%d)\n", name, namebuffer[name], namebuffer[name]&HASH_MASK, f, f_ );
				if( newlen >= 0 ) {
					if( ( namebuffer[name] & HASH_MASK ) == imax ) {
						uintptr_t* ptr;
						//lprintf( "MOVE %d %d %d", f, imax, namebuffer[name] );
						//LogBinary( ( ( !hash->parent ) ? 0 : 1 ) + namebuffer + name, namebuffer[name - 1] - ( ( !hash->parent ) ? 0 : 1 ) );
						insertFlowerHashEntry( &newHash
							, ( ( !hash->parent ) ? 0 : 1 ) + namebuffer + name
							, namebuffer[name - 1] - ( ( !hash->parent ) ? 0 : 1 )
							, &ptr );
						ptr[0] = hash->entries[f].stored_data;
						deleteFlowerHashEntry( hash, f, 1 << getLevel( f ) );
						maxc++;
						f_--;  // retry this record... with new value in it.
					}
				}
				else {
					printf( "key has no data, must match in this block\n" );
				}
			}
		}
#if 0
		// did we get rid of all the things expected to move?
		if( counters[imax] - maxc ) DebugBreak();
		memset( counters, 0, sizeof( counters ) );

		for( f = 0; f < KEY_DATA_ENTRIES; f++ ) {
			FPI name;
			int count;
			name = hash->key_data_offset[f];
			if( name ) {
				count = ( ++counters[namebuffer[name] & HASH_MASK] );
			}
		}
		if( counters[imax] )DebugBreak();

#endif
//		validateBlock( hash );
		return newHash;
	}
}



void AddFlowerHashEntry( struct flower_hash_lookup_block *hash, uint8_t const* key, size_t keylen, uintptr_t**userPointer ) {
	insertFlowerHashEntry( &hash, key, keylen, userPointer );
}

void AddFlowerHashIntEntry( struct flower_hash_lookup_block* hash, int key, uintptr_t** userdata ) {
	AddFlowerHashEntry( hash, (uint8_t*)&key, sizeof( int ), userdata );
}

#if 0 
// want a fancy macro?
#define DeclareTypeAdder( tpyeName, type )  void AddFlowerHash##typeName##Entry( struct flower_hash_lookup_block *hash, type key, uintptr_t** userdata ) { AddFlowerHashEntry( hash, (uint8_t const*)&key, sizeof(type), userdata ); }

// etc...
DeclareTypeAdder( Int32, int32_t );
DeclareTypeAdder( Uint32, uint32_t );

DeclareTypeAdder( Float32, float );
DeclareTypeAdder( Float64, double );
#endif

void DestroyFlowerHash( struct flower_hash_lookup_block* hash ) {
	int n;
	for( n = 0; n < HASH_MASK + 1; n++ ) {
		if( hash->next_block[n] ) DestroyFlowerHash( hash->next_block[n] );
	}
	// free aligned
	fa( hash->key_data_first_block );
	fa( hash );
}
