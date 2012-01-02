require 'helper'

class TestHash < Test::Unit::TestCase
  class HashRuby < BDB1::Hash
    def bdb1_h_hash a
      a.hash
    end
  end

  class AZ < BDB1::Hash
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
    assert_raises(BDB1::Fatal, "invalid name") do
      BDB1::Hash.new(tmpdir("."), "a")
    end
    assert_raises(BDB1::Fatal, "invalid Env") do
      BDB1::Hash.open(tmpdir("aa"), "env" => 1)
    end
    assert_raises(TypeError) { BDB1::Hash.new(tmpdir("aa"), "set_h_ffactor" => "a") }
    assert_raises(BDB1::Fatal) { BDB1::Hash.new(tmpdir("aa"), "set_h_nemem" => "a") }
    assert_raises(BDB1::Fatal) { BDB1::Hash.new(tmpdir("aa"), "set_h_hash" => "a") }
    assert_raises(TypeError) { BDB1::Hash.new(tmpdir("aa"), "set_cachesize" => "a") }
    assert_raises(BDB1::Fatal) { BDB1::Hash.new(tmpdir("aa"), "set_fetch_key" => "a") }
    assert_raises(BDB1::Fatal) { BDB1::Hash.new(tmpdir("aa"), "set_store_key" => "a") }
    assert_raises(BDB1::Fatal) { BDB1::Hash.new(tmpdir("aa"), "set_fetch_value" => "a") }
    assert_raises(BDB1::Fatal) { BDB1::Hash.new(tmpdir("aa"), "set_store_value" => "a") }
    assert_raises(TypeError) { BDB1::Hash.new(tmpdir("aa"), "set_lorder" => "a") }
  end

  def test_01_file
    sub_file_init
    sub_file_get_set
    sub_file_delete
    sub_file_cursor
    sub_file_reopen
    sub_file_close
  end

  def sub_file_init
    assert_kind_of(BDB1::Hash, @bdb = BDB1::Hash.new(tmpdir("aa"), "a"), "<open>")
  end

  def sub_file_get_set
    assert_equal(true, @bdb.empty?, "<empty>")
    assert_equal("alpha", @bdb["alpha"] = "alpha", "<set value>")
    assert_equal(false, @bdb.empty?, "<empty>")
    assert_equal("alpha", @bdb["alpha"], "<retrieve value>")
    assert_equal("alpha", @bdb.fetch("alpha"), "<fetch value>")
    assert_equal("beta", @bdb.fetch("xxx", "beta"), "<fetch nil>")
    assert_equal("xxx", @bdb.fetch("xxx") {|x| x}, "<fetch nil>")
    assert_raises(IndexError) { @bdb.fetch("xxx") }
    assert_raises(ArgumentError) { @bdb.fetch("xxx", "beta") {} }
    assert_equal(nil, @bdb["gamma"] = nil, "<set nil>")
    assert_equal(nil, @bdb["gamma"], "<retrieve nil>")
    assert(@bdb.key?("alpha") == "alpha", "<has key>")
    assert_equal(false, @bdb.key?("unknown"), "<has unknown key>")
    assert(@bdb.value?(nil), "<has nil>")
    assert(@bdb.value?("alpha"), "<has value>")
    assert_equal(false, @bdb.value?("unknown"), "<has unknown value>")
    assert_equal(false, @bdb.put("alpha", "gamma", BDB1::NOOVERWRITE), "<nooverwrite>")
    assert_equal("alpha", @bdb["alpha"], "<must not be changed>")
    assert(@bdb.both?("alpha", "alpha"), "<has both>")
    assert(! @bdb.both?("alpha", "beta"), "<don't has both>")
    assert(! @bdb.both?("unknown", "alpha"), "<don't has both>")
    assert_equal([1, 2, 3], @bdb["array"] = [1, 2, 3], "<array>")
    assert_equal([1, 2, 3].to_s, @bdb["array"], "<retrieve array>")
    assert_equal({"a" => "b"}, @bdb["hash"] = {"a" => "b"}, "<hash>")
    assert_equal({"a" => "b"}.to_s, @bdb["hash"], "<retrieve hash>")
    assert(@bdb.sync, "<sync>")
  end

  def sub_file_delete
    size = @bdb.size
    i, arr = 0, []
    @bdb.each do |key, value|
      arr << key
      i += 1
    end
    arr.each do |key|
      assert_equal(@bdb, @bdb.delete(key), "<delete value>")
    end
    assert(size == i, "<delete count>")
    assert_equal(0, @bdb.size, "<empty>")
    $hash = {}
    (33 .. 126).each do |i|
      key = i.to_s * 5
      @bdb[key] = i.to_s * 7
      $hash[key] = i.to_s * 7
      assert_equal(@bdb[key], $hash[key], "<set #{key}>")
    end
    assert_equal(@bdb.size, $hash.size, "<size after load>")
    if @bdb.respond_to?(:select)
      assert_raises(ArgumentError) { @bdb.select("xxx") {}}
      assert_equal([], @bdb.select { false }, "<select none>")
      assert_equal($hash.values.sort, @bdb.select { true }.sort, "<select all>")
    end
    arr0 = []
    @bdb.each_key {|key| arr0 << key }
    assert_equal($hash.keys.sort, arr0.sort, "<each key>")
    arr0.each do |key|
      assert($hash.key?(key), "<key after delete>")
      assert_equal(@bdb, @bdb.delete(key), "<delete value>")
    end
  end

  def sub_file_cursor
    array = ["a", "b", "c", "d", "e", "f", "g", "h", "i"]
    array.each do |x|
      assert_equal(x, @bdb[x] = x, "<set value>")
    end
    assert(array.size == @bdb.size, "<set count>")
    arr = []
    @bdb.each_value do |x|
      arr << x
    end
    assert_equal(array, arr.sort, "<order>")
  end

  def sub_file_reopen
    assert_equal(nil, @bdb.close, "<close>")
    assert_kind_of(BDB1::Hash, @bdb = BDB1::Hash.open(tmpdir("aa"), "w",
	"set_flags" => BDB1::DUP),
      "<reopen with DB_DUP>")
    assert_equal(0, @bdb.size, "<must be 0 after reopen>")
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
    assert_kind_of(BDB1::Hash, @bdb = BDB1::Hash.open(nil), "<open in memory>")
    assert_equal(0, @bdb.size, "<must be 0 after open>")
  end

  def sub_memory_get_set
    (33 .. 126).each do |i|
      key = i.to_s * 5
      val = i.to_s * 7
      assert_equal(val, @bdb[key] = val, "<set in memory>")
    end
    assert_equal(94, @bdb.size, "<length in memory>")
  end

  def sub_memory_close
    assert_equal(nil, @bdb.close, "<close>")
  end

  def test_04_hash
    sub_index
    sub_convert
    sub_has
    sub_sh
    sub_sh_call
  end

  def sub_index
    assert_kind_of(HashRuby,
      @bdb = HashRuby.open(tmpdir("aa"), "w",
        "set_pagesize" => 1024,
        "set_cachesize" => 32 * 1024),
      "<open>")
    @hash = {}
    File.foreach(examplesdir("wordtest")) do |line|
      line.chomp!
      @hash[line] = line.reverse
      @bdb[line] = line.reverse
    end
    @bdb.each do |k, v|
      assert_equal(@hash[k], v, "<value>")
    end
    assert_equal(@bdb.size, @hash.size, "<size after delete_if>")
  end

  def sub_convert
    h = @bdb.to_hash
    h.each do |k, v|
      assert_equal(v, @hash[k], "<to_hash>")
    end
    @hash.each do |k, v|
      assert_equal(v, h[k], "<to_hash>")
    end
    h1 = @hash.to_a
    h2 = @bdb.to_a
    assert_equal(h1.size, h2.size, "<equal>")
  end

  def sub_has
    assert_kind_of(HashRuby,
      @bdb = HashRuby.open(tmpdir("aa"), "w",
        "set_pagesize" => 1024,
        "set_cachesize" => 32 * 1024),
      "<open>")
    @hash = {}
    ('a' .. 'z').each do |k|
      assert_equal((@hash[k] = k * 4), (@bdb[k] = k * 4), "<set>")
    end
    @hash.each_key do |k|
      assert(@bdb.has_key?(k) == (k * 4), "<has key>")
    end
    @hash.each_value do |v|
      assert(@bdb.has_value?(v), "<has key>")
    end
    @bdb.each do |k, v|
      assert(@hash.has_key?(k), "<has key>")
      assert(@hash.has_value?(v), "<has key>")
    end
  end

  def intern_sh
    val = 'a' .. 'zz'
    val.each do |l|
      assert_equal(l, @bdb[l] = l, "<store>")
    end
    @bdb.each do |k, v|
      assert_equal(k, v, "<fetch>")
    end
    assert_equal(nil, @bdb.close, "<close>")
    assert_kind_of(BDB1::Hash, @bdb = BDB1::Hash.open(tmpdir("aa")), "<sh>")
    val.each do |l|
      assert_equal("yy_#{l}", @bdb["xx_#{l}"], "<fetch value>")
    end
    @bdb.each do |k, v|
      assert_equal("xx_", k[0, 3], "<fetch key>")
      assert_equal("yy_", v[0, 3], "<fetch key>")
    end
    assert_equal(nil, @bdb.close, "<close>")
    clean_tmpdir
  end

  def sub_sh
    assert_equal(nil, @bdb.close, "<close>")
    assert_kind_of(BDB1::Hash, @bdb = AZ.open(tmpdir("aa"), "w"), "<sh>")
    intern_sh
  end

  def sub_sh_call
    @bdb = BDB1::Hash.new(tmpdir("aa"), "w",
      "set_store_key" => proc {|a| "xx_" + a },
      "set_fetch_key" => proc {|a| a.sub(/^xx_/, '') },
      "set_store_value" => proc {|a| "yy_" + a },
      "set_fetch_value" => proc {|a| a.sub(/^yy_/, '') })
    assert_kind_of(BDB1::Hash, @bdb)
    intern_sh
  end
end
