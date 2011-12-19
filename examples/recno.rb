#!/usr/bin/ruby

require 'bdb1'
out = open("tmp/recno.rb", "w")
print "====> BEFORE\n"
File.foreach("recno.rb") do |line|
  print line
  out.print line
end
out.close
db = BDB1::Recno.open "tmp/recno.rb"
db.each do |v|
  db.delete(v) if /^db/ =~ v
end
print "====> AFTER\n"
db.each do |v|
  puts v
end
db.close
