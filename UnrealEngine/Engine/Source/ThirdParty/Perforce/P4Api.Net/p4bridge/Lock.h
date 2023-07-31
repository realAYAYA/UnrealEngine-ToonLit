#pragma once

#include <string>

using std::string;

//forward ref
class Lock;

class ILockable
{
	friend Lock;

public:
	ILockable();

	int InitCritSection();
	void FreeCriticalSection();

private:

int CriticalSectionInitialized;

#ifdef _WIN32
	CRITICAL_SECTION CriticalSection; 	
#else
	pthread_mutex_t Mutex;		
#endif	

#ifdef _DEBUG
	// doubly linked list of active locks for debugging deadlocks
	Lock* pFirstLockDebugData;
	Lock* pLastLockDebugData;
#endif
	int activeLockCount;
};

class Lock
{
public:
#ifdef _DEBUG
	Lock(ILockable* it, const char *_file, int _line);
#else
	Lock(ILockable* it);
#endif
	virtual ~Lock(void);

private:
	ILockable* It;

#ifdef _DEBUG
	string file;
	int line;

	// doubly linked list
	Lock* pNextLockDebugData;
	Lock* pPrevLockDebugData;
#endif
};

#ifdef _DEBUG
#define LOCK(it) Lock __LOCK_IT__(it, __FILE__, __LINE__)
#else
#define LOCK(it) Lock __LOCK_IT__(it)
#endif