// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "MuR/MutableMemory.h"


namespace mu
{

	//
	class Task : public Base
	{
	public:

		//! Ensure virtual destruction.
		virtual ~Task() = default;

		//! Execute this task.
		virtual void Run() = 0;
	};


    //---------------------------------------------------------------------------------------------
    //! Implement concurrent tasks for the mutable compilation process.
	//! \TODO: Move to unreal way of doing this.
    //---------------------------------------------------------------------------------------------
    class TaskManager : public Base
    {
    public:

        class Task : public mu::Task
        {
        public:

            //! Executed from the managing thread when the task has been run, before deleting it.
            virtual void Complete() = 0;
        };

		TaskManager(bool InUseConcurrency) : UseConcurrency(InUseConcurrency) {}

        //! Add a new task to be executed.
        void AddTask( Task* task );

        //! Make sure all tasks are completed.
        void CompleteTasks();

        //!
		inline bool IsConcurrencyEnabled() const { return UseConcurrency; };

    private:

		bool UseConcurrency = false;
		TArray<TPair<Task*,FGraphEventRef>> m_running;
		TArray<Task*> m_pending;

		void SendOne();
	};

}

