

```
import {BloomNHash} from "./bloomNHash.mjs"

{
	const hash = new BloomNHash();
        hash.set( "asdf", 1 );
        const value = hash.get( "asdf" );
	// value = 1.
        hash.delete( "asdf" );
}
