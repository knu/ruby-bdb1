=begin
== Acces Methods

These are the common methods for ((|BDB1::Btree|)), ((|BDB1::Hash|)),

* ((<Class Methods>))
* ((<Methods>))

=== Class Methods

--- create([name, flags, mode, options])
--- new([name, flags, mode, options])
--- open([name, flags, mode, options])
     open the database

       : ((|name|))
           The argument name is used as the name of a single physical
           file on disk that will be used to back the database.

       : ((|flags|))
         The flags must be the string "r", "r+", "w", "w+", "a", "a+" or
         and integer value.

         The flags value must be set to 0 or by bitwise inclusively
         OR'ing together one or more of the following values

           : ((|BDB1::CREATE|))
               Create any underlying files, as necessary. If the files
               do not already exist and the DB_CREATE flag is not
               specified, the call will fail. 

           : ((|BDB1::EXCL|))
                Return an error if the database already exists. Underlying
                filesystem primitives are used to implement this
                flag. For this reason it is only applicable to the
                physical database file and cannot be used to test if a
                subdatabase already exists. 

           : ((|BDB1::NOMMAP|))
                Do not map this database into process memory.

           : ((|BDB1::RDONLY|))
                Open the database for reading only. Any attempt to
                modify items in the database will fail regardless of the
                actual permissions of any underlying files. 

           : ((|BDB1::TRUNCATE|))
                Physically truncate the underlying database file,
                discarding all previous subdatabases or databases.
                Underlying filesystem primitives are used to implement
                this flag. For this reason it is only applicable to the
                physical database file and cannot be used to discard
                subdatabases.

                The DB_TRUNCATE flag cannot be transaction protected,
                and it is an error to specify it in a transaction
                protected environment. 

           : ((|BDB1::WRITE|))
               Open the database for writing. Without this flag, any
               attempt to modify items in the database will fail.

       : ((|options for all classes|))
         Hash, Possible options are

           : ((|set_flags|))
                general database configuration
           : ((|set_cachesize|))
                set the database cache size
           : ((|set_pagesize|))
                set the underlying database page size
           : ((|set_lorder|))
                set the database byte order
           : ((|set_store_key|))
                specify a Proc called before a key is stored
           : ((|set_fetch_key|))
                specify a Proc called after a key is read
           : ((|set_store_value|))
                specify a Proc called before a value is stored
           : ((|set_fetch_value|))
                specify a Proc called after a value is read

       : ((|options specific to BDB1::Btree|))

           : ((|set_bt_compare|))
                specify a Btree comparison function 
           : ((|set_bt_minkey|))
                set the minimum number of keys per Btree page
           : ((|set_bt_prefix|))
                specify a Btree prefix comparison function 

       : ((|options specific to BDB1::Hash|))

           : ((|set_h_ffactor|))
                set the Hash table density
           : ((|set_h_hash|))
                specify a hashing function 
           : ((|set_h_nelem|))
                set the Hash table size

Proc given to ((|set_bt_compare|)), ((|set_bt_prefix|)), 
((|set_h_hash|)), ((|set_store_key|)), ((|set_fetch_key|)),
((|set_store_value|)) and ((|set_fetch_value|)) can be also specified as
a method (replace the prefix ((|set_|)) with ((|bdb1_|)))

For example 

            module BDB1
               class Btreesort < Btree
                  def bdb1_bt_compare(a, b)
                     b.downcase <=> a.downcase
                  end
               end
            end
 
=== Methods

--- self[key]
     Returns the value corresponding the ((|key|))

--- db_get(key [, flags])
--- get(key [, flags])
--- fetch(key [, flags])
     Returns the value correspondind the ((|key|))

--- self[key] = value
     Stores the ((|value|)) associating with ((|key|))

     If ((|nil|)) is given as the value, the association from the key will be
     removed. 

--- db_put(key, value [, flags])
--- put(key, value [, flags])
--- store(key, value [, flags])
     Stores the ((|value|)) associating with ((|key|))

     If ((|nil|)) is given as the value, the association from the ((|key|))
     will be removed. It return the object deleted or ((|nil|)) if the
     specified key don't exist.

     ((|flags|)) can have the value ((|DBD::NOOVERWRITE|)), in this case
     it will return ((|nil|)) if the specified key exist, otherwise ((|true|))

--- append(key, value)
--- db_append(key, value)
     Append the ((|value|)) associating with ((|key|))

--- byteswapped?
--- get_byteswapped
     Return if the underlying database is in host order

--- close([flags])
--- db_close([flags])
    Closes the file.

--- delete(key)
--- db_del(key)
      Removes the association from the key. 

      It return the object deleted or ((|nil|)) if the specified
      key don't exist.

--- delete_if([set]) { |key, value| ... }
--- reject!([set]) { |key, value| ... }
      Deletes associations if the evaluation of the block returns true. 

      Only for ((|BDB1::Btree|))

--- duplicates(key [, assoc = true])
      Return an array of all duplicate associations for ((|key|))

      if ((|assoc|)) is ((|false|)) return only the values.

      Only for ((|BDB1::Btree|))

--- each { |key, value| ... }
--- each_pair { |key, value| ... }
      Iterates over associations.

--- each_dup(key) { |key, value| ... }
      Iterates over each duplicate associations for ((|key|))

      Only for ((|BDB1::Btree|))

--- each_dup_value(key) { |value| ... }
      Iterates over each duplicate values for ((|key|))

      Only for ((|BDB1::Btree|))

--- each_key { |key| ... }
      Iterates over keys. 

--- each_value { |value| ... }
      Iterates over values. 

--- empty?() 
       Returns true if the database is empty. 

--- has_key?(key) 
--- key?(key) 
--- include?(key) 
--- member?(key) 
       Returns true if the association from the ((|key|)) exists.

--- has_both?(key, value)
--- both?(key, value)
      Returns true if the association from ((|key|)) is ((|value|)) 

--- has_value?(value) 
--- value?(value) 
       Returns true if the association to the ((|value|)) exists. 

--- index(value)
       Returns the first ((|key|)) associated with ((|value|))

--- indexes(value1, value2, )
       Returns the ((|keys|)) associated with ((|value1, value2, ...|))

--- keys 
       Returns the array of the keys in the database

--- length 
--- size 
       Returns the number of association in the database.

--- reject { |key, value| ... }
      Create an hash without the associations if the evaluation of the
      block returns true. 

--- reverse_each([set]) { |key, value| ... }
--- reverse_each_pair([set]) { |key, value| ... }
      Iterates over associations in reverse order 

      Only for ((|BDB1::Btree|))

--- reverse_each_key([set]) { |key| ... }
      Iterates over keys in reverse order 

      Only for ((|BDB1::Btree|))

--- reverse_each_value([set]) { |value| ... }
      Iterates over values in reverse order.

      Only for ((|BDB1::Btree|))

--- to_a
       Return an array of all associations [key, value]

--- to_hash
       Return an hash of all associations {key => value}

--- truncate
--- clear
       Empty a database
       
--- values 
       Returns the array of the values in the database.

=end
