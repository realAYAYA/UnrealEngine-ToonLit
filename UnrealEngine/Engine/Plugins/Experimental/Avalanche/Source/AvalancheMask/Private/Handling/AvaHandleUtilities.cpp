// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handling/AvaHandleUtilities.h"

#include "Engine/Engine.h"
#include "Handling/AvaObjectHandleSubsystem.h"

namespace UE::Ava::Internal
{
	UAvaObjectHandleSubsystem* GetObjectHandleSubsystem()
	{
		return GEngine->GetEngineSubsystem<UAvaObjectHandleSubsystem>();
	}
}
