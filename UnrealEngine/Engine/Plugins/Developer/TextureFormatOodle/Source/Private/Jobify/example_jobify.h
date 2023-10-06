// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// use only oodle2base so we work with core/net/tex :
#include "../include/oodle2base.h"

// returns void * jobifyUserPtr you must pass back
void * example_jobify_init();

OODEFFUNC typedef void (OODLE_CALLBACK t_fp_example_jobify_Job)( void * job_data );
OODEFFUNC typedef OO_U64 (OODLE_CALLBACK t_fp_example_jobify_RunJob)( t_fp_example_jobify_Job * fp_job, void * job_data , OO_U64 * dependencies, int num_dependencies, void * user_ptr );
OODEFFUNC typedef void (OODLE_CALLBACK t_fp_example_jobify_WaitJob)( OO_U64 job_handle, void * user_ptr );


extern t_fp_example_jobify_RunJob * example_jobify_run_job_fptr;
extern t_fp_example_jobify_WaitJob *example_jobify_wait_job_fptr;
extern int example_jobify_target_parallelism;

