	
const KEY_DATA_ENTRIES_BITS = 8;
const KEY_DATA_ENTRIES = 1<<KEY_DATA_ENTRIES_BITS;
const HASH_MASK = 31;
const HASH_MASK_BITS = 5;
const ROOT_ENTRY_INDEX = 127;
const ROOT_ENTRY_MASK = ( 1 << (KEY_DATA_ENTRIES_BITS-1) );

const FLOWER_HASH_DEBUG_MOVES = false;
const FLOWER_TICK_PERF_COUNTERS = false

// this adds a bunch of sanity checks/ASSERT sort of debugging
const FLOWER_DIAGNOSTIC_DEBUG = 1
const pft = (!FLOWER_TICK_PERF_COUNTERS)?{
	 moves_left : 0,
	 moves_right : 0,
	 lenMovedLeft: new Array(128),
	 lenMovedRight : new Array(128)
}:null;

import {bitReader} from "./bits.mjs";

function BloomHash( comparer ) {
	
	this.root = new hashBlock();
	this.cmp = comparer;
}

BloomHash.prototype.get = function( key ) {
	lookupFlowerHashKey( this.root, key, null );
}


function hashBlock(  ){
	var n;
	this.nextBlock = [];
	for( n = 0; n <= HASH_MASK; n++ ) this.nextBlock.push(null);
	this.entries = [];
	for( n = 0; n < KEY_DATA_ENTRIES; n++ ) this.entries.push(null);
	this.used = bitReader( KEY_DATA_ENTRIES>>1 );
}
 

// get the number of most significant bit that's on
function maxBit(x) {
	var n;  for( n = 0; n < 32; n++ ) if( x & ( 1 << ( 31 - n ) ) ) break; return 32 - n; 
}

function linearToTreeIndex( f_ ) 
{
	var b;
	var fMask;
	for( b = 0; b < KEY_DATA_ENTRIES_BITS; b++ ) {
		if( f_ < ( fMask = ( ( 1 << ( b + 1 ) ) - 1 ) ) )
			break;
	}
	var f = treeEnt( f_ - ( ( fMask ) >> 1 ), b, KEY_DATA_ENTRIES_BITS );
	return f;
}



