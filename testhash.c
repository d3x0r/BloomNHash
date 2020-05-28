#include "genhash.h"

#ifndef USE_SACK_PORTABILITY
typedef uint32_t TEXTRUNE;
int ConvertToUTF8( char *output, TEXTRUNE rune )
{
	int ch = 1;
	if( !( rune & 0xFFFFFF80 ) )
	{
		// 7 bits
		(*output++) = (char)rune;
		goto plus0;
	}
	else if( !( rune & 0xFFFFF800 ) )
	{
		// 11 bits
		(*output++) = 0xC0 | ( ( ( rune & 0x07C0 ) >> 6 ) & 0xFF );
		goto plus1;
	}
	else if( !( rune & 0xFFFF0000 ) )
	{
		// 16 bits
		(*output++) = 0xE0 | ( ( ( rune & 0xF000 ) >> 12 ) & 0xFF );
		goto plus2;
	}
	else //if( !( rune & 0xFFE00000 ) )
	{
		// 21 bits
		(*output++) = 0xF0 | ( ( ( rune & 0x1C0000 ) >> 15 ) & 0xFF );
	}
	ch++; (*output++) = 0x80 | (((rune & 0x03F000) >> 12) & 0xFF);
plus2:
	ch++; (*output++) = 0x80 | (((rune & 0x0FC0) >> 6) & 0xFF);
plus1:
	ch++; (*output++) = 0x80 | (rune & 0x3F);
plus0:
	return ch;
}
#endif

int main( void ) {
	int n;
	unsigned char mykey[8] = "KEY\0\0\0\0\0";
	struct flower_hash_lookup_block* root;
	uintptr_t* dataPtr;
	printf( "Blocks: %zd  %lld\n", sizeof( struct flower_hash_lookup_block ), KEY_DATA_ENTRIES );

	
	srand( 1 );
	// -- always lesser
	root = InitFlowerHash( IFHEO_CONVERTIBLE );
	for( n = 0; n < 237; n++ ) {
		ConvertToUTF8( mykey + 3, (~(rand() + 0x2000))&0xFFFFF );
		//ConvertToUTF8( mykey + 3, n );
		uintptr_t* data;
		AddFlowerHashEntry( root, mykey, strlen( mykey ), &data );
		data[0] = n;
	}

	if(1)
	{
		DestroyFlowerHash( root );
		root = InitFlowerHash( IFHEO_CONVERTIBLE );
		for( n = 0; n < 237000; n++ ) {
			int64_t r = ((int64_t)rand())* ((uint32_t)-1);
			char* mykey = (char*)&r;
			//r = r ^ ( r >> 24 ) ^ ( r << 8 ) ^ ( r << 16 );
			uintptr_t* data;
			AddFlowerHashEntry( root, (uint8_t*)&r, 8, &data );
			data[0] = n;
		}
	}


	{
		int j;
		int k;
		for( k = 0; k < 1000; k++ ) {
			printf( "Tick %d %d\n", k, k*5000*237 );
			for( j = 0; j < 5000; j++ ) {
				DestroyFlowerHash( root );
				root = InitFlowerHash( IFHEO_CONVERTIBLE );
				for( n = 0; n < 237; n++ ) {
					int64_t r = rand();
					char* mykey = (char*)&r;
					//r = r ^ ( r >> 24 ) ^ ( r << 8 ) ^ ( r << 16 );
					uintptr_t* data;
					AddFlowerHashEntry( root, mykey, 4, &data );
					data[0] = n;
				}
			}
		}
	}

	DestroyFlowerHash( root );
	root = InitFlowerHash( IFHEO_CONVERTIBLE );
	// key is greater than first key, but always lesser than itself.
	AddFlowerHashEntry( root, "my Key", 5, &dataPtr );
	dataPtr[0] = (uintptr_t)"MyData";
	mykey[0] = 'z';
	for( n = 0; n < 236; n ++ ) {
		ConvertToUTF8( mykey+3, 1000000-n );
		//ConvertToUTF8( mykey + 3, n );
		uintptr_t *data;
		AddFlowerHashEntry( root, mykey, strlen(mykey), &data );
		data[0] = n;
	}


	DestroyFlowerHash( root );
	root = InitFlowerHash( IFHEO_CONVERTIBLE );

	// key is greater than first key, but always lesser than itself.
	AddFlowerHashEntry( root, "my Key", 5, &dataPtr );
	dataPtr[0] = (uintptr_t)"MyData";
	mykey[0] = 'z';
	for( n = 0; n < 236; n++ ) {
		ConvertToUTF8( mykey + 3, 1000000 - n );
		//ConvertToUTF8( mykey + 3, n );
		uintptr_t* data;
		AddFlowerHashEntry( root, mykey, strlen( mykey ), &data );
		data[0] = n;
	}


	mykey[0] = 'K';

	// lesser than the first key, but always greater than than itself.
	DestroyFlowerHash( root );
	root = InitFlowerHash( IFHEO_CONVERTIBLE );
	
	AddFlowerHashEntry( root, "my Key", 5, &dataPtr );
	dataPtr[0] = (uintptr_t)"MyData";

	// always less than key; but more than each other.
	for( n = 0; n < 236; n++ ) {
		ConvertToUTF8( mykey+3, 1000000-n );
		//ConvertToUTF8( mykey + 3, n );
		uintptr_t* data;
		AddFlowerHashEntry( root, mykey, strlen( mykey ), &data );
		data[0] = n;
	}


	// lesser than the first key, but always greater than than itself.
	DestroyFlowerHash( root );
	root = InitFlowerHash( IFHEO_CONVERTIBLE );
	
	AddFlowerHashEntry( root, "my Key", 5, &dataPtr );
	dataPtr[0] = (uintptr_t)"MyData";

	// always less than key; but more than each other.
	for( n = 0; n < 236; n++ ) {
		//ConvertToUTF8( mykey+3, 1000000-n );
		ConvertToUTF8( mykey + 3, n );
		uintptr_t* data;
		AddFlowerHashEntry( root, mykey, strlen( mykey ), &data );
		data[0] = n;
	}

	// -- always greater
	DestroyFlowerHash( root );
	root = InitFlowerHash( IFHEO_CONVERTIBLE );
	for( n = 0; n < 237; n++ ) {
		//ConvertToUTF8( mykey+3, 1000000-n );
		ConvertToUTF8( mykey + 3, n );
		uintptr_t* data;
		AddFlowerHashEntry( root, mykey, strlen( mykey ), &data );
		data[0] = n;
	}

	// -- always lesser
	DestroyFlowerHash( root );
	root = InitFlowerHash( IFHEO_CONVERTIBLE );
	for( n = 0; n < 237; n++ ) {
		ConvertToUTF8( mykey+3, 1000000-n );
		//ConvertToUTF8( mykey + 3, n );
		uintptr_t* data;
		AddFlowerHashEntry( root, mykey, strlen( mykey ), &data );
		data[0] = n;
	}






	{
		char *result;
		result = (char*)FlowerHashShallowLookup( root, "my key", 5 );
		printf( "Retreived: %s\n", result );
	}


}

