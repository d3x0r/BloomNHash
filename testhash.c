#include "genhash.h"

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

	{
		DestroyFlowerHash( root );
		root = InitFlowerHash( IFHEO_CONVERTIBLE );
		for( n = 0; n < 237000; n++ ) {
			int64_t r = rand();
			char* mykey = (char*)&r;
			//r = r ^ ( r >> 24 ) ^ ( r << 8 ) ^ ( r << 16 );
			uintptr_t* data;
			AddFlowerHashEntry( root, mykey, 4, &data );
			data[0] = n;
		}
	}


	{
		int j;
		int k;
		for( k = 0; k < 1000; k++ ) {
			printf( "Tick %d\n", k );
			for( j = 0; j < 500000; j++ ) {

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

