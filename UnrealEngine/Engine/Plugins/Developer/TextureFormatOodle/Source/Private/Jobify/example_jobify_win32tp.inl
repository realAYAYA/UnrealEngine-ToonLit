// Copyright Epic Games, Inc. All Rights Reserved.
/*

  Oodle pluggable job system example using the Win32 thread pool API (Windows Vista and later).

  The Vista+ thread pool API is part of the regular Windows APIs and is exposed with
  _WIN32_WINNT >= 0x0600.

  The thread pool doesn't have direct support for DAGs of jobs, but does permit tasks on the
  pool to perform blocking waits (creating more pool threads when required), which is stronger
  than what we need.

  The main difficulty in this implementation is implementing wake-up of dependent tasks
  efficiently. It was written carefully to work even when CreateThreadpoolWork calls
  fail spuriously (not that this is expected to happen).

*/

#include "example_jobify.h"

#ifdef OOEX_PLATFORM_NT
#include <stdlib.h>
#include <assert.h>

#if defined(UE_BUILD_DEBUG) || defined(UE_BUILD_DEVELOPMENT) || defined(UE_BUILD_TEST) || defined(UE_BUILD_SHIPPING)

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsPlatformAtomics.h"

#define InterlockedExchangeAdd(ptr,v) FPlatformAtomics::InterlockedAdd((int32 *)ptr,v)

#else

// For the Windows Vista-level thread pool APIs
#define _WIN32_WINNT 0x0600
#include <Windows.h>

#endif

#ifdef RTL_SRWLOCK_INIT

//#pragma message("example_jobify_win32tp!")

// ---- Implementation of the jobify callbacks using the Windows thread pool

//#define DEBUG printf
#define DEBUG(...)

class JobContext
{
	struct DepLink
	{
		JobContext * job;
		DepLink * next; // singly-linked list
	};

	// The job callback and its (opaque) data pointer
	t_fp_Oodle_Job * fp_job;
	void * job_data;

	// The thread pool work handle
	PTP_WORK work_handle;

	// "Inverted" lock; held from creation until we've finished executing the job.
	SRWLOCK busy_lock;

	// Dependency slots. Not actually our data, it belongs to the jobs
	// we depend on, which use it to store a linked list of their
	// dependents.
	DepLink dep_slot[OODLE_JOB_MAX_DEPENDENCIES];

	// Dependency tracking information (manipulated via Interlocked ops, don't touch directly outside init!)
	LONG volatile num_unsatisfied_deps;

	SRWLOCK dependents_lock; // Just used as a non-recursive mutex; protects the following fields:
	bool job_completed;
	DepLink * dependents_head;
	DepLink * * dependents_tail_next;
	// ---- end data protected by dependents_lock

	static VOID CALLBACK work_callback(PTP_CALLBACK_INSTANCE /*instance*/, PVOID context, PTP_WORK /*work*/)
	{
		JobContext * me = static_cast<JobContext *>(context);

		// Actually run the function
		me->fp_job(me->job_data);

		// Flag our job as complete.
		me->notify_completion();
	}

	void notify_dep_satisfied(LONG num_deps = 1)
	{
		LONG deps_before = InterlockedExchangeAdd(&num_unsatisfied_deps, -num_deps);

		if (deps_before == num_deps)
		{
			DEBUG("submit 0x%p\n", work_handle);
			SubmitThreadpoolWork(work_handle);
		}
	}

	void notify_completion()
	{
		// Flag the job as complete and grab the dependents list head
		DepLink * dep;

		AcquireSRWLockExclusive(&dependents_lock);
		job_completed = true; // prevent anyone from adding further dependencies on us
		dep = dependents_head;
		ReleaseSRWLockExclusive(&dependents_lock);

		// This notifies wait() that the job is complete.
		ReleaseSRWLockExclusive(&busy_lock);
		// note our "this" can now be deleted!

		// Unblock the dependents we know about; setting job_completed guarantees the
		// dependency list is now fixed and will not be modified any further.
		
		// Notify all dependents we know about that this dependency is now satisfied
 		while(dep)
 		{
 			DepLink * next = dep->next;
 			JobContext * job = dep->job;
 			// as soon as you call this, "dep" can be deleted :
			job->notify_dep_satisfied();
 			dep = next;
 		}

	}

	// Returns 1 if the job was already completed before we set up the
	// dependency edge.
	LONG add_dependent(JobContext *which, int slot)
	{
		// Set up the dependency record
		DepLink *dep = &which->dep_slot[slot];
		dep->job = which;
		dep->next = 0;

		AcquireSRWLockExclusive(&dependents_lock);

		LONG ret = 1; // assume job is already complete
		if (!job_completed)
		{
			// Insert a new dependent at the end. (We maintain the dependents
			// list in FIFO order so that we unblock the longest-waiting
			// dependent jobs first.)
			*dependents_tail_next = dep;
			dependents_tail_next = &dep->next;

			// Return 0 to signal the dependency was actually added.
			ret = 0;
		}

		ReleaseSRWLockExclusive(&dependents_lock);
		return ret;
	}

public:

