#!/usr/bin/ruby
$LOAD_PATH.unshift "../src"
require 'bdb1'
module BDB1
   class Btreesort < Btree
      def bdb1_bt_compare(a, b)
	 b.downcase <=> a.downcase
      end
   end
end
a = { "gamma" => 4, "Alpha" => 1, "delta" => 3, "Beta" => 2, "delta" => 3}
db = BDB1::Btreesort.open "alpha", "w"
a.each do |x, y|
   db[x] = y
end
db.each do |x, y|
   puts "SORT : #{x} -- #{y}"
end
db = BDB1::Hash.open "alpha", "w",
   "set_h_hash" => lambda { |x| x.hash }
a.each do |x, y|
   puts "#{x} -- #{y}"
   db[x] = y
end
db.each do |x, y|
   puts "HASH : #{x} -- #{y}"
end


