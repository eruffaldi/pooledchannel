#include "pooledchannel.hpp"
#include <array>
#include <signal.h>

class BasePublisher
{
public:
	virtual std::type_info * type() = 0;
	cast
};

class BaseSubscriber
{
public:
	virtual std::type_info * type() = 0;
	cast
};

template <class T, class PC>
class Publisher;

template <class T, class PC = PooledChannel<T> >
class Subscriber : public BaseSubscriber
{
public:
	virtual std::type_info * type() {} ...
	cast

	std::string name_;
	std::shared_ptr<PooledChannel<T>> pool_;
	std::vector<std::shared_ptr<Publisher<T,CT> > > pubs_;

	Subscriber(std::string name) : name_(name) {}

};


template <class T, class PC = PooledChannel<T> >
class Publisher : public BasePublisher
{
public:
	virtual std::type_info * type() {} ...
	cast

	std::string name_;

	Publisher(std::string name) : name_(name) {}
	void set_termination(bool * p) { terminator_ = p; }

	void addSubscriber(std::shared_ptr<Subscriber<T,CT> > a)
	{
		pools_.push_back(a);
		// TODO latch support
	}

#if 0
	// directly creates it
	std::shared_ptr<PC>  addSubscriber(int queue_size, bool p1, bool p2)
	{
		std::shared_ptr<PC>  r = std::make_shared<PC>  (queue_size,p1,p2);
		r->set_termination(terminator_);
		pools_.push_back(r);
		// TODO latch support
		return r;
	}
#endif

	void publish(const T & x)
	{
		for(auto & p: pools_)
			p->write(x);
		// TODO save for latch
	}

	std::vector<std::shared_ptr<Publisher<T,CT> > > pools_;
	bool * terminator_ = 0;
};

class Router
{
public:
	template <class T>
	std::shared_ptr<Publisher<T> > publisher(std::string name)
	{
		std::shared_ptr<Publisher<T> > p = std::make_shared<Publisher<T> > ();
		pubs_[name].push_back(p);
		auto su = subs_.find(name);

		if(su != subs_.end())
		{
			for(auto it : su->second)
			{
				// FIX it type
				it->pubs_.push_back(p);
				p->addSubscriber(*it);
			}
		}
		return p;
	}

	template <class T>
	std::shared_ptr<PooledChannel<T> > subscriber(std::string name)
	{
		std::shared_ptr<PooledChannel<T> > s = std::make_shared<PooledChannel<T> > ();
		subs_[name].push_back(s);
		auto pub = pubs_.find(name);
		if(su != pubs_.end())
		{
			for(auto it : su->second)
			{
				// FIX it type
				s.pubs_.push_back(*it);
				it->addSubscriber(s); 
			}
		}
		return s;
	}

	template <class T>
	void remove_subscriber(std::string name, std::shared_ptr<PooledChannel<T> > su)
	{
		auto it = subs_.find(name);
		if(it == subs_.end())
			return;
		if(it.second.erase(su) != it.second.end())
		{
			for(auto & pu: su.pubs_)
			{
				// remove su from pu list
			}
		}
	}

	template <class T>
	void remove_publisher(std::shared_ptr<Publisher<T> >  pu)
	{
		auto it = pubs_.find(pu->name_);
		if(it == pubs_.end())
			return;		
		if(it.second.erase(pu) != it.second.end())
		{
			for(auto & su: pu.subs_)
			{
				// remove pu from su list
			}
		}
	}

	std::unordered_map<std::string,std::vector<std::shared_ptr<BasePublisher> > > pubs_;
	std::unordered_map<std::string,std::vector<std::shared_ptr<BaseSubscriber > > > subs_;
};


int main(int argc , char* argv[])
{
	struct sigaction new_action, old_action;

	bool done = false;

	Router r;
	r.add
	Publisher<int> pub;
	pub.set_termination(&done);

	auto s1 = pub.addSubscriber(2,true,false);
	auto s2 = pub.addSubscriber(2,true,false);

	pub.publish(10);
	int a;
	s1->read(a);
	std::cout << "s1 " << a << std::endl;
	s2->read(a);
	std::cout << "s2 " << a << std::endl;
	return 0;
}