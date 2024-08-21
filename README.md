# mmap — Memory Mapping Object Library

A C++ library providing RAII objects abstracting memory mapping system calls and error handling.


## Introduction

The `mmap` system call is a versatile call in Unix-like operating systems which provides allows mapping of files or devices into memory via the kernel. 
The use cases of this system call are various such as inter-process memory sharing, file I/O optimization, user-space device control to name a few.
These calls have a C-style API that requires intricate knowledge the combinations of the many flags, options, and protocols available. 

This library provides a simpler approach that does much of the heavy lifting, while still exposing all the options availble with native use. 
With inspiration from other languages' API for memory mappings and dynamic allocation of structures within STL, RAII objects are used to block 
together system calls required for a proper memory mapping and provide automatic reallocation and deallocation, with silent error handling as well.
