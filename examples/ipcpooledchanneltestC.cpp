#include "ipcpooledchannel.hpp"
#include <chrono>

int main(int argc , char* argv[])
{
struct Content
{
std::chrono::time_point<std::chrono::high_resolution_clock> time;
int value;
int datum[128000];
};

	IPCPooledChannel<Content> pc("pippo",ReaderTag(),ReadOrderPolicy::Ordered);

	std::cout << "after ctor\n";
	std::cout << "sizes " << pc.readySize() << " " << pc.freeSize() << std::endl;

	while(true)
	{
		Content * p2 = 0;
		pc.readerGet(p2);
	auto got = std::chrono::high_resolution_clock::now();		
		if(p2)
		{
			std::cout << "p2 is " << p2->value << " after " << std::chrono::duration<double>(got-p2->time).count()*1000000 <<  "us" << std::endl;
		}
		else
		{
			std::cout << "no data\n";
		}
		pc.readerDone(p2);
	}

	pc.remove();
}