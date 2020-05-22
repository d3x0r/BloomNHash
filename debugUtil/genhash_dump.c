
#define HASH_DEBUG_BLOCK_DUMP_INCLUDED

static void dumpBlock( struct flower_hash_lookup_block*hash ) {
	static char buf[512];
	int n;
	int o = 0;
	//printf( "\x1b[H\x1b[2J" );
	//printf( "\x1b[H" );
	{
		int b;
		for( b = 0; b < 8; b++ ) {
			o = 0;
			for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
				buf[o++] = ( n & ( 1 << ( KEY_DATA_ENTRIES_BITS - b -1) ) ) ? '1' : '0';
			}
			buf[o] = 0;
			printf( "    :%s\n", buf );
		}
	}

	o = 0;
	for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
		buf[o++] = '-';
	}
	buf[o] = 0;
	printf( "    :%s\n", buf );

	{
		o = 0;
		// output vertical number
		for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
			buf[o++] = ( n / 100 ) + '0';
		}
		buf[o] = 0;
		printf( "    :%s\n", buf );

		o = 0;
		for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
			buf[o++] = ( ( n / 10 ) % 10 ) + '0';
		}
		buf[o] = 0;
		printf( "    :%s\n", buf );

		o = 0;
		for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
			buf[o++] = ( ( n ) % 10 ) + '0';
		}
		buf[o] = 0;
		printf( "    :%s\n", buf );
	}

	o = 0;
	for( n = 0; n < KEY_DATA_ENTRIES/2; n++ ) {
		buf[o++] = ' ';
		if( TESTFLAG( hash->used_key_data, n ) ) buf[o++] = '1'; else buf[o++] = '0';
	}
	buf[o] = 0;
	printf( "FULL:%s\n", buf );


	o = 0;
	for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
		if( hash->key_data_offset[n] ) buf[o++] = '1'; else buf[o++] = '0';
	}
	buf[o] = 0;
	printf( "USED:%s\n", buf );

	{
		// output empty/full tree in-levels
		int l;
		for( l = 7; l >= 0; l-- ) {
			o = 0;
			for( n = 0; n < KEY_DATA_ENTRIES; n++ ) {
				if( ( n & ( 0xFF >> l ) ) == ( 127 >> l ) ) {
					if( l < 7 )
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
			printf( "    :%s\n", buf );
		}
	}

	{
		// output empty/full tree in-levels
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
				printf( "    :%s\n", buf );
		}
	}
}
