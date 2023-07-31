// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRewindDebuggerView.h"
#include "IRewindDebuggerViewCreator.h"

namespace TraceServices
{
	class IRewindDebuggerViewCreator;
}

class FRewindDebuggerViewCreators
{
public:
	static void EnumerateCreators(TFunctionRef<void(const IRewindDebuggerViewCreator*)> Callback);
	static const IRewindDebuggerViewCreator* GetCreator(FName CreatorName);
};
