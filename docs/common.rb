# Implement common methods for access to data
#
class BDB1::Common
   class << self
      #open the database
      #
      #* <em>name</em>
      #  The argument name is used as the name of a single physical
      #  file on disk that will be used to back the database.
      #
      #* <em>flags</em>
      #  The flags must be the string "r", "r+", "w", "w+", "a", "a+" or
      #  and integer value.
      #
      #  The flags value must be set to 0 or by bitwise inclusively
      #  OR'ing together one or more of the following values
      #
      #  * <em>BDB1::CREATE</em>
      #    Create any underlying files, as necessary. If the files
      #    do not already exist and the DB_CREATE flag is not
      #    specified, the call will fail. 
      #
      #  * <em>BDB1::EXCL</em>
      #    Return an error if the database already exists. Underlying
      #    filesystem primitives are used to implement this
      #    flag. For this reason it is only applicable to the
      #    physical database file and cannot be used to test if a
      #    subdatabase already exists. 
      #
      #  * <em>BDB1::NOMMAP</em>
      #    Do not map this database into process memory.
      #
      #  * <em>BDB1::RDONLY</em>
      #    Open the database for reading only. Any attempt to
      #    modify items in the database will fail regardless of the
      #    actual permissions of any underlying files. 
      #
      #  * <em>BDB1::TRUNCATE</em>
      #    Physically truncate the underlying database file,
      #    discarding all previous subdatabases or databases.
      #    Underlying filesystem primitives are used to implement
      #    this flag. For this reason it is only applicable to the
      #    physical database file and cannot be used to discard
      #    subdatabases.
      #
      #    The DB_TRUNCATE flag cannot be transaction protected,
      #    and it is an error to specify it in a transaction
      #    protected environment. 
      #
      #  * <em>BDB1::WRITE</em>
      #    Open the database for writing. Without this flag, any
      #    attempt to modify items in the database will fail.
      #
      #
      #* <em>mode</em>
      #  mode to create the file
      #
      #* <em>options</em>
      #  Hash, Possible options are (see the documentation of Berkeley DB
      #  for more informations) 
      #
      #  * <em>set_flags</em>: general database configuration
      #  * <em>set_cachesize</em>: set the database cache size
      #  * <em>set_pagesize</em>: set the underlying database page size
      #  * <em>set_lorder</em>: set the database byte order
      #  * <em>set_store_key</em>: specify a Proc called before a key is stored
      #  * <em>set_fetch_key</em>: specify a Proc called after a key is read
      #  * <em>set_store_value</em>: specify a Proc called before a value is stored
      #  * <em>set_fetch_value</em>: specify a Proc called after a value is read
      #
      #* <em>options specific to BDB1::Btree</em>
      #
      #  * <em>set_bt_compare</em>: specify a Btree comparison function 
      #  * <em>set_bt_minkey</em>: set the minimum number of keys per Btree page
      #  * <em>set_bt_prefix</em>: specify a Btree prefix comparison function 
      #
      #* <em>options specific to BDB1::Hash</em>
      #
      #  * <em>set_h_ffactor</em>: set the Hash table density
      #  * <em>set_h_hash</em>: specify a hashing function 
      #  * <em>set_h_nelem</em>: set the Hash table size
      #
      #* <em>options specific to BDB1::Recnum</em>
      #
      #  * <em>set_re_delim</em>: set the variable-length record delimiter
      #  * <em>set_re_len</em>: set the fixed-length record length
      #  * <em>set_re_pad</em>: set the fixed-length record pad byte
      #
      #  Proc given to <em>set_bt_compare</em>, <em>set_bt_prefix</em>, 
      #  <em>set_h_hash</em>, <em>set_store_key</em>, <em>set_fetch_key</em>,
      #  <em>set_store_value</em> and <em>set_fetch_value</em> can be also
      #  specified as a method (replace the prefix <em>set_</em> with 
      #  <em>bdb1_</em>)
      #
      #  For example 
      #
      #   module BDB1
      #      class Btreesort < Btree
      #         def bdb1_bt_compare(a, b)
      #            b.downcase <=> a.downcase
      #         end
      #      end
      #   end
      def  new(name = nil, flags = "r", mode = 0, options = {})
      end

      #same than <em>new</em>
      def  create(name = nil, flags = "r", mode = 0, options = {})
      end

      #same than <em>new</em>
      def  open(name = nil, flags = "r", mode = 0, options = {})
	 yield bdb1
      end

      #create a new temporary db file, populated with the given object.
      #The given object can be an <em>Hash</em>
      def  [](hash)
      end
   end

   #Returns the value corresponding the <em>key</em>
   #
   def  [](key)
   end
   
   #Returns the value correspondind the <em>key</em>
   #
   def  get(key, flags = 0)
   end

   #Stores the <em>value</em> associating with <em>key</em>
   #
   #return <em>value</em>
   #
   def  []=(key, value)
   end

   #Stores the <em>value</em> associating with <em>key</em>
   #
   #Return the  value correspondind the <em>key</em>
   #
   #<em>flags</em> can have the value <em>DBD::NOOVERWRITE</em>, in this case
   #it will return <em>nil</em> if the specified key exist, otherwise 
   #<em>true</em>
   #
   def put(key, value, flags = 0)
   end

   #same than <em>put</em>
   def  store(key, value, flags = 0)
   end

   #Append the <em>value</em> associating with <em>key</em>
   #
   def  append(key, value)
   end

   #Return <em>true</em> if the underlying database is in host order
   #
   def  byteswapped?
   end

   #same than <em> byteswapped?</em>
   def  get_byteswapped
   end

   #Closes the file.
   #
   def  close(flags = 0)
   end

   #Removes the association from the key. 
   #
   #It return the object deleted or <em>nil</em> if the specified
   #key don't exist.
   #
   def  delete(key)
   end

   #Iterates over associations.
   #
   def  each 
      yield key, value
   end

   #same than <em> each { |key, value| ... }</em>
   def  each_pair 
      yield key, value
   end

   #Iterates over keys. 
   #
   def  each_key 
      yield key
   end

   #Iterates over values. 
   #
   def  each_value 
      yield value
   end

   #Returns true if the database is empty. 
   #
   def  empty?() 
   end

   #Returns true if the association from the <em>key</em> exists.
   #
   def  has_key?(key) 
   end

   #same than <em> has_key?</em>
   def  key?(key) 
   end

   #same than <em> has_key?</em>
   def  include?(key) 
   end

   #same than <em> has_key?</em>
   def  member?(key) 
   end

   #Returns true if the association from <em>key</em> is <em>value</em> 
   #
   def  has_both?(key, value)
   end

   #same than <em> has_both?</em>
   def  both?(key, value)
   end

   #Returns true if the association to the <em>value</em> exists. 
   #
   def  has_value?(value) 
   end

   #same than <em> has_value?</em>
   def  value?(value) 
   end

   #Returns the first <em>key</em> associated with <em>value</em>
   #
   def  index(value)
   end

   #Returns the <em>keys</em> associated with <em>value1, value2, ...</em>
   #
   def  indexes(value1, value2, )
   end

   #Returns the array of the keys in the database
   #
   def  keys 
   end

   #Returns the number of association in the database.
   #
   def  length 
   end
   #same than <em> length </em>
   def  size 
   end
   
   #Create an hash without the associations if the evaluation of the
   #block returns true. 
   #
   def  reject 
      yield key, value
   end

   #Return an array of all associations [key, value]
   #
   def  to_a
   end
   
   #Return an hash of all associations {key => value}
   #
   def  to_hash
   end
   
   #Empty a database
   #       
   def  truncate
   end

   #same than <em> truncate</em>
   def  clear
   end

   #Returns the array of the values in the database.
   def  values 
   end
end
