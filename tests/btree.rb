#!/usr/bin/ruby
$LOAD_PATH.unshift *%w{../src ./src tests}
require 'bdb1'
require 'runit_'

module BDB1
   class BTCompare < Btree
      def bdb1_bt_compare(a, b)
	 a <=> b
      end
   end

   class AZ < Btree
      def bdb1_store_key(a)
	 "xx_" + a
      end
      def bdb1_fetch_key(a)
	 a.sub(/^xx_/, '')
      end
      def bdb1_store_value(a)
	 "yy_" + a
      end
      def bdb1_fetch_value(a)
	 a.sub(/^yy_/, '')
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

Inh = defined?(RUNIT) ? RUNIT : Test::Unit

class TestBtree < Inh::TestCase
   def test_00_error
      assert_raises(BDB1::Fatal, "invalid name") do
	 BDB1::Btree.new(".", "a")
      end
      assert_raises(BDB1::Fatal, "invalid Env") do
	 BDB1::Btree.open("tmp/aa", "env" => 1)
      end
   end
   def test_01_init
      assert_kind_of(BDB1::Btree, $bdb = BDB1::Btree.new("tmp/aa", "a"), "<open>")
   end
   def test_02_get_set
      assert_equal("alpha", $bdb["alpha"] = "alpha", "<set value>")
      assert_equal("alpha", $bdb["alpha"], "<retrieve value>")
      assert_equal(nil, $bdb["gamma"] = nil, "<set nil>")
      assert_equal(nil, $bdb["gamma"], "<retrieve nil>")
      assert($bdb.key?("alpha") == "alpha", "<has key>")
      assert_equal(false, $bdb.key?("unknown"), "<has unknown key>")
      assert($bdb.value?(nil), "<has nil>")
      assert($bdb.value?("alpha"), "<has value>")
      assert_equal(false, $bdb.value?("unknown"), "<has unknown value>")
      assert_equal(false, $bdb.put("alpha", "gamma", BDB1::NOOVERWRITE), "<nooverwrite>")
      assert_equal("alpha", $bdb["alpha"], "<must not be changed>")
      assert($bdb.both?("alpha", "alpha"), "<has both>")
      assert(! $bdb.both?("alpha", "beta"), "<don't has both>")
      assert(! $bdb.both?("unknown", "alpha"), "<don't has both>")
      assert_equal([1, 2, 3], $bdb["array"] = [1, 2, 3], "<array>")
      assert_equal([1, 2, 3].to_s, $bdb["array"], "<retrieve array>")
      assert_equal({"a" => "b"}, $bdb["hash"] = {"a" => "b"}, "<hash>")
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
      $hash = {}
      (33 .. 126).each do |i|
	 key = i.to_s * 5
	 $bdb[key] = i.to_s * 7
	 $hash[key] = i.to_s * 7
	 assert_equal($bdb[key], $hash[key], "<set #{key}>")
      end
      assert_equal($bdb.size, $hash.size, "<size after load>")
      $bdb.each do |key, value|
	 if rand < 0.5
	    assert_equal($bdb, $bdb.delete(key), "<delete value>")
	    $hash.delete(key)
	 end
      end
      assert_equal($bdb.size, $hash.size, "<size after load>")
      $bdb.each do |key, value|
	 assert_equal($hash[key], value, "<after delete>")
      end
      $bdb.each do |key, value|
	 assert($hash.key?(key), "<key after delete>")
	 assert_equal($bdb, $bdb.delete(key), "<delete value>")
      end
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
      $bdb.reject! {|k, v| k == "e" || v == "i" }
      array.reject! {|k, v| k == "e" || k == "i" }
      assert_equal(array, $bdb.keys.sort, "<keys after reject>")
      assert_equal(array, $bdb.values.sort, "<values after reject>")
   end

   def test_05_reopen
      assert_equal(nil, $bdb.close, "<close>")
      assert_kind_of(BDB1::Btree, $bdb = BDB1::Btree.open("tmp/aa", "w", 
	"set_flags" => BDB1::DUP),
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
      rep = [["a", "b", "c", "d"], ['aa', 'bb', 'cc'], ['aaa', 'bbb'], ['aaaa']]
      for i in [0, 1, 2, 3]
	 k0, v0 = [], []
	 $bdb.each_dup(i.to_s) {|k, v| k0 << k; v0 << v}
	 assert_equal(k0, [i.to_s] * (4 - i), "<dup key #{i}>")
	 assert_equal(v0.sort, rep[i], "<dup val #{i}>")
	 v0 = []
	 $bdb.each_dup_value(i.to_s) {|v|  v0 << v}
	 assert_equal(v0.sort, rep[i], "<dup val #{i}>")
      end
   end

   def test_07_in_memory
      assert_equal(nil, $bdb.close, "<close>")
      assert_kind_of(BDB1::Btree, $bdb = BDB1::Btree.open, "<open in memory>")
      assert_equal(0, $bdb.size, "<must be 0 after reopen>")
   end

   def test_08_in_memory_get_set
      (33 .. 126).each do |i|
	 key = i.to_s * 5
	 val = i.to_s * 7
	 assert_equal(val, $bdb[key] = val, "<set in memory>")
      end
      assert_equal(94, $bdb.size, "<length in memory>")
      assert_equal(nil, $bdb.close, "<close>")
   end

   def test_09_btree_delete
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

   def test_10_index
      lines = $hash.keys
      array = []
      10.times do
	 h = lines[rand(lines.size - 1)]
	 array.push h
	 assert_equal($hash.index(h.reverse), $bdb.index(h.reverse), "<index>")
      end
      assert_equal($hash.indexes(array), $bdb.indexes(array), "<indexes>")
   end

   def test_11_convert
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

   def test_12_sh
      val = 'a' .. 'zz'
      assert_equal(nil, $bdb.close, "<close>")
      assert_kind_of(BDB1::Btree, $bdb = BDB1::AZ.open("tmp/aa", "w"), "<sh>")
      val.each do |l|
	 assert_equal(l, $bdb[l] = l, "<store>")
      end
      $bdb.each do |k, v|
	 assert_equal(k, v, "<fetch>")
      end
      assert_equal(nil, $bdb.close, "<close>")
      assert_kind_of(BDB1::Btree, $bdb = BDB1::Btree.open("tmp/aa"), "<sh>")
      val.each do |l|
	 assert_equal("yy_#{l}", $bdb["xx_#{l}"], "<fetch value>")
      end
      $bdb.each do |k, v|
	 assert_equal("xx_", k[0, 3], "<fetch key>")
	 assert_equal("yy_", v[0, 3], "<fetch key>")
      end
      assert_equal(nil, $bdb.close, "<close>")
      clean
   end

end

if defined?(RUNIT)
   RUNIT::CUI::TestRunner.run(TestBtree.suite)
end
