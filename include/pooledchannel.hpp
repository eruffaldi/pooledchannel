/**
 Emanuele Ruffaldi @SSSA 2014

 C++11 Pooled Channel AKA Queue with some bonuses

 Future Idea: specialize the pool for std::array removing the need of the lists
 */
#include <iostream>
#include <list>
#include <vector>
#include <mutex>
#include <condition_variable>

/// Conceptual Life of pools
/// [*] -> free -> writing -> ready -> reading -> free
///
/// policy: if discardold is true on overflow: ready -> writing
/// policy: always last: ready -> reading of the last 
///
/// TODO: current version requires default constructor of data
/// NOTE: objects are not destructed when in free state
namespace detailpool
{
	/// TODO: add support for populating the free list

	template <class CT>
	struct pooltrait;

	template<class T>
	struct pooltrait<std::vector<T> >
	{
		typedef std::vector<T> container_t;
		static const bool unlimited = true;
		static const int size = -1;
		static void initandpopulate(container_t & c, int n, std::list<T*> & fl)
		{
			c.resize(n);
			for(int i = 0; i < c.size(); ++i)
				fl.push_back(&c[i]);
		}

		static T* extend(container_t & c)
		{
			int n = c.size();
			c.push_back(T());
			return &c[n];
		}
	};

	template<class T, int n>
	struct pooltrait<std::array<T,n> >
	{
		typedef std::array<T,n> container_t;
		static const bool unlimited = false;
		static const int size = n;
		static void initandpopulate(container_t & c, int nignored, std::list<T*> & fl)
		{
			for(int i = 0; i < n; ++i)
				fl.push_back(&c[i]);			
		}
		static T* extend(container_t & c)
		{
			return 0;
		}
	};

	template<class T>
	struct pooltrait<std::list<T> >
	{
		typedef std::list<T> container_t;
		static const bool unlimited = true;
		static const int size = -1;
		static void initandpopulate(container_t & c, int n, std::list<T*> & fl)
		{
			for(int i = 0; i < n; ++i)
			{
				fl.push_back(&(*c.emplace(c.end())));
			}
		}
		static T* extend(container_t & c)
		{
			return &(*c.emplace(c.end()));
		}
	};
}

template <class T, class CT = std::vector<T> >
class PooledChannel
{
	CT data_; /// array of data
	std::list<T*> free_list_; /// list of free buffers
	std::list<T*> ready_list_; /// list of ready buffers
	bool discard_old_; /// when full discard old data instead of overflow
	bool alwayslast_;  /// always return last value 
	bool unlimited_;   /// unlimited
	mutable std::mutex mutex_;
	mutable std::condition_variable write_ready_var_,read_ready_var_;

public:	

	/// creates the pool with n buffers, and the flag for the policy of discard in case of read
	PooledChannel(int n, bool adiscardold, bool aalwayslast): discard_old_(adiscardold),alwayslast_(aalwayslast),
		unlimited_(detailpool::pooltrait<CT>::unlimited && n <= 0)
	{
		int nn = detailpool::pooltrait<CT>::size == -1 ? n : detailpool::pooltrait<CT>::size;
		detailpool::pooltrait<CT>::initandpopulate(data_,nn,free_list_);
	}

	/// returns the count of data ready
	int readySize() const 
	{
		std::unique_lock<std::mutex> lk(mutex_);
		return ready_list_.size();
	}

	/// returns the count of free buffers
	int freeSize() const 
	{
		std::unique_lock<std::mutex> lk(mutex_);
		return free_list_.size() + (unlimited_ ? 1 : 0);
	}

