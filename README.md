About
-----
__res_redis__ is an asterisk wrapper module around HIREDIS, a C client library that offers access to redis servers. HIREDIS is available at https://github.com/redis/hiredis . you will need it as prerequisite.

This library use as reference [drivefast/asterisk-res_memcached](https://github.com/drivefast/asterisk-res_memcached), a very good asterisk wrapper for Memcached. Thank you [drivefast](https://github.com/drivefast) for the amazing job.


basic functions
---------------
__res_redis__ implements the basic Redis access functions: _get_, _set_.

    exten => s,n,set(REDIS(${key})=some text)
    exten => s,n,noop(value now: ${MCD(${key})}) ; prints "some text" to CLI


install
-------
__res_redis__ needs to be built into asterisk. You will need to compile asterisk from source and have it take care of linking to HIREDIS. therefore, step by step, this is what you have to do.

(1) Install the client library Hiredis (from https://github.com/redis/hiredis) on the same system where you plan to install asterisk.

(2) obtain the asterisk source code, from https://www.asterisk.org/downloads. unzip and untar it, but
dont proceed to building it yet.

(3) cd into the directory where you unzipped / untarred asterisk, and get the __res_memcached__ module
(git must be installed on your machine): `git clone git://github.com/rodrigowbazevedo/asterisk-res_redis.git`

(4) we now need to move the source files to their appropriate places in the asterisk directory. a
shell script was provided for that, so run `./asterisk-res_redis/install.sh`

(5) edit the file `configure.ac` and add the following lines next to the similar ones:

    AST_EXT_LIB_SETUP([REDIS])
    AST_EXT_LIB_CHECK([REDIS], [libhiredis/hiredis.h])

(6) edit the file `makeopts.in` and add the following lines next to the similar ones:

    REDIS_INCLUDE=@REDIS_INCLUDE@
    REDIS_LIB=@REDIS_LIB@

(7) edit the file `build_tools/menuselect-deps.in` and add the following line next to the similar ones:

    REDIS=@PBX_REDIS@

(8) run `./bootstrap.sh`. if you previously built from this asterisk directory, also do a `make clean`

(9) only now proceed with building asterisk (`./configure; make menuconfig; make; make install`).

(10) Edit the file `/etc/asterisk/res_redis.conf` and configure the startup parameters.

(11) start asterisk, login to its console, and try `"core show function REDIS"`. you should get an
usage description.


what'd you get
--------------

a bunch of apps and functions:

- `REDIS(key)` (r/w function) - gets or sets the value in the cache store for the given key

none of the functions or the apps above would fail in such a way that it would terminate the call.
if any of them would need to return an abnormal result, they would do so by setting the value of a
dialplan variable called `REDISRESULT`. the values returned in `REDISRESULT` are the same as the ones
documented for hiredis. one more value were added to the __res_redis__ module:

* `REDIS_ARGUMENT_NEEDED` - missing or invalid argument type in the app or function call


apps and functions
------------------

- `REDIS(key)`

>sets or returns the value for a key in the cache store. when written to, this function uses the
>'set' Redis operation.
>
> `key`: the key; may be prefixed with the value in the configuration file



author, licensing and credits
-----------------------------

Rodrigo Azevedo

Copyright (c) 2018 Rodrigo Azevedo

the __res_redis__ module is distributed under the MIT License.

the __res_redis__ module is built on top of HIREDIS, and dynamically links to it.
the __res_redis module__ is intended to be used with asterisk, so you will have to follow their
usage and distribution policy. and i guess so do i. i'm no lawyer and i have to take the safe route,
and this is why i go with the same level of license restriction as asterisk does.