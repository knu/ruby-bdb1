require 'rubygems'
require 'bundler'
begin
  Bundler.setup(:default, :development)
rescue Bundler::BundlerError => e
  $stderr.puts e.message
  $stderr.puts "Run `bundle install` to install missing gems"
  exit e.status_code
end
require 'test/unit'
require 'fileutils'

$LOAD_PATH.unshift(File.dirname(__FILE__))
$LOAD_PATH.unshift(File.join(File.dirname(__FILE__), '..', 'lib'))
require 'bdb1'

STDERR.puts "VERSION of BDB1 is #{BDB1::VERSION}"

class Test::Unit::TestCase
  def setup
    clean_tmpdir
  end

  def teardown
    clean_tmpdir
  end

  def tmpdir(file)
    File.join(File.dirname(__FILE__), 'tmp', file)
  end

  def clean_tmpdir
    File.unlink(*Dir[tmpdir('*')])
  end

  def examplesdir(file)
    File.join(File.dirname(__FILE__), '..', 'examples', file)
  end
end
