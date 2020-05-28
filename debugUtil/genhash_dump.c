
#include <math.h>
#define HASH_DEBUG_BLOCK_DUMP_INCLUDED

static void dumpBlock_( struct flower_hash_lookup_block*hash, int opt ) {
	static char buf[512];
	static char leader[32];
	int n;
	int o = 0;
	if( !opt ) {
		//printf( "\x1b[H\x1b[3J" );
		//printf( "\x1b[H" );
		while( hash->parent ) hash = hash->parent;
	}
	else {
		for( n = 0; n < opt; n++ )
			leader[n] = '\t';
		leader[n] = 0;
	}
	printf( "HASH TABLE: %p parent %p\n", hash, hash->parent );

	if( 0 ) {
		// this is binary dump of number... just a pretty drawing really
		{
			int b;
			for( b = 0; b < KEY_DATA_ENTRIES_BITS; b++ ) {
				o = 0;
				for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
					buf[o++] = ( n & ( 1 << ( KEY_DATA_ENTRIES_BITS - b - 1 ) ) ) ? '1' : '0';
				}
				buf[o] = 0;
				printf( "%s    :%s\n", leader, buf );
			}
		}

		o = 0;
		for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
			buf[o++] = '-';
		}
		buf[o] = 0;
		printf( "%s    :%s\n", leader, buf );
	}

	{
		o = 0;
		// output vertical number
		for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
			buf[o++] = ( n / 100 ) + '0';
		}
		buf[o] = 0;
		printf( "%s    :%s\n", leader, buf );

		o = 0;
		for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
			buf[o++] = ( ( n / 10 ) % 10 ) + '0';
		}
		buf[o] = 0;
		printf( "%s    :%s\n", leader, buf );

		o = 0;
		for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
			buf[o++] = ( ( n ) % 10 ) + '0';
		}
		buf[o] = 0;
		printf( "%s    :%s\n", leader, buf );
	}

	o = 0;
	for( n = 0; n < KEY_DATA_ENTRIES/2; n++ ) {
		buf[o++] = ' ';
		if( TESTFLAG( hash->used_key_data, n ) ) buf[o++] = '1'; else buf[o++] = '0';
	}
	buf[o] = 0;
	printf( "%sFULL:%s\n", leader, buf );


	o = 0;
	for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
		if( hash->key_data_offset[n] ) buf[o++] = '1'; else buf[o++] = '0';
	}
	buf[o] = 0;
	printf( "%sUSED:%s\n", leader, buf );

	if(0)
	{
		// output empty/full tree in-levels
		int l;
		for( l = (KEY_DATA_ENTRIES_BITS-1); l >= 0; l-- ) {
			o = 0;
			for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
				if( ( n & ( 0x1FF >> l ) ) == ( 255 >> l ) ) {
					if( l < (KEY_DATA_ENTRIES_BITS-1) )
						if( TESTFLAG( hash->used_key_data, n >> 1 ) )
							buf[o++] = '*';
						else
							buf[o++] = '-';
					else
						if( hash->key_data_offset[n] )
							buf[o++] = '*';
						else
							buf[o++] = '-';
				}
				else
					buf[o++] = ' ';
			}
			buf[o] = 0;
			printf( "%s    :%s\n", leader, buf );
		}
	}

	{
		// output key data bytes tree in-levels
		int l;
		for( l = 0; l < 20; l++ ) {
			int d; d = 0;
			o = 0;
			for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
				if( hash->key_data_offset[n] 
					&& (l /2)<= hash->key_data_first_block[hash->key_data_offset[n]-1]  
					) {
					d = 1;
					buf[o] = ( hash->key_data_first_block[hash->key_data_offset[n]-1+l/2]);
					if( !(l & 1) ) buf[o] >>= 4;
					buf[o] &= 0xF;
					if( buf[o] < 10 ) buf[o] += '0';
					else buf[o] += 'A' - 10;

					o++;
				} else
					buf[o++] = ' ';
			}
			buf[o] = 0;
			if( d )
				printf( "%s    :%s\n", leader, buf );
		}
	}

	if( 0 ) {
		// dump name offset value ... (meaningless really; 0 or not zero)
		o = 0;
		for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
			buf[o++] = '-';
		}
		buf[o] = 0;
		printf( "%s    :%s\n", leader, buf );
		{
			int b;
			for( b = 0; b < 4; b++ ) {
				o = 0;
				for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
					buf[o++] = ( (int)( hash->key_data_offset[n] / pow( 10, 3 - b ) ) % 10 ) + '0';
				}
				buf[o] = 0;
				printf( "%s    :%s\n", leader, buf );
			}
		}
	}

	for( n = 0; n < ( HASH_MASK + 1 ); n++ ) {
		if( hash->next_block[n] ) dumpBlock_( hash->next_block[n], opt+1 );
	}

}

static void dumpBlock( struct flower_hash_lookup_block* hash ) {
	dumpBlock_( hash, 0 );
}