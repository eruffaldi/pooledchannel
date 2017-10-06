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

	IPCPooledChannel<Content> pc("pippo",WriterTag(),3,DiscardPolicy::NoDiscard,argc > 1); // resume
	int frame = 0;
	std::cout << "itemsize " << sizeof(Content) << std::endl;

	while(true)
	{
		Content * p1;
		p1  = pc.writerGet();
			auto got = std::chrono::high_resolution_clock::now();		

		std::cout << "writer got " << p1 << "\n";
		p1->value= frame++;
		p1->time = got;
		pc.writerDone(p1);
	}

	pc.remove();
}