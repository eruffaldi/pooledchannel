/**
 Emanuele Ruffaldi @SSSA 2014-2016

 C++11 Pooled Channel over boost IPC

 Design:

 - publisher/subscriber access to the Shared Memory queue

	subscriber 
		what do to when there is no publisher? 
			e.g. ROS uses a mediator to handle this


 	subscriber connection with master: (XMLRPC HTTP)
 		on new pusblisher (name, Type):
 			connect to publisher


 	subscriber connection X to publisher Y (name,Type)
 		on disconnection of Y
 			remove

 - multiple subscriber in the single publisher shared memory

 	instead T we store pair<T,int> as reference count

	freepushfront(X) => move X into freelist
	freepushfront(X) =>
		decrease counter(X)
		if counter(X) == 0 move into freelist

	readypushback(X) => assign number of current subscribers	

 */
#include <iostream>
#include <list>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>

/// Conceptual Life of pools
/// [*] -> free -> writing -> ready -> reading -> free
///
/// policy: if discardold is true on overflow: ready -> writing
/// policy: always last: ready -> reading of the last 
///
/// TODO: current version requires default constructor of data
/// TODO: objects are not destructed when in free state

enum class DiscardPolicy { DiscardOld, NoDiscard};
enum class ReadOrderPolicy { AlwaysLast, Ordered};

class WriterTag {};
class ReaderTag {};

template <class T>
class IPCPooledChannel
{
public:
   typedef  boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> scoped_lock;

   /**
    * This is a data structure stored in the Shared Memory, it should NOT contain pointers
    */
   struct Header
   {	
   		struct dynamic_content
   		{
   			int freeindex;
   			int readyindex;
   			int readycounter; // NOT USED
   		};

   		int readysize = 0;
   		int freesize = 0;
   		int buffersize = 0;
   		std::atomic<int> inited_;
   		boost::interprocess::interprocess_mutex mutex_;
   		boost::interprocess::interprocess_condition  read_ready_var_, write_ready_var_;
   		boost::interprocess::offset_ptr<dynamic_content > readfreelist; // all together but separate

   		/// Constructs with a freelist
   		Header(int effectiven,Header::dynamic_content *pq) : freesize(effectiven),buffersize(effectiven)
   		{
   			readfreelist = pq;
   			inited_ = 1;
			for(int i = 0; i < effectiven; i++)
				pq[i].freeindex = i;
   		}

   		/// pushes back the pointer wrt given base
   		inline void readypushback(T* p, T*pbase) // newest = append
		{
			readfreelist[readylisttail].readyindex = p-pbase;
			readfreelist[readylisttail].readycounter = 10; // not used
			readylisttail = (readylisttail+1) % buffersize;
			readysize++;
		}

		/// pops a pointer from the ready list
		inline void readypopfront(T* & p, T*pbase) // oldest
		{
			readysize--;
			p = pbase+readfreelist[readylisthead].readyindex;
			readylisthead = (readylisthead+1) % buffersize;
		}

		/// pushes a new pointer into the free
		inline void freepushfront(T* p, T*pbase)
		{
			// TODO verify counter => keep it in main ready list or otherwise free it
			readfreelist[freesize].freeindex = p-pbase;
			freesize++;
		}

		/// pops any new
		void freepopany(T * & p,T*pbase)
		{
		    p = pbase+readfreelist[freesize-1].freeindex;
		    freesize--;			
		}
	private:
   		int readylisthead = 0;
   		int readylisttail = 0; // circular list structures
   };

   boost::interprocess::shared_memory_object shm_obj;
   boost::interprocess::shared_memory_object shm_objmeta;
   boost::interprocess::mapped_region region;
std::shared_ptr<boost::interprocess::named_mutex> mutex;

   Header * pheader;
   T * pbase;
   bool discard_old_,alwayslast_; /// policy 
   int effectiven; 
   std::string aname;

   struct PayloadMeta
   {
   		int sharedmemorysize;
   		char typesignature[256];
   		int itemsize;
   };

   /// this is the effective content of the shared memory objects. It is configured as a
   /// variable length array
   struct Payload
   {
   		Header h;
   		boost::interprocess::offset_ptr<T> first;
   };


   void remove()
   {
		shm_obj.remove(aname.c_str());
   }

   bool valid() const { return !aname.empty(); }

