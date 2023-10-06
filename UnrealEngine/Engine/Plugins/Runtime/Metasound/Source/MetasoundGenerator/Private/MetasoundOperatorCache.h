// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "MetasoundOperatorInterface.h"
#include "Misc/Guid.h"
#include "HAL/CriticalSection.h"
#include "Templates/UniquePtr.h"

namespace Metasound
{
	struct FOperatorCacheSettings
	{
		uint32 MaxNumOperators = 64;
	};

	struct FOperatorAndInputs
	{
		TUniquePtr<IOperator> Operator;
		FInputVertexInterfaceData Inputs;
	};

	class FOperatorCache
	{
	public:

		FOperatorCache(const FOperatorCacheSettings& InSettings);
		FOperatorAndInputs ClaimCachedOperator(const FGuid& InOperatorID);
		void AddOperatorToCache(const FGuid& InOperatorID, TUniquePtr<IOperator>&& InOperator, FInputVertexInterfaceData&& InputData);
		void RemoveOperatorsWithID(const FGuid& InOperatorID);
		void SetMaxNumOperators(uint32 InMaxNumOperators);

	private:

		void TrimCache();

		FOperatorCacheSettings Settings;
		FCriticalSection CriticalSection;
		TMap<FGuid, TArray<FOperatorAndInputs>> Operators;
		TArray<FGuid> Stack;
	};
}

