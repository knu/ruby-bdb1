# This interface if for the version 1.85 and 1.86 of Berkeley DB (for Berkeley
# version >= 2 see bdb) 
# 
# Developers may choose to store data in any of several different
# storage structures to satisfy the requirements of a particular
# application. In database terminology, these storage structures and the
# code that operates on them are called access methods.
#
# The following classes implement these access methods
# 
# * BDB1::Common
#
#   Implement common methods for access to data
#
# * BDB1::Btree < BDB1::Common
#
#   Implement specific methods for a B+tree. Keys are stored in sorted
#   order, using a default function that does lexicographical ordering of
#   keys.
#
# * BDB1::Hash < BDB1::Common
#
#   Hashing: Stores records in a hash table for fast searches based
#   on strict equality, using a default that hashes on the key as a bit
#   string. Extended Linear Hashing modifies the hash function used by the
#   table as new records are inserted, in order to keep buckets underfull in
#   the steady state.
#
# * BDB1::Recnum < BDB1::Common
#
#   Implement specific methods for fixed and variable-length records.
#   Stores fixed- or variable-length records in sequential order.
#
# * Exception : BDB1::Fatal < StandardError
#
module BDB1
end
