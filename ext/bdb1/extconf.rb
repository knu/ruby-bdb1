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

create_makefile("bdb1")
