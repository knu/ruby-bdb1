#!/usr/bin/ruby
ARGV.collect! {|x| x.sub(/^--with-db-prefix=/, "--with-db-dir=") }

require 'mkmf'

def resolve(key)
   name = key.dup
   true while name.gsub!(/\$\((\w+)\)/) { CONFIG[$1] }
   name
end

if ! find_library(resolve(CONFIG["LIBRUBY"]).sub(/^lib(.*)\.\w+\z/, '\\1'), 
                  "ruby_init", resolve(CONFIG["archdir"]))
   raise "can't find -lruby"
end

dir_config("db")

test = enable_config("test")
unless (!test && (have_func("dbopen") ||
		  have_library("db1", "dbopen")) || 
	have_library("db", "dbopen"))
    raise "libdb.a not found"
end

if enable_config("shared", true)
   $static = nil
end

create_makefile("bdb1")

begin
   make = open("Makefile", "a")
   make.print <<-EOF

unknown: $(DLLIB)
\t@echo "main() {}" > /tmp/a.c
\t$(CC) -static /tmp/a.c $(OBJS) $(CPPFLAGS) $(DLDFLAGS) #$stat_lib #{Config::CONFIG["LIBS"]} $(LIBS) $(LOCAL_LIBS)
\t@-rm /tmp/a.c a.out

test: $(DLLIB)
   EOF
ensure
   make.close
end
