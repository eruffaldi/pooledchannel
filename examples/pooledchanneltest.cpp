#include "pooledchannel.hpp"
#include <array>

template <class T,class CT>
void atest()
{

	PooledChannel<T,CT> pc(2,false,true);

	// make test
	{
		typename PooledChannel<T,CT>::WriteScope ws(pc);
		*ws = 1;
	}

	{
		typename PooledChannel<T,CT>::WriteScope ws(pc);
		*ws = 2;
	}

	{
		typename PooledChannel<T,CT>::ReadScope rs(pc,false,false); 
		if(rs)
			std::cout << "pc " << *rs << std::endl;			
	}
}

int main(int argc , char* argv[])
{
	atest<int,std::vector<int> >();
	atest<int,std::array<int,3> >();
	atest<int,std::list<int> >();
//	atest<int,std::std::list<int,3> >();

}