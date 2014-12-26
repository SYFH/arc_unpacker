require 'test/unit'
require_relative '../lib/arg_parser'

# Unit tests for ArgParser
class ArgParserTest < Test::Unit::TestCase
  def test_instantiation
    assert_nothing_thrown { ArgParser.new([]) }
  end

  def test_optional_switch_single_arity_missing
    args = ArgParser.new(['--nope'])
    assert_nothing_thrown { args.on('-s', '--long') { |x| assert_nil(x) } }
  end

  def test_optional_switch_single_arity_consuming_short
    args = ArgParser.new(['-o', 'bad', '-s', 'good'])
    x_out = nil
    args.on('-s', '--long') { |x| x_out = x }
    assert_equal(x_out, 'good')
  end

  def test_optional_switch_single_arity_consuming_long
    args = ArgParser.new(['--other', 'bad', '--long', 'good'])
    x_out = nil
    args.on('-s', '--long') { |x| x_out = x }
    assert_equal(x_out, 'good')
  end

  def test_optional_switch_double_arity_missing
    args = ArgParser.new(['--other', 'bad', '--long', 'good'])
    x_out, y_out = nil, nil
    args.on('-s', '--long') { |x, y| x_out, y_out = x, y }
    assert_equal(x_out, 'good')
    assert_nil(y_out)
  end

  def test_mandatory_switch_double_arity_missing
    args = ArgParser.new(['--other', 'bad', '--long', 'good'])
    assert_raise(OptionError) do
      args.get('-s', '--long') { |_, _| fail 'Don\'t run me' }
    end
  end

  def test_mandatory_switch_double_arity_consuming
    args = ArgParser.new(['--other', 'good', 'good2', '--long', 'good3'])
    x_out, y_out, z_out = nil, nil, nil
    args.get(nil, '--other') { |x, y| x_out, y_out = x, y }
    args.get(nil, '--long') { |z| z_out = z }
    assert_equal(x_out, 'good')
    assert_equal(y_out, 'good2')
    assert_equal(z_out, 'good3')
  end

  def test_mixed_switches_and_stray_consuming
    args = ArgParser.new(%w(stray1 -s1 switch1 switch2 --flag stray2 --flag2))

    switch1, switch2, stray1, stray2, flag1, flag2 = nil
    args.get('-s1', nil) { |x, y| switch1, switch2 = x, y }
    args.get(nil, '--flag') { flag1 = true }
    args.get_stray { |x, y| stray1, stray2 = x, y }
    args.get(nil, '--flag2') { flag2 = true }

    assert_equal(switch1, 'switch1')
    assert_equal(switch2, 'switch2')
    assert_equal(stray1, 'stray1')
    assert_equal(stray2, 'stray2')
    assert_equal(flag1, true)
    assert_equal(flag2, true)
  end

  def test_optional_stray_single_arity_missing
    args = ArgParser.new([])
    assert_nothing_thrown { args.on_stray { |x| assert_nil(x) } }
  end

  def test_optional_stray_double_arity_consuming
    args = ArgParser.new(['1'])
    assert_nothing_thrown do
      args.on_stray do |x, y|
        assert_equal(x, '1')
        assert_nil(y)
      end
    end
  end

  def test_mandatory_stray_single_arity_missing
    args = ArgParser.new([])
    assert_raise(OptionError) { args.get_stray { |_| fail 'Don\'t run me' } }
  end

  def test_mandatory_stray_double_arity_missing
    args = ArgParser.new(['1'])
    assert_raise(OptionError) { args.get_stray { |_, _| fail 'Don\'t run me' } }
  end

  def test_mandatory_stray_double_arity_consuming
    args = ArgParser.new(%w(1 2))
    assert_nothing_thrown do
      args.get_stray do |x, y|
        assert_equal(x, '1')
        assert_equal(y, '2')
      end
    end
  end

  def test_stray_multiple_consuming
    args = ArgParser.new(%w(1 2 3))
    assert_nothing_thrown do
      args.get_stray do |x, y|
        assert_equal(x, '1')
        assert_equal(y, '2')
      end
    end
    assert_nothing_thrown do
      args.on_stray { |x| assert_equal(x, '3') }
    end
  end
end
