------------------------------------------
-- Test that the is_main() function is working correctly.
-- This test must be called directly from the command line.
------------------------------------------

print('Testing is_main() for a main module..')
assert(require('l2dbus.is_main')())

print('Testing is_main() for a library module..')
assert(require('test_is_main_lib') == false)

