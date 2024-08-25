/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef PAS_SCAVENGER_UE_H
#define PAS_SCAVENGER_UE_H

#ifdef __cplusplus
extern "C" {
#endif

PAS_API extern unsigned pas_scavenger_should_suspend_count;

PAS_API void pas_scavenger_suspend(void);
PAS_API void pas_scavenger_resume(void);

PAS_API void pas_scavenger_clear_local_tlcs(void);
PAS_API void pas_scavenger_do_everything_except_remote_tlcs(void);
PAS_API void pas_scavenger_run_synchronously_now(void);

#ifdef __cplusplus
}
#endif

#endif /* PAS_SCAVENGER_UE_H */

