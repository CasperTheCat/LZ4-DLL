#include "Queue.h"

PathQueue::PathQueue()
{
	 // Default Constructor
	back = nullptr;
	arbiter = new QueueItem(path("NULL"), nullptr);
	itemCount = 0;
}

PathQueue::~PathQueue()
{
	while (itemCount != 0)
	{
		pop();
	}

	// Last move
	free(arbiter);
}

// Push to forward
void PathQueue::push(path _path)
{
	QueueItem* temp;
	if (itemCount == 0)
	{
		temp = new QueueItem(_path, nullptr);
	}
	else
	{
		temp = new QueueItem(_path, arbiter->next);
	}

	arbiter->next = temp;
	if (back == nullptr) back = temp;
	itemCount++;
}

// Pop Element off
void PathQueue::pop()
{
	if (back != nullptr) free(back);
	// Relocate Back Node
	auto temp = arbiter;
	for (auto i = 0; i < itemCount - 1; ++i) // Move to 1 back from end
	{
		temp = temp->next;
	}

	// Set Back 
	if (temp != arbiter) back = temp;
	else back = nullptr;
	--itemCount;
}

path PathQueue::accPop()
{
	auto retVal = back->data;
	if (back != nullptr) free(back);
	// Relocate Back Node
	auto temp = arbiter;
	for (auto i = 0; i < itemCount - 1; ++i) // Move to 1 back from end
	{
		temp = temp->next;
	}

	// Set Back 
	if (temp != arbiter) back = temp;
	else back = nullptr;
	--itemCount;
	return retVal;
}

// Access Last Item
path PathQueue::access() const
{
	if (back == nullptr) return path("NULLPTR_T");
	return back->data;
}

unsigned long int PathQueue::size() const
{
	return itemCount;
}

PathQueue::QueueItem::QueueItem(path _path, QueueItem *nextNode)
{
	next = nextNode;
	data = _path;
}

PathQueue::QueueItem::~QueueItem()
{
	// Do Nothing
}

