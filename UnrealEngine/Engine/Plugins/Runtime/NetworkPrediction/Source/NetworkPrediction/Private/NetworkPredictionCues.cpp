// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPredictionCues.h"

DEFINE_LOG_CATEGORY(LogNetworkPredictionCues);

FGlobalCueTypeTable FGlobalCueTypeTable::Singleton;

FGlobalCueTypeTable::FRegisteredCueTypeInfo& FGlobalCueTypeTable::GetRegistedTypeInfo()
{
	static FGlobalCueTypeTable::FRegisteredCueTypeInfo PendingCueTypes;
	return PendingCueTypes;
}
