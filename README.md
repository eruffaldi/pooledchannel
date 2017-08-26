
# Pooled Channel in Local or Shared memory
Emanuele Ruffaldi 2014-2016

Conceptually similar to Python Queue

Bonus Shared memory

Bonus reader/writer scheme for large  objects

# Usage

Template 
  <T, container<T> > allows to specify the container (vector or list or array). Default is vector
  
## Constructor
Constructor specifies the policy:

    PooledChannel(int n, bool adiscardold, bool aalwayslast)
    
* n is the size of the buffer, n<=0 means infinite queue, but only if the container is not array<T,q>, otherwise use array size
* adiscardold means that on overflow discard old data, otherwise discard new data
* aalwayslast means that return only last one

## Usage

* write(const T &)
* read(T &) returns bool
* readNoWait(T&) 
* readySize() returns available data for read
* freeSize() returns available data for write

In addition there is the transactional API

# TODO 
Make examples similar to Go channels

# TODO Regular
* implement the lockfree scheme of boost using C++11 policy

# TODO IPC
Currently we use boost for sharing in IPC case but there are other interesting options that overcome the issues with the naming scheme of boost/Unix shared memory:

- memfd under linux + Unix socket for transferring the descriptor
- mmap under OSX/Linux provides MAP_ANON that needs to be passed via fork
- CreateFileMapping with invalid HANDLE is similar, then passed via CreateProcess inheritance or alternatively DuplicateHandle
