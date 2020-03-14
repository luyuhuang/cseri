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
        b = true,
        c = [[
            {1, 2, 3, 4, 5}
            "aaa\\bbb"
            'c''d'
        ]],
        ['sdf"\'\n\r,'] = 3.1415926,
    },
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

print("passed")
