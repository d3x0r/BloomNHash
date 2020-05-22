

## Object storage filesystem interface

| method | args | description |
|---|---|---|
| getRoot() | none | Gets the root directory of the filesystem.  Resolves with a FileDirectory type object.  This also updates the object Storage object `root` value with the resulting value. |
| defineClass | (a,b) | defines a class in the default stringifier |
| scan | (from) | get list of changed files since a time |
| createIndex | (id,index) | |
| index | (obj,fieldName,opts) | create a index for an object |
| put | ( obj,opts) | returns a Promise that resolves when the data is output.  Opts is an object that allows specifying options of how to put the object.  |
| \_get | ( opts )  | Option parameter can either be a string of an object to read, or can be an object specifying read options (see below).  Returns a Promise that resolves with the object stored in the file specified.  |



### Object Storage put() Options

| Name | Meaning |
|---|---|
| id | use this ID for the file |
| extraEncoders | an array of objects specifying additional encoding options for the object passed. |


### Object Storage _get() options

| Name | Meaning |
|---|---|
| id | specific object to read |
| extraDecoders | an array of objects specifying additional decoding options to convert stored data to memory object. |




## File Directory Interface 

Filenames can be split using `/` and/or `\`, and each path part leading up to the final is a folder.

| method | args | description |
|---|---|---|
| open | ( filename ) | Returns a promise that throws if the file does not exist, and resolves with the directory information of the file. Returns a FileEntry type object |
| folder | ( filename ) | Returns a promise that throws if the folder does not exist, and resolves with a FileDirectory object that have that folder's directory loaded.|
| has | (filepath ) | returns a promise that resolves to true/false whether the path exists |


## File Entry Interface

| method | args | description |
|----|----|----|
| open | () | returns a promise that resolves with the directory content of this file |
| read | ( [optional from, or length if no length], [optional length] ) | returns a promise that resolves with the data in the file as an ArrayBuffer |
| write | (string/ArrayBuffer), [optional offset, or length to write if no length], [optional length] | returns a promise that resolves once setting the contents of the file to the passed string/array buffer completes. |
| on | ( eventName, callback ) | Defines event handlers for associated with the file. (?) on('data', /\* chunk of data callback \*/ ) |

