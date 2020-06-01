
import {default as util} from 'util';
import {BloomNHash} from "./bloomNHash.mjs"


const hash = new BloomNHash();

var start, end;
var n;
var keys = []

for( var z = 0; z < 10; z++ ) {

start = Date.now()
for( n = 0; n < 50000; n++ ) {
	const key = Math.random() + "Key";
        keys.push( key );
	hash.set( key, n );
	if(0)
	for( let m = 0; m < keys.length; m++ ) {
		let val = hash.get( keys[m] );
	        if( val != m ) console.log( util.format( "INSERT Key-data mismatch!", m, val, "expect", keys[m] ) );
	}

}
end = Date.now();
console.log( "Did",n,"in",end-start, "for", n/((end-start)/1000) );
start = end;


for( n = 0; n < 50000; n++ ) {
	let val = hash.get( keys[n] );
        if( val != n ) {
		console.log( util.format( "LOOKUP Key-data mismatch!", n, val ) );
	}
}

end = Date.now();
console.log( "Did",n,"in",end-start, "for", n/((end-start)/1000) );
start = end;
}