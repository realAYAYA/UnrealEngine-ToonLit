// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"

class UClass;
struct FConcertPropertyChain;

namespace UE::ConcertReplicationScriptingEditor
{
	DECLARE_DELEGATE(FOnPressed);
	
	DECLARE_DELEGATE_OneParam(FOnClassChanged, const UClass* Class);
	DECLARE_DELEGATE_TwoParams(FOnSelectedPropertiesChanged, const FConcertPropertyChain& Property, bool bIsSelected);
}