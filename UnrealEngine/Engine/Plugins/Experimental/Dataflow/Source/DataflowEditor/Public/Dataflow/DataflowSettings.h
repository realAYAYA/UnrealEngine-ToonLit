// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"

#include "DataflowSettings.generated.h"

UCLASS(config = Dataflow, defaultconfig, meta = (DisplayName = "Dataflow"))
class DATAFLOWEDITOR_API UDataflowSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	/** TArray<> pin type color. The other pin colors are defined in the general editor settings. */
	UPROPERTY(config, EditAnywhere, Category = PinColors)
	FLinearColor ArrayPinTypeColor;

	/** FManagedArrayCollection pin type color. The other pin colors are defined in the general editor settings. */
	UPROPERTY(config, EditAnywhere, Category = PinColors)
	FLinearColor ManagedArrayCollectionPinTypeColor;

	/** FBox pin type color. The other pin colors are defined in the general editor settings. */
	UPROPERTY(config, EditAnywhere, Category = PinColors)
	FLinearColor BoxPinTypeColor;

	/** GeometryCollection category NodeTitle color. */
	UPROPERTY(config, EditAnywhere, Category = NodeColors)
	FLinearColor GeometryCollectionCategoryNodeTitleColor;

	/** GeometryCollection category NodeBodyTint color. */
	UPROPERTY(config, EditAnywhere, Category = NodeColors)
	FLinearColor GeometryCollectionCategoryNodeBodyTintColor;

	// Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override;
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
#endif
	// END UDeveloperSettings Interface

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDataflowSettingsChanged, const FName&, const UDataflowSettings*);

	/** Gets a multicast delegate which is called whenever one of the parameters in this settings object changes. */
	static FOnDataflowSettingsChanged& OnSettingsChanged();

protected:
	static FOnDataflowSettingsChanged SettingsChangedDelegate;

#endif
};
