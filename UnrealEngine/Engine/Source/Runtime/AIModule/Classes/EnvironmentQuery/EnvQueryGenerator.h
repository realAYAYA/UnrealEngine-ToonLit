// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "EnvironmentQuery/Items/EnvQueryItemType.h"
#include "EnvironmentQuery/EnvQueryNode.h"
#include "EnvQueryGenerator.generated.h"

namespace EnvQueryGeneratorVersion
{
	inline const int32 Initial = 0;
	inline const int32 DataProviders = 1;

	inline const int32 Latest = DataProviders;
}

UCLASS(EditInlineNew, Abstract, meta = (Category = "Generators"), MinimalAPI)
class UEnvQueryGenerator : public UEnvQueryNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, Category=Option)
	FString OptionName;

	/** type of generated items */
	UPROPERTY()
	TSubclassOf<UEnvQueryItemType> ItemType;

	/** if set, tests will be automatically sorted for best performance before running query */
	UPROPERTY(EditDefaultsOnly, Category = Option, AdvancedDisplay)
	uint32 bAutoSortTests : 1;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const { checkNoEntry(); }
	virtual bool IsValidGenerator() const { return ItemType != nullptr; }

	AIMODULE_API virtual void PostLoad() override;
	AIMODULE_API void UpdateNodeVersion() override;
};