	IPCPooledChannel(std::string name, ReaderTag, ReadOrderPolicy orderpolicy):
		discard_old_(false),alwayslast_(orderpolicy == ReadOrderPolicy::AlwaysLast)
	{

		int effectivsize = 0;
		try
		{

	 		boost::interprocess::shared_memory_object shm_obj1(boost::interprocess::open_only, (name+"meta").c_str(), boost::interprocess::read_only);
	 		std::cout << "truncated\n";
			boost::interprocess::mapped_region r(shm_obj1,boost::interprocess::read_only);
	 		std::cout << "cmapped\n";
		 	PayloadMeta * ptr = (PayloadMeta*)r.get_address();
		 	effectivsize = ptr->sharedmemorysize;
		 	if(strcmp(typeid(T).name(),ptr->typesignature) != 0)
		 	{
		 		std::cerr << "IPC client type mismatch. Expected " << typeid(T).name() << " got " << ptr->typesignature << std::endl;
		 		return;
		 	}
		 	if(ptr->itemsize != sizeof(T))
		 	{
		 		std::cerr << "IPC client type size mismatch. Expected " << sizeof(T) << " got " <<  ptr->itemsize << std::endl;
		 		return;		 		
		 	}
	 	}
	 	catch(...)
	 	{
	 		std::cerr << "IPC client missing shared memory " << name << "meta\n";
	 		return;
	 	}
	 	if(!effectivsize)
	 		return;
	 	aname = name; //

		try
		{
	 		boost::interprocess::shared_memory_object shm_obj1(boost::interprocess::open_only, name.c_str(), boost::interprocess::read_write);
	 		shm_obj.swap(shm_obj1);
	 		shm_obj.truncate(effectivsize);
	 	}
	 	catch(...)
	 	{
	 		std::cerr << "IPC client missing shared memory " << name << "\n";
	 		return;
	 	}

		boost::interprocess::mapped_region r(shm_obj,boost::interprocess::read_write);
		region.swap(r);
		
		uint8_t * ptr = (uint8_t*)region.get_address();
		Payload * pp = (Payload*)ptr;

		pheader = &pp->h; // pointer local to the process
		pbase = pp->first.get(); // pointer local to the process
			
	}

	IPCPooledChannel(std::string name, WriterTag, int n, DiscardPolicy discardpolicy,bool resume):
		discard_old_(discardpolicy == DiscardPolicy::DiscardOld),alwayslast_(false)
	{

		// 1) Payload should be aligned but shm memory is aligned
		// 2) Payload size is effeviely: sizeof(Payload)+align_of(T)
		int s = sizeof(Payload) + (sizeof(typename Header::dynamic_content )+sizeof(T))*n;

		if(!resume)
		{
	 		boost::interprocess::shared_memory_object::remove((name+"meta").c_str());
	 		boost::interprocess::shared_memory_object::remove((name).c_str());
		}

		try
		{
	 		boost::interprocess::shared_memory_object shm_obj1(boost::interprocess::open_or_create, (name+"meta").c_str(), boost::interprocess::read_write);
	 		std::cerr << "opened meta\n";
	 		shm_objmeta.swap(shm_obj1);
	 		shm_objmeta.truncate(sizeof(PayloadMeta));
	 		std::cerr << "truncated meta\n";
			boost::interprocess::mapped_region r(shm_objmeta,boost::interprocess::read_write);
		 	PayloadMeta * ptr = (PayloadMeta*)r.get_address();
		 	if(ptr->typesignature[0] != 0 && strcmp(typeid(T).name(),ptr->typesignature) != 0)
		 	{
		 		std::cerr << "IPC serve type mismatch. Expected " << typeid(T).name() << " got " << ptr->typesignature << std::endl;
		 		return;
		 	}
		 	if(ptr->itemsize != 0 && ptr->itemsize != sizeof(T))
		 	{
		 		std::cerr << "IPC server type size mismatch. Expected " << sizeof(T) << " got " <<  ptr->itemsize << std::endl;
		 		return;		 		
		 	}
		 	strcpy(ptr->typesignature,typeid(T).name());
		 	ptr->itemsize = sizeof(T);
		 	if(ptr->sharedmemorysize != 0 && ptr->sharedmemorysize < s)
		 	{
		 		s = ptr->sharedmemorysize;
		 	}
	 	}
	 	catch(...)
	 	{
	 		std::cerr << "existing server " + name << "meta\n";
	 		return;
	 	}

	 	try
	 	{
	 		boost::interprocess::shared_memory_object shm_obj1(boost::interprocess::open_or_create, name.c_str(), boost::interprocess::read_write);
	 		shm_obj.swap(shm_obj1);
	 		shm_obj.truncate(s);
	 		// TODO CHECK if existing size is < than this one
	 		// TODO CHECK the type signature
	 	}
	 	catch(...)
	 	{
	 		std::cerr << "meta failed on create " << name <<"\n";
	 		return; 
	 	}

		aname = name; 

		{
			boost::interprocess::mapped_region rm(shm_objmeta,boost::interprocess::read_write);
			
			PayloadMeta * ptr = (PayloadMeta*)rm.get_address();
			ptr->sharedmemorysize = s;
		}

		boost::interprocess::mapped_region r(shm_obj,boost::interprocess::read_write);
		region.swap(r);
		
		effectiven = n;
		uint8_t * ptr = (uint8_t*)region.get_address();
		Payload * pp = (Payload*)ptr;
		pp->first = (T*)(ptr+sizeof(Payload) + sizeof(typename Header::dynamic_content)*n);

		pheader = &pp->h; // pointer local to the process
		pbase = pp->first.get(); // pointer local to the process
			
		std::cout << "allocating " << effectiven << " " << pheader << std::endl;
		if(pheader->inited_ != 0)
		{
		}
 		else
 		{
 			new (pheader) Header(effectiven,(typename Header::dynamic_content *)(ptr+sizeof(Payload)));
 		}
		std::cout << "done "<<std::endl;
	}

