
#include "genhash.h"


#include "genhash_dump.c"


struct perfTickCounters {
	int moves_left;
	int moves_right;
	int lenMovedLeft[128];
	int lenMovedRight[128];

};
static struct perfTickCounters pft;

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
	void* newBlock = allocate_aligned( newSize, align );
	memcpy( newBlock, p, oldSize );
	free( p );
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

// this results in an absolute disk position
// pass the hash_lookup_block which is gaining a new key value.
// pass the key and key length (offset in bytes as nessecary)
// dropHash will drop the least significant HASH_MASK_BITS bits from
// the least significant byte.  (leaving the remaining bits)

static FPI saveKeyData( struct flower_hash_lookup_block *hash, const char *key, size_t keylen ) {
	uint8_t * this_name_block = hash->key_data_first_block;
	uint8_t * start = this_name_block;
	size_t max_key_data = hash->info.key_data_blocks * KEY_BLOCK_SIZE;
	//int keydatalen;
	while( 1 ) {
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
void lookupFlowerHashKey( struct flower_hash_lookup_block **root, uint8_t const *key, size_t keylen, uintptr_t **userdata ) {
	if( !key ) {
		userdata[0] = NULL;
		return; // only look for things, not nothing.
	}
	int ofs = 0;

	struct flower_hash_lookup_block *hash = root;
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
	lookupFlowerHashKey( &root, key, keylen, &t );
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
	// zero means, track as index number...
	hash->info.userRecordSize = 0;// sizeof( struct key_data_entry );
	return hash;
}

struct flower_hash_lookup_block* CreateFlowerHashMap( enum insertFlowerHashEntryOptions options, void* userDataBuffer, size_t userRecordSize, int maxRecords ) {
	struct flower_hash_lookup_block* hash = NewPlus( struct flower_hash_lookup_block, sizeof( struct key_data_entry ) * KEY_DATA_ENTRIES );
	memset( hash, 0, sizeof( *hash ) );
	expand_key_data_space( hash );
	hash->entries = userDataBuffer;
	hash->info.userRecordSize = userRecordSize;
	hash->key_data_first_block[0] = 0;
	hash->info.dense = ( options & IFHEO_DENSE ) ? 1 : 0;
	hash->info.convertible = ( options & IFHEO_CONVERTIBLE ) ? 1 : 0;
	hash->info.immutable = ( options & IFHEO_IMMUTABLE ) ? 1 : 0;
	return hash;

}


static void updateEmptiness( struct flower_hash_lookup_block* hash, int entryIndex, int entryMask ) {
	while( ( entryMask < KEY_DATA_ENTRIES / 2 ) && ( ( entryIndex > KEY_DATA_ENTRIES ) || TESTFLAG( hash->used_key_data, entryIndex >> 1 ) ) ) {
		if( entryIndex < KEY_DATA_ENTRIES )
			RESETFLAG( hash->used_key_data, entryIndex >> 1 );
		entryIndex &= ~( entryMask << 1 ) | entryMask;
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
				} else {
					// this could just always be inserting into a place that IS full; and doesn't BECOME full...
					printf( "Parent should NOT be full. (if it was, " );
					DebugBreak();
					break; // parent is already full.
				}
			}
			else {

				int tmpIndex = pIndex;
				int tmpMask = entryMask;
				while( tmpMask && ( tmpIndex = tmpIndex & ~tmpMask ) ) { // go to left child of this out of bounds parent..
					if( tmpMask == 1 ) {
						if( tmpIndex >= KEY_DATA_ENTRIES || hash->key_data_offset[tmpIndex] ) {
							break;
						}
						break;
					}
					else {
						if( TESTFLAG( hash->used_key_data, tmpIndex >> 1 ) ) {
							break;
						}
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
					// move this to the parent, and empty old.
					hash->entries[parent] = hash->entries[entryIndex];

					// have to at least clear 'used'
					//hash->key_data_offset[entryIndex] = 0;
					memset( hash->entries + entryIndex, 0, sizeof( hash->entries[0] ) );
					entryIndex = parent;
					entryMask = parentMask;
				}
				else break;

			}
		}
	}
	else
		if( !hash->key_data_offset[entryIndex] ) {
			return;
		}

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
//				if( ( parent & ( parentMask << 2 ) ) ) {  
					// came from the right side.
					parent = parent & ~( parentMask << 1 ) | parentMask;
					parentMask <<= 1;

					//  check if value is empty, and swap
					if( !hash->key_data_offset[parent] ) {
						hash->entries[parent] = hash->entries[entryIndex];
						memset( hash->entries + entryIndex, 0, sizeof( hash->entries[0] ) );
						entryIndex = parent;
						entryMask = parentMask;
						continue;
					}
//				}
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
	int n = __builtin_ffs( ~N );
#else
	int n;
	for( n = 0; n < 32; n++ ) if( !(N&(1<<n)) ) break;
#endif
	return n;	
}

