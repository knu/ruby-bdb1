=begin
== Acces Methods

These are the methods for ((|BDB1::Recnum|))

=== Class Methods

--- create([name, flags, mode, options])
--- new([name, flags, mode, options])
--- open([name, flags, mode, options])
     open the database

       : ((|options|))

           : ((|set_flags|))
                general database configuration
           : ((|set_cachesize|))
                set the database cache size
           : ((|set_pagesize|))
                set the underlying database page size
           : ((|set_lorder|))
                set the database byte order
           : ((|set_re_delim|))
                set the variable-length record delimiter
           : ((|set_re_len|))
                set the fixed-length record length
           : ((|set_re_pad|))
                set the fixed-length record pad byte

=== Methods

((*All this methods has the same syntax than for the class ((|Array|))*))

self[nth] 

self[start..end] 

self[start, length] 


self[nth] = val 

self[start..end] = val 

self[start, length] = val 

self + other 

self * times 

self - other 

self & other 

self | other 

self << obj 

self <=> other 

clear 

concat(other) 

delete(val) 

delete_at(pos) 

delete_if {...} 

reject!{|x|...} 

each {...} 

each_index {...} 

empty? 

fill(val) 

fill(val, start[, length]) 

fill(val, start..end) 

filter{|item| ..} 

freeze 

frozen 

include?(val) 

index(val) 

indexes(index_1,..., index_n) 

indices(index_1,..., index_n) 

join([sep]) 

length
 
size 

nitems 

pop 

push(obj...) 

replace(other) 

reverse 

reverse! 

reverse_each {...} 

rindex(val) 

shift 

unshift(obj) 

=end
