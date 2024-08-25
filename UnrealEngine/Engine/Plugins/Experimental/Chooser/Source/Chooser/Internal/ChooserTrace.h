// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Trace/Config.h"
#include "TraceFilter.h"
#include "Misc/Variant.h"
#include "IObjectChooser.h"
#include "Serialization/BufferArchive.h"

#if CHOOSER_TRACE_ENABLED

class UObject;

struct FChooserEvaluationContext;

struct FChooserTrace
{
	CHOOSER_API static void OutputChooserEvaluation(const UObject* ChooserAsset, const FChooserEvaluationContext& Context, uint32 SelectedIndex);
	
	CHOOSER_API static void OutputChooserValueArchive(const FChooserEvaluationContext& Context, const TCHAR* Key, const FBufferArchive& ValueArchive);
	
	template<typename T>
	CHOOSER_API static void OutputChooserValue(const FChooserEvaluationContext& Context, const TCHAR* Key, const T& Value)
	{
		FBufferArchive Archive;
		Archive << const_cast<T&>(Value);
		OutputChooserValueArchive(Context,Key,Archive);
	}
};

#define TRACE_CHOOSER_EVALUATION(Chooser, Context, SelectedIndex) \
	FChooserTrace::OutputChooserEvaluation(Chooser, Context, SelectedIndex)
	
#define TRACE_CHOOSER_VALUE(Context, Key, Value) \
	FChooserTrace::OutputChooserValue(Context, Key, Value);
#else

#define TRACE_CHOOSER_EVALUATION(...)
#define TRACE_CHOOSER_VALUE(...)

#endif