#!/usr/bin/ruby
$LOAD_PATH.unshift *%w{.. . tests}
require 'bdb1'
require 'runit_'

module BDB1
   class BTCompare < Btree
      def bdb_bt_compare(a, b)
	 a <=> b
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

class TestBtree < RUNIT::TestCase
   def test_00_error
      assert_error(BDB1::Fatal, 'BDB1::Btree.new(".", "a")', "invalid name")
      assert_error(BDB1::Fatal, 'BDB1::Btree.open("tmp/aa", "env" => 1)', "invalid Env")
   end
   def test_01_init
      assert_kind_of(BDB1::Btree, $bdb = BDB1::Btree.new("tmp/aa", "a"), "<open>")
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
      assert_equal([1, 2, 3].to_s, $bdb["array"] = [1, 2, 3], "<array>")
      assert_equal([1, 2, 3].to_s, $bdb["array"], "<retrieve array>")
      assert_equal({"a" => "b"}.to_s, $bdb["hash"] = {"a" => "b"}, "<hash>")
      assert_equal({"a" => "b"}.to_s, $bdb["hash"], "<retrieve hash>")
      assert($bdb.sync, "<sync>")
   end
   def test_03_delete
      size = $bdb.size
      i = 0
      $bdb.each do |key, value|
	 assert_equal($bdb, $bdb.delete(key), "<delete value>")
	 i += 1
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
      $bdb.reverse_each_value do |x|
	 arr << x
      end
      assert_equal(array, arr.sort, "<reverse order>")
      arr = $bdb.reject {|k, v| k == "e" || v == "i" }
      has = array.reject {|k, v| k == "e" || k == "i" }
      assert_equal(has, arr.keys.sort, "<reject>")
   end
   def test_05_reopen
      assert_equal(nil, $bdb.close, "<close>")
      assert_kind_of(BDB1::Btree, $bdb = BDB1::Btree.open("tmp/aa", "w", 
	"set_flags" => BDB1::DUP,
	"set_dup_compare" => lambda {|a, b| a <=> b}),
        "<reopen with DB_DUP>")
      assert_equal(0, $bdb.size, "<must be 0 after reopen>")
   end
   def test_06_dup
      assert_equal("a", $bdb["0"] = "a", "<set dup>")
      assert_equal("b", $bdb["0"] = "b", "<set dup>")
      assert_equal("c", $bdb["0"] = "c", "<set dup>")
      assert_equal("d", $bdb["0"] = "d", "<set dup>")
      assert_equal("aa", $bdb["1"] = "aa", "<set dup>")
      assert_equal("bb", $bdb["1"] = "bb", "<set dup>")
      assert_equal("cc", $bdb["1"] = "cc", "<set dup>")
      assert_equal("aaa", $bdb["2"] = "aaa", "<set dup>")
      assert_equal("bbb", $bdb["2"] = "bbb", "<set dup>")
      assert_equal("aaaa", $bdb["3"] = "aaaa", "<set dup>")
   end
   def test_07_in_memory
      assert_equal(nil, $bdb.close, "<close>")
      assert_kind_of(BDB1::Btree, $bdb = BDB1::Btree.open, "<open in memory>")
      assert_equal(0, $bdb.size, "<must be 0 after reopen>")
   end
   def test_08_in_memory_get_set
      assert_equal("aa", $bdb["bb"] = "aa", "<set in memory>")
      assert_equal("cc", $bdb["bb"] = "cc", "<set in memory>")
      assert_equal("cc", $bdb["bb"], "<get in memory>")
      assert_equal(nil, $bdb.close, "<close>")
   end
   def test_18_btree_delete
      clean
      assert_kind_of(BDB1::BTCompare, 
		     $bdb = BDB1::BTCompare.open("tmp/aa", "w", 
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
      $bdb.delete_if {|k, v| k[0] == ?a}
      $hash.delete_if {|k, v| k[0] == ?a}
      assert_equal($bdb.size, $hash.size, "<size after delete_if>")
      $bdb.each do |k, v|
	 assert_equal($hash[k], v, "<value>")
      end
   end

   def test_19_index
      lines = $hash.keys
      array = []
      10.times do
	 h = lines[rand(lines.size - 1)]
	 array.push h
	 assert_equal($hash.index(h.reverse), $bdb.index(h.reverse), "<index>")
      end
      assert_equal($hash.indexes(array), $bdb.indexes(array), "<indexes>")
   end

   def test_20_convert
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

end

RUNIT::CUI::TestRunner.run(TestBtree.suite)
