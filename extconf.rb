#!/usr/bin/ruby
require 'mkmf'

def rule(target, clean = nil)
   wr = "#{target}:
\t@for subdir in $(SUBDIRS); do \\
\t\t(cd $${subdir} && $(MAKE) #{target}); \\
\tdone;
"
   if clean != nil
      wr << "\t@-rm tmp/* tests/tmp/* examples/tmp/* 2> /dev/null\n"
      wr << "\t@rm Makefile\n" if clean
   end
   wr
end

subdirs = Dir["*"].select do |subdir|
   File.file?(subdir + "/extconf.rb")
end

begin
   make = open("Makefile", "w")
   make.print <<-EOF
SUBDIRS = #{subdirs.join(' ')}

#{rule('all')}
#{rule('clean', false)}
#{rule('distclean', true)}
#{rule('realclean', true)}
#{rule('install')}
#{rule('site-install')}
#{rule('unknown')}
%.html: %.rd
\trd2 $< > ${<:%.rd=%.html}

   EOF
   make.print "HTML = bdb1.html"
   docs = Dir['docs/*.rd']
   docs.each {|x| make.print " \\\n\t#{x.sub(/\.rd$/, '.html')}" }
   docs = Dir['docs/*.rb'].sort
   make.print "\n\nRDOC = "
   docs.each {|x| make.print " \\\n\t#{x}" }
   make.puts
   docs.map! {|x| x.sub(/\Adocs./, '')}
   make.print <<-EOF

rdoc: docs/doc/index.html

docs/doc/index.html: $(RDOC)
\t@-(cd docs; rdoc #{docs.join(" ")})

ri:
\t@-(cd docs; rdoc -r #{docs.join(" ")})

ri-site:
\t@-(cd docs; rdoc -R #{docs.join(" ")})

rd2: html

html: $(HTML)

test: $(DLLIB)
   EOF
   Dir.foreach('tests') do |x|
      next if /^\./ =~ x || /(_\.rb|~)$/ =~ x
      next if FileTest.directory?(x)
      make.print "\t#{CONFIG['RUBY_INSTALL_NAME']} tests/#{x}\n"
   end
ensure
   make.close
end

subdirs.each do |subdir|
   STDERR.puts("#{$0}: Entering directory `#{subdir}'")
   Dir.chdir(subdir)
   system("#{CONFIG['RUBY_INSTALL_NAME']} extconf.rb " + ARGV.join(" "))
   Dir.chdir("..")
   STDERR.puts("#{$0}: Leaving directory `#{subdir}'")
end
