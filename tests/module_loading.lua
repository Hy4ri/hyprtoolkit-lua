local ht = require("hyprtoolkit")

assert(type(ht) == "table")
assert(type(ht.version) == "string")
assert(type(ht.Backend.create) == "function")
assert(type(ht.WindowBuilder.begin) == "function")
assert(type(ht.TextBuilder.begin) == "function")
assert(type(ht.color) == "function")
assert(require("hyprtoolkit") == ht)