	/// returns a new writer buffer
	///
	/// if dowait and underflow it waits indefinetely
	T* writerGet(bool dowait = true)
	{
		T * r = 0;
		{
			std::unique_lock<std::mutex> lk(mutex_);

			if(free_list_.empty())
			{
				if(unlimited_)
				{
					return detailpool::pooltrait<CT>::extend(data_);
				}
				// check what happens when someone read, why cannot discard if there is only one element in read_list
				else if(!discard_old_ || ready_list_.size() < 2)
				{
					if(!dowait)
						return 0;
					write_ready_var_.wait(lk, [this]{return !this->free_list_.empty();});
				}
				else
				{
					// policy deleteold: kill the oldest
					r = ready_list_.front();
					ready_list_.pop_front();
					return r;
				}
			}
			// pick any actually
			r = free_list_.front();
			free_list_.pop_front();
			return r;
		}
	}

	/// simple write
	bool write(const T& x)
	{
		T * p = writerGet();
		if(!p)
			return false;
		*p = x;
		writerDone(p);
	}

	/// simple read
	bool read(T & x)
	{
		T * p = 0;
		readerGet(p);
		if(!p)
			return false;
		x = *p;
		readerDone(p);
		return true;	
	}

	/// reader no wait
	bool readNoWait(T & x)
	{
		T * p = 0;
		readerGetNoWait(p);
		if(!p)
			return false;
		x = *p;
		readerDone(p);
		return true;	
	}

	/// releases a writer buffer without storing it (aborted transaction)
	void writeNotDone(T * x)
	{
		if(x)
		{
			std::unique_lock<std::mutex> lk(mutex_);
			free_list_.push_back(x);
		}		
	}

	/// releases a writer buffer storing it (commited transaction)
	void writerDone(T * x)
	{
		if(!x)
			return;
		{
			std::unique_lock<std::mutex> lk(mutex_);
			ready_list_.push_back(x);
		}
		read_ready_var_.notify_one();
	}

	/// gets a buffer to be read and in case it is not ready it returns a null pointer
	void readerGetNoWait(T * & out)
	{
		std::unique_lock<std::mutex> lk(mutex_);
		if(this->ready_list_.empty())
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
		std::unique_lock<std::mutex> lk(mutex_);
	    read_ready_var_.wait(lk, [this]{return !this->ready_list_.empty();});
	    readerGetReady(out);
	}

	/// releases a buffer provided by the readerGet
	void readerDone(T * in)
	{
		if(!in)
			return;
		else
		{
			std::unique_lock<std::mutex> lk(mutex_);
			free_list_.push_back(in);
		}
		write_ready_var_.notify_one();
	}

	/*
	 This scope object allows to acquire a pointer to a block for writing
	 and then release it on distructor. The scope allows to abort the writing
	 operation at the end
	*/
	struct WriteScope
	{
		T * p_;
		PooledChannel& c_;
		WriteScope(PooledChannel & c):c_(c) {
			p_ = c.writerGet();
		}

		void abort()
		{
			c_.writeNotDone(p_);
			p_ = 0;
		}

		~WriteScope()
		{
			if(p_)
				c_.writerDone(p_);
		}

		operator bool () { return p_ != 0;}
		operator T* () { return p_; }
		T & operator * () { return *p_; }
		T * operator -> () { return p_; }
	};

	/**
	 * This scope allows to get a pointer and release it
	 */
	struct ReadScope
	{
		T * p_;
		PooledChannel & c_;
		ReadScope(PooledChannel & c, bool dowait,  bool wait): c_(c),p_(0) {
			if(wait)
				c.readerGet(p_);
			else
				c.readerGetNoWait(p_);
		}

		~ReadScope()
		{
			if(p_)
				c_.readerDone(p_);
		}

		operator bool () { return p_ != 0;}
		operator T* () { return p_; }
		T & operator * () { return *p_; }
		T * operator -> () { return p_; }
	};

private:
	/// invoked by readerGet and readerGetNoWait to get one piece (the last or the most recent depending on policy)
	void readerGetReady(T * &out)
	{
		int n = ready_list_.size();
		if(alwayslast_ && n > 1)
		{
			do
			{
				T * tmp =  ready_list_.front();
				ready_list_.pop_front();
				free_list_.push_front(tmp);				
			} while(--n > 1);
			write_ready_var_.notify_one(); // because we have freed resources
		}
		out = ready_list_.front();
		ready_list_.pop_front();
	}

};