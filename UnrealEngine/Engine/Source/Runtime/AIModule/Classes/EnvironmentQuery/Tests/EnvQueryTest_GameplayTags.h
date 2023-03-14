// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_GameplayTags.generated.h"

class IGameplayTagAssetInterface;

/** 
 * EnvQueryTest_GameplayTags attempts to cast items to IGameplayTagAssetInterface and test their tags with TagQueryToMatch.
 * The behavior of IGameplayTagAssetInterface-less items is configured by bRejectIncompatibleItems.
 */
UCLASS(MinimalAPI)
class UEnvQueryTest_GameplayTags : public UEnvQueryTest
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * @note Calling function only makes sense before first run of given query
	 * by the EQS manager. The query gets preprocessed and cached then so the query 
	 * value will get stored and calling this function will not change it (unless 
	 * you call it on the cached test's instance, see UEnvQueryManager::CreateQueryInstance).
	 */
	void SetTagQueryToMatch(FGameplayTagQuery& GameplayTagQuery);

protected:
	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionDetails() const override;

	bool SatisfiesTest(const IGameplayTagAssetInterface* ItemGameplayTagAssetInterface) const;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	/**
	 * Presave function. Gets called once before an object gets serialized for saving. This function is necessary
	 * for save time computation as Serialize gets called three times per object from within SavePackage.
	 *
	 * @warning: Objects created from within PreSave will NOT have PreSave called on them!!!
	 */
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

	virtual void PostLoad() override;

	UPROPERTY(EditAnywhere, Category=GameplayTagCheck)
	FGameplayTagQuery TagQueryToMatch;

	// This controls how to treat actors that do not implement IGameplayTagAssetInterface.
	// When set to True, actors that do not implement the interface will be ignored, meaning
	// they will not be scored and will not be considered when filtering.
	// When set to False, actors that do not implement the interface will be included in
	// filter and score operations with a zero score.
	UPROPERTY(EditAnywhere, Category=GameplayTagCheck)
	bool bRejectIncompatibleItems;

	// Used to determine whether the file format needs to be updated to move data into TagQueryToMatch or not.
	UPROPERTY()
	bool bUpdatedToUseQuery;

	// Deprecated property.  Used only to load old data into TagQueryToMatch.
	UPROPERTY()
	EGameplayContainerMatchType TagsToMatch;

	// Deprecated property.  Used only to load old data into TagQueryToMatch.
	UPROPERTY()
	FGameplayTagContainer GameplayTags;
};
