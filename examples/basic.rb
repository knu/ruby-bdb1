#!/usr/bin/ruby
$LOAD_PATH.unshift "../src"
require 'bdb1'
db = BDB1::Btree.open "tmp/basic", "w", 0644,
     "set_pagesize" => 1024, "set_cachesize" => 32 * 1024
File.foreach("wordtest") do |line|
    line.chomp!
    db[line] = line.reverse
end
db.each do |k, v|
    if k != v.reverse
	print "ERROR : #{k} -- #{v}\n"
    end
end
db.close
