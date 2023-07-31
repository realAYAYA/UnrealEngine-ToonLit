// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RewindDebuggerTrack.h"
#include "IRewindDebuggerTrackCreator.h"

namespace RewindDebugger
{
	class FRewindDebuggerTrackCreators
	{
	public:
		static void EnumerateCreators(TFunctionRef<void(const IRewindDebuggerTrackCreator*)> Callback);
		static const IRewindDebuggerTrackCreator* GetCreator(FName CreatorName);
	};
}