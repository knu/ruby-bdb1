#!/usr/bin/ruby -I../src
require 'bdb1'
module ZeroC
   def bdb1_fetch_value(a)
      a.sub(/\0$/, '')
   end
   def bdb1_store_value(a)
      a + "\0"
   end
   alias bdb1_fetch_key bdb1_fetch_value
   alias bdb1_store_key bdb1_store_value
end
      
module BDB1
   class A < Btree
      include ZeroC
   end
end

$option = {"set_pagesize" => 1024, "set_cachesize" => 32 * 1024}

db = BDB1::A.open "basic", "w", $option
File.foreach("wordtest") do |line|
    line.chomp!
    db[line] = line.reverse
end
db.each do |k, v|
   if k != v.reverse || /\0/ =~ k || /\0/ =~ v
      print "ERROR : #{k.inspect} -- #{v.inspect}\n"
   end
end
db.close

db = BDB1::Btree.open "basic", $option
db.each do |k, v|
   if k[-1] != 0 || v[-1] != 0
      print "ERROR : #{k.inspect} -- #{v.inspect}\n"
   end
end