 	JobContext(t_fp_Oodle_Job * _fp_job, void * _job_data, OO_U64 * dependencies, int num_dependencies, void * user_ptr)
   	{
 		// @@ CB make sure I'm initialized :
 		memset(this,0,sizeof(*this));
 
 		fp_job = _fp_job;
 		job_data = _job_data;

		// One extra dependency, for us completing setup, because the job may
		// not launch until we're done setting up all dependencies.
		num_unsatisfied_deps = num_dependencies + 1;

		InitializeSRWLock(&busy_lock);
		AcquireSRWLockExclusive(&busy_lock); // Only gets released when the job completes

		InitializeSRWLock(&dependents_lock);
		job_completed = false;
		dependents_head = 0;
		dependents_tail_next = &dependents_head;

		// user_ptr is optional in this example (can be NULL)
		//	PTP_CALLBACK_ENVIRON of NULL means use the process's default thread pool
		PTP_CALLBACK_ENVIRON tpenviron = (PTP_CALLBACK_ENVIRON) user_ptr;

		work_handle = CreateThreadpoolWork(work_callback, this, tpenviron);
		DEBUG("create 0x%p\n", work_handle);

		// If the work item was created successfully, set up the dependencies
		if (work_handle)
		{
			// Initially, the only dependency we're going to release on completion
			// of setup is the explicit setup dependency.
			LONG release_count = 1;

			// Add our dependencies. If we discover already-completed jobs during
			// this, our release_count will go up.
			for (int i = 0; i < num_dependencies; i++)
				release_count += reinterpret_cast<JobContext *>(dependencies[i])->add_dependent(this, i);

			// We're done setting up. Release our setup dependency as well as
			// any other already-satisfied dependencies we ran into, and if that
			// brings our outstanding dependency count down to 0, launch the job.
			notify_dep_satisfied(release_count);
		}
	}

	~JobContext()
	{
		DEBUG("close  0x%p\n", work_handle);
		if (work_handle)
			CloseThreadpoolWork(work_handle);
	}

	bool valid() const
	{
		return work_handle != NULL;
	}

	void wait()
	{
		// The busy_lock is held until the job completes;
		// so trying to grab it "parks" us until then.
		AcquireSRWLockExclusive(&busy_lock);

		// Immediately release it. We use the lock just for the
		// explicit hand-off. Most jobs only get waited for exactly
		// once, but we can end up calling this multiple times in
		// the error-handling case in thread_pool_run_job below.
		ReleaseSRWLockExclusive(&busy_lock);
	}
};

OO_U64 OODLE_CALLBACK example_jobify_win32tp_run_job(t_fp_Oodle_Job * fp_job, void * job_data, OO_U64 * dependencies, int num_dependencies, void * user_ptr)
{
	assert(num_dependencies <= OODLE_JOB_MAX_DEPENDENCIES);

	// user_ptr coming back NULL is fine in this example

	// Allocate a context struct to pass to the work callback
	JobContext * ctx = new JobContext(fp_job, job_data, dependencies, num_dependencies, user_ptr);

	if (!ctx || !ctx->valid())
	{
		// If something went wrong, bail on the job context, manually wait for
		// our dependencies and then run the job directly.
		if (ctx)
			delete ctx;

		for (int i = 0; i < num_dependencies; i++)
			reinterpret_cast<JobContext *>(dependencies[i])->wait();

		fp_job(job_data);
		return 0;
	}

	// Return our context pointer as the handle.
	return reinterpret_cast<OO_U64>(ctx);
}

void OODLE_CALLBACK example_jobify_win32tp_wait_job(OO_U64 job_handle, void * /*user_ptr*/)
{
	JobContext * job = reinterpret_cast<JobContext *>(job_handle);

	// Wait for the outstanding job to finish
	job->wait();

	// Then dispose of the context and clean up
	delete job;
}

void * example_jobify_init()
{
	SYSTEM_INFO systeminfo;
	GetSystemInfo( &systeminfo );
	int logical_cpu_count = systeminfo.dwNumberOfProcessors;
		
	example_jobify_run_job_fptr = example_jobify_win32tp_run_job;
	example_jobify_wait_job_fptr = example_jobify_win32tp_wait_job;
	
	// target_parallelism is desired number of jobs for Oodle to run simultaneously
	// logical_cpu_count includes hyperthreads
	// often it's better to only run on physical cores
	// unfortunately there's no simple API to get that
	//	so for simplicity in this example code we use logical_cpu_count
	//example_jobify_target_parallelism = logical_cpu_count/2; 
	example_jobify_target_parallelism = logical_cpu_count;
	
	// The jobifyUserPtr in this implementation optionally points at a
	// TP_CALLBACK_ENVIRON; if you're OK with using the defaults (this
	// example is), just pass 0.
	void * jobifyUserPtr = NULL;
	
	return jobifyUserPtr;
}

#else

#pragma message("windows.h too old, no Jobify!")

void * example_jobify_init()
{
	example_jobify_run_job_fptr = NULL;
	example_jobify_wait_job_fptr = NULL;
	example_jobify_target_parallelism = 0;
	
	return NULL;
}

#endif

#endif // OOEX_PLATFORM_NT


