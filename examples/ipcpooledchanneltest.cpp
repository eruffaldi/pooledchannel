#include "ipcpooledchannel.hpp"
int main(int argc , char* argv[])
{

	IPCPooledChannel<int,3> pc("pippo",3,true,true,true);

	std::cout << "after ctor\n";
	std::cout << "sizes " << pc.readySize() << " " << pc.freeSize() << std::endl;
	// make test
	int * p1;
	p1  = pc.writerGet();
	std::cout << "writer got " << p1 << "\n";
	*p1 = 1;
	pc.writerDone(p1);

	p1  = pc.writerGet();
	*p1 = 2;
	std::cout << "writer got " << p1 << "\n";
	pc.writerDone(p1);

	std::cout << "after two writes sizes " << pc.readySize() << " " << pc.freeSize() << std::endl;

	int * p2 = 0;

	pc.readerGet(p2);
	std::cout << "p2 is " << p2 << std::endl;
	if(p2)
	{
		std::cout << "pc " << *p2 << std::endl;
	}
	else
		std::cout << "no data\n";
	pc.readerDone(p2);

	std::cout << "bye " << std::endl;

	pc.remove();
}