## Memory
We have two heaps, one for symbols and another for everything else.  This enables us to identify whether something is a symbol or not by looking at the pointer value.

Or maybe have a string space??

Lots of spaces seems a good way to go:

* Cons space
* String space
* Function space

Things in cons space will need a mark bit for GC.

## Lisp objects
An object or object reference is represented as an unsigned 64-bit integer.  Pointers are 8-byte aligned so we can use the bottom three bits for tagging as follows:

| Bits |Use            |Notes                                       |
|------|---------------|--------------------------------------------|
| 000  | Signed integer| Shift right three bits to get actual value |
| 001  | Symbol        |                                            |
| 010  | Cons          |                                            |
| 011  | String        |                                            |
| 100  | Short string  | Next 5 bits hold length (only need 3), remaining 7 bytes hold data|
| 101  | Function      |                                            |


### Numbers
The only supported numeric type is the signed integer.  The integer value can be obtained from:

    int64_t numeric_value = (int64_t) lisp_object >> 3;

Since integers are not tagged (or rather tagged with zero), arithmetic can be performed directly, with the result shifted right as above.

### Strings

Options for representing strings:

   * List of characters
      * This kind of sucks
   * Tagged pointer to NUL-terminated string in special string memory
      * Quite easy to implement; would suck slightly to use C strings
   * Blobs with headers in the heap
      * Seems quite nice

If we have a string type, we ideally need a character type.

But sounds like strings are really just byte arrays.

#### Blob + header option

This is not a Lisp object, so does not have type tagging.  It can just be a length field followed by the data, followed by padding to 8-byte alignment.  Maybe 4-byte length header.  Or a `size_t`-sized one

#### Short string optimization

We could also have a short string that fits into 8 bytes including tag and header.  3 bits type, 5 bits length, up to 7 bytes data.


### Symbols
A symbol is a value pointed to by an object reference whose referent is a symbol.  A symbol is always passed by reference; the symbol itself is in the symbol table.

#### Symbol table
A symbol reference points to a vector of 64-bit reference values:

   1. Value cell
   2. Function cell
   3. Pointer to symbol name (a nul-terminated string)
   4. Unused for now

How do we know what a reference value points to?  Need type tagging either in the reference or in the value.

We will need a string-to-symbol index for the "reader".  I think this is the job of a package in Common Lisp.  Would be nice to represent this as an alist using Lisp structures.  Think we need a Lisp string type.


### NIL and T
`NIL` and `T` are represented as a special binary values.  These need to be values that are not a valid pointer or integer.  They need to satisfy `SYMBOLP`; if we choose the appropriate values we get this for free via tagging.  Let's use

|Symbol| Binary value|
|------|-------------|
|`NIL`   |`0xfffffffffffffff9`|
|`T`     |`0xfffffffffffffff1`|

Although these do not require special treatment in type checking, they will require special cases when printing.

## Garbage collection
Would be nicest to have a copying collector, then no need for a mark bit.  Roots for GC scanning are
   * Global variables
   * Stack
