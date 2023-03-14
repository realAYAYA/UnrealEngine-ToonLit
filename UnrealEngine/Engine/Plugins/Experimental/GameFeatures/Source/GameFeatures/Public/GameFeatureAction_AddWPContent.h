// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"

#include "GameFeatureAction_AddWPContent.generated.h"

class UGameFeatureData;
class IPlugin;
class FContentBundleClient;
class UContentBundleDescriptor;

/**
 *
 */
UCLASS(meta = (DisplayName = "Add World Partition Content"))
class GAMEFEATURES_API UGameFeatureAction_AddWPContent : public UGameFeatureAction
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UGameFeatureAction interface
	virtual void OnGameFeatureRegistering() override;
	virtual void OnGameFeatureUnregistering() override;
	virtual void OnGameFeatureActivating() override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~ End UGameFeatureAction interface

	UGameFeatureData* GetGameFeatureData() const;

	const UContentBundleDescriptor* GetContentBundleDescriptor() const { return ContentBundleDescriptor; }

private:
	UPROPERTY(EditAnywhere, Category = ContentBundle)
	TObjectPtr<UContentBundleDescriptor> ContentBundleDescriptor;

	TSharedPtr<FContentBundleClient> ContentBundleClient;
};