// returns pointr to user data value
function lookupFlowerHashKey( root, key, result ) {

	if( !key ) {
		userdata[0] = NULL;
		return; // only look for things, not nothing.
	}
	var ofs = 0;

	hash = root.root;
	var next_entries;

	// provide 'goto top' by 'continue'; otherwise returns.
	do {
		// look in the binary tree of keys in this block.
		let curName = ROOT_ENTRY_INDEX;//treeEnt( 0, 0, KEY_DATA_ENTRIES_BITS );
		let curMask = ROOT_ENTRY_MASK;
		next_entries = hash->entries;
		while( curMask )
		{
			let entry = ;
			if( !( entkey=hash->key_data_offset[curName] )) break; // no more entries to check.
			// sort first by key length... (really only compare same-length keys)
			let d = key.length - entkey.length;
			curName &= ~( curMask >> 1 );
			if( ( d == 0 ) && ( d = key.localeCompare(entkey ) ) == 0 ) {
				result.entryIndex = curName;
				result.entryMask = curMask;
				result.hash = hash;
				return hash->entries[curName];
			}
			if( d > 0 ) curName |= curMask;
			curMask >>= 1;
		}
		{
			// follow converted hash blocks...
			const nextblock = hash->next_block[key.codePointAt(0) & HASH_MASK];
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


function InitFlowerHash(  ) {
	const hash = new hashBlock();
	return hash;
}


function updateEmptiness( hash,  entryIndex,  entryMask ) {
	while( ( entryMask < KEY_DATA_ENTRIES ) && ( ( entryIndex > KEY_DATA_ENTRIES ) || TESTFLAG( hash->used, entryIndex >> 1 ) ) ) {
		if( entryIndex < KEY_DATA_ENTRIES && ( entryIndex & 1 ) ) {
			RESETFLAG( hash->used, entryIndex >> 1 );
			//dumpBlock( hash );
		}
		entryIndex = entryIndex & ~( entryMask << 1 ) | entryMask;
		entryMask <<= 1;
	}
}

function updateFullness( hash, entryIndex, entryMask ) {
	//if( hash->key_data_offset[entryIndex ^ (entryMask<<1)] ) 
	{ // if other leaf is also used
		var broIndex;
		var pIndex = entryIndex;
		do {
			pIndex = ( pIndex & ~(entryMask<<1) ) | entryMask; // go to the parent
			if( pIndex < KEY_DATA_ENTRIES ) { // this is full automatically otherwise 
				if( !TESTFLAG( hash->used, pIndex >> 1 ) ) {
					SETFLAG( hash->used, pIndex >> 1 ); // set node full status.
				} 
				else if( FLOWER_DIAGNOSTIC_DEBUG ){
					// this could just always be inserting int a place that IS full; and doesn't BECOME full...
					console.log( "Parent should NOT be full. (if it was, " );
					debugger;
					break; // parent is already full.
				}
			}
			else {

				var tmpIndex = pIndex;
				var tmpMask = entryMask;
				while( tmpMask && ( tmpIndex = tmpIndex & ~tmpMask ) ) { // go to left child of this out of bounds parent..
					if( tmpMask == 1 )  break;
					
					if( TESTFLAG( hash->used, tmpIndex >> 1 ) ) {
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
				var ok = 1;
				var tmpIndex = broIndex & ~entryMask;
				var tmpMask = entryMask>> 1;
				while( tmpMask && ( tmpIndex = tmpIndex & ~tmpMask ) ) { // go to left child of this out of bounds parent..
					if( tmpIndex < KEY_DATA_ENTRIES ) {
						if( tmpMask == 1 ) 
							ok = ( hash->key_data_offset[tmpIndex] != 0 );
						else
							ok = ( TESTFLAG( hash->used, tmpIndex >> 1 ) ) != 0;
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
					&& TESTFLAG( hash->used, broIndex >> 1 ) ) )
					continue;
			}
			// and then while the parent's peer also 
			if( entryMask >= (KEY_DATA_ENTRIES/2) )
				break;
			break;
		} while( 1);
	}
}


function moveOneEntry_( struct flower_hash_lookup_block* hash, var from, var to, var update );
#define moveOneEntry(a,b,c) moveOneEntry_(a,b,c,1)

// Rotate Right.  (The right most node may need to bubble up...)
// the only way this can end up more than max, is if the initial input
// is larger than the size of the tree.
function upRightSideTree( struct flower_hash_lookup_block* hash, var entryIndex, var entryMask ) {
	// if it's not at least this deep... it doesn't need balance.
	if( entryMask == 1 ) {
		if( !( entryIndex & 2 ) ) {
			var parent = entryIndex;
			var parentMask = entryMask;
			var bit = 0;
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
	var broIndex = entryIndex ^ ( entryMask << 1 );
	if( entryMask == 1 
		&& ( ( broIndex >= KEY_DATA_ENTRIES ) 
			|| hash->key_data_offset[broIndex] ) ) { // if other leaf is also used
		updateFullness( hash, entryIndex, entryMask );
	}
}

// rotate left
// left most node might have to bubble up.
function upLeftSideTree( struct flower_hash_lookup_block* hash, var entryIndex, var entryMask ) {
	if( ( entryIndex & 0x3 ) == 2 ) {
		while( 1 ) {
			var parent = entryIndex;
			var parentMask = entryMask;
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
	var broIndex = entryIndex ^ ( entryMask << 1 );
	if( entryMask == 1 && ( ( broIndex >= KEY_DATA_ENTRIES )
		|| hash->key_data_offset[broIndex] ) ) { // if other leaf is also used
		updateFullness( hash, entryIndex, entryMask );
	}
}

function getLevel( var N ) {
	return Math.floor(Math.log(n))-1;
	var n;
	for( n = 0; n < 32; n++ ) if( !(N&(1<<n)) ) break;
	return n;	
}


function validateBlock( struct flower_hash_lookup_block* hash ) {
	return;
	var realUsed = 0;
	var prior = nll;
	var n;
	var m;
	for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
		for( m = n+1; m < KEY_DATA_ENTRIES; m++ ) {
			if( hash->key_data_offset[n] && hash->key_data_offset[m] ) {
				if( hash->key_data_offset[n] === hash->key_data_offset[m] ) {
					console.log( "Duplicate key in %d and %d", n, m );
					debugger;
				}
			}
		}
	}

	for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
		if( hash->key_data_offset[n] ) {
			var nLevel = 1 << getLevel( n );
			if( ((n & ~( nLevel << 1 ) | nLevel) < KEY_DATA_ENTRIES )&&  !hash->key_data_offset[n & ~( nLevel << 1 ) | nLevel] ) {
				console.log( "Parent should always be filled:%d", n );
				debugger;
			}
			if( prior ) {
				if( prior.length > hash->key_data_offset.length ) {
					debugger;
				}
				if( prior.localeCompare( hash->key_data_offset[n]  ) > 0 ) {
					console.log( "Entry Out of order:%d", n );
					debugger;
				}
			}
			prior = hash->key_data_offset[n];
			realUsed++;
		}
	}
	hash->info.used = realUsed;
}


function moveOneEntry_( struct flower_hash_lookup_block* hash, var from, var to, var update ) {
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

function moveTwoEntries_( struct flower_hash_lookup_block* hash, var from, var to, var update ) {
#define moveTwoEntries(a,b,c) moveTwoEntries_(a,b,c,1)
	var const to_ = to;
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

function moveEntrySpan( struct flower_hash_lookup_block* hash,  from,  to, count ) {
	const to_ = to;
	const count_ = count;
if( FLOWER_DIAGNOSTIC_DEBUG ) {
	if( to >= KEY_DATA_ENTRIES || ( to + count ) >= KEY_DATA_ENTRIES ) debugger;
}
if( FLOWER_TICK_PERF_COUNTERS ){
	if( to < from ) {
		pft.lenMovedLeft[count]++;
		pft.moves_left++;
	}
	else {
		pft.lenMovedRight[count]++;
		pft.moves_right++;
	}
}
	if( to < 0 ) debugger;
	if( from > to ) {
		for( ; count > 1; count -= 2, from += 2, to += 2 ) moveTwoEntries_( hash, from, to, 0 );
		for( ; count > 0; count-- ) moveOneEntry_( hash, from++, to++, 0 );
	}
	else {
		for( from += count, to += count; count > 0; count-- ) moveOneEntry_( hash, --from, --to, 0 );
	}
	if( to < from ) {
		upLeftSideTree( hash, to_, 1 << getLevel( to_ ) );
	}
	else {
		upRightSideTree( hash, to_+ count_ -1, 1 << getLevel( to_ + count_ - 1 ) );
	}
}

var c = 0;  // static global

function insertFlowerHashEntry( root
	, key
	, result
) {
	if( keylen > 255 ) { console.log( "Key is too long to store." ); userPointer[0] = NULL; return; }
	struct flower_hash_lookup_block* hash = hash_[0];
	var entryIndex = treeEnt( 0, 0, KEY_DATA_ENTRIES_BITS );
	var entryMask = 1 << getLevel( entryIndex ); // index of my zero.
	var edge = -1;
	var edgeMask;
	var leastEdge = KEY_DATA_ENTRIES;
	var leastMask;
	var mustLeft = 0;
	var mustLeftEnt = 0;
	var full = 0;
if( HASH_DEBUG_BLOCK_DUMP_INCLUDED ) {
	var dump = 0;
}
	struct flower_hash_lookup_block* next;

	full = TESTFLAG( hash->used, entryIndex >> 1 ) != 0;
	{
		c++;
if( HASH_DEBUG_BLOCK_DUMP_INCLUDED ) {
	//	if( ( c > ( 294050 ) ) )
	//		dump = 1;
}
		//if( c == 236 ) debugger;
	}
	if( !hash->info.dense ) {
		struct flower_hash_lookup_block* next;
		// not... dense
		if( hash->parent && full )
			debugger;
		do {
			if( full && ( hash->info.convertible || hash->info.dense ) ) {
				convertFlowerHashBlock( hash );
				full = TESTFLAG( hash->used, entryIndex >> 1 ) != 0;
			}
			if( next = hash->next_block[key[0] & HASH_MASK] ) {
				if( hash->parent ) {
					key++; keylen--;
				}
				hash = next;
				full = TESTFLAG( hash->used, entryIndex >> 1 ) != 0;
			}
		} while( next );
	}

	while( 1 ) {
		var d_ = 1;
		var d = 1;
		while( 1 ) {

			const offset = hash->key_data_offset[entryIndex];
			// if entry is in use....
			if( mustLeftEnt || offset ) {
				// compare the keys.
				if( d && offset ) {
					// mustLeftEnt ? -1 : 
					d = key.length - offset.length;
					if( d == 0 ) {
						d = key.localeCompare( offset );
						if( d == 0 ) {
							// duplicate already found in tree, use existing entry.
							result.entryIndex = entryIndex;
							result.entryMask = entryMask;
							result.hash = hash;
							return;
						}
						// not at a leaf...
					}
				}

				if( !full && !hash->info.immutable )
					if( edge < 0 )
						if( TESTFLAG( hash->used, ( entryIndex >> 1 ) ) ) {
							edgeMask = entryMask;
							// already full... definitly will need a peer.
							edge = entryIndex ^ ( entryMask << 1 );
							if( edge > KEY_DATA_ENTRIES ) {
								var backLevels;
								// 1) immediately this edge is off the chart; this means the right side is also full (and all the dangling children)
								for( backLevels = 0; ( edge > KEY_DATA_ENTRIES ) && backLevels < 8; backLevels++ ) {
									edge = ( edge & ~( edgeMask << 1 ) ) | ( edgeMask );
									edgeMask <<= 1;
								}

								for( backLevels = backLevels; backLevels > 0; backLevels-- ) {
									var next = edge & ~( edgeMask >> 1 );
									if( ( ( next | ( edgeMask ) ) < KEY_DATA_ENTRIES ) ) {
										if( !TESTFLAG( hash->used, ( next | ( edgeMask ) ) >> 1 ) ) {
											edge = next | ( edgeMask );
										}
										else edge = next;
									}
									else {
										if( TESTFLAG( hash->used, ( next ) >> 1 ) ) {
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
							var next = edge & ~( entryMask >> 1 ); // next left.
							if( mustLeft ) { if( next < KEY_DATA_ENTRIES ) mustLeft = 0; }
							else
								if( edge > entryIndex ) {
									next = edge & ~( entryMask >> 1 ); // next left.
									//console.log( "Is Used?: %d  %d   %lld ", next >> 1, edge >> 1, TESTFLAG( hash->used, next >> 1 ) ) ;
									if( ( next & 1 ) && TESTFLAG( hash->used, next >> 1 ) ) {
										if( !mustLeft ) next |= entryMask; else mustLeft = 0;
										if( next >= KEY_DATA_ENTRIES )
											mustLeft = 1;
if( FLOWER_DIAGNOSTIC_DEBUG ) {
										if( TESTFLAG( hash->used, next >> 1 ) ) {
											console.log( "The parent of this node should already be marked full." );
											debugger;
										}
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
									//console.log( "Is Used?: %d  %d   %lld  %lld", ( next | entryMask ) >> 1, edge >> 1, TESTFLAG( hash->used, ( next | entryMask ) >> 1 ), TESTFLAG( hash->used, ( next ) >> 1 ) ) ;
									if( ( next | entryMask ) < KEY_DATA_ENTRIES ) {
										if( entryMask == 2 ) {
											if( !hash->key_data_offset[next | entryMask] )
												next |= entryMask;
										}
										else {
											if( !TESTFLAG( hash->used, ( next | entryMask ) >> 1 ) ) {
												if( entryMask != 2 || !hash->key_data_offset[next | entryMask] ) {
													if( !mustLeft ) next |= entryMask; else mustLeft = 0;
													if( next >= KEY_DATA_ENTRIES )
														mustLeft = 1;
												}
											}
										}
									} 
								}

							edge = next;
							edgeMask = entryMask>>1;
							//console.log( "Edge follow: %d %s %d  %s", edge, toBinary( edge ), entryIndex, toBinary( entryIndex ) );
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
					//------------ 
					//  This huge block Shuffles the data in the array to the left or right 1 space
					//  to allow inserting the entryIndex in it's currently set spot.
					if( edge < entryIndex ) {
						if( edge == -1 ) {
							// collided on a leaf, but leaves to the left or right are available.
							// so definatly my other leaf is free, just rotate through the parent.
							var side = entryIndex & ( entryMask << 1 );
							var next = entryIndex;
							var nextToRight = 0;
							var p = entryIndex;
							var pMask = entryMask;
							while( pMask < ( KEY_DATA_ENTRIES / 2 ) ) {
								if( !TESTFLAG( hash->used, p >> 1 ) )break;
								if( p >= KEY_DATA_ENTRIES ) {
									side = 1; // force from-right
									break;
								}
								p = ( p | pMask ); p &= ~( pMask = ( pMask << 1 ) );
							}

							var nextMask;
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
										console.log( "The tree is full to the left, full to the right, why isn't the root already full??" );
										debugger;
									}
									else {
										nextToRight = 1;
										nextMask = 1 << getLevel( next );
									}
								}

								if( side ) {
									// I'm the greater of my parent.
									if( d > 0 ) {
										//console.log( "01 Move %d to %d for %d", next - 0, next - 1, ( 1 + ( entryIndex - next ) ) );
										if( next < entryIndex ) {
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
										//console.log( "02 Move %d to %d for %d", next - 1, next - 2, ( 1 + ( entryIndex - next ) ) );
										if( next <= 1 ) {
											if( entryIndex > next ) {
												moveEntrySpan( hash, next, next - 1, 1+( entryIndex - next ) );
												entryIndex--;
												entryMask = 1 << getLevel( next );
												next = 0;
												nextMask = 1;
												//debugger;
											}
											else {
												moveEntrySpan( hash, next, next - 1, ( next - entryIndex ) );
												entryIndex--;
												entryMask = 1 << getLevel( next );
												next = 0;
												nextMask = 1;
												//debugger;
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
										//console.log( "03 Move %d to %d for %d  %d", entryIndex + 1, entryIndex,  2 , ( entryIndex - next )  );
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
										//console.log( "04 Move %d to %d for %d  %d", entryIndex+1, entryIndex+2,  1 , ( entryIndex - next ) );
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
							//console.log( "A Move %d to %d for %d  d:%d", edge + 1, edge, entryIndex - edge, d );
							if( d < 0 )
								entryIndex--;
							moveEntrySpan( hash, edge+1, edge, entryIndex-edge );
						}
					}
					else {
						//console.log( "B Move %d   %d to %d for %d", d, entryIndex, entryIndex + 1, edge - entryIndex );
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
							}
							if( edge >= KEY_DATA_ENTRIES ) {

							}
							moveEntrySpan( hash, entryIndex, entryIndex + 1, edge - entryIndex );
					}
					//console.log( "A Store at %d %s", entryIndex, toBinary( entryIndex ) );
					// this insert does not change the fullness of this node
					hash->key_data_offset[entryIndex] = saveKeyData( hash, key, (uvar8_t)keylen );
					userPointer[0] = &hash->entries[entryIndex].stored_data;
if( HASH_DEBUG_BLOCK_DUMP_INCLUDED ) {
					if( dump )
						dumpBlock( hash );
					if( dump )
						console.log( "Added at %d", entryIndex );
					validateBlock( hash );
}
					result.entryIndex = entryIndex;
					result.entryMask = entryMask;
					result.hash = hash;
					return;
				}
				break;
			}
			else {
			// this entry is free, save here...
			hash->key_data_offset[entryIndex] = saveKeyData( hash, key, (uvar8_t)keylen );
			//console.log( "B Store at %d  %s", entryIndex, toBinary( entryIndex ) );
			if( !( entryIndex & 1 ) ) { // filled a leaf.... 
				if( ( entryMask == 1 && ( entryIndex ^ 2 ) >= KEY_DATA_ENTRIES ) || hash->key_data_offset[entryIndex ^ ( entryMask << 1 )] ) { // if other leaf is also used
					updateFullness( hash, entryIndex, entryMask );
				}
			}
			hash->entries[entryIndex] = userPointer;

			userPointer[0] = &hash->entries[entryIndex].stored_data;

if( HASH_DEBUG_BLOCK_DUMP_INCLUDED ) {
			if( dump )
				dumpBlock( hash );
			if( dump )
				console.log( "Added at %d", entryIndex );
			validateBlock( hash );
}
			result.entryIndex = entryIndex;
			result.entryMask = entryMask;
			result.hash = hash;
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
}

// pull one of the nodes below me into this spot.
function bootstrap( hash, entryIndex, entryMask ) {
	var c1, c2 = 0, c3;
	var cMask;
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


function deleteFlowerHashEntry( hash, entryIndex, entryMask )
{
	if( entryMask > 1 ) {
		bootstrap( hash, entryIndex, entryMask );
	}
	else {
		hash->key_data_offset[entryIndex] = 0;
		updateEmptiness( hash, entryIndex &~(entryMask<<1)|entryMask, entryMask<<1 );
	}
	if( HASH_DEBUG_BLOCK_DUMP_INCLUDED) {
		//dumpBlock( hash );
		validateBlock( hash );
	}
}


function DeleteFlowerHashEntry( hash, key )
{
	const resultEx = {};
	const t = lookupFlowerHashKey( &hash, key, resultEx );
	if( t ) {
		deleteFlowerHashEntry( hash, resultEx.entryIndex, resultEx.entryMask );
	}
}


function convertFlowerHashBlock( hash ) {

	var new_dir_block;
	
	var counters = new Uint16Array(HASH_MASK+1);
	var maxc = 0;
	var imax = 0;
	var f_;
	var f;
	// read name block chain varo a single array

	for( f = 0; f < KEY_DATA_ENTRIES; f++ ) {
		name = hash->key_data_offset[f];
		if( name ) {
			let ch;
			let count = ( ++counters[ch = name.codePointAt(0) & HASH_MASK] );
			if( count > maxc ) {
				imax = ch;
				maxc = count;
			}
		}
	}
	//console.log( "--------------- Converting varo hash:%d", imax );
	// after finding most used first byte; get a new block, and povar
	// hash entry to that.

	maxc = 0;
	{
		const newHash = new hashBlock( hash );
		hash->next_block[imax] = newHash;

		newHash->parent = hash;
		var b = 0;
		for( f_ = 0; 1; f_++ ) {
			var fMask;
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
			//console.log( "test at %d  -> %d   %d  %d   %02x  ch:%02x", f_, f, b, f_ - ( ( fMask ) >> 1 ), fMask, hash->key_data_offset[f] );


			const name = hash->key_data_offset[f];
			if( name ) {
				var newlen = namebuffer[name - 1] - ( ( !hash->parent ) ? 0 : 1 );
				//console.log( "Found %d  %02x = %02x at %d(%d)", name, namebuffer[name], namebuffer[name]&HASH_MASK, f, f_ );
				if( newlen >= 0 ) {
					if( ( name.codePointAt(0) & HASH_MASK ) == imax ) {
						insertFlowerHashEntry( newHash, ( !hash->parent ) ? name : name.substr(1)  );
						ptr[0] = hash->entries[f].stored_data;
						deleteFlowerHashEntry( hash, f, 1 << getLevel( f ) );
						maxc++;
						f_--;  // retry this record... with new value in it.
					}
				}
				else {
					console.log( "key has no data, must match in this block" );
				}
			}
		}
//		validateBlock( hash );
		return newHash;
	}
}



