#!/usr/bin/ruby
$LOAD_PATH.unshift *%w{../src src tests}
require 'bdb1'
require 'runit_'

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
      assert_kind_of(BDB1::Btree, $bdb = BDB1::Btree.new("tmp/aa", "a", "marshal" => Marshal), "<open>")
   end
   def test_02_get_set
      assert_equal([12, "alpha"], $bdb["alpha"] = [12, "alpha"], "<set value>")
      assert_equal([12, "alpha"], $bdb["alpha"].to_orig, "<retrieve value>")
      assert_equal(nil, $bdb["gamma"] = nil, "<set nil>")
      assert_equal(nil, $bdb["gamma"].to_orig, "<retrieve nil>")
      assert($bdb.key?("alpha"), "<has key>")
      assert_equal(false, $bdb.key?("unknown"), "<has unknown key>")
      assert($bdb.value?(nil), "<has nil>")
      assert($bdb.value?([12, "alpha"]), "<has value>")
      assert_equal(false, $bdb.value?("unknown"), "<has unknown value>")
      assert_equal(false, $bdb.put("alpha", "gamma", BDB1::NOOVERWRITE), "<nooverwrite>")
      assert_equal([12, "alpha"], $bdb["alpha"].to_orig, "<must not be changed>")
      assert_equal([1, 2, [3, 4]], $bdb["array"] = [1, 2, [3, 4]], "<array>")
      assert_equal([1, 2, [3, 4]], $bdb["array"].to_orig, "<retrieve array>")
      assert_equal({"a" => "b"}, $bdb["hash"] = {"a" => "b"}, "<hash>")
      assert_equal({"a" => "b"}, $bdb["hash"].to_orig, "<retrieve hash>")
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
      cat = Struct.new("Cat", :name, :age, :life)
      array = ["abc", [1, 3], {"aa" => 12}, [2, {"bb" => "cc"}, 4],
	 cat.new("cat", 15, 7)]
      array.each do |x|
	 assert_equal(x, $bdb[x] = x, "<set value>")
      end
      assert(array.size == $bdb.size, "<set count>")
      $bdb.each_value do |x|
	 assert(array.index(x) != nil)
      end
      $bdb.reverse_each_value do |x|
	 assert(array.index(x) != nil)
      end
   end
   def test_05_reopen
      assert_equal(nil, $bdb.close, "<close>")
      assert_kind_of(BDB1::Btree, $bdb = BDB1::Btree.open("tmp/aa", "w", 
	"set_flags" => BDB1::DUP, "marshal" => Marshal),
        "<reopen with DB_DUP>")
      assert_equal(0, $bdb.size, "<must be 0 after reopen>")
   end
   def test_06_dup
      array = [[[0, "a"], [1, "b"], [2, "c"], [3, "d"]],
	 [{"aa" => 0}, {"bb" => 1}, {"cc" => 2}],
	 [["aaa", 12], [12, "bbb"]],
	 ["aaaa"]]
      ind = 0
      array.each do |arr|
	 arr.each do |i|
	    assert_equal(i, $bdb[ind.to_s] = i, "<set dup>")
	 end
	 ind += 1
      end
   end
   def test_07_in_memory
      assert_equal(nil, $bdb.close, "<close>")
      assert_kind_of(BDB1::Btree, $bdb = BDB1::Btree.open("marshal" => Marshal), "<open in memory>")
      assert_equal(0, $bdb.size, "<must be 0 after reopen>")
   end
   def test_08_in_memory_get_set
      assert_equal([1, 2, [3, 4]], $bdb["array"] = [1, 2, [3, 4]], "<set in memory>")
      assert_equal([1, 2, [3, 4]], $bdb["array"].to_orig, "<get in memory>")
      assert_equal({"a" => "b"}, $bdb["hash"] = {"a" => "b"}, "<set in memory>")
      assert_equal({"a" => "b"}, $bdb["hash"].to_orig, "<get in memory>")
      assert_equal("cc", $bdb["bb"] = "cc", "<set in memory>")
      assert_equal("cc", $bdb["bb"].to_orig, "<get in memory>")
      assert_equal(nil, $bdb.close, "<close>")
   end
   def test_09_modify
      assert_kind_of(BDB1::Btree, $bdb = BDB1::Btree.open("tmp/aa", "w",
							"marshal" => Marshal),
		     "<reopen RECNUM>")
      array = [1, "a", {"a" => 12}]
      assert_equal(array, $bdb["a"] = array, "<set>")
      arr = $bdb["a"]
      arr.push [1, 2]; array.push [1, 2]
      assert_equal(array, $bdb["a"].to_orig, "<get>")
      $bdb["a"][-1] = 4; array[-1] = 4
      assert_equal(array, $bdb["a"].to_orig, "<get>")
      $bdb["a"][-1] = ["abc", 4]; array[-1] = ["abc", 4]
      assert_equal(array, $bdb["a"].to_orig, "<get>")
      assert_equal(nil, $bdb.close, "<close>")
      clean
   end
end

RUNIT::CUI::TestRunner.run(TestBtree.suite)
