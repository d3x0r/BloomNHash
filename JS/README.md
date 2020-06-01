

```
import {BloomNHash} from "./bloomNHash.mjs"

{
	const hash = new BloomNHash();
        hash.set( "asdf", 1 );
        hash.get( "asdf" );
        hash.delete( "asdf" );
}
