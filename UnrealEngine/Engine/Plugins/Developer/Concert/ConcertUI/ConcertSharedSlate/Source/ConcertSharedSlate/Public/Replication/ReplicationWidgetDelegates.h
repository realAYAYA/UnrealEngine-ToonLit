// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"

class FMenuBuilder;
struct FSoftObjectPath;

namespace UE::ConcertSharedSlate
{
	class FReplicatedPropertyData;

	/** A predicate for determining Left < Right. */
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FSortPropertyPredicate, const FReplicatedPropertyData& Left, const FReplicatedPropertyData& Right);

	/** Extends a context menu that is being built for a select of objects. */
	DECLARE_DELEGATE_TwoParams(FExtendObjectMenu, FMenuBuilder&, TConstArrayView<FSoftObjectPath> ContextObjects);
}