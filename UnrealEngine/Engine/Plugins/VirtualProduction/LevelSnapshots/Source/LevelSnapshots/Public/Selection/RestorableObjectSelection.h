// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "AddedAndRemovedComponentInfo.h"
#include "PropertySelection.h"

class AActor;

struct FPropertySelectionMap;

namespace UE::LevelSnapshots
{
	struct FCustomSubobjectRestorationInfo;
	
	class LEVELSNAPSHOTS_API FRestorableObjectSelection
	{
		friend FPropertySelectionMap; 

		const FSoftObjectPath ObjectPath;
		const FPropertySelectionMap& Owner;

		FRestorableObjectSelection(const FSoftObjectPath& ObjectPath, const FPropertySelectionMap& Owner)
			:
			ObjectPath(ObjectPath),
			Owner(Owner)
		{}

		public:
		
		const FPropertySelection* GetPropertySelection() const;
		const FAddedAndRemovedComponentInfo* GetComponentSelection() const;
		const FCustomSubobjectRestorationInfo* GetCustomSubobjectSelection() const;
	};
}