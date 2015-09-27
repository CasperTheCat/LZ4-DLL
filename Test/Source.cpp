#include "../Core/Core.h"
#include "Queue.h"
#include <iostream>
#include <boost/algorithm/string/predicate.hpp>

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
	std::string::size_type pAt;
	bool bIsCompressed;
	char *data;

	while (fileQueue->size() > 0)
	{
		// FIRST THINGS
		oName = fileQueue->accPop().string();
		bIsCompressed = boost::algorithm::ends_with(oName,".lz4");
	

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
		if (bIsCompressed)
		{
			lz4Proc->decompress(data, int(fEnd - fBeg));
			pAt = oName.find_last_of('.');
			oName = oName.substr(0, pAt);
		}
		else
		{
			lz4Proc->compress(data, int(fEnd - fBeg));
			oName = oName + ".lz4";
		}
		delete data;
		if (lz4Proc->len() == 0) continue;
		std::cout << lz4Proc->len() << std::endl;

		fOut.open(oName, std::ios::binary);
		if (!fOut.is_open()) continue;
		fOut.write(lz4Proc->ptr(), lz4Proc->len());
		fOut.close();

	}

	delete fileQueue;


	getchar();
	return 0;
}