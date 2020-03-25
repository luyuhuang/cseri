# Cseri

A Lua object serializer implemented by C.

## Introduction

Cseri is a Lua object serializer implemented by C, which is able to serialize Lua objects to binary data or readable strings. For serialize to binary data, it borrows partly from [cloudwu/lua-serialize](https://github.com/cloudwu/lua-serialize). For serialize to readable strings, it runs about 4 times as fast as Lua's implementation.

- [x] Serialize to binary data and deserialize from binary data;
- [x] Serialize to readable strings;
- [x] High performance;
- [x] Handling endianness;
- [ ] Compressing serialized data

## Build

### For Linux

Make sure your machine has been installed Lua.

```sh
git clone https://github.com/luyuhuang/cseri
cd cseri
make && make test
```

### For other platforms

Cseri is simple enough. So I guess it's easy for you to build on the platform you want.

## Usage

```lua
local cseri = require "cseri"

-- Serialize a table to binary data
local bin = cseri.tobin{a = 1, b = "2", c = {1, 2, 3}}
print(type(bin)) -- string

-- Deserialize to the table
local t = cseri.frombin(bin)

-- Serialize multiple objects
local bin = cseri.tobin(1, "abc", true, 3.14)
print(cseri.frombin(bin)) -- 1 abc true 3.14

-- Serialize to a readable string
print(cseri.totxt({a = 1, b = "value"}, "str")) -- {a=1,b="value"},"str"
```
