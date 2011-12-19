#!/usr/bin/ruby
require 'mkmf'

dir_config("db")

unless have_func("dbopen") || have_library("db1", "dbopen") ||
  have_library("db", "dbopen")
  raise "libdb not found"
end

if enable_config("shared", true)
  $static = nil
end

%w[rb_frame_this_func rb_block_call].each do |f|
  have_func(f)
end

%w[insert values_at map select].each do |f|
  print "checking for Array\##{f}... "
  if [].respond_to?(f)
    puts "yes"
    $CFLAGS += " -DHAVE_RB_ARY_#{f.upcase}"
  else
    puts "no"
  end
end

create_makefile("bdb1")

File.open("Makefile", "a") { |make|
  make.puts "test: $(DLLIB)"
}
