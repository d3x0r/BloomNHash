#include <math.h>
#include <stdio.h>
#include "sack.h"


int masks[32][2];

void initMasks( void ){
	int n;
	for( n= 0; n < 32; n++ ) {
		masks[n][0] = ~(0x2 << n);
		masks[n][1] = (0x1 << n );
	}
}


#define testZero_(n,bias,else)     ( (!(n&(1<<(bias))))?(bias+1):(else))
#define firstZero__(n,bias,else)   testZero_(n,bias+0, testZero_(n,bias+1,testZero_(n,bias+2, testZero_(n,bias+3,else )) ) )
#define firstZero8_(n,bias,else)   firstZero__( n,bias  , firstZero__( n, 4+bias, else ) )
#define firstZero16_(n,bias,else)  firstZero8_( n,bias , firstZero8_( n, 8+bias, else ) )
#define firstZero32_(n,bias,else)  firstZero16_( n,bias, firstZero16_( n, 16+bias, else ) )
#define firstZero64_(n,bias,else)  firstZero32_( n,bias, firstZero32_( n, 32+bias, else ) )

#define firstZero(n) firstZero32_(n,0,0)




int masks[32][2];

#ifdef __GNUC__

int upTreeUnrolledLookup( int N ) {
	int n = __builtin_ffs( ~N );
	return (N & ~((0x1)<<(n+1))) | ( 0x01 << n );
	//return (N & masks[n][0]) | masks[n][1];
}

#endif


int upTreeUnrolledLookup2( int N ) {
	int n = firstZero( N );
	
	return (N & masks[n][0]) | masks[n][1];
}


int upTreeUnrolled( int N ) {
	int n = firstZero( N );
	int const val = (0x1)<<(n);
	return (N & ~val) | ( val>>1 );
}

int upTree( int N ) {
	int n;
	for( n = 0; n < 32; n++ ) if( !(N&(1<<n)) ) break;

#if 0
return (N & ~((0x2)<<(n))) | ( 0x01 << n );
        not     eax
        and     edi, eax
        mov     eax, edi
        bts     eax, ecx
        ret

return (N & ~((0x1)<<(n+1))) | ( 0x01 << n );
        btr     edi, ecx
        mov     eax, edi
        bts     eax, edx
        ret


	mov     eax, 2
        sal     eax, cl
        mov     edx, eax
return (N & ~(val<<1)) | (val);
	add     eax, eax
        not     eax
        and     edi, eax
        mov     eax, edi
        or      eax, edx
        ret

	mov     edx, 2
        sal     edx, cl
return (N & ~val) | ( val>>1 );
	mov     eax, edx
        not     eax
        and     edi, eax
        mov     eax, edx
        sar     eax
        or      eax, edi
        ret
#endif

	//return (N & ~((0x1)<<(n+1))) | ( 0x01 << n );

#if 0
   0x000000000040158f <+31>:    mov    r8d,0x1
   0x0000000000401595 <+37>:    mov    r9d,r8d
   0x0000000000401598 <+40>:    shl    r9d,cl
   0x000000000040159b <+43>:    mov    ecx,r9d
   0x000000000040159e <+46>:    not    ecx
   0x00000000004015a0 <+48>:    and    eax,ecx
   0x00000000004015a2 <+50>:    mov    ecx,edx
   0x00000000004015a4 <+52>:    shl    r8d,cl
   0x00000000004015a7 <+55>:    or     eax,r8d
   0x00000000004015aa <+58>:    ret
#endif

	int const val = (0x1)<<(n+1);
	return (N & ~val) | ( val>>1 );
#if 0
   0x000000000040158d <+29>:    mov    $0x1,%edx
   0x0000000000401592 <+34>:    shl    %cl,%edx
   0x0000000000401594 <+36>:    mov    %edx,%ecx
   0x0000000000401596 <+38>:    sar    %edx
   0x0000000000401598 <+40>:    not    %ecx
   0x000000000040159a <+42>:    and    %ecx,%eax
   0x000000000040159c <+44>:    or     %edx,%eax
   0x000000000040159e <+46>:    retq
#endif

//	return (N & ~(val<<1)) | (val);
}

int upTreeL( int N ) {
	int n = (int)(floor(log2( N ))-1);
	return (N & ~((0x3)<<n)) | ( 0x01 << n ) ;
}

uint8_t bits[]= {0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,7,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0};
int upTreeLk( int N ) {
	int n = bits[N&0xFF];
	if(!n) { n = bits[(N>>8)&0xFF];
		if(!n) { n = bits[(N>>16)&0xFF];
			if(!n) { n = bits[(N>>24)&0xFF];
				if(!n) n += 24;
			} else n += 16;
		} else n += 8;
	}
	return (N & ~((0x3)<<n)) | ( 0x01 << n ) ;
}

void test(void ) {
	uint64_t n;
	uint64_t start = timeGetTime64ns();
   uint64_t end;
	for( n = 0; n < 100000000; n++ )
		upTree( n );
	end = timeGetTime64ns();
   printf( "Scan Did %Ld in %Ld.%Ld\n", n, (end-start)/1000000000ULL, (end-start)%1000000000ULL );

	start = timeGetTime64ns();
	for( n = 0; n < 100000000; n++ )
		upTreeUnrolled( n );
	end = timeGetTime64ns();
   printf( "Unrolled Did %Ld in %Ld.%Ld\n", n, (end-start)/1000000000ULL, (end-start)%1000000000ULL );

	start = timeGetTime64ns();
	for( n = 0; n < 100000000; n++ )
		upTreeUnrolledLookup( n );
	end = timeGetTime64ns();
   printf( "Lookup Did %Ld in %Ld.%Ld\n", n, (end-start)/1000000000ULL, (end-start)%1000000000ULL );
}


int main( void )
{
	initMasks();

	printf( "Test1: %d %d\n", 23, upTree( 23 ) );
	printf( "Test1: %d %d\n", 23, upTreeUnrolled( 23 ) );
	printf( "Test1: %d %d\n", 23, upTreeUnrolledLookup( 23 ) );
	test();
	test();
	test();
	test();
	test();
	test();

}

