// Copyright Epic Games, Inc. All Rights Reserved.
/*

  Oodle pluggable job system example using Intel TBB on Linux. The TBB logic in here
  is not Linux-specific and should work on any platform supported by TBB.

  TBB's model is one of fork-join parallelism and doesn't directly support the somewhat
  more general job graphs we want, but it is relatively straightforward to build the
  semantics we need on top of TBB tasks. This works the same way as in TBBs "General
  Acyclic Graphs of Tasks" example, although it takes a bit of plumbing, encapsulated
  in the JobContext class.

  TBB "wait_for_all()" operations don't actually perform a blocking wait on the calling
  thread, and instead make it run other work until the task it was waiting for is
  complete. Therefore this implementation never needs more worker threads than logical
  cores to fill up the machine and is quite efficient.

*/

#include "example_jobify.h"

#ifdef OOEX_PLATFORM_LINUX
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <tbb/spin_mutex.h>
#include <tbb/task.h>
#include <tbb/task_scheduler_init.h>

// ---- Implementation of the job callbacks using TBB

class JobContext
{
	struct DepLink
	{
		JobContext * job;
		DepLink * next; // singly-linked list
	};

	class Task : public tbb::task
	{
		// The job that owns this task.
		JobContext * job;

		// The job callback and its (opaque) data pointer
		t_fp_Oodle_Job * fp_job;
		void * job_data;

	public:
		Task(JobContext * job, t_fp_Oodle_Job * fp_job, void * job_data)
			: job(job), fp_job(fp_job), job_data(job_data)
		{
		}

		virtual tbb::task * execute();
	};

	// Root task for WaitJob (this is the parent of the actual work task)
	tbb::task * root_task;
	// The actual child task we spawn
	tbb::task * child_task;

	// Dependency slots. Not actually our data, it belongs to the jobs we depend
	// on, which use it to store a linked list of their dependents.
	DepLink dep_slot[OODLE_JOB_MAX_DEPENDENCIES];

	// Dependency tracking information
	tbb::spin_mutex dependents_lock;
	bool job_completed;
	DepLink * dependents_head;
	DepLink * * dependents_tail_next;
	// ---- end data protected by dependents_lock

	// continue_with is the next task to run after this one, or NULL
	// if we don't have a viable candidate yet.
	//
	// Returns the new continue_with.
	tbb::task * notify_dep_satisfied(tbb::task * continue_with, int num_deps=1)
	{
		// Once its oustanding ref count reaches zero, spawn the child task.
		if (child_task->add_ref_count(-num_deps) == 0)
		{
			// If we didn't have a viable candidate for continue_with, now we do.
			if (!continue_with)
				return child_task;
			else
				tbb::task::spawn(*child_task);
		}

		return continue_with;
	}

	// Returns next task to run
	tbb::task * notify_completion()
	{
		// When the task completes, we need to stop accepting new dependencies
		// and grab our current dependency list. The only thing happening inside
		// the mutex is grabbing the dependencies; do the actual spawning outside.
		DepLink * dep;
		{
			tbb::spin_mutex::scoped_lock guard(dependents_lock);
			job_completed = true;
			dep = dependents_head;
		}

		// Spawn the dependent jobs. The first dependency, we don't actually
		// spawn(), but rather return from execute() (so it immediately gets
		// scheduled on the calling thread).
		tbb::task * continue_with = 0;
		while(dep)
		{
			DepLink * depnext = dep->next;
			continue_with = dep->job->notify_dep_satisfied(continue_with);
			// dep is in child, it could be deleted now
			dep = depnext;
		}

		return continue_with;
	}

