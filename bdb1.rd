=begin
= BDB1

((<Download|URL:ftp://moulon.inra.fr/pub/ruby/>))

This interface if for the version 1.85 and 1.86 of Berkeley DB (for Berkeley
version >= 2 see bdb) 

Developers may choose to store data in any of several different
storage structures to satisfy the requirements of a particular
application. In database terminology, these storage structures and the
code that operates on them are called access methods. The library
includes support for the following access methods:

   * B+tree: Stores keys in sorted order, using a default function that does
     lexicographical ordering of keys.
       
   * Hashing: Stores records in a hash table for fast searches based
     on strict equality, using a default that hashes on the key as a bit
     string. Extended Linear Hashing modifies the hash function used by the
     table as new records are inserted, in order to keep buckets underfull in
     the steady state.
       

   * Fixed and Variable-Length Records. Stores fixed- or
     variable-length records in sequential order.

Acces Methods
   
    * ((<Hash like interface|URL:docs/hashlike1.html>))
      these are the common methods for BDB1::Btree, BDB1::Hash

    * ((<Array like interface|URL:docs/arraylike1.html>))
      methods for BDB1::Recnum (same than BDB1::Recno)

=end
      