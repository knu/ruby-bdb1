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

class TestRecno < RUNIT::TestCase
   def test_00_error
      assert_error(BDB1::Fatal, 'BDB1::Recno.new(".", "a")', "invalid name")
      assert_error(BDB1::Fatal, 'BDB1::Recno.open("tmp/aa", "env" => 1)', "invalid Env")
   end
   def test_01_init
      assert_kind_of(BDB1::Recno, $bdb = BDB1::Recno.new("tmp/aa", "a"), "<open>")
   end
   def test_02_get_set
      assert_equal("alpha", $bdb[1] = "alpha", "<set value>")
      assert_equal("alpha", $bdb[1], "<retrieve value>")
      assert_equal(nil, $bdb[2] = nil, "<set nil>")
      assert_equal(nil, $bdb[2], "<retrieve nil>")
      assert($bdb.key?(1), "<has key>")
      assert_equal(false, $bdb.key?(3), "<has unknown key>")
      assert($bdb.value?(nil), "<has nil>")
      assert($bdb.value?("alpha"), "<has value>")
      assert_equal(false, $bdb.value?("unknown"), "<has unknown value>")
      assert_equal(false, $bdb.put(1, "gamma", BDB1::NOOVERWRITE), "<nooverwrite>")
      assert_equal("alpha", $bdb[1], "<must not be changed>")
      assert_equal([1, 2, 3].to_s, $bdb[4] = [1, 2, 3], "<array>")
      assert_equal([1, 2, 3].to_s, $bdb[4], "<retrieve array>")
      assert_equal({"a" => "b"}.to_s, $bdb[5] = {"a" => "b"}, "<hash>")
      assert_equal({"a" => "b"}.to_s, $bdb[5], "<retrieve hash>")
      assert($bdb.sync, "<sync>")
   end
   def test_03_delete
      size = $bdb.size
      i, arr = 0, []
      $bdb.each_key do |key|
	 arr << key
      end
      arr.each do |key|
	 assert_equal($bdb, $bdb.delete(1), "<delete value>")
	 i  += 1
      end
      assert(size == i, "<delete count>")
      assert_equal(0, $bdb.size, "<empty>")
   end
   def test_04_cursor
      array = ["a", "b", "c", "d", "e", "f", "g", "h", "i"]
      i = 1
      array.each do |x|
	 assert_equal(x, $bdb[i] = x, "<set value>")
	 i += 1
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
      arr = $bdb.reject {|k, v| k == 5 || v == "i" }
      has = array.reject {|k| k == "e" || k == "i" }
      assert_equal(has, arr.values.sort, "<reject>")
   end
   def test_05_reopen
      assert_equal(nil, $bdb.close, "<close>")
      assert_kind_of(BDB1::Recno, $bdb = BDB1::Recno.open("tmp/aa", "w", 
        "set_array_base" => 0), "<reopen with BDB1::RENUMBER>")
      assert_equal(0, $bdb.size, "<must be 0 after reopen>")
   end

   def test_07_in_memory
      assert_equal(nil, $bdb.close, "<close>")
      assert_kind_of(BDB1::Recno, $bdb = BDB1::Recno.open(nil), "<open in memory>")
      assert_equal(0, $bdb.size, "<must be 0 after reopen>")
   end
   def test_08_in_memory_get_set
      assert_equal("aa", $bdb[1] = "aa", "<set in memory>")
      assert_equal("cc", $bdb[1] = "cc", "<set in memory>")
      assert_equal("cc", $bdb[1], "<get in memory>")
      assert_equal(nil, $bdb.close, "<close>")
   end

end

RUNIT::CUI::TestRunner.run(TestRecno.suite)
