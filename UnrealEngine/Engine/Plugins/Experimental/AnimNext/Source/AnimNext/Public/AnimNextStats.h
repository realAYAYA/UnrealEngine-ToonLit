// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats/Stats2.h"

DECLARE_STATS_GROUP(TEXT("AnimNext"), STATGROUP_AnimNext, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Initialize Entry"), STAT_AnimNext_InitializeEntry, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Create Instance Data"), STAT_AnimNext_CreateInstanceData, STATGROUP_AnimNext, ANIMNEXT_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext Task: Graph"), STAT_AnimNext_Task_Graph, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext Task: External Params"), STAT_AnimNext_Task_ExternalParams, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext Task: Port"), STAT_AnimNext_Task_Port, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext Task: Scope Entry"), STAT_AnimNext_Task_ScopeEntry, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext Task: Scope Exit"), STAT_AnimNext_Task_ScopeExit, STATGROUP_AnimNext, ANIMNEXT_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Allocate Graph Instance"), STAT_AnimNext_Graph_AllocateInstance, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Graph RigVM"), STAT_AnimNext_Graph_RigVM, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Update Graph"), STAT_AnimNext_UpdateGraph, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Evaluate Graph"), STAT_AnimNext_EvaluateGraph, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Execute Evaluation Program"), STAT_AnimNext_EvaluationProgram_Execute, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Copy Transforms (SoA)"), STAT_AnimNext_CopyTransforms_SoA, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Normalize Rotations (SoA)"), STAT_AnimNext_NormalizeRotations_SoA, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Blend Overwrite (SoA)"), STAT_AnimNext_BlendOverwrite_SoA, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Blend Accumulate (SoA)"), STAT_AnimNext_BlendAccumulate_SoA, STATGROUP_AnimNext, ANIMNEXT_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Param Type Handle Lock"), STAT_AnimNext_ParamTypeHandle_Lock, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Param Stack Get Param"), STAT_AnimNext_ParamStack_GetParam, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Param Stack Adapter"), STAT_AnimNext_ParamStack_Adapter, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Param Stack Coalesce"), STAT_AnimNext_ParamStack_Coalesce, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Param Stack Decoalesce"), STAT_AnimNext_ParamStack_Decoalesce, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Run Param Block"), STAT_AnimNext_ParamBlock_UpdateLayer, STATGROUP_AnimNext, ANIMNEXT_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Generate Reference Pose"), STAT_AnimNext_GenerateReferencePose, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Remap Pose"), STAT_AnimNext_RemapPose, STATGROUP_AnimNext, ANIMNEXT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Convert Local Space To Component Space"), STAT_AnimNext_ConvertLocalSpaceToComponentSpace, STATGROUP_AnimNext, ANIMNEXT_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimNext: Skeletal Mesh Component Port"), STAT_AnimNext_Port_SkeletalMeshComponent, STATGROUP_AnimNext, ANIMNEXT_API);
