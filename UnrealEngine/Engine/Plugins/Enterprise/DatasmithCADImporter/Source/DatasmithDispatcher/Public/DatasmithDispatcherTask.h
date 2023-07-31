// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CADData.h"

namespace DatasmithDispatcher
{
using ETaskState = CADLibrary::ECADParsingResult;

struct FTask
{
	FTask() = default;

	FTask(const CADLibrary::FFileDescriptor& InFile)
		: FileDescription(InFile)
		, State(ETaskState::UnTreated)
	{
	}

	CADLibrary::FFileDescriptor FileDescription;
	int32 Index = -1;
	ETaskState State = ETaskState::Unknown;
};

} // NS DatasmithDispatcher
