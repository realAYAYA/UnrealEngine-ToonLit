#pragma once

//forward references
class DoublyLinkedList;
class ILockable;

class DoublyLinkedListItem
{
	friend class DoublyLinkedList;

private:
	DoublyLinkedListItem(void){};

public:
	DoublyLinkedListItem(DoublyLinkedList *list);
	DoublyLinkedListItem(DoublyLinkedList *list, int id);
	virtual ~DoublyLinkedListItem(void);
	
	int  Id;

	DoublyLinkedListItem* Next() { return pNextItem;}
	DoublyLinkedListItem* Prev() { return pPrevItem;}

protected:
	// doubly linked list of objects of a given type
	DoublyLinkedListItem* pNextItem;
	DoublyLinkedListItem* pPrevItem;

	DoublyLinkedList *pList;
};

class DoublyLinkedList
{
private:
	DoublyLinkedList(void){};

public:
	DoublyLinkedList(ILockable* locker);
	virtual ~DoublyLinkedList(void);
	
	void Add(DoublyLinkedListItem* object);
	void Remove(DoublyLinkedListItem* object, int deleteObj = 1);

	void Remove(int Id);

	DoublyLinkedListItem* Find(int Id);
	const DoublyLinkedListItem* First() const { return pFirstItem;}
	const DoublyLinkedListItem* Last() const { return pLastItem;}

	int Count() { return itemCount; }

protected:
	// Maintain a list of items
	DoublyLinkedListItem* pFirstItem;
	DoublyLinkedListItem* pLastItem;

	int disposed;
	int itemCount;

	ILockable* Locker;
};
