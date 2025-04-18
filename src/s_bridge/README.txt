
Other modules mark `s_` usually are standalone. 
Meaning that they can be used from both user or kernel space and only
depend on other module marked `s_`.

This module is an exception. When switching between kernel and user mode
we must execute out of an area which lives in both the kernel and user
memory spaces. That is the purpose of this module.
