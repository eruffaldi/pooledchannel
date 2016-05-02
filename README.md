
#Pooled Channel in Local or Shared memory
Emanuele Ruffaldi 2014-2016

Conceptually similar to Python Queue

Bonus Shared memory

Bonus reader/writer scheme for large  objects

#TODO 
Make examples similar to Go channels

#TODO Regular
* implement the lockfree scheme of boost using C++11 policy

#TODO IPC
Currently we use boost for sharing in IPC case but there are other interesting options that overcome the issues with the naming scheme of boost/Unix shared memory:

- memfd under linux + Unix socket for transferring the descriptor
- mmap under OSX/Linux provides MAP_ANON that needs to be passed via fork
- CreateFileMapping with invalid HANDLE is similar, then passed via CreateProcess inheritance or alternatively DuplicateHandle
