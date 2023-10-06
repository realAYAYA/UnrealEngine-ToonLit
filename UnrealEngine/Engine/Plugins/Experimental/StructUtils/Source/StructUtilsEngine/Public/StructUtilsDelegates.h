// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"

class UUserDefinedStruct;

namespace UE::StructUtils::Delegates
{
#if WITH_EDITOR
	/** Called after the FInstancedStructs has been reinstantiated. E.g. safe to update UI. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUserDefinedStructReinstanced, const UUserDefinedStruct& /*UserDefinedStruct*/);
	extern STRUCTUTILSENGINE_API FOnUserDefinedStructReinstanced OnUserDefinedStructReinstanced;
#endif
} // UE::StructUtils::Delegates
