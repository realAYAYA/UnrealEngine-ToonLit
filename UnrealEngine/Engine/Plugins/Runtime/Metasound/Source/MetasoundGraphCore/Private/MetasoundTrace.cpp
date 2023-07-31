// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundTrace.h"

#include "HAL/LowLevelMemStats.h"
#include "HAL/LowLevelMemTracker.h"

DECLARE_LLM_MEMORY_STAT(TEXT("MetaSound"), STAT_MetaSoundLLM, STATGROUP_LLMFULL);

LLM_DEFINE_TAG(Audio_MetaSound, TEXT("MetaSound"), TEXT("Audio"), GET_STATFNAME(STAT_MetaSoundLLM), GET_STATFNAME(STAT_AudioSummaryLLM));
