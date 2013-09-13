#!/usr/bin/env lua
-----------------------------------------------------------
-- Test that the isMain() function is working correctly.
-- This test must be called directly from the command line.
-----------------------------------------------------------

package.path = package.path .. ";./utils/?.lua"
print('Testing isMain() for a main module..')
assert(require('l2dbus').isMain())

print('Testing isMain() for a library module..')
assert(require('test_ismainlib') == false)

