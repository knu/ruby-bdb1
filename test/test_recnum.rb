require 'helper'

class TestRecnum < Test::Unit::TestCase
  def reinject(i)
    1.upto(i) do |i|
      @array.push i.to_s
      @bdb1.push i
    end
  end

  def test_00_error
    clean_tmpdir
    assert_raises(BDB1::Fatal, "invalid name") do
      BDB1::Recnum.new(tmpdir("."), "a")
    end
    assert_raises(BDB1::Fatal, "invalid Env") do
      BDB1::Recnum.open(tmpdir("aa"), "env" => 1)
    end
  end

  def test_01
    sub_init
    sub_get_set
    sub_subseq
    sub_op
    sub_at
    sub_slice
    sub_slice_bang
    sub_reverse
    sub_index
    sub_delete
    sub_reject
    sub_clear
    sub_fill
    sub_include
    sub_replace
    sub_reopen
    sub_collect
    sub_insert
    sub_flp
    sub_cmp
    sub_compact
    sub_close
  end

  def sub_init
    assert_kind_of(BDB1::Recnum, @bdb1 = BDB1::Recnum.new(tmpdir("aa"), "w"), "<open>")
    @array = []
  end

  def sub_get_set
    reinject(99)
    @bdb1.each_index do |i|
      assert_equal(@bdb1[i], @array[i], "<sequential get>")
    end
    -1.downto(-99) do |i|
      assert_equal(@bdb1[i], @array[i], "<sequential get reversed>")
    end
    200.times do
      i = rand(@array.size)
      assert_equal(@bdb1[i], @array[i], "<random get>")
    end
  end

  def sub_subseq
    assert_equal(@bdb1[-1], @array[-1], "<negative index>")
    assert_equal(@bdb1[2 .. 7], @array[2 .. 7], "<subseq>")
    @bdb1[2 .. 7] = "a", "b"
    @array[2 .. 7] = "a", "b"
    assert_equal(@bdb1.size, @array.size, "<size>")
    assert_equal(@bdb1.to_a, @array, "<small subseq>")
    @bdb1[3 .. 6] = "a", "b", "c", "d", "e", "g", "h", "i"
    @array[3 .. 6] = "a", "b", "c", "d", "e", "g", "h", "i"
    assert_equal(@bdb1.size, @array.size, "<size>")
    assert_equal(@bdb1.to_a, @array, "<big subseq>")
  end

  def sub_op
    assert_equal(@bdb1 & ["1", "2", "3"], @array & ["1", "2", "3"], "<&>")
    assert_equal(@bdb1 & [], @array & [], "<&>")
    assert_equal(@bdb1 + ["3", "4"], @array + ["3", "4"], "<plus>")
    assert_equal(["3", "4"] + @bdb1, ["3", "4"] + @array, "<plus>")
    assert_equal(@bdb1 - ["3", "4"], @array - ["3", "4"], "<minus>")
    assert_equal(["3", "4"] - @bdb1, ["3", "4"] - @array, "<minus>")
    assert_equal(@bdb1 * 2, @array * 2, "<multiply>")
    assert_equal(@bdb1 * ":", @array * ":", "<multiply>")
    assert_equal(@bdb1 * "", @array * "", "<multiply>")
    assert_equal(@bdb1.size, @array.size, "<size>")
  end

  def sub_at
    assert_equal(@bdb1.to_a, @array, "<equal array>")
    assert_equal(@bdb1.at(0), @array.at(0), "<positive at>")
    assert_equal(@bdb1.at(10), @array.at(10), "<positive at>")
    assert_equal(@bdb1.at(99), @array.at(99), "<positive at>")
    assert_equal(@bdb1.at(205), @array.at(205), "<positive at>")
    assert_equal(@bdb1.at(-1), @array.at(-1), "<negative at>")
    assert_equal(@bdb1.at(-100), @array.at(-100), "<negative at>")
    assert_equal(@bdb1.at(205), @array.at(205), "<negative at>")
  end

  def sub_slice
    assert_equal(@bdb1.to_a, @array, "<equal array>")
    100.times do
      i = rand(@bdb1.size)
      assert_equal(@bdb1.slice(i), @array.slice(i), "<slice>")
    end
    100.times do
      i = rand(@bdb1.size)
      assert_equal(@bdb1.slice(-i), @array.slice(-i), "<negative slice>")
    end
    100.times do
      i = rand(@bdb1.size)
      j = rand(@bdb1.size)
      assert_equal(@bdb1.slice(i, j), @array.slice(i, j), "<slice>")
    end
    100.times do
      i = rand(@bdb1.size)
      j = rand(@bdb1.size)
      assert_equal(@bdb1.slice(-i, j), @array.slice(-i, j), "<negative slice>")
    end
    100.times do
      i = rand(@bdb1.size)
      j = rand(@bdb1.size)
      assert_equal(@bdb1.slice(i .. j), @array.slice(i .. j), "<range>")
    end
    100.times do
      i = rand(@bdb1.size)
      j = rand(@bdb1.size)
      assert_equal(@bdb1.slice(-i, j), @array.slice(-i, j), "<negative range>")
    end
  end

  def sub_slice_bang
    assert_equal(@bdb1.to_a, @array, "<equal array>")
    10.times do |iter|
      i = rand(@bdb1.size)
      assert_equal(@bdb1.slice!(i), @array.slice!(i), "<#{iter} slice!(#{i})>")
      assert_equal(@bdb1.size, @array.size, "<size after slice!(#{i})  #{iter}>")
      reinject(60) if @bdb1.size < 20
    end
    10.times do |iter|
      i = rand(@bdb1.size)
      assert_equal(@bdb1.slice!(-i), @array.slice!(-i), "<slice!(#{-i})>")
      assert_equal(@bdb1.size, @array.size, "<size after slice!(#{-i}) #{iter}>")
      reinject(60) if @bdb1.size < 20
    end
    10.times do  |iter|
      i = rand(@bdb1.size)
      j = rand(@bdb1.size)
      assert_equal(@bdb1.slice!(i, j), @array.slice!(i, j), "<slice!(#{i}, #{j})>")
      assert_equal(@bdb1.size, @array.size, "<size after slice!(#{i}, #{j}) #{iter}>")
      reinject(60) if @bdb1.size < 20
    end
    10.times do |iter|
      i = rand(@bdb1.size)
      j = i + rand(@bdb1.size - i)
      assert_equal(@bdb1.slice!(-i, j), @array.slice!(-i, j), "<slice!(#{-i}, #{j})>")
      assert_equal(@bdb1.size, @array.size, "<size after slice!(#{-i}, #{j}) #{iter}>")
      reinject(60) if @bdb1.size < 20
    end
    reinject(60)
    another = 0
    10.times do |iter|
      i = rand(@bdb1.size)
      j = i + rand(@bdb1.size - i)
      if ! @array.slice(i .. j)
        assert_raises(RangeError, "<invalid range>") { @bdb1.slice!(i .. j) }
        another += 1
        redo if another < 10
        another = 0
        next
      end
      assert_equal(@bdb1.slice!(i .. j), @array.slice!(i .. j), "<slice!(#{i} .. #{j})>")
      assert_equal(@bdb1.size, @array.size, "<size after slice! #{iter}>")
      another = 0
      reinject(60) if @bdb1.size < 20
    end
    another = 0
    10.times do |iter|
      i = rand(@bdb1.size)
      j = 1 + rand(@bdb1.size - i)
      if ! @array.slice(-i .. -j)
        assert_raises(RangeError, "<invalid range>") { @bdb1.slice!(-i .. -j) }
        another += 1
        redo if another < 10
        another = 0
        next
      end
      assert_equal(@bdb1.slice!(-i .. -j), @array.slice!(-i .. -j), "<slice!(#{-i} .. #{-j})>")
      assert_equal(@bdb1.size, @array.size, "<size after slice! #{iter}>")
      another = 0
      reinject(60) if @bdb1.size < 20
    end
    reinject(40) if @bdb1.size < 40
    assert_equal(@bdb1.size, @array.size, "<size end slice!>")
    assert_equal(@bdb1.to_a, @array, "<size end slice!>")
  end

  def sub_reverse
    assert_equal(@bdb1.reverse, @array.reverse, "<reverse>")
    @array.reverse! ; @bdb1.reverse!
    assert_equal(@bdb1.to_a, @array, "<reverse bang>")
    assert_equal(@bdb1.size, @array.size, "<size after reverse>")
  end

  def sub_index
    100.times do
      i = rand(@bdb1.size)
      assert_equal(@bdb1.index(i), @array.index(i), "<index #{i}>")
      assert_equal(@bdb1.index(-i), @array.index(-i), "<negative index #{i}>")
    end
    100.times do
      i = rand(@bdb1.size)
      assert_equal(@bdb1.rindex(i), @array.rindex(i), "<rindex #{i}>")
      assert_equal(@bdb1.rindex(-i), @array.rindex(-i), "<negative rindex #{i}>")
    end
    100.times do
      aa = []
      rand(12).times do
        aa.push(rand(@bdb1.size))
      end
      assert_equal(@array.values_at(*aa), @bdb1.values_at(*aa), "<values_at>")
    end
    100.times do
      aa = []
      rand(12).times do
        aa.push(-1 * rand(@bdb1.size))
      end
      assert_equal(@array.values_at(*aa), @bdb1.values_at(*aa), "<negative values_at #{aa}>")
    end
  end

  def sub_delete
    assert_equal(@bdb1.to_a, @array, "<before delete>")
    100.times do
      i = rand(@bdb1.size)
      assert_equal(@bdb1.delete(i), @array.delete(i), "<delete #{i}>")
      reinject(60) if @bdb1.size < 20
    end
    assert_equal(@bdb1.to_a, @array, "<after delete>")
    100.times do
      i = rand(@bdb1.size)
      assert_equal(@bdb1.delete_at(i), @array.delete_at(i), "<delete at #{i}>")
      reinject(60) if @bdb1.size < 20
    end
    assert_equal(@bdb1.to_a, @array, "<after delete_at>")
    reinject(60) if @bdb1.size < 60
    assert_equal(@bdb1.delete_if { false }, @bdb1, "<delete_if false>")
    assert_equal(@bdb1.to_a, @array, "<after delete_if false>")
    assert_equal(@bdb1.delete_if { true }, @bdb1, "<delete_if true>")
    assert_equal(@bdb1.size, 0, "<after delete_if true>")
    @bdb1.push *@array
    assert_equal(@bdb1.to_a, @array, "<after push>")
    100.times do
      i = rand(@bdb1.size)
      assert_equal(@bdb1.delete_if {|i| i.to_i > 32}, @bdb1, "<delete_if condition>")
      @array.delete_if {|i| i.to_i > 32}
      assert_equal(@bdb1.to_a, @array, "<delete_if condition compare>")
      reinject(60) if @bdb1.size < 60
    end
    reinject(99) if @bdb1.size < 99
  end

  def sub_reject
    assert_equal(@bdb1.to_a, @array, "<before reject!>")
    assert_equal(nil, @bdb1.reject! { false }, "<reject! false>")
  end

  def sub_clear
    @bdb1.clear ; @array.clear
    assert_equal(@array.size, @bdb1.size, "<size after clear>")
    assert_equal(@array, @bdb1.to_a, "<after clear>")
  end

  def sub_fill
    @bdb1.fill "32", 0, 99; @array.fill "32", 0, 99
    assert_equal(@array.size, @bdb1.size, "<size after fill>")
    assert_equal(@array, @bdb1.to_a, "<after fill>")
    @bdb1.fill "42"; @array.fill "42"
    assert_equal(@array.size, @bdb1.size, "<size after fill>")
    assert_equal(@array, @bdb1.to_a, "<after fill>")
    10.times do |iter|
      k = rand(@bdb1.size).to_s
      i = rand(@bdb1.size)
      assert_equal(@bdb1.fill(k, i).to_a, @array.fill(k, i), "<#{iter} fill(#{i})>")
      assert_equal(@bdb1.size, @array.size, "<size after fill(#{i})  #{iter}>")
      reinject(60) if @bdb1.size < 20
    end
    10.times do |iter|
      k = rand(@bdb1.size).to_s
      i = rand(@bdb1.size)
      assert_equal(@bdb1.fill(k, -i).to_a, @array.fill(k, -i), "<fill(#{-i})>")
      assert_equal(@bdb1.size, @array.size, "<size after fill(#{-i}) #{iter}>")
      reinject(60) if @bdb1.size < 20
    end
    10.times do  |iter|
      k = rand(@bdb1.size).to_s
      i = rand(@bdb1.size)
      j = rand(@bdb1.size)
      assert_equal(@bdb1.fill(k, i, j).to_a, @array.fill(k, i, j), "<fill(#{i}, #{j})>")
      assert_equal(@bdb1.size, @array.size, "<size after fill(#{i}, #{j}) #{iter}>")
      reinject(60) if @bdb1.size < 20
    end
    10.times do |iter|
      k = rand(@bdb1.size).to_s
      i = rand(@bdb1.size)
      j = i + rand(@bdb1.size - i)
      assert_equal(@bdb1.fill(k, -i, j).to_a, @array.fill(k, -i, j), "<fill(#{-i}, #{j})>")
      assert_equal(@bdb1.size, @array.size, "<size after fill(#{-i}, #{j}) #{iter}>")
      reinject(60) if @bdb1.size < 20
    end
    reinject(60)
    another = 0
    10.times do |iter|
      k = rand(@bdb1.size).to_s
      i = rand(@bdb1.size)
      j = i + rand(@bdb1.size - i)
      if ! @array.slice(i .. j)
        assert_raises(RangeError, "<invalid range>") { @bdb1.fill(k, i .. j) }
        another += 1
        redo if another < 10
        another = 0
        next
      end
      assert_equal(@bdb1.fill(k, i .. j).to_a, @array.fill(k, i .. j), "<fill(#{i} .. #{j})>")
      assert_equal(@bdb1.size, @array.size, "<size after fill #{iter}>")
      another = 0
      reinject(60) if @bdb1.size < 20
    end
    another = 0
    10.times do |iter|
      k = rand(@bdb1.size).to_s
      i = rand(@bdb1.size)
      j = 1 + rand(@bdb1.size - i)
      if ! @array.slice(-i .. -j)
        assert_raises(RangeError, "<invalid range>") { @bdb1.fill(k, -i .. -j) }
        another += 1
        redo if another < 10
        another = 0
        next
      end
      assert_equal(@bdb1.fill(k, -i .. -j).to_a, @array.fill(k, -i .. -j), "<fill(#{-i} .. #{-j})>")
      assert_equal(@bdb1.size, @array.size, "<size after fill #{iter}>")
      another = 0
      reinject(60) if @bdb1.size < 20
    end
    reinject(60) if @bdb1.size < 60
    assert_equal(@bdb1.size, @array.size, "<size end fill>")
    assert_equal(@bdb1.to_a, @array, "<size end fill>")
  end

  def sub_include
    @bdb1.clear; @array.clear; reinject(99)
    assert_equal(@bdb1.size, @array.size, "<size end clear>")
    assert_equal(@bdb1.to_a, @array, "<size end clear>")
    100.times do
      k = rand(@bdb1.size + 20).to_s
      assert_equal(@array.include?(k), @bdb1.include?(k), "<include(#{k})>")
    end
  end

  def sub_replace
    @array.replace(('a' .. 'zz').to_a)
    @bdb1.replace(('a' .. 'zz').to_a)
    assert_equal(@bdb1.size, @array.size, "<size end fill>")
    assert_equal(@bdb1.to_a, @array, "<size end fill>")
  end

  def sub_reopen
    @bdb1.close
    assert_kind_of(BDB1::Recnum, @bdb1 = BDB1::Recnum.new(tmpdir("aa"), "a"), "<open>")
    assert_equal(@bdb1.size, @array.size, "<size end reopen>")
    assert_equal(@bdb1.to_a, @array, "<size end open>")
  end

  def sub_collect
    a = @bdb1.collect
    assert_equal(@array, a, "<collect>")
    a = @array.collect {|k| k + "!"}
    b = @bdb1.collect {|k| k + "!"}
    assert_equal(a, b, "<collect>")
    assert_equal(@bdb1.to_a, @array, "<collect bdb>")

    a = @array.collect! {|k| k + "!"}
    b = @bdb1.collect! {|k| k + "!"}
    assert_equal(a, b, "<collect!>")
    assert_equal(@bdb1.to_a, @array, "<collect bdb>")
  end

  def sub_insert
    @array.insert(2, "1", "2", "3")
    @array.insert(-1, "4", "5")
    @array.insert(-4, "3", "5", "5", "5", "5")
    @bdb1.insert(2, 1, 2, 3)
    @bdb1.insert(-1, 4, 5)
    @bdb1.insert(-4, 3, 5, 5, 5, 5)
    assert_equal(@bdb1.to_a, @array, "<insert bdb>")
  end

  def sub_flp
    assert_equal(@array.first, @bdb1.first, "<first>")
    assert_equal(@array.last, @bdb1.last, "<last>")
    @bdb1.concat(@array)
    @array.concat(@array)
    assert_equal(@bdb1.to_a, @array, "<insert bdb>")
    assert_equal(@bdb1, @bdb1 << 12, "<push>")
    @array << "12"
    assert_equal(@bdb1.to_a, @array, "<insert bdb>")
    assert_equal(@array.pop, @bdb1.pop, "<pop>")
    @array.unshift("5", "6")
    @bdb1.unshift(5, 6)
    assert_equal(@bdb1.to_a, @array, "<unshift bdb>")
    assert_equal(@array.shift, @bdb1.shift, "<shift>")
    assert_equal(@bdb1.to_a, @array, "<unshift bdb>")
    assert_equal(0, @bdb1 <=> @array, "<cmp>")
  end

  def sub_cmp
    assert_kind_of(BDB1::Recnum, bdb1 = BDB1::Recnum.new(tmpdir("bb"), "a"), "<open>")
    assert_equal(true, bdb1.empty?, "<empty>")
    @array.each {|a| bdb1 << a}
    assert_equal(false, bdb1.empty?, "<empty>")
    assert_equal(0, @bdb1 <=> bdb1, "<=>")
    bdb1 << 12
    assert_equal(1, bdb1 <=> @bdb1, "<=>")
    assert_equal(-1, @bdb1 <=> bdb1, "<=>")
  end

  def sub_compact
    assert_equal(@array.compact, @bdb1.compact, "<compact>")
    @array.compact!
    @bdb1.compact!
    assert_equal(@bdb1.to_a, @array, "<compact!>")
  end

  def sub_close
    assert_equal(nil, @bdb1.close, "<close>")
  end
end
