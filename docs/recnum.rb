# Stores both fixed and variable-length records with logical record 
# numbers as keys
#
# All instance methods has the same syntax than the methods of Array
#
# Inherit from BDB1::Common
class BDB1::Recnum < BDB1::Common
   #Element reference - with the following syntax
   #
   #self[nth]
   #
   #retrieves the <em>nth</em> item from an array. Index starts from zero.
   #If index is the negative, counts backward from the end of the array. 
   #The index of the last element is -1. Returns <em>nil</em>, if the <em>nth</em>
   #element is not exist in the array. 
   #
   #self[start..end]
   #
   #returns an array containing the objects from <em>start</em> to <em>end</em>,
   #including both ends. if end is larger than the length of the array,
   #it will be rounded to the length. If <em>start</em> is out of an array 
   #range , returns <em>nil</em>.
   #And if <em>start</em> is larger than end with in array range, returns
   #empty array ([]). 
   #
   #self[start, length]    
   #
   #returns an array containing <em>length</em> items from <em>start</em>. 
   #Returns <em>nil</em> if <em>length</em> is negative. 
   #
   def  [](args) 
   end

   #Element assignement -- with the following syntax
   #
   #self[nth] = val 
   #
   #changes the <em>nth</em> element of the array into <em>val</em>. 
   #If <em>nth</em> is larger than array length, the array shall be extended
   #automatically. Extended region shall be initialized by <em>nil</em>. 
   #
   #self[start..end] = val 
   #
   #replace the items from <em>start</em> to <em>end</em> with <em>val</em>. 
   #If <em>val</em> is not an array, the type of <em>val</em> will be 
   #converted into the Array using <em>to_a</em> method. 
   #
   #self[start, length] = val 
   #
   #replace the <em>length</em> items from <em>start</em> with <em>val</em>. 
   #If <em>val</em> is not an array, the type of <em>val</em> will be
   #converted into the Array using <em>to_a</em>. 
   #
   def  []=(args, val)
   end

   #concatenation
   #
   def  +(other)
   end
   
   #repetition
   #
   def  *(times)
   end
   
   #substraction
   #
   def  -(other)
   end
   
   #returns a new array which contains elements belong to both elements.
   #
   def  &(other)
   end
   
   #join
   #
   def  |(other)
   end

   #append a new item with value <em>obj</em>. Return <em>self</em>
   #
   def  <<(obj)
   end
   
   #comparison : return -1, 0 or 1
   #
   def  <=>(other) 
   end
   
   #delete all elements 
   #
   def  clear
   end
   
   #Returns a new array by invoking block once for every element,
   #passing each element as a parameter to block. The result of block
   #is used as the given element 
   #
   def  collect  
      yield item
   end
   
   #invokes block once for each element of db, replacing the element
   #with the value returned by block.
   #
   def  collect!  
      yield item
   end
   
   #append <em>other</em> to the end
   #
   def  concat(other) 
   end
   
   #delete the item which matches to <em>val</em>
   #
   def  delete(val) 
   end
   
   #delete the item at <em>pos</em>
   #
   def  delete_at(pos) 
   end
   
   #delete the item if the block return <em>true</em>
   #
   def  delete_if  
      yield x
   end
   
   #delete the item if the block return <em>true</em>
   #
   def  reject! 
      yield x
   end
   
   #iterate over each item
   #
   def  each  
      yield x
   end
   
   #iterate over each index
   #
   def  each_index  
      yield i
   end
   #return <em>true</em> if the db file is empty 
   #
   def  empty?
   end
   
   #set the entire db with <em>val</em> 
   #
   def  fill(val)
   end
   
   #fill the db with <em>val</em> from <em>start</em> 
   #
   def  fill(val, start[, length])
   end
   
   #set the db with <em>val</em> from <em>start</em> to <em>end</em> 
   #
   def  fill(val, start..end)
   end
   
   #returns true if the given <em>val</em> is present
   #
   def  include?(val) 
   end
   
   #returns the index of the item which equals to <em>val</em>. 
   #If no item found, returns <em>nil</em>
   #
   def  index(val) 
   end
   
   #returns an array consisting of elements at the given indices
   #
   def  indexes(index_1,..., index_n) 
   end
   
   #returns an array consisting of elements at the given indices
   #
   def  indices(index_1,..., index_n) 
   end
   
   #returns a string created by converting each element to a string
   #
   def  join([sep]) 
   end
   
   #return the number of elements of the db file
   #
   def  length
   end
   #same than <em> length</em>
   def  size 
   end

   #return the number of non-nil elements of the db file
   #
   def  nitems 
   end
   
   #pops and returns the last value
   #
   def  pop 
   end
   
   #appends obj
   #
   def  push(obj, ...) 
   end
   
   #replaces the contents of the db file  with the contents of <em>other</em>
   #
   def  replace(other) 
   end
   
   #returns the array of the items in reverse order
   #
   def  reverse 
   end
   
   #replaces the items in reverse order.
   #
   def  reverse! 
   end
   
   #iterate over each item in reverse order
   #
   def  reverse_each  
      yield x
   end
   
   #returns the index of the last item which verify <em>item == val</em>
   #
   def  rindex(val) 
   end
   
   #remove and return the first element
   #
   def  shift 
   end
   
   #return an <em>Array</em> with all elements
   #
   def  to_a
   end
   #same than <em> to_a</em>
   def  to_ary
   end

   #insert <em>obj</em> to the front of the db file
   #
   def  unshift(obj) 
   end
end
