# zproto
a simple protocol buffer for lua

## Description

####the protocol syntax defined like this:

	#comments line
	info {
        .name:string 1
        .age:integer 2
        .girl:boolean 3
	}

	packet 0xfe {
        	phone {
               		.home:integer 1
                	.work:integer 2
       		}
       		.phone:phone 1
        	.info:info[] 2
        	.address:string 3
        	.luck:integer[] 4
	  }

- all the line begin with '#' will regarded as a comment line
- basic type only support boolean/integer/string
- structure consist of the basic type
- all the name begin with a-z will be regarded as a struct name or basic type
- all the name begin with '.' will be regarded as a field name
- the structure name/field name/field tag must be unique, and the tag must large then 0
- when the suffix of typename(include structure) is [], it means this field is a array
- the structure can be defined as follows: 

		#protocol is option, it's aka typename when it's explicitly specified
		#protocol value can be queryed by zproto:querytag@zproto.lua
		#protocol value will be 0, when it not be explicitly specified
                typename [protocol] {
			.field1:integer 1
			...
		}


##Cppbind

The code in the cppbind folder is a simply C++ binding of zproto.

The cppbind code also provide a simple test code and you can run it as follow:

- run 'make' to build a executable file named 'zproto', 
	the 'zproto' file can be used to generate c++ bind code from \*.zproto file
- run './zproto test.zproto' to generate c++ bind code for test.zproto protocol
- run 'make test' to build a test program
- run './test' to run the test code.

