#!/usr/bin/ruby
require 'mkmf'
if prefix = with_config("db-prefix")
    $CFLAGS += " -I#{prefix}/include"
    $LDFLAGS += " -L#{prefix}/lib"
end
$CFLAGS += " -DBDB_NO_THREAD" if with_config("no-thread")
$CFLAGS += " -I#{incdir}" if incdir = with_config("db-include-dir")
$LDFLAGS += " -I#{libdir}" if libdir = with_config("db-lib-dir")
if ! have_library("db", "__hash_open") 
    raise "libdb.a not found"
end
create_makefile("bdb1")
begin
   make = open("Makefile", "a")
   make.print <<-EOF

unknown: $(DLLIB)
\t@echo "main() {}" > /tmp/a.c
\t$(CC) -static /tmp/a.c $(OBJS) $(CPPFLAGS) $(DLDFLAGS) -lruby #{CONFIG["LIBS"
]} $(LIBS) $(LOCAL_LIBS)
\t@-rm /tmp/a.c a.out

test: $(DLLIB)
   EOF
   Dir.foreach('tests') do |x|
      next if /^\./ =~ x || /(_\.rb|~)$/ =~ x
      next if FileTest.directory?(x)
      make.print "	ruby tests/#{x}\n"
   end
ensure
   make.close
end