static void insertFlowerHashEntry( struct flower_hash_lookup_block* *hash_
	, uint8_t const* key
	, int keylen
	, uintptr_t** userPointer
	, enum insertFlowerHashEntryOptions  opt_immutable
) {
	struct flower_hash_lookup_block* hash = hash_[0];
	int level = 1;
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
		static int c;
		c++;
#ifdef HASH_DEBUG_BLOCK_DUMP_INCLUDED
		//if( c > 3454100 )
	//		dump = 1;
#endif
		//if( c == 236 ) DebugBreak();
	}
	if( !hash->info.dense ) {
		struct flower_hash_lookup_block* next;
		// not... dense
		do {
			if( hash->info.convertible || hash->info.dense ) {
				if( full )
					convertFlowerHashBlock( hash );
			}

			next = hash->next_block[key[0] & HASH_MASK];
			if( hash->parent ) {
				key++; keylen--;
			}
			if( next )
				hash = next;
		} while( next );
	}
	while( 1 ) {
		int d_ = 1;
		int d = 1;
		while( 1 ) {

			int offset;
			// if entry is in use....
			if( mustLeftEnt || ( offset = hash->key_data_offset[entryIndex] ) ) {
				// compare the keys.
				if( d ) {
					d = mustLeftEnt ? -1 : (int)( keylen - hash->key_data_first_block[offset - 1] );
					if( d == 0 ) {
						d = memcmp( key, hash->key_data_first_block + offset, keylen );
						// not at a leaf...
					}
				}

				if( !full && !opt_immutable )
					if( edge < 0 )
						if( TESTFLAG( hash->used_key_data, ( entryIndex >> 1 ) ) ) {
							edgeMask = entryMask;
							// already full... definitly will need a peer.
							edge = entryIndex ^ ( entryMask << 1 );
							int b;
							if( edge > KEY_DATA_ENTRIES ) {

								// 1) immediately this edge is off the chart; this means the right side is also full (and all the dangling children)

								for( b = 0; ( edge > KEY_DATA_ENTRIES ) && b < 8; b++ ) {
									edge = ( edge & ~( edgeMask << 1 ) ) | ( edgeMask );
									edgeMask <<= 1;
								}

								for( b = b; b > 0; b-- ) {
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
					if( !opt_immutable )
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
										if( TESTFLAG( hash->used_key_data, next >> 1 ) ) {
											printf( "The parent of this node should already be marked full.\n" );
											DebugBreak();
										}
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
												if( TESTFLAG( hash->used_key_data, next >> 1 ) ) {
													printf( "The parent of this node should already be marked full.\n" );
													DebugBreak();
												}
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
									else {
										if( TESTFLAG( hash->used_key_data, next >> 1 ) ) {
											printf( "The parent of this node should already be marked full.\n" );
											DebugBreak();
										}
									}
								}

							edge = next;
							edgeMask = entryMask>>1;
							//printf( "Edge follow: %d %s %d  %s\n", edge, toBinary( edge ), entryIndex, toBinary( entryIndex ) );
						}
					if( d_ ) {
						entryIndex &= ~( entryMask >> 1 );
						if( d > 0 ) entryIndex |= entryMask;
						if( entryIndex >= KEY_DATA_ENTRIES ) mustLeftEnt = 1; else mustLeftEnt = 0;
					}
					else break;
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

				// this key needs to go here.
				// the value that IS here... d < 0 is needs to move left
				// if the d > 0 , then this value needs to move to the right.

				if( hash->info.immutable ) // if entries may not move, stop here, and store in hash.
					break;
				if( !full ) {
#if 0
					// this can catch the tree in a certain (unique?) state... (copy row of dumped 'USED' bits)
					{
						char tests[][KEY_DATA_ENTRIES] = { 
							  "00000001000000010000000101010101000000000000000100000000000000011111111111111111000100011111010100000000000000000000000000000001111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111101010011110100010000000000000001000101010000000100000000000000010000000000000001000000000000000100000001000101111111111111111111111111111111",
							  //"00000000000000000000000000000001000000000000000000000000000000010000000000000000000000000000000100000000000000010000000000000001000000000000000000000000000000010000000000000000000000000000000100000000000000000000000000000001000000000000000000000000000000010000000000000001000000010001000100000000000000010000000000000001111111110000000100000001000000111111111111111111111111110101",
						};
						int b;
						int t = 0;
						for( t = 0; t < sizeof( tests ) / sizeof(tests[0]); t++ )
							for( b = 0; b < KEY_DATA_ENTRIES; b++ ) {
								if( tests[t][b] ) {
									if( tests[t][b] == '1' ) { if( !hash->key_data_offset[b] )break; }
									else if( tests[t][b] == '0' ) { if( hash->key_data_offset[b] )break; }
								}
							}
						if( b == KEY_DATA_ENTRIES )DebugBreak();
					}
#endif
					if( edge < entryIndex ) {
						if( edge == -1 ) {
							// collided on a leaf, but leaves to the left or right are available.
							// so definatly my other leaf is free, just rotate through the parent.
							int side = entryIndex & ( entryMask << 1 );
							int next = entryIndex;
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
							while( ( next >= 0 ) && hash->key_data_offset[next - 1] ) next--;
							if( next > 0 ){
								nextMask = 1 << getLevel( next );
							} else {
								next = entryIndex;
								while( ( next < KEY_DATA_ENTRIES ) && hash->key_data_offset[next + 1] ) next++;
								if( next == KEY_DATA_ENTRIES ) {
									printf( "The tree is full to the left, full to the right, why isn't the root already full??" );
									DebugBreak();
								}
								else {
									nextMask = 1 << getLevel( next );
								}
							}

							{
								if( side ) {
									// I'm the greater of my parent.
									if( d > 0 ) {
										pft.lenMovedLeft[2]++;
										pft.moves_left++;
										//printf( "01 Move %d to %d for %d\n", next - 0, next - 1, ( 1 + ( entryIndex - next ) ) );
										if( next < entryIndex ) {
											if( next <= 0 )DebugBreak();
											memmove( hash->entries + next - 1, hash->entries + next - 0, ( 1 + ( entryIndex - next ) ) * sizeof( *hash->entries ) );
											next = next - 1;
											nextMask = 1 << getLevel( next );
										} else {

											memmove( hash->entries + entryIndex + 1, hash->entries + entryIndex + 0, ( 1 + ( next - entryIndex ) ) * sizeof( *hash->entries ) );
											entryIndex = entryIndex + 1;
											entryMask = 1 << getLevel( next );
										}
									}
									else {
										pft.lenMovedLeft[1]++;
										pft.moves_left++;
										//printf( "02 Move %d to %d for %d\n", next - 1, next - 2, ( 1 + ( entryIndex - next ) ) );
										if( next <= 1 ) {
											memmove( hash->entries + next - 1, hash->entries + next, ( ( entryIndex - next ) ) * sizeof( *hash->entries ) );
											entryIndex--;
											entryMask = 1 << getLevel( next );
											next = 0;
											nextMask = 1;
											//DebugBreak();
										}
										else {
											if( next > entryIndex ) {
												memmove( hash->entries + entryIndex + 1, hash->entries + entryIndex, ( 1 + ( next - entryIndex ) ) * sizeof( *hash->entries ) );;
												next = next++;
												nextMask = 1 << getLevel( next );
											}
											else {
												memmove( hash->entries + next - 1, hash->entries + next, ( 1 + ( entryIndex - next ) ) * sizeof( *hash->entries ) );;
												next = next - 1;
												nextMask = 1 << getLevel( next );
											}
										}
									}
								}
								else {
									// I'm the lesser of my parent...
									if( d < 0 ) {
										pft.lenMovedRight[2]++;
										pft.moves_right++;
										//printf( "03 Move %d to %d for %d  %d\n", entryIndex + 1, entryIndex,  2 , ( entryIndex - next )  );
										if( entryIndex <= -1 )DebugBreak();
										if( entryIndex >= ( KEY_DATA_ENTRIES - 1 ) ) {
											memmove( hash->entries + next -1, hash->entries + next, 1 * sizeof( *hash->entries ) );;
											next = next - 1;
											nextMask = 1 << getLevel( next );
											entryIndex--;
											entryMask = 1 << getLevel( entryIndex );
										}
										else {
											if( entryIndex < next ) {
												memmove( hash->entries + entryIndex + 1, hash->entries + entryIndex + 0, 2 * sizeof( *hash->entries ) );;
												next = entryIndex + 2;
												nextMask = 1 << getLevel( next );
											} else {
												memmove( hash->entries + next - 1, hash->entries + next, 2 * sizeof( *hash->entries ) );;
												next--;
												nextMask = 1 << getLevel( next );
											}
										}
									}
									else {
										pft.lenMovedLeft[1]++;
										pft.moves_right++;
										//printf( "04 Move %d to %d for %d  %d\n", entryIndex+1, entryIndex+2,  1 , ( entryIndex - next ) );
										if( entryIndex <= -2 )DebugBreak();
										if( entryIndex >= 235 ) {
											memmove( hash->entries + next-1, hash->entries + next, 2 * sizeof( *hash->entries ) );;
											next--;
											nextMask = 1 << getLevel( next );
										}
										else {
											memmove( hash->entries + entryIndex + 2, hash->entries + entryIndex + 1, 1 * sizeof( *hash->entries ) );;
											next = entryIndex + 2;
											nextMask = 1 << getLevel( next );
											entryIndex++;
											if( entryIndex >= KEY_DATA_ENTRIES )
												DebugBreak();
											entryMask = 1 << getLevel( entryIndex );
										}
									}
								}
							}

							if( next != entryIndex
								&& !(next&1)
								&& ( hash->key_data_offset[next ^ 2] )
								&& !TESTFLAG( hash->used_key_data, next>>1) ) { // if other leaf is also used
								updateFullness( hash, next, nextMask );
							} else if( ( ( entryIndex ^ ( entryMask ) ) >= KEY_DATA_ENTRIES ) ) {
								int cEntry = entryIndex ^ ( entryMask );
								int cMask = entryMask>>1;
								while( cMask > 1 ) {
									cEntry = cEntry & ~( cMask >>1 );
									if( cEntry < KEY_DATA_ENTRIES ) {
										if( TESTFLAG( hash->used_key_data, entryIndex >> 1 ) ) { // if other leaf is also used
											updateFullness( hash, entryIndex, entryMask );
										}
										break;
									}
									cMask >>= 1;
								}
							} else if( !(entryIndex&1) 
								&& ( entryIndex ^ ( entryMask << 1 ) ) < KEY_DATA_ENTRIES
								&& ( hash->key_data_offset[entryIndex ^ ( entryMask << 1 )] ) 
								&& !TESTFLAG( hash->used_key_data, entryIndex >> 1 ) ) { // if other leaf is also used
								updateFullness( hash, entryIndex, entryMask );
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
							pft.lenMovedLeft[( entryIndex - edge ) >> 1]++;
							pft.moves_left++;

							if( edge < 0 )DebugBreak();
							memmove( hash->entries + edge, hash->entries + edge + 1, ( entryIndex - edge ) * sizeof( *hash->entries ) );
							// this may update fullness...
							upLeftSideTree( hash, edge, edgeMask );
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
							if( entryIndex >= KEY_DATA_ENTRIES )
								DebugBreak();

						}
						pft.lenMovedLeft[( edge - entryIndex ) >> 1]++;
						pft.moves_left++;
						if( entryIndex <= -1 )DebugBreak();
						memmove( hash->entries + entryIndex + 1, hash->entries + entryIndex, ( edge - entryIndex ) * sizeof( *hash->entries ) );
						// this may update fullness...
						//entryIndex = 
						upRightSideTree( hash, edge, edgeMask );
					}
					if( entryIndex >= KEY_DATA_ENTRIES )
						DebugBreak();
					//printf( "A Store at %d %s\n", entryIndex, toBinary( entryIndex ) );
					// this insert does not change the fullness of this node
					hash->key_data_offset[entryIndex] = saveKeyData( hash, key, keylen );
#ifdef FLOWER_HASH_ENABLE_LIVE_USER_DATA
					hash->entries[entryIndex].user_data_ = userPointer;
#endif
					userPointer[0] = &hash->entries[entryIndex].stored_data;
#ifdef HASH_DEBUG_BLOCK_DUMP_INCLUDED
					if( dump  )
					dumpBlock( hash );
#endif
					//return hash->entries + entryIndex;
					hash_[0] = hash;
					return;
				}
				break;
			}
			else {
				// this entry is free, save here...
				if( entryIndex >= KEY_DATA_ENTRIES )
					DebugBreak();
				hash->key_data_offset[entryIndex] = saveKeyData( hash, key, keylen );
				//printf( "B Store at %d  %s\n", entryIndex, toBinary( entryIndex ) );
				if( !( entryIndex & 1 ) ) { // filled a leaf.... 
					if( ( entryMask == 1 && (entryIndex ^ 2) >= KEY_DATA_ENTRIES ) || hash->key_data_offset[entryIndex ^ ( entryMask << 1 )] ) { // if other leaf is also used
						int broIndex;
						int pIndex = entryIndex;
						int pMask = entryMask;

						do {
							int pLevel;
							pIndex = ( pIndex & ~( pLevel = pMask << 1 ) ) | pMask; // go to the parent
							if( pIndex < KEY_DATA_ENTRIES ) {
								if( !TESTFLAG( hash->used_key_data, pIndex >> 1 ) )
									SETFLAG( hash->used_key_data, pIndex >> 1 ); // set node full status.
								else {
									printf( "Parent should NOT be full. (if it was, " );
									DebugBreak();
									break; // parent is already full.
								}
							}
							broIndex = pIndex ^ ( pLevel<<1);
							pMask <<= 1;
							// and then while the parent's peer also 
						} while( (broIndex>=KEY_DATA_ENTRIES) || ( hash->key_data_offset[broIndex] 
							&& TESTFLAG( hash->used_key_data, broIndex >> 1 ) ) );
					}
				}
#ifdef FLOWER_HASH_ENABLE_LIVE_USER_DATA
				hash->entries[entryIndex].user_data_ = userPointer;
#endif
				userPointer[0] = &hash->entries[entryIndex].stored_data;
#ifdef HASH_DEBUG_BLOCK_DUMP_INCLUDED
				if( dump )
				dumpBlock( hash );
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
					next = ( hash->next_block[key[0] & HASH_MASK] = InitFlowerHash(0) );
					next->info = hash->info;
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

static void bootstrap( struct flower_hash_lookup_block* hash, int entryIndex, int entryMask ) {
	int c1 = entryIndex & ~( entryMask >> 1 );
	entryMask >>= 1;
	FPI n;
	if( n = hash->key_data_offset[c1] ) {
		hash->key_data_offset[entryIndex] = n;
		hash->entries[entryIndex] = hash->entries[c1];
		hash->key_data_offset[c1] = 0;
		if( entryMask > 1 )
			bootstrap( hash, c1, entryIndex, entryMask );
	}
	else if( n = hash->key_data_offset[c1|entryMask] ) {
		hash->key_data_offset[entryIndex] = n;
		hash->entries[entryIndex] = hash->entries[c1];
		hash->key_data_offset[c1] = 0;
		if( entryMask > 1 )
			bootstrap( hash, c1, entryIndex, entryMask );
	}
	else {
		updateEmptiness( hash, entryIndex, entryMask );
	}
}


static void deleteFlowerHashEntry( struct flower_hash_lookup_block* hash, int entryIndex, int entryMask )
{
	uintptr_t* data;
	hash->key_data_offset[entryIndex] = 0;
	if( entryMask > 1 ) {
		
	}
	
}


void DeleteFlowerHashEntry( struct flower_hash_lookup_block* hash, uint8_t const* key, int keylen )
{
	uintptr_t* data;

}


struct flower_hash_lookup_block* convertFlowerHashBlock( struct flower_hash_lookup_block* hash ) {

	FPI nameoffset_temp = 0;
	struct flower_hash_lookup_block* new_dir_block;
	
	static int counters[HASH_MASK];
	static uint8_t* namebuffer;
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
	// after finding most used first byte; get a new block, and point
	// hash entry to that.
	hash->next_block[imax]
		= ( new_dir_block
			= InitFlowerHash(0) );
	new_dir_block->info = hash->info;


	{
		struct flower_hash_lookup_block* newHash;
		uint8_t* newFirstNameBlock;
		size_t const memSize = hash->info.key_data_blocks*KEY_BLOCK_SIZE;
		newHash = new_dir_block;

		newFirstNameBlock = newHash->key_data_first_block;

		newHash->parent = hash;
		int b = 0;
		for( f_ = 0; f_ < KEY_DATA_ENTRIES; f_++ ) {
			int fMask;

			for( ; b < KEY_DATA_ENTRIES_BITS; b++ ) {
				if( f_ < ( fMask = ( ( 1 << ( b+1) ) - 1 ) ) )
					break;
			}
			f = treeEnt( f_ - ( ( fMask ) >> 1 ), b, KEY_DATA_ENTRIES_BITS );
			if( f > KEY_DATA_ENTRIES ) continue;

			FPI first = hash->key_data_offset[f];
			struct key_data_entry* entry;
			FPI name;
			entry = hash->entries + ( f );
			name = hash->key_data_offset[f];
			if( ( namebuffer[name] & HASH_MASK ) == imax ) {
				uintptr_t* ptr;
				insertFlowerHashEntry( &newHash
					                    , ((!hash->parent)?0:1)+namebuffer+name
					                    , namebuffer[name-1]-((!hash->parent)?0:1)
					                    , &ptr, 0 );
				ptr[0] = entry->stored_data;
				hash->key_data_offset[f] = 0;
				int entryMask = 1 << getLevel(f);
				updateEmptiness( hash, f, entryMask );
				int from = name + namebuffer[name - 1];
				int offset = namebuffer[name - 1] + 1;
				memmove( namebuffer + name - 1, namebuffer + offset-1, memSize - from );
				int g;
				for( g = f + 1; g < KEY_DATA_ENTRIES; g++ ) {
					if( hash->key_data_offset[g]  > name )
						hash->key_data_offset[g] -= offset;
				}
			}
		}
		return newHash;
	}
	// a set of names has been moved out of this block.
	// has block.

	// unlink here
	// unlink hash->key_data_first_block
}



void AddFlowerHashEntry( struct flower_hash_lookup_block *hash, uint8_t const* key, int keylen, uintptr_t**userPointer ) {
	insertFlowerHashEntry( &hash, key, keylen, userPointer, 0 );
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
