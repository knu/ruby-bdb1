#!/usr/bin/ruby
ARGV.collect! {|x| x.sub(/^--with-db-prefix=/, "--with-db-dir=") }

require 'mkmf'

if unknown = enable_config("unknown")
   libs = if CONFIG.key?("LIBRUBYARG_STATIC")
	     Config::expand(CONFIG["LIBRUBYARG_STATIC"].dup).sub(/^-l/, '')
	  else
	     Config::expand(CONFIG["LIBRUBYARG"].dup).sub(/lib([^.]*).*/, '\\1')
	  end
   unknown = find_library(libs, "ruby_init", 
			  Config::expand(CONFIG["archdir"].dup))
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
   if unknown
      make.print <<-EOF

unknown: $(DLLIB)
\t@echo "main() {}" > /tmp/a.c
\t$(CC) -static /tmp/a.c $(OBJS) $(CPPFLAGS) $(LIBPATH) $(LIBS) $(LOCAL_LIBS)
\t@-rm /tmp/a.c a.out

EOF
   end
   make.puts "test: $(DLLIB)"
ensure
   make.close
end
