// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeatureAction.h"

#include "GameFeatureAction_AddWPContent.generated.h"

class UGameFeatureData;
class IPlugin;
class FContentBundleClient;
class UContentBundleDescriptor;

/**
 *
 */
UCLASS(meta = (DisplayName = "Add World Partition Content (Content Bundle)"))
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

	const UContentBundleDescriptor* GetContentBundleDescriptor() const { return ContentBundleDescriptor; }

private:
	UPROPERTY(VisibleAnywhere, Category = ContentBundle)
	TObjectPtr<UContentBundleDescriptor> ContentBundleDescriptor;

	TSharedPtr<FContentBundleClient> ContentBundleClient;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
