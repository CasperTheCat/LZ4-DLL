#include "../Core/Core.h"
#include "Queue.h"
#include <iostream>

//////////////////////////////////////////////////
// Global Queue
PathQueue *fileQueue;

//////////////////////////////////////////////////////////////////////////
// Iterate and process
// 
void recurseProcessor(boost::filesystem::path _path)
{
	boost::filesystem::directory_iterator finalIter;
	boost::filesystem::directory_iterator nDI(_path);
	while (nDI != finalIter)
	{
		if (boost::filesystem::is_directory(*nDI))
		{
			recurseProcessor(*nDI);

		}
		else
		{
#ifdef FILEIO
			std::cout << std::endl << "Reading file" << *nDI << std::endl;
#endif
			//snglProcess(*nDI);
			fileQueue->push(*nDI);
		}
		++nDI;
	}
	return;
}




int main(int argc, char **argv)
{
	if (argc < 2)
	{
		return 1;
	}

	{
		// Queue
		fileQueue = new PathQueue();

		for (int i = 1; i < argc; ++i)
		{
			boost::filesystem::path argPath(argv[i]);
			if (boost::filesystem::exists(argPath)) // Does the file exist?
			{
				if (boost::filesystem::is_regular_file(argPath))
				{
					// TODO: NOT YET IMPLEMENTED
					// Execute file process
#ifdef FILEIO
					std::cout << std::endl << "Reading file" << argPath << std::endl;
#endif
					fileQueue->push(argPath);
				}
				else if (boost::filesystem::is_directory(argPath))
				{
					// TODO: NOT YET IMPLEMENTED
					// Execute on all files in the directory
					recurseProcessor(argPath);
				}
				else
				{
					return -1;
				}
			}
		}
	}

	auto lz4Proc = new LZ4::Processor();

	std::ifstream fIn;
	std::ofstream fOut;
	size_t fBeg, fEnd;
	std::string oName;
	char *data;

	while (fileQueue->size() > 0)
	{
		oName = fileQueue->accPop().string();
		fIn.open(oName,std::ios::binary);
		if (!fIn.is_open())
		{
			continue;
		}
		fIn.seekg(0, std::ios::end);
		fEnd = fIn.tellg();
		fIn.seekg(0, std::ios::beg);
		fBeg = fIn.tellg();

		data = new char[(fEnd - fBeg)];
		fIn.read(data, fEnd - fBeg);
		fIn.close();

		std::cout << "File is " << fEnd - fBeg << std::endl;

		lz4Proc->compress(data, int(fEnd - fBeg));
		delete data;

		oName = oName + ".lz4";
		fOut.open(oName, std::ios::binary);
		
		if (lz4Proc->len() == 0) continue;

		std::cout << lz4Proc->len() << std::endl;

		if (!fOut.is_open()) continue;
		fOut.write(lz4Proc->ptr(), lz4Proc->len());
		fOut.close();

	}

	delete fileQueue;


	getchar();
	return 0;
}