#Implementation of a sorted, balanced tree structure
#
#Inherit from BDB1::Common
class BDB1::Btree < BDB1::Common 
   #Deletes associations if the evaluation of the block returns true. 
   #
   def  delete_if([set]) 
      yield key, value
   end

   #same than <em> delete_if</em>
   def  reject!([set]) 
      yield key, value
   end

   #Return an array of all duplicate associations for <em>key</em>
   #
   #if <em>assoc</em> is <em>false</em> return only the values.
   #
   def  duplicates(key, assoc = true)
   end

   #Iterates over each duplicate associations for <em>key</em>
   #
   def  each_dup(key) 
      yield key, value
   end

   #Iterates over each duplicate values for <em>key</em>
   #
   def  each_dup_value(key) 
      yield value
   end

   #Iterates over associations in reverse order 
   #
   def  reverse_each 
      yield key, value
   end

   #same than <em> reverse_each { |key, value| ... }</em>
   def  reverse_each_pair 
      yield key, value
   end

   #Iterates over keys in reverse order 
   #
   def  reverse_each_key 
      yield key
   end

   #Iterates over values in reverse order.
   #
   def  reverse_each_value 
      yield value
   end

end

