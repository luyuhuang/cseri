local cseri = require "cseri"

local function compare(t1, t2)
    for k, v in pairs(t1) do
        if type(v) == 'table' then
            if not compare(v, t2[k]) then
                return false
            end
        else
            if v ~= t2[k] then
                return false
            end
        end
    end
    for k in pairs(t2) do
        if t1[k] == nil then return false end
    end
    return true
end

if not load then load = loadstring end

local t = {
    a = 1,
    b = 2,
    3, 4, 5,
    key = {
        a = '123',
        [false] = true,
        c = [[
            {1, 2, 3, 4, 5}
            "aaa\\bbb"
            'c''d'/
        ]],
        ['sdf"\'\n\r,'] = 3.1415926,
        [''] = '',
        ['\xff'] = '\x01\x7f',
    },
    name = {
        _1 = 1, ['1ab'] = 2,
        ['1_2_a3'] = 3, _1ab_ = 4,
    }
}

local bin = cseri.tobin(t)
local nt = cseri.frombin(bin)

assert(compare(t, nt))

local txt = cseri.totxt(t)
local nt = load("return " .. txt)()

assert(compare(t, nt))

local bin = cseri.tobin(1, '2', true, {a = 1})
local a, b, c, d = cseri.frombin(bin)

assert(a == 1 and b == '2' and c == true and d.a == 1)

local txt = cseri.totxt(1, '2', true, {a = 1})
local a, b, c, d = load("return " .. txt)()

assert(a == 1 and b == '2' and c == true and d.a == 1)

local llstr = 'a'
for i = 1, 14 do
    llstr = llstr .. llstr
end
t.llstr = llstr

local bin = cseri.tobin(t)
local nt = cseri.frombin(bin)

assert(compare(t, nt))

local txt = cseri.totxt(t)
local nt = load("return " .. txt)()

assert(compare(t, nt))

local deep = {}
local p = deep
for i = 1, 32 do
    p[i] = {}
    p = p[i]
end

local txt = cseri.totxt(deep)
local nt = load("return " .. txt)()

assert(compare(deep, nt))

local bin = cseri.tobin(deep)
local nt = cseri.frombin(bin)

assert(compare(deep, nt))

p[33] = 1

local ok, msg = pcall(cseri.totxt, deep)
assert(ok == false and msg == "serialize can't pack too depth table")

local ok, msg = pcall(cseri.tobin, deep)
assert(ok == false and msg == "serialize can't pack too depth table")

assert(cseri.frombin(cseri.tobin(0x7fffffffffffffff)) == 0x7fffffffffffffff)
assert(cseri.frombin(cseri.tobin(0xffffffffffffffff)) == 0xffffffffffffffff)

print("passed")

local bin = cseri.tobin("aaa"):sub(1, 2)
local ok, msg = pcall(cseri.frombin, bin)
assert(ok == false and msg == "Invalid serialize stream 1 (line:336)")
