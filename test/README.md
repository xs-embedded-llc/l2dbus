
##Overview

This directory contains various scripts that are used to test various l2dbus
features.  These scripts are also meant as example code for various operations.

##Usage

Go to the test directory in a terminal and source: ./setLuaEnv.sh
The "setLuaEnv.sh" will setup the Lua paths as well as check for various
dependencies needed to run the files in the test directory.


##File Details

**/utils** - This directory contains source used within some of the files in the "test" directory.

**test_*.lua** - These are various test scripts testing and showing how to use various features.  

**test_monitor.lua** - Simple script that implements basic "dbus-monitor" output. Can be easy and useful to create very specific filters.

**reg_tests.lua** - This is a script used to regression test much of the API and various other l2dbus features. This is an ongoing work in progress. As bugs are found they should be covered here.

**ping_stress.lua** - This is a ping program that implements both the server and client side of the test. This can be used as an example as well as test various feature and throughput. Use:

        lua ./ping_stress.lua -h for detailed help.

**stresstest_client.lua** - This is another stress testing program but it implements the *client* part of the test. It should be run against the **stresstest_service.lua** program. Detailed options can be seen by invoking it with the *-h* option. Options that take arguments **must** separate the option by the argument with a **=**, e.g.

        lua ./stresstest_client.lua --rxsize=8192

**stresstest_service.lua** - This is the service side of the test program called by **stresstest_client.lua***. This doesn't have any options but instead receives its options from the client program as the first message of the test.

**bluez.lua** - This is an example showing how you can use l2dbus to communicate with a 3rd party component. Some features still need work (see file header for specifics).