	// Returns 1 if the job was already completed before we set up the
	// dependency edge.
	int add_dependent(JobContext * which, int slot)
	{
		// Set up the dependency record
		DepLink * dep = &which->dep_slot[slot];
		dep->job = which;
		dep->next = 0;

		tbb::spin_mutex::scoped_lock guard(dependents_lock);
		if (!job_completed)
		{
			// Insert the new dependent at the end. (We maintain the list in FIFO order
			// so we unblock the longest-waiting dependent first.)
			*dependents_tail_next = dep;
			dependents_tail_next = &dep->next;
			return 0;
		}
		else
			return 1;
	}

public:
	JobContext(t_fp_Oodle_Job * fp_job, void * job_data, U64 * dependencies, int num_dependencies, tbb::task_group_context *group_ctx)
	{
		job_completed = false;
		dependents_head = 0;
		dependents_tail_next = &dependents_head;

		// Set up the root TBB task; this task is empty, it's just a handle for us to use
		// for bookkeeping.
		if (group_ctx)
			root_task = new( tbb::task::allocate_root(*group_ctx) ) tbb::empty_task;
		else
			root_task = new( tbb::task::allocate_root() ) tbb::empty_task;
		root_task->set_ref_count(2); // 1 for child, 1 for wait
		
		// Set up the child task that does the actual work
		child_task = new( root_task->allocate_child() ) Task(this, fp_job, job_data);

		// We use the child_task's ref_count to count the number of outstanding dependencies
		// before it may launch. Note that this is different from the way TBB normally
		// uses ref counts: they usually count outstanding jobs before tasks may join,
		// not before they may fork. (It's idiomatic for graphs though, see the "General
		// Acyclic Graphs of Tasks" example.)
		//
		// The extra dependency is on us completing the setup; until the job has finished
		// setting up, the child task may not launch.
		child_task->set_ref_count(num_dependencies + 1);

		// Number of dependencies to release when we finish setup. This is initially just 1
		// but will be higher if we notice during dependency setup that some of the jobs
		// we depend on have already completed.
		int release_count = 1;

		// Add our dependencies
		for (int i = 0; i < num_dependencies; i++)
			release_count += reinterpret_cast<JobContext *>(dependencies[i])->add_dependent(this, i);

		// Release our setup dependency, along with dependencies on jobs that had already
		// completed when we ran add_dependent. We pass "child_task" as continue_with to
		// signal that we don't want to elide tbb::task::spawn calls.
		notify_dep_satisfied(child_task, release_count);
	}

	// This has the same semantics as WaitJob(), i.e. you only get to do this once.
	void wait()
	{
		// This waits for the child task (the actual work) to be completed and then frees
		// the root task. The child task got freed when the work was actually done, whenever
		// that was.
		root_task->wait_for_all();

		// The root task was only there for us to do the wait; that's done now!
		tbb::task::destroy(*root_task);

		child_task = 0;
		root_task = 0;
	}
};

tbb::task * JobContext::Task::execute()
{
	fp_job(job_data);
	return job->notify_completion();
}

//================================

void * g_example_jobify_jobifyUserPtr = NULL;

static U64 OODLE_CALLBACK tbb_run_job(t_fp_Oodle_Job * fp_job, void * job_data, U64 * dependencies, int num_dependencies, void * user_ptr)
{
	assert(num_dependencies <= OODLE_JOB_MAX_DEPENDENCIES);

	if ( user_ptr == NULL )
	{
		// someone invoked a Jobify'ed Oodle call but didn't pass the user pointer
		// in this example, we'll make it still work by using a global pointer
		// you could also choose to fail or assert here
		//	if you want to enforce in your code that the correct user pointer is always used
		printf("example_jobify : user_ptr was not passed through Jobified call; using global.\n");

		user_ptr = g_example_jobify_jobifyUserPtr;
		
		#if 0
		// alternatively you could run synchronously in this case
		// dependencies must all be done already
		(*fp_job)(job_data);
		// no handle :
		return 0;
		#endif
	}
	
	// Create the new job
	tbb::task_group_context * group_ctx = static_cast<tbb::task_group_context *>(user_ptr);
	assert( group_ctx != NULL );
		
	JobContext * ctx = new JobContext(fp_job, job_data, dependencies, num_dependencies, group_ctx);

	// Return our context pointer as the handle
	return reinterpret_cast<U64>(ctx);
}

static void OODLE_CALLBACK tbb_wait_job(U64 job_handle, void * /*user_ptr*/)
{
	JobContext * job = reinterpret_cast<JobContext *>(job_handle);
	job->wait();
	delete job;
}

void * example_jobify_init()
{
	//tbb::task_scheduler_init tbb_init;	// Automatic number of threads
	int tbb_threads = tbb::task_scheduler_init::default_num_threads();
  
	// Run the job system test.
	//Oodle_JobSystemStressTest(tbb_run_job, tbb_wait_job, NULL, 0, 256, 50000);

	int target_parallelism = tbb_threads; // in tbb this is logical processor count
	
	// Install TBB as our job system.
	//OodleCore_Plugins_SetJobSystemAndCount(tbb_run_job, tbb_wait_job, target_parallelism);
	
	example_jobify_run_job_fptr = tbb_run_job;
	example_jobify_wait_job_fptr = tbb_wait_job;
	example_jobify_target_parallelism = tbb_threads; // in tbb this is logical processor count

	// Set up our task group context to use for spawning jobs, with default mode
	// (in particular, no "concurrent wait")
	tbb::task_group_context * oodle_group_ctx = new tbb::task_group_context;
	void * jobifyUserPtr = oodle_group_ctx;
	
	g_example_jobify_jobifyUserPtr = jobifyUserPtr;
	
	return jobifyUserPtr;
}

#endif

