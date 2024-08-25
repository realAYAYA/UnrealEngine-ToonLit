// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"

struct FConcertPropertyChain;

namespace UE::ConcertClientSharedSlate
{
	DECLARE_DELEGATE_TwoParams(FExtendProperties, const FSoftObjectPath& Object, TArray<FConcertPropertyChain>& InOutPropertiesToAdd);
}