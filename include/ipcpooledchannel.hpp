/**
 Emanuele Ruffaldi @SSSA 2014

 C++11 Pooled Channel over boost IPC

 */
#include <iostream>
#include <list>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

/// Conceptual Life of pools
/// [*] -> free -> writing -> ready -> reading -> free
///
/// policy: if discardold is true on overflow: ready -> writing
/// policy: always last: ready -> reading of the last 
///
/// TODO: current version requires default constructor of data
/// TODO: objects are not destructed when in free state
template <class T, int maxN>
class IPCPooledChannel
{
public:

   typedef  boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> scoped_lock;
   struct Header
   {	
   		std::atomic<uint32_t> inited;

   		boost::interprocess::interprocess_mutex mutex_;
   		boost::interprocess::interprocess_condition  read_ready_var_, write_ready_var_;
   		int readysize = 0,freesize = 0;

   		Header(int effectiven, T * pbase)
   		{
			freesize = effectiven;
			for(int i = 0; i < effectiven; i++)
			{
				freelist[i] = pbase+i;
			}
   		}


   		inline void readypushback(T* p) // newest = append
		{
			readylist[readylisttail] = p;
			readylisttail = (readylisttail+1) % maxN;
			readysize++;
		}

		inline void readypopfront(T* & p) // oldest
		{
			readysize--;
			p = readylist[readylisthead];
			readylisthead = (readylisthead+1) % maxN;
		}


		inline void freepushfront(T* p)
		{
			freelist[freesize] = p;
			freesize++;
		}

		void freepopany(T * & p)
		{
		    p = freelist[freesize-1];
		    freesize--;			
		}
	private:
   		T* readylist[maxN]; // circular list
   		T* freelist[maxN]; // indices of free slots

   		int readylisthead = 0,readylisttail = 0; // circular list structures

   };
   boost::interprocess::shared_memory_object shm_obj;
   boost::interprocess::mapped_region region;



   Header * pheader;
   T * pbase;
   bool discard_old_,alwayslast_; /// policy 
   int effectiven;

   struct Payload{
   		Header h;
   		T first[1];
   };

   std::string aname ;


   void remove()
   {
   	shm_obj.remove(aname.c_str());
   }
	/* creates the pool with n buffers, and the flag for the policy of discard in case of read
     TODO: check name
	 TODO: check size
	 */
	IPCPooledChannel(const char * name, int n, bool adiscardold, bool aalwayslast,bool first):
		discard_old_(adiscardold),alwayslast_(aalwayslast)
	{
		if(n > maxN)
			n = maxN;
		else if(n < 0)
		{
			throw "ciao";			
		}
		aname = name; //
		// TODO check
		bool existent = false;
		int s = sizeof(Payload) + sizeof(T)*(n-1);
		try
		{
	 		boost::interprocess::shared_memory_object shm_obj1(boost::interprocess::create_only, name, boost::interprocess::read_write);
	 		shm_obj.swap(shm_obj1);
	 		shm_obj.truncate(s);
	 	}
	 	catch(...)
	 	{
	 		existent = true;
	 	}

	 	if(existent)
	 	{
	 		boost::interprocess::shared_memory_object shm_obj1(boost::interprocess::open_only, name, boost::interprocess::read_write);
	 		shm_obj.swap(shm_obj1);
	 		shm_obj.truncate(s);
	 	}

		boost::interprocess::mapped_region r(shm_obj,boost::interprocess::read_write);
		region.swap(r);
		
		effectiven = n;
		Payload * pp = (Payload*)region.get_address();
		pheader = &pp->h;
		pbase = pp->first;

			
		uint32_t desidered = 0;
		if(pheader->inited.compare_exchange_strong(desidered,1))
		{
			new (pheader) Header(effectiven,pbase);

		 	// TODO: use a message queue for notifying the state ... or better use a named semaphore
		 	pheader->inited = 2;
		}
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
					if(!discard_old_)
						std::cout << "Queues are too small, no free, and only one (just prepared) ready\n";

					while(pheader->freesize == 0)
						pheader->write_ready_var_.wait(lk);
				}
				else
				{
					// policy deleteold: kill the oldest
					pheader->readypopfront(r);
					return r;
				}
			}
			// free pop any
			pheader->freepopany(r);
		    return r;
		}
	}

	/// releases a writer buffer without storing it (aborted transaction)
	void writeNotDone(T * x)
	{
		if(x)
		{
			scoped_lock lk(pheader->mutex_);
			pheader->freepushfront(x);
		}		
	}

	/// releases a writer buffer storing it (commited transaction)
	void writerDone(T * x)
	{
		if(x)
		{
			{
				scoped_lock lk(pheader->mutex_);
				pheader->readypushback(x);
			}
			pheader->read_ready_var_.notify_one();
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
			pheader->freepushfront(in);
		}
		pheader->write_ready_var_.notify_one();
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
				pheader->readypopfront(tmp); // remove oldest
				pheader->freepushfront(tmp); // recycle
			} while(--n > 1);
			pheader->write_ready_var_.notify_all(); // because we have freed resources
		}
		pheader->readypopfront(out); // remove oldest
	}
};