
-- This is a test script for tshark/wireshark.
-- This script runs inside tshark/wireshark, so to run it do:
-- wireshark -X lua_script:<path_to_testdir>/lua/struct.lua
-- tshark -r bogus.cap -X lua_script:<path_to_testdir>/lua/struct.lua

-- Tests Struct functions

local testlib = require("testlib")
local OTHER = "other"

--
-- auxiliary function to print an hexadecimal `dump' of a given string
-- (not used by the test)
--
local function tohex(s, sep)
  local patt = "%02x" .. (sep or "")
  s = string.gsub(s, "(.)", function(c)
        return string.format(patt, string.byte(c))
      end)
  if sep then s = s:sub(1,-(sep:len()+1)) end
  return s
end

local function bp (s)
  s = tohex(s)
  print(s)
end


-----------------------------

print("Lua version: ".._VERSION)

testlib.init({ [OTHER] = 0 })

testlib.testing(OTHER, "Struct library")

local lib = Struct
testlib.test(OTHER,"global",_G.Struct == lib)

for name, val in pairs(lib) do
	print("\t"..name.." = "..type(val))
end

testlib.test(OTHER,"class1",type(lib) == 'table')
testlib.test(OTHER,"class2",type(lib.pack) == 'function')
testlib.test(OTHER,"class3",type(lib.unpack) == 'function')
testlib.test(OTHER,"class4",type(lib.size) == 'function')


local val1 = "\42\00\00\00\00\00\00\01\00\00\00\02\00\00\00\03\00\00\00\04"
local fmt1_le = "<!4biii4i4"
local fmt1_be = ">!4biii4i4"
local fmt1_64le = "<!4ieE"
local fmt1_64be = ">!4ieE"
local fmt2_be = ">!4bi(ii4)i"

testlib.testing(OTHER, "basic size")

testlib.test(OTHER,"basic_size1", lib.size(fmt1_le) == string.len(val1))
testlib.test(OTHER,"basic_size2", lib.size(fmt1_le) == Struct.size(fmt1_be))
testlib.test(OTHER,"basic_size3", lib.size(fmt1_le) == Struct.size(fmt1_64le))
testlib.test(OTHER,"basic_size4", lib.size(fmt2_be) == Struct.size(fmt1_64le))

testlib.testing(OTHER, "basic values")

testlib.test(OTHER,"basic_values1", lib.values(fmt1_le) == 5)
testlib.test(OTHER,"basic_values2", lib.values(fmt1_be) == lib.values(fmt1_le))
testlib.test(OTHER,"basic_values3", lib.values(fmt1_64le) == 3)
testlib.test(OTHER,"basic_values4", lib.values(fmt2_be) == lib.values(fmt1_64le))
testlib.test(OTHER,"basic_values4", lib.values(" (I)  s x i XxX c0") == 3)

testlib.testing(OTHER, "tohex")
local val1hex = "2A:00:00:00:00:00:00:01:00:00:00:02:00:00:00:03:00:00:00:04"
testlib.test(OTHER,"tohex1", Struct.tohex(val1) == tohex(val1):upper())
testlib.test(OTHER,"tohex2", Struct.tohex(val1,true) == tohex(val1))
testlib.test(OTHER,"tohex3", Struct.tohex(val1,false,":") == val1hex)
testlib.test(OTHER,"tohex4", Struct.tohex(val1,true,":") == val1hex:lower())

testlib.testing(OTHER, "fromhex")
testlib.test(OTHER,"fromhex1", Struct.fromhex(val1hex,":") == val1)
local val1hex2 = val1hex:gsub(":","")
testlib.test(OTHER,"fromhex2", Struct.fromhex(val1hex2) == val1)
testlib.test(OTHER,"fromhex3", Struct.fromhex(val1hex2:lower()) == val1)

testlib.testing(OTHER, "basic unpack")
local ret1, ret2, ret3, ret4, ret5, pos = lib.unpack(fmt1_le, val1)
testlib.test(OTHER,"basic_unpack1", ret1 == 42 and ret2 == 0x01000000 and ret3 == 0x02000000 and ret4 == 0x03000000 and ret5 == 0x04000000)
testlib.test(OTHER,"basic_unpack_position1", pos == string.len(val1) + 1)

ret1, ret2, ret3, ret4, ret5, pos = lib.unpack(fmt1_be, val1)
testlib.test(OTHER,"basic_unpack2", ret1 == 42 and ret2 == 1 and ret3 == 2 and ret4 == 3 and ret5 == 4)
testlib.test(OTHER,"basic_unpack_position2", pos == string.len(val1) + 1)

ret1, ret2, ret3, pos = lib.unpack(fmt1_64le, val1)
testlib.test(OTHER,"basic_unpack3", ret1 == 42 and ret2 == Int64.new( 0x01000000, 0x02000000) and ret3 == UInt64.new( 0x03000000, 0x04000000))
print(typeof(ret2),typeof(ret3))
testlib.test(OTHER,"basic_unpack3b", typeof(ret2) == "Int64" and typeof(ret3) == "UInt64")
testlib.test(OTHER,"basic_unpack_position3", pos == string.len(val1) + 1)

ret1, ret2, ret3, pos = lib.unpack(fmt1_64be, val1)
testlib.test(OTHER,"basic_unpack4", ret1 == 0x2A000000 and ret2 == Int64.new( 2, 1) and ret3 == UInt64.new( 4, 3))
testlib.test(OTHER,"basic_unpack4b", typeof(ret2) == "Int64" and typeof(ret3) == "UInt64")
testlib.test(OTHER,"basic_unpack_position4", pos == string.len(val1) + 1)

ret1, ret2, ret3, pos = lib.unpack(fmt2_be, val1)
testlib.test(OTHER,"basic_unpack5", ret1 == 42 and ret2 == 1 and ret3 == 4)
testlib.test(OTHER,"basic_unpack_position5", pos == string.len(val1) + 1)

testlib.testing(OTHER, "basic pack")
local pval1 = lib.pack(fmt1_le, lib.unpack(fmt1_le, val1))
testlib.test(OTHER,"basic_pack1", pval1 == val1)
testlib.test(OTHER,"basic_pack2", val1 == lib.pack(fmt1_be, lib.unpack(fmt1_be, val1)))
testlib.test(OTHER,"basic_pack3", val1 == lib.pack(fmt1_64le, lib.unpack(fmt1_64le, val1)))
testlib.test(OTHER,"basic_pack4", val1 == lib.pack(fmt1_64be, lib.unpack(fmt1_64be, val1)))
testlib.test(OTHER,"basic_pack5", lib.pack(fmt2_be, lib.unpack(fmt1_be, val1)) == lib.pack(">!4biiii", 42, 1, 0, 0, 2))

----------------------------------
-- following comes from:
-- https://github.com/LuaDist/struct/blob/master/teststruct.lua
-- unfortunately many of his tests assumed a local machine word
-- size of 4 bytes for long and such, so I had to muck with this
-- to make it handle 64-bit compiles.
-- $Id: teststruct.lua,v 1.2 2008/04/18 20:06:01 roberto Exp $


-- some pack/unpack commands are host-size dependent, so we need to pad
local l_pad, ln_pad = "",""
if lib.size("l") == 8 then
	-- the machine running this script uses a long of 8 bytes
	l_pad = "\00\00\00\00"
	ln_pad = "\255\255\255\255"
end

local a,b,c,d,e,f,x

testlib.testing(OTHER, "pack")
testlib.test(OTHER,"pack_I",#Struct.pack("I", 67324752) == 4)

testlib.test(OTHER,"pack_b1",lib.pack('b', 10) == string.char(10))
testlib.test(OTHER,"pack_b2",lib.pack('bbb', 10, 20, 30) == string.char(10, 20, 30))

testlib.test(OTHER,"pack_h1",lib.pack('<h', 10) == string.char(10, 0))
testlib.test(OTHER,"pack_h2",lib.pack('>h', 10) == string.char(0, 10))
testlib.test(OTHER,"pack_h3",lib.pack('<h', -10) == string.char(256-10, 256-1))

testlib.test(OTHER,"pack_l1",lib.pack('<l', 10) == string.char(10, 0, 0, 0)..l_pad)
testlib.test(OTHER,"pack_l2",lib.pack('>l', 10) == l_pad..string.char(0, 0, 0, 10))
testlib.test(OTHER,"pack_l3",lib.pack('<l', -10) == string.char(256-10, 256-1, 256-1, 256-1)..ln_pad)

testlib.testing(OTHER, "unpack")
testlib.test(OTHER,"unpack_h1",lib.unpack('<h', string.char(10, 0)) == 10)
testlib.test(OTHER,"unpack_h2",lib.unpack('>h', string.char(0, 10)) == 10)
testlib.test(OTHER,"unpack_h3",lib.unpack('<h', string.char(256-10, 256-1)) == -10)

testlib.test(OTHER,"unpack_l1",lib.unpack('<l', string.char(10, 0, 0, 1)..l_pad) == 10 + 2^(3*8))
testlib.test(OTHER,"unpack_l2",lib.unpack('>l', l_pad..string.char(0, 1, 0, 10)) == 10 + 2^(2*8))
testlib.test(OTHER,"unpack_l3",lib.unpack('<l', string.char(256-10, 256-1, 256-1, 256-1)..ln_pad) == -10)

-- limits
lims = {{'B', 255}, {'b', 127}, {'b', -128},
        {'I1', 255}, {'i1', 127}, {'i1', -128},
        {'H', 2^16 - 1}, {'h', 2^15 - 1}, {'h', -2^15},
        {'I2', 2^16 - 1}, {'i2', 2^15 - 1}, {'i2', -2^15},
        {'L', 2^32 - 1}, {'l', 2^31 - 1}, {'l', -2^31},
        {'I4', 2^32 - 1}, {'i4', 2^31 - 1}, {'i4', -2^31},
       }

for _, a in pairs{'', '>', '<'} do
  local i = 1
  for _, l in pairs(lims) do
    local fmt = a .. l[1]
    testlib.test(OTHER,"limit"..i.."("..l[1]..")", lib.unpack(fmt, lib.pack(fmt, l[2])) == l[2])
    i = i + 1
  end
end


testlib.testing(OTHER, "fixed-sized ints")
-- tests for fixed-sized ints
local num = 1
for _, i in pairs{1,2,4} do
  x = lib.pack('<i'..i, -3)
  testlib.test(OTHER,"pack_fixedlen"..num, string.len(x) == i)
  testlib.test(OTHER,"pack_fixed"..num, x == string.char(256-3) .. string.rep(string.char(256-1), i-1))
  testlib.test(OTHER,"unpack_fixed"..num, lib.unpack('<i'..i, x) == -3)
  num = num + 1
end


testlib.testing(OTHER, "alignment")
-- alignment
d = lib.pack("d", 5.1)
ali = {[1] = string.char(1)..d,
       [2] = string.char(1, 0)..d,
       [4] = string.char(1, 0, 0, 0)..d,
       [8] = string.char(1, 0, 0, 0, 0, 0, 0, 0)..d,
      }

num = 1
for a,r in pairs(ali) do
  testlib.test(OTHER,"pack_align"..num, lib.pack("!"..a.."bd", 1, 5.1) == r)
  local x,y = lib.unpack("!"..a.."bd", r)
  testlib.test(OTHER,"unpack_align"..num, x == 1 and y == 5.1)
  num = num + 1
end


testlib.testing(OTHER, "string")
-- strings
testlib.test(OTHER,"string_pack1",lib.pack("c", "alo alo") == "a")
testlib.test(OTHER,"string_pack2",lib.pack("c4", "alo alo") == "alo ")
testlib.test(OTHER,"string_pack3",lib.pack("c5", "alo alo") == "alo a")
testlib.test(OTHER,"string_pack4",lib.pack("!4b>c7", 1, "alo alo") == "\1alo alo")
testlib.test(OTHER,"string_pack5",lib.pack("!2<s", "alo alo") == "alo alo\0")
testlib.test(OTHER,"string_pack6",lib.pack(" c0 ", "alo alo") == "alo alo")
num = 1
for _, f in pairs{"B", "l", "i2", "f", "d"} do
  for _, s in pairs{"", "a", "alo", string.rep("x", 200)} do
    local x = lib.pack(f.."c0", #s, s)
    testlib.test(OTHER,"string_unpack"..num, lib.unpack(f.."c0", x) == s)
    num = num + 1
  end
end


testlib.testing(OTHER, "indeces")
-- indices
x = lib.pack("!>iiiii", 1, 2, 3, 4, 5)
local i = 1
local k = 1
num = 1
while i < #x do
  local v, j = lib.unpack("!>i", x, i)
  testlib.test(OTHER,"index_unpack"..num, j == i + 4 and v == k)
  i = j; k = k + 1
  num = num + 1
end

testlib.testing(OTHER, "absolute")
-- alignments are relative to 'absolute' positions
x = lib.pack("!8 xd", 12)
testlib.test(OTHER,"absolute_unpack1",lib.unpack("!8d", x, 3) == 12)


testlib.test(OTHER,"absolute_pack1",lib.pack("<lhbxxH", -2, 10, -10, 250) ==
  string.char(254, 255, 255, 255) ..ln_pad.. string.char(10, 0, 246, 0, 0, 250, 0))

a,b,c,d = lib.unpack("<lhbxxH",
  string.char(254, 255, 255, 255) ..ln_pad.. string.char(10, 0, 246, 0, 0, 250, 0))
testlib.test(OTHER,"absolute_unpack2",a == -2 and b == 10 and c == -10 and d == 250)

testlib.test(OTHER,"absolute_pack2",lib.pack(">lBxxH", -20, 10, 250) ==
                ln_pad..string.char(255, 255, 255, 236, 10, 0, 0, 0, 250))


testlib.testing(OTHER, "position")

a, b, c, d = lib.unpack(">lBxxH",
                 ln_pad..string.char(255, 255, 255, 236, 10, 0, 0, 0, 250))
-- the 'd' return val is position in string, so will depend on size of long 'l'
local vald = 10 + string.len(l_pad)
testlib.test(OTHER,"position_unpack1",a == -20 and b == 10 and c == 250 and d == vald)

a,b,c,d,e = lib.unpack(">fdfH",
                  '000'..lib.pack(">fdfH", 3.5, -24e-5, 200.5, 30000),
                  4)
testlib.test(OTHER,"position_unpack2",a == 3.5 and b == -24e-5 and c == 200.5 and d == 30000 and e == 22)

a,b,c,d,e = lib.unpack("<fdxxfH",
                  '000'..lib.pack("<fdxxfH", -13.5, 24e5, 200.5, 300),
                  4)
testlib.test(OTHER,"position_unpack3",a == -13.5 and b == 24e5 and c == 200.5 and d == 300 and e == 24)

x = lib.pack(">I2fi4I2", 10, 20, -30, 40001)
testlib.test(OTHER,"position_pack1",string.len(x) == 2+4+4+2)
testlib.test(OTHER,"position_unpack4",lib.unpack(">f", x, 3) == 20)
a,b,c,d = lib.unpack(">i2fi4I2", x)
testlib.test(OTHER,"position_unpack5",a == 10 and b == 20 and c == -30 and d == 40001)

testlib.testing(OTHER, "string length")
local s = "hello hello"
x = lib.pack(" b c0 ", string.len(s), s)
testlib.test(OTHER,"stringlen_unpack1",lib.unpack("bc0", x) == s)
x = lib.pack("Lc0", string.len(s), s)
testlib.test(OTHER,"stringlen_unpack2",lib.unpack("  L  c0   ", x) == s)
x = lib.pack("cc3b", s, s, 0)
testlib.test(OTHER,"stringlen_pack1",x == "hhel\0")
testlib.test(OTHER,"stringlen_unpack3",lib.unpack("xxxxb", x) == 0)

testlib.testing(OTHER, "padding")
testlib.test(OTHER,"padding_pack1",lib.pack("<!l", 3) == string.char(3, 0, 0, 0)..l_pad)
testlib.test(OTHER,"padding_pack2",lib.pack("<!xl", 3) == l_pad..string.char(0, 0, 0, 0, 3, 0, 0, 0)..l_pad)
testlib.test(OTHER,"padding_pack3",lib.pack("<!xxl", 3) == l_pad..string.char(0, 0, 0, 0, 3, 0, 0, 0)..l_pad)
testlib.test(OTHER,"padding_pack4",lib.pack("<!xxxl", 3) == l_pad..string.char(0, 0, 0, 0, 3, 0, 0, 0)..l_pad)

testlib.test(OTHER,"padding_unpack1",lib.unpack("<!l", string.char(3, 0, 0, 0)..l_pad) == 3)
testlib.test(OTHER,"padding_unpack2",lib.unpack("<!xl", l_pad..string.char(0, 0, 0, 0, 3, 0, 0, 0)..l_pad) == 3)
testlib.test(OTHER,"padding_unpack3",lib.unpack("<!xxl", l_pad..string.char(0, 0, 0, 0, 3, 0, 0, 0)..l_pad) == 3)
testlib.test(OTHER,"padding_unpack4",lib.unpack("<!xxxl", l_pad..string.char(0, 0, 0, 0, 3, 0, 0, 0)..l_pad) == 3)

testlib.testing(OTHER, "format")
testlib.test(OTHER,"format_pack1",lib.pack("<!2 b l h", 2, 3, 5) == string.char(2, 0, 3, 0)..l_pad..string.char(0, 0, 5, 0))
a,b,c = lib.unpack("<!2blh", string.char(2, 0, 3, 0)..l_pad..string.char(0, 0, 5, 0))
testlib.test(OTHER,"format_pack2",a == 2 and b == 3 and c == 5)

testlib.test(OTHER,"format_pack3",lib.pack("<!8blh", 2, 3, 5) == string.char(2, 0, 0, 0)..l_pad..string.char(3, 0, 0, 0)..l_pad..string.char(5, 0))

a,b,c = lib.unpack("<!8blh", string.char(2, 0, 0, 0)..l_pad..string.char(3, 0, 0, 0)..l_pad..string.char(5, 0))
testlib.test(OTHER,"format_pack4",a == 2 and b == 3 and c == 5)

testlib.test(OTHER,"format_pack5",lib.pack(">sh", "aloi", 3) == "aloi\0\0\3")
testlib.test(OTHER,"format_pack6",lib.pack(">!sh", "aloi", 3) == "aloi\0\0\0\3")

x = "aloi\0\0\0\0\3\2\0\0"
a, b, c = lib.unpack("<!si4", x)
testlib.test(OTHER,"format_unpack1",a == "aloi" and b == 2*256+3 and c == string.len(x)+1)

x = lib.pack("!4sss", "hi", "hello", "bye")
a,b,c = lib.unpack("sss", x)
testlib.test(OTHER,"format_unpack2",a == "hi" and b == "hello" and c == "bye")
a, i = lib.unpack("s", x, 1)
testlib.test(OTHER,"format_unpack3",a == "hi")
a, i = lib.unpack("s", x, i)
testlib.test(OTHER,"format_unpack4",a == "hello")
a, i = lib.unpack("s", x, i)
testlib.test(OTHER,"format_unpack5",a == "bye")



-- test for weird conditions
testlib.testing(OTHER, "weird conditions")
testlib.test(OTHER,"weird_pack1",lib.pack(">>>h <!!!<h", 10, 10) == string.char(0, 10, 10, 0))
testlib.test(OTHER,"weird_pack2",not pcall(lib.pack, "!3l", 10))
testlib.test(OTHER,"weird_pack3",not pcall(lib.pack, "3", 10))
testlib.test(OTHER,"weird_pack4",not pcall(lib.pack, "i33", 10))
testlib.test(OTHER,"weird_pack5",not pcall(lib.pack, "I33", 10))
testlib.test(OTHER,"weird_pack6",lib.pack("") == "")
testlib.test(OTHER,"weird_pack7",lib.pack("   ") == "")
testlib.test(OTHER,"weird_pack8",lib.pack(">>><<<!!") == "")
testlib.test(OTHER,"weird_unpack1",not pcall(lib.unpack, "c0", "alo"))
testlib.test(OTHER,"weird_unpack2",not pcall(lib.unpack, "s", "alo"))
testlib.test(OTHER,"weird_unpack3",lib.unpack("s", "alo\0") == "alo")
testlib.test(OTHER,"weird_pack9",not pcall(lib.pack, "c4", "alo"))
testlib.test(OTHER,"weird_pack10",pcall(lib.pack, "c3", "alo"))
testlib.test(OTHER,"weird_unpack4",not pcall(lib.unpack, "c4", "alo"))
testlib.test(OTHER,"weird_unpack5",pcall(lib.unpack, "c3", "alo"))
testlib.test(OTHER,"weird_unpack6",not pcall(lib.unpack, "bc0", "\4alo"))
testlib.test(OTHER,"weird_unpack7",pcall(lib.unpack, "bc0", "\3alo"))

testlib.test(OTHER,"weird_unpack8",not pcall(lib.unpack, "b", "alo", 4))
testlib.test(OTHER,"weird_unpack9",lib.unpack("b", "alo\3", 4) == 3)

testlib.test(OTHER,"weird_pack11",not pcall(lib.pack, "\250\22", "alo"))
testlib.test(OTHER,"weird_pack12",not pcall(lib.pack, 1, "alo"))
testlib.test(OTHER,"weird_pack13",not pcall(lib.pack, nil, "alo"))
testlib.test(OTHER,"weird_pack14",not pcall(lib.pack, {}, "alo"))
testlib.test(OTHER,"weird_pack15",not pcall(lib.pack, true, "alo"))
testlib.test(OTHER,"weird_unpack10",not pcall(lib.unpack, "\250\22", "\3alo"))
testlib.test(OTHER,"weird_unpack11",not pcall(lib.unpack, 1, "\3alo"))
testlib.test(OTHER,"weird_unpack12",not pcall(lib.unpack, nil, "\3alo"))
testlib.test(OTHER,"weird_unpack13",not pcall(lib.unpack, {}, "\3alo"))
testlib.test(OTHER,"weird_unpack14",not pcall(lib.unpack, true, "\3alo"))

-- done
testlib.getResults()
