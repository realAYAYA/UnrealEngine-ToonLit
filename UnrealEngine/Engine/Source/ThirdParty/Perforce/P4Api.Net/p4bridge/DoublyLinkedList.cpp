#include "stdafx.h"
#include "DoublyLinkedList.h"

class DoublyLinkedList;

DoublyLinkedListItem::DoublyLinkedListItem(DoublyLinkedList *list)
{
	Id = -1;

    // Initialize the list pointers
    pNextItem = NULL;
    pPrevItem = NULL;

	pList = list;

	if (list)
	{
		list->Add(this);
	}
}

DoublyLinkedListItem::DoublyLinkedListItem(DoublyLinkedList *list, int newId)
{
	Id = newId;

    // Initialize the list pointers
    pNextItem = NULL;
    pPrevItem = NULL;

	pList = list;

	if (list)
	{
		list->Add(this);
	}
}

DoublyLinkedListItem::~DoublyLinkedListItem(void)
{
	Id = -1;

    // Initialize the list pointers
    pNextItem = NULL;
    pPrevItem = NULL;
}

DoublyLinkedList::DoublyLinkedList(ILockable* locker)
{
	Locker = locker;
	//InitializeCriticalSectionAndSpinCount(&CriticalSection, 0x00000400); 
	
	pFirstItem = NULL;
	pLastItem = NULL;
	
	disposed = 0;

	itemCount = 0;
}

DoublyLinkedList::~DoublyLinkedList(void)
{
	if (disposed != 0)
	{
		return;
	}
	LOCK(Locker); 

	disposed = 1;

	DoublyLinkedListItem* curObj = pFirstItem;
	while (curObj != NULL)
	{	
		DoublyLinkedListItem* nextObj = curObj->pNextItem;
		delete curObj;
		curObj = nextObj;
	}
	pFirstItem = NULL;
	pLastItem = NULL;
}

void DoublyLinkedList::Add(DoublyLinkedListItem* object)
{
	LOCK(Locker); 
	int  cmdId = -1;

	// Initialize the list pointers
	object->pNextItem = NULL;
	object->pPrevItem = NULL;
	object->pList = this;

	// Add to the list of objects registered to be exported
	if(!pFirstItem)
	{
		// first object, initialize the list with this as the only element
		pFirstItem = object;
		pLastItem = object;
	}
	else
	{
		// add to the end of the list
		pLastItem->pNextItem = object;
		object->pPrevItem = pLastItem;
		pLastItem = object;
	}
	itemCount++;
}

void DoublyLinkedList::Remove(DoublyLinkedListItem* pObject, int deleteObj)
{
	LOCK(Locker); 
	// Remove from the list
	if ((pFirstItem == pObject) && (pLastItem == pObject))
	{
		// only object in the list, so NULL out the list head and tail pointers
		pFirstItem = NULL;
		pLastItem = NULL;
	}
	else if (pFirstItem == pObject)
	{
		// first object in list, set the head to the next object in the list
		pFirstItem = pObject->pNextItem;
		pFirstItem->pPrevItem = NULL;
	}
	else if (pLastItem == pObject)
	{
		// last object, set the tail to the pervious object in the list
		pLastItem = pObject->pPrevItem;
		pLastItem->pNextItem = NULL;
	}
	else 
	{
		// in the middle of the list, so link the pointers for the previous 
		//  and next objects.
		pObject->pPrevItem->pNextItem = pObject->pNextItem;
		pObject->pNextItem->pPrevItem = pObject->pPrevItem;
	}
	pObject->pPrevItem = NULL;
	pObject->pNextItem = NULL;

	if (deleteObj)
	{
		delete pObject;
	}
	itemCount--;
}

void DoublyLinkedList::Remove(int id)
{
	LOCK(Locker); 

	DoublyLinkedListItem* item = DoublyLinkedList::Find(id);

	Remove(item);
}

DoublyLinkedListItem* DoublyLinkedList::Find(int id)
{
	LOCK(Locker); 

	DoublyLinkedListItem* value = NULL;

	DoublyLinkedListItem* curObj = pFirstItem;
	while (curObj != NULL)
	{	
		if (curObj->Id == id)
		{
			value = curObj;
			curObj = NULL;
			break;
		}
		curObj = curObj->pNextItem;
	}
	return value;
}
