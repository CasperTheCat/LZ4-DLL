#pragma once
#include <cstdint>
#include <boost/filesystem.hpp>

using namespace boost::filesystem;

class PathQueue
{
public:
	// Constructor
	PathQueue();

	// Destructor
	~PathQueue();

	// Push Element to start
	void push(path _path);

	// Pop Element off
	void pop();

	// Pop and return
	path accPop();

	// Access Last Item
	path access() const;

	// Access the count - Force at least 32bits for the int!
	unsigned long int size() const;

private:
	// Class
	class QueueItem
	{
	public:
		QueueItem(path _path, QueueItem* nextNode);
		~QueueItem();
		path data;
		QueueItem *next;
	};

	// Item Pointers
	QueueItem *back;
	QueueItem *arbiter; // Omnipresent node
	unsigned long int itemCount;

};