import {bitReader} from "./bits.mjs";

const bits = bitReader( 256 );

console.log( bits );
bits.set( 3 );
bits.get( 3 );
console.log( bits );
