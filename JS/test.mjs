
import {default as util} from 'util';
import {BloomNHash} from "./bloomNHash.mjs"


const hash = new BloomNHash();

var n;
var keys = []
for( n = 0; n < 500; n++ ) {
	const key = Math.random() + "Key";
        keys.push( key );
	hash.set( key, n );

	for( let m = 0; m < keys.length; m++ ) {
		let val = hash.get( keys[m] );
	        if( val != m ) console.log( util.format( "Key-data mismatch!", m, val ) );
	}

}

console.log( "KEYS:", keys);
for( n = 0; n < 500; n++ ) {
	let val = hash.get( keys[n] );
        if( val != n ) console.log( util.format( "Key-data mismatch!", n, val ) );
}
