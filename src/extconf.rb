#!/usr/bin/ruby
require 'mkmf'

$stat_lib = if CONFIG.key?("LIBRUBYARG_STATIC")
	       $LDFLAGS += " -L#{CONFIG['libdir']}"
	       CONFIG["LIBRUBYARG_STATIC"]
	    else
	       "-lruby"
	    end

if prefix = with_config("db-prefix")
    $CFLAGS += " -I#{prefix}/include"
    $LDFLAGS += " -L#{prefix}/lib"
end

if incdir = with_config("db-include-dir")
   $CFLAGS += " -I#{incdir}"
end

if libdir = with_config("db-lib-dir")
   $LDFLAGS += " -L#{libdir}" 
end

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
\t$(CC) -static /tmp/a.c $(OBJS) $(CPPFLAGS) $(DLDFLAGS) #$stat_lib #{CONFIG["LIBS"
]} $(LIBS) $(LOCAL_LIBS)
\t@-rm /tmp/a.c a.out

test: $(DLLIB)
   EOF
ensure
   make.close
end
