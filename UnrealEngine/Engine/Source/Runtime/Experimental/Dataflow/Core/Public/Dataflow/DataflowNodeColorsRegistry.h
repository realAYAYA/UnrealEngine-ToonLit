// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowGraph.h"
#include "Dataflow/DataflowSettings.h"
#include "ChaosLog.h"

struct FDataflowNode;
struct FDataflowConnection;
	
class FLazySingleton;

namespace Dataflow
{
	//
	//
	//
	class FNodeColorsRegistry
	{
	public:
		static DATAFLOWCORE_API FNodeColorsRegistry& Get();
		static DATAFLOWCORE_API void TearDown();

		DATAFLOWCORE_API void RegisterNodeColors(const FName& Category, const FNodeColors& NodeColors);
		DATAFLOWCORE_API FLinearColor GetNodeTitleColor(const FName& Category);
		DATAFLOWCORE_API FLinearColor GetNodeBodyTintColor(const FName& Category);
		DATAFLOWCORE_API void NodeColorsChangedInSettings(const FNodeColorsMap& NodeColorsMap);

	private:
		DATAFLOWCORE_API FNodeColorsRegistry();
		DATAFLOWCORE_API ~FNodeColorsRegistry();

		TMap<FName, FNodeColors > ColorsMap;					// [Category] -> Colors
		FDelegateHandle DataflowSettingsChangedDelegateHandle;

		friend FLazySingleton;
	};

}
