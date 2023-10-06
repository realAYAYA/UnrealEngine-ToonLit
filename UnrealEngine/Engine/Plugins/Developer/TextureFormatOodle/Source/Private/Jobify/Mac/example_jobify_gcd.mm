// Copyright Epic Games, Inc. All Rights Reserved.
/*

  Oodle pluggable job system example using NSOperationQueue on OS X (part of the Foundation framework),
  which in turn builds on top of GCD (Grand Central Dispatch).

  NSOperationQueue and NSOperations have direct support for DAGs of jobs. Therefore, the only thing
  this implementation has to do is create the relevant operation objects and set up dependencies.

  One important caveat is that a OodleLZ_Compress call using jobify callbacks will complete
  synchronously, which includes waiting for its spawned jobs to finish. That means that if you
  call OodleLZ_Compress on a GCD/worker thread, it will perform a blocking wait on it. GCD will
  run more threads than the number of logical cores, but it won't keep adding threads indefinitely
  (a typical limit seems to be 64 worker threads); therefore it is possible for an application to
  deadlock if there are (say) 64 worker threads all running OodleLZ_Compress and waiting for a child
  job to complete, with no worker thread actually running say child job. If you are concerned about this,
  your options are:

  1. Disable Oodle jobs. If you have enough independent parallel work to keep 64 worker threads
     busy, you presumably don't need the extra parallelism. OodleLZ_Compress calls with jobs
	 disabled contain no blocking waits and will always complete with no further intervention.
  2. Throttle the number of in-flight OodleLZ_Compress calls. Having more compressors running
     than logical cores available has no benefit to begin with.
  3. Use the OodleX thread pool. The OodleX thread pool will not actually block a thread waiting
     for child jobs; it will have blocked threads run other work instead until the work they
	 depend on has been completed.

*/

#include "example_jobify.h"
#include "ooex.h" // for platform detection

#ifdef OOEX_PLATFORM_MAC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#import <Foundation/Foundation.h>


// ---- Implementation of the job callbacks using GCD (well, NSOperation)

static void * g_example_jobify_jobifyUserPtr = NULL;

static OO_U64 OODLE_CALLBACK gcd_run_job(t_fp_Oodle_Job * fp_job, void * job_data, OO_U64 * dependencies, int num_dependencies, void * user_ptr)
{
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

	NSBlockOperation * op = [NSBlockOperation blockOperationWithBlock: ^{
		fp_job(job_data);
	}];
	[op retain]; // Need to do this manually since we'll cast it to a OO_U64 which ARC doesn't track

	for (int i = 0; i < num_dependencies; i++)
		[op addDependency:reinterpret_cast<NSOperation *>(dependencies[i])];

	NSOperationQueue * queue = reinterpret_cast<NSOperationQueue*>(user_ptr);
	[queue addOperation: op];

	// Our block operation is the handle
	return reinterpret_cast<OO_U64>(op);
}

static void OODLE_CALLBACK gcd_wait_job(OO_U64 job_handle, void * /*user_ptr*/)
{
	NSBlockOperation * op = reinterpret_cast<NSBlockOperation *>(job_handle);

	// Wait for the outstanding job to finish
	[op waitUntilFinished];

	// Then release our reference on it
	[op release];
}

extern "C"
void * example_jobify_init_gcd();

#if 0
int mac_get_core_count()
{
	//int	count;
	//size_t count_len = sizeof(count);
	//sysctlbyname("hw.logicalcpu", &count, &count_len, NULL, 0);
	//sysctlbyname("hw.physicalcpu", &count, &count_len, NULL, 0);

    int nm[2];
    size_t len = 4;
    uint32_t count;

    nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);

    if(count < 1) {
        nm[1] = HW_NCPU;
        sysctl(nm, 2, &count, &len, NULL, 0);
        if(count < 1) { count = 1; }
    }
    return count;
}
#endif

void * example_jobify_init_gcd()
{
	// Allocate our operation queue
	NSOperationQueue * queue = [[NSOperationQueue alloc] init];

	int target_parallelism = 0; // physical core count unknown, let Oodle choose
	// std::thread::hardware_concurrency(); ?
	// logicalcpucount = sysconf( _SC_NPROCESSORS_ONLN ); ?

	// Install GCD as our job system.
	//OodleCore_Plugins_SetJobSystemAndCount(gcd_run_job, gcd_wait_job, target_parallelism);

	example_jobify_run_job_fptr = gcd_run_job;
	example_jobify_wait_job_fptr = gcd_wait_job;
	example_jobify_target_parallelism = target_parallelism;

	// Pass the NSOperationQueue to use as our user pointer
	void * jobifyUserPtr = queue;

	// save to global :
  	g_example_jobify_jobifyUserPtr = jobifyUserPtr;
  	
	return jobifyUserPtr;
}

#endif

