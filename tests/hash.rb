#!/usr/bin/ruby
$LOAD_PATH.unshift *%w{../src src tests}
require 'bdb1'
require 'runit_'

module BDB1
   class HashRuby < Hash
      def bdb1_h_hash a
	 a.hash
      end
   end
end

def clean
   Dir.foreach('tmp') do |x|
      if FileTest.file?("tmp/#{x}")
	 File.unlink("tmp/#{x}")
      end
   end
end

$bdb, $env = nil, nil
clean

print "\nVERSION of BDB1 is #{BDB1::VERSION}\n"

class TestHash < RUNIT::TestCase
   def test_00_error
      assert_error(BDB1::Fatal, 'BDB1::Hash.new(".", "a")', "invalid name")
      assert_error(BDB1::Fatal, 'BDB1::Hash.open("tmp/aa", "env" => 1)', "invalid Env")
   end
   def test_01_init
      assert_kind_of(BDB1::Hash, $bdb = BDB1::Hash.new("tmp/aa", "a"), "<open>")
   end
   def test_02_get_set
      assert_equal("alpha", $bdb["alpha"] = "alpha", "<set value>")
      assert_equal("alpha", $bdb["alpha"], "<retrieve value>")
      assert_equal(nil, $bdb["gamma"] = nil, "<set nil>")
      assert_equal(nil, $bdb["gamma"], "<retrieve nil>")
      assert($bdb.key?("alpha"), "<has key>")
      assert_equal(false, $bdb.key?("unknown"), "<has unknown key>")
      assert($bdb.value?(nil), "<has nil>")
      assert($bdb.value?("alpha"), "<has value>")
      assert_equal(false, $bdb.value?("unknown"), "<has unknown value>")
      assert_equal(false, $bdb.put("alpha", "gamma", BDB1::NOOVERWRITE), "<nooverwrite>")
      assert_equal("alpha", $bdb["alpha"], "<must not be changed>")
      assert($bdb.both?("alpha", "alpha"), "<has both>")
      assert(! $bdb.both?("alpha", "beta"), "<don't has both>")
      assert(! $bdb.both?("unknown", "alpha"), "<don't has both>")
      assert_equal([1, 2, 3].to_s, $bdb["array"] = [1, 2, 3], "<array>")
      assert_equal([1, 2, 3].to_s, $bdb["array"], "<retrieve array>")
      assert_equal({"a" => "b"}.to_s, $bdb["hash"] = {"a" => "b"}, "<hash>")
      assert_equal({"a" => "b"}.to_s, $bdb["hash"], "<retrieve hash>")
      assert($bdb.sync, "<sync>")
   end

   def test_03_delete
      size = $bdb.size
      i, arr = 0, []
      $bdb.each do |key, value|
	 arr << key
	 i += 1
      end
      arr.each do |key|
	 assert_equal($bdb, $bdb.delete(key), "<delete value>")
      end
      assert(size == i, "<delete count>")
      assert_equal(0, $bdb.size, "<empty>")
   end

   def test_04_cursor
      array = ["a", "b", "c", "d", "e", "f", "g", "h", "i"]
      array.each do |x|
	 assert_equal(x, $bdb[x] = x, "<set value>")
      end
      assert(array.size == $bdb.size, "<set count>")
      arr = []
      $bdb.each_value do |x|
	 arr << x
      end
      assert_equal(array, arr.sort, "<order>")
      arr = []
   end

   def test_05_in_memory
      assert_equal(nil, $bdb.close, "<close>")
      assert_kind_of(BDB1::Hash, $bdb = BDB1::Hash.open(nil), "<open in memory>")
      assert_equal(0, $bdb.size, "<must be 0 after reopen>")
   end

   def test_06_in_memory_get_set
      (33 .. 126).each do |i|
	 key = i.to_s * 5
	 val = i.to_s * 7
	 assert_equal(val, $bdb[key] = val, "<set in memory>")
      end
      assert_equal(94, $bdb.size, "<length in memory>")
      assert_equal(nil, $bdb.close, "<close>")
   end

   def test_07_index
      assert_kind_of(BDB1::HashRuby, 
		     $bdb = BDB1::HashRuby.open("tmp/aa", "w", 
					       "set_pagesize" => 1024,
					       "set_cachesize" => 32 * 1024),
		     "<open>")
      $hash = {}
      File.foreach("examples/wordtest") do |line|
	 line.chomp!
	 $hash[line] = line.reverse
	 $bdb[line] = line.reverse
      end
      $bdb.each do |k, v|
	 assert_equal($hash[k], v, "<value>")
      end
      assert_equal($bdb.size, $hash.size, "<size after delete_if>")
   end

   def test_08_convert
      h = $bdb.to_hash
      h.each do |k, v|
	 assert_equal(v, $hash[k], "<to_hash>")
      end
      $hash.each do |k, v|
	 assert_equal(v, h[k], "<to_hash>")
      end
      h1 = $hash.to_a
      h2 = $bdb.to_a
      assert_equal(h1.size, h2.size, "<equal>")
   end

   def test_09_has
      assert_kind_of(BDB1::HashRuby, 
		     $bdb = BDB1::HashRuby.open("tmp/aa", "w", 
					       "set_pagesize" => 1024,
					       "set_cachesize" => 32 * 1024),
		     "<open>")
      $hash = {}
      ('a' .. 'z').each do |k|
	 assert_equal(($hash[k] = k * 4), ($bdb[k] = k * 4), "<set>")
      end
      $hash.each_key do |k|
	 assert($bdb.has_key?(k), "<has key>")
      end
      $hash.each_value do |v|
	 assert($bdb.has_value?(v), "<has key>")
      end
      $bdb.each do |k, v|
	 assert($hash.has_key?(k), "<has key>")
	 assert($hash.has_value?(v), "<has key>")
      end
   end

end

RUNIT::CUI::TestRunner.run(TestHash.suite)
