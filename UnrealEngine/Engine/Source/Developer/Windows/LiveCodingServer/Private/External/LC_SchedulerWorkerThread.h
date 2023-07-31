// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

// BEGIN EPIC MOD
#include "CoreTypes.h"
// END EPIC MOD
#include "LC_ThreadTypes.h"


namespace scheduler
{
	class TaskQueue;


	class WorkerThread
	{
	public:
		explicit WorkerThread(TaskQueue* queue);
		~WorkerThread(void);

	private:
		Thread::ReturnValue ThreadFunction(TaskQueue* queue);

		Thread::Handle m_thread;
	};
}
