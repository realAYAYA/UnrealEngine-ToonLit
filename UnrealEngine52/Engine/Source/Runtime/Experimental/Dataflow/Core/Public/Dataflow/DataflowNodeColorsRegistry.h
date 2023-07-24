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
	class DATAFLOWCORE_API FNodeColorsRegistry
	{
	public:
		static FNodeColorsRegistry& Get();
		static void TearDown();

		void RegisterNodeColors(const FName& Category, const FNodeColors& NodeColors);
		FLinearColor GetNodeTitleColor(const FName& Category);
		FLinearColor GetNodeBodyTintColor(const FName& Category);
		void NodeColorsChangedInSettings(const FNodeColorsMap& NodeColorsMap);

	private:
		FNodeColorsRegistry();
		~FNodeColorsRegistry();

		TMap<FName, FNodeColors > ColorsMap;					// [Category] -> Colors
		FDelegateHandle DataflowSettingsChangedDelegateHandle;

		friend FLazySingleton;
	};

}
