
const storages = [];

class BitReader {
       	entropy = null;
	storage_ = null;

	constructor( bits ) {
	
		if( "number" === typeof bits ) {
			bits = new Uint8Array( ( (bits+7)/8)|0 );
		}
	}

	hook( storage ) {
		if( !storages.find( s=>s===storage ) ) {
			storage.addEncoders( [ { tag:"btr", p:bitReader, f:this.encode } ] );
			storage.addDecoders( [ { tag:"btr", p:bitReader, f:this.decode } ] );
			storages.push( storage );
		}
		this.storage_ = storage;
	}
	encode( stringifier ){
		return `{e:${stringifier.stringify(this.entropy)}}`;
	}
	decode(field,val){
		if( field === "e" ) this.entropy = val;
		//if( field === "a" ) this.available = val;
		//if( field === "u" ) this.used = val;
		if( field )
			return undefined;
		else {
			this.storage_ = val;
		}
		return this;
	}
	get(N) {
		const bit = this.entropy[N>>3] & ( 1 << (N&7)) ;
		if( bit ) return true; 
		return false;
	}
	set(N) {
		this.entropy[N>>3] |= ( 1 << (N&7)) ;
	}
	getBit(N) {
		const bit = this.entropy[N>>3] & ( 1 << (N&7)) ;
		if( bit ) return true; 
		return false;
	}
	setBit(N) {
		this.entropy[N>>3] |= ( 1 << (N&7)) ;
	}
	clearBit(N) {
		this.entropy[N>>3] &= ~( 1 << (N&7)) ;
	}
	getBits(start, count, signed) {
		if( !count ) { count = 32; signed = true } 
		if (count > 32)
			throw "Use getBuffer for more than 32 bits.";
		var tmp = this.getBuffer(start, count);
		if( signed ) {
			var val = new Uint32Array(tmp)[0];
			if(  val & ( 1 << (count-1) ) ) {
				var negone = ~0;
				negone <<= (count-1);
				//console.log( "set arr_u = ", arr_u[0].toString(16 ) , negone.toString(16 ) );
				val |= negone;
			}
			return val;
		}
		else {
			var arr = new Uint32Array(tmp);
			return arr[0];
		}
	}

}

//module.exports = exports = bitReader;
export {bitReader};