	/// returns the count of data ready
	int readySize() const 
	{
		scoped_lock sc(pheader->mutex_);
		return pheader->readysize;
	}

	/// returns the count of free buffers
	int freeSize() const 
	{
		scoped_lock sc(pheader->mutex_);
		return pheader->freesize;
	}

	/// returns a new writer buffer
	T* writerGet()
	{
		T * r = 0;
		{
			scoped_lock lk(pheader->mutex_);

			if(pheader->freesize == 0)
			{
				// TODO check what happens when someone read, why cannot discard if there is only one element in read_list
				if(!discard_old_ || pheader->readysize < 2)
				{
					//if(!discard_old_)
					//	std::cout << "Queues are too small, no free, and only one (just prepared) ready\n";

					while(pheader->freesize == 0)
						pheader->write_ready_var_.wait(lk);
				}
				else
				{
					// policy deleteold: kill the oldest
					pheader->readypopfront(r,pbase);
					return r;
				}
			}
			// free pop any
			pheader->freepopany(r,pbase);
		    return r;
		}
	}

	/// releases a writer buffer without storing it (aborted transaction)
	void writeNotDone(T * x)
	{
		if(x)
		{
			scoped_lock lk(pheader->mutex_);
			pheader->freepushfront(x,pbase);
		}		
	}

	/// releases a writer buffer storing it (commited transaction)
	void writerDone(T * x)
	{
		if(x)
		{
			{
				scoped_lock lk(pheader->mutex_);
				pheader->readypushback(x,pbase);
			}
			pheader->read_ready_var_.notify_all();
		}
	}

	/// gets a buffer to be read and in case it is not ready it returns a null pointer
	void readerGetNoWait(T * & out)
	{
		scoped_lock lk(pheader->mutex_);
		if(pheader->readysize == 0)
		{
			out = 0;
			return;
		}
		else
		{
			readerGetReady(out);
		}
	}

	/// gets a buffer to be read, in case it is not ready it wais for new data
	void readerGet(T * & out)
	{
		scoped_lock lk(pheader->mutex_);
		while(pheader->readysize == 0)
			pheader->read_ready_var_.wait(lk);
	    readerGetReady(out);
	}

	/// releases a buffer provided by the readerGet
	void readerDone(T * in)
	{
		if(!in)
			return;
		else
		{
			scoped_lock lk(pheader->mutex_);
			pheader->freepushfront(in,pbase);
		}
		pheader->write_ready_var_.notify_all();
	}
private:
	
	/// invoked by readerGet and readerGetNoWait to get one piece (the last or the most recent depending on policy)
	void readerGetReady(T * &out)
	{
		int n = pheader->readysize;
		if(alwayslast_ && n > 1)
		{
			do
			{
				T* tmp;
				pheader->readypopfront(tmp,pbase); // remove oldest
				pheader->freepushfront(tmp,pbase); // recycle
			} while(--n > 1);
			pheader->write_ready_var_.notify_all(); // because we have freed resources
		}
		pheader->readypopfront(out,pbase); // remove oldest
	}
};