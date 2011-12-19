require 'helper'

class TestMarshal < Test::Unit::TestCase
  class AZ < BDB1::Btree
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

  def test_00_error
    clean_tmpdir
    assert_raises(BDB1::Fatal, "invalid name") do
      BDB1::Btree.new(".", "a")
    end
    assert_raises(BDB1::Fatal, "invalid Env") do
      BDB1::Btree.open(tmpdir("aa"), "env" => 1)
    end
  end

  def test_01_file
    sub_file_init
    sub_file_get_set
    sub_file_delete
    sub_file_cursor
    sub_file_reopen
    sub_file_dup
    sub_file_close
  end

  def sub_file_init
    assert_kind_of(BDB1::Btree, @bdb = BDB1::Btree.new(tmpdir("aa"), "a", "marshal" => Marshal), "<open>")
  end

  def sub_file_get_set
    assert_equal([12, "alpha"], @bdb["alpha"] = [12, "alpha"], "<set value>")
    assert_equal([12, "alpha"], @bdb["alpha"].to_orig, "<retrieve value>")
    assert_equal(nil, @bdb["gamma"] = nil, "<set nil>")
    assert_equal(nil, @bdb["gamma"].to_orig, "<retrieve nil>")
    assert(@bdb.key?("alpha").to_orig == [12, "alpha"], "<has key>")
    assert_equal(false, @bdb.key?("unknown"), "<has unknown key>")
    assert(@bdb.value?(nil), "<has nil>")
    assert(@bdb.value?([12, "alpha"]), "<has value>")
    assert_equal(false, @bdb.value?("unknown"), "<has unknown value>")
    assert_equal(false, @bdb.put("alpha", "gamma", BDB1::NOOVERWRITE), "<nooverwrite>")
    assert_equal([12, "alpha"], @bdb["alpha"].to_orig, "<must not be changed>")
    assert_equal([1, 2, [3, 4]], @bdb["array"] = [1, 2, [3, 4]], "<array>")
    assert_equal([1, 2, [3, 4]], @bdb["array"].to_orig, "<retrieve array>")
    assert_equal({"a" => "b"}, @bdb["hash"] = {"a" => "b"}, "<hash>")
    assert_equal({"a" => "b"}, @bdb["hash"].to_orig, "<retrieve hash>")
    assert(@bdb.sync, "<sync>")
  end

  def sub_file_delete
    size = @bdb.size
    i = 0
    @bdb.each do |key, value|
      assert_equal(@bdb, @bdb.delete(key), "<delete value>")
      i += 1
    end
    assert(size == i, "<delete count>")
    assert_equal(0, @bdb.size, "<empty>")
  end

  def sub_file_cursor
    cat = Struct.new("Cat", :name, :age, :life)
    array = ["abc", [1, 3], {"aa" => 12}, [2, {"bb" => "cc"}, 4],
      cat.new("cat", 15, 7)]
    array.each do |x|
      assert_equal(x, @bdb[x] = x, "<set value>")
    end
    assert(array.size == @bdb.size, "<set count>")
    @bdb.each_value do |x|
      assert(array.index(x) != nil)
    end
    @bdb.reverse_each_value do |x|
      assert(array.index(x) != nil)
    end
  end

  def sub_file_reopen
    assert_equal(nil, @bdb.close, "<close>")
    assert_kind_of(BDB1::Btree, @bdb = BDB1::Btree.open(tmpdir("aa"), "w",
	"set_flags" => BDB1::DUP, "marshal" => Marshal),
      "<reopen with DB_DUP>")
    assert_equal(0, @bdb.size, "<must be 0 after reopen>")
  end

  def sub_file_dup
    array = [[[0, "a"], [1, "b"], [2, "c"], [3, "d"]],
      [{"aa" => 0}, {"bb" => 1}, {"cc" => 2}],
      [["aaa", 12], [12, "bbb"]],
      ["aaaa"]]
    ind = 0
    array.each do |arr|
      arr.each do |i|
        assert_equal(i, @bdb[ind.to_s] = i, "<set dup>")
      end
      ind += 1
    end
  end

  def sub_file_close
    assert_equal(nil, @bdb.close, "<close>")
  end

  def test_02_memory
    sub_memory_open
    sub_memory_get_set
    sub_memory_close
  end

  def sub_memory_open
    assert_kind_of(BDB1::Btree, @bdb = BDB1::Btree.open("marshal" => Marshal), "<open in memory>")
    assert_equal(0, @bdb.size, "<must be 0 after open>")
  end

  def sub_memory_get_set
    assert_equal([1, 2, [3, 4]], @bdb["array"] = [1, 2, [3, 4]], "<set in memory>")
    assert_equal([1, 2, [3, 4]], @bdb["array"].to_orig, "<get in memory>")
    assert_equal({"a" => "b"}, @bdb["hash"] = {"a" => "b"}, "<set in memory>")
    assert_equal({"a" => "b"}, @bdb["hash"].to_orig, "<get in memory>")
    assert_equal("cc", @bdb["bb"] = "cc", "<set in memory>")
    assert_equal("cc", @bdb["bb"].to_orig, "<get in memory>")
  end

  def sub_memory_close
    assert_equal(nil, @bdb.close, "<close>")
  end

  def test_04_btree
    sub_modify
    sub_sh
  end

  def sub_modify
    assert_kind_of(BDB1::Btree, @bdb = BDB1::Btree.open(tmpdir("aa"), "w",
        "marshal" => Marshal),
      "<reopen RECNUM>")
    array = [1, "a", {"a" => 12}]
    assert_equal(array, @bdb["a"] = array, "<set>")
    arr = @bdb["a"]
    arr.push [1, 2]; array.push [1, 2]
    assert_equal(array, @bdb["a"].to_orig, "<get>")
    @bdb["a"][-1] = 4; array[-1] = 4
    assert_equal(array, @bdb["a"].to_orig, "<get>")
    @bdb["a"][-1] = ["abc", 4]; array[-1] = ["abc", 4]
    assert_equal(array, @bdb["a"].to_orig, "<get>")
    assert_equal(nil, @bdb.close, "<close>")
  end

  def sub_sh
    val = 'a' .. 'zz'
    assert_equal(nil, @bdb.close, "<close>")
    assert_kind_of(BDB1::Btree, @bdb = AZ.open(tmpdir("aa"), "w"), "<sh>")
    val.each do |l|
      assert_equal(l, @bdb[l] = l, "<store>")
    end
    @bdb.each do |k, v|
      assert_equal(k, v, "<fetch>")
    end
    assert_equal(nil, @bdb.close, "<close>")
    assert_kind_of(BDB1::Btree, @bdb = BDB1::Btree.open(tmpdir("aa")), "<sh>")
    val.each do |l|
      assert_equal("yy_#{l}", @bdb["xx_#{l}"], "<fetch value>")
    end
    @bdb.each do |k, v|
      assert_equal("xx_", k[0, 3], "<fetch key>")
      assert_equal("yy_", v[0, 3], "<fetch key>")
    end
    assert_equal(nil, @bdb.close, "<close>")
  end
end
