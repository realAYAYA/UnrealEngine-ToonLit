// Copyright Epic Games, Inc. All Rights Reserved.

/****

example_jobify :

example implementations of the Oodle job system

to be plugged into OodleCore_Plugins_SetJobSystemAndCount
 OodleNet_Plugins_SetJobSystemAndCount

You likely have a worker thread system already and may wish to plug that in instead.

etc.

****/

/* 
@cdep pre

	$if($equals($BuildPlatform,linux),
	
		$// use TBB on linux if found :
		$if($exists(/usr/include/tbb),
			$addtocswitches(-DHAVE_TBB)
			$requiresbinary( -ltbb ),
			$addtocswitches(-DUSE_OODLEX)			
		)
		, )
   
	$if($equals($BuildPlatform,mac),
		$requires( $projectspath/oodle2/examples/example_jobify_gcd.mm )
		$requiresbinary(-framework Foundation)
		, )
*/

/* 
@cdep post

*/

#include "Jobify/example_jobify.h"
#ifdef USE_OODLEX
#include "../include/oodle2x.h"
#endif

#include "Jobify/ooex.h" // for platform detection

#include <stdlib.h> 
#include <stdio.h> 

//===================================

t_fp_example_jobify_RunJob * example_jobify_run_job_fptr = NULL;
t_fp_example_jobify_WaitJob *example_jobify_wait_job_fptr = NULL;
int example_jobify_target_parallelism = 0;

//===================================

#ifdef HAVE_TBB 
// enabled if /usr/include/tbb is found by cdep
// modern clang & gcc have "__has_include" , could use that instead
// HAVE_TBB is semi-standard name set by autoconf

#include "Jobify/example_jobify_linuxtbb.inl"

#elif defined(OOEX_PLATFORM_NT)

#include "Jobify/example_jobify_win32tp.inl"

#elif defined(OOEX_PLATFORM_MAC)

// in example_jobify_gcd.mm

extern "C"
void * example_jobify_init_gcd();

void * example_jobify_init()
{
	void * userPtr = example_jobify_init_gcd();
	
	return userPtr;
}

#else

// host tool platforms (Linux,Windows,Mac) should have been caught above
// here we are either on a non-host platform
// or Linux without TBB

#ifdef USE_OODLEX

// if you have OodleX, you can use it :

void * example_jobify_init()
{
	// OodleX_Init installs jobify plugins to Oodle Core
	
	OodleXInitOptions oo;
	OodleX_Init_GetDefaults(OODLE_HEADER_VERSION,&oo,
		OodleX_Init_GetDefaults_DebugSystems_No,
		OodleX_Init_GetDefaults_Threads_Yes),
	// Texture work likes to use all hyper threads :
	oo.m_OodleInit_Workers_Count = OODLE_WORKERS_COUNT_ALL_HYPER_CORES;
	if ( ! OodleX_Init(OODLE_HEADER_VERSION,&oo) )
	{
		fprintf(stderr,"OodleX_Init failed\n");
		exit(10);
	}
	
	// OodleX automatically installs itself to Oodle Core
	// but it doesn't do so to Oodle Net or others
	// you can manually do it thusly :
	
	/*	
	OodleNet_Plugins_SetPrintf(OodleXLog_Printf_Raw);
	OodleNet_Plugins_SetAssertion(OodleX_DisplayAssertion);
	OodleNet_Plugins_SetAllocators(OodleXMallocAligned,OodleXFree);
	OodleNet_Plugins_SetJobSystemAndCount(OodleX_CorePlugin_RunJob,OodleX_CorePlugin_WaitJob,OodleX_GetNumWorkerThreads());
	*/
	
	example_jobify_run_job_fptr = OodleX_CorePlugin_RunJob;
	example_jobify_wait_job_fptr = OodleX_CorePlugin_WaitJob;
	example_jobify_target_parallelism = OodleX_GetNumWorkerThreads();
	
	// no jobify user ptr
	
	return NULL;
}

#else

void * example_jobify_init()
{
	fprintf(stderr,"warning: no jobify.\n");
	
	example_jobify_run_job_fptr = NULL;
	example_jobify_wait_job_fptr = NULL;
	example_jobify_target_parallelism = 0;
	
	return NULL;
}

#endif

#endif
	
	
