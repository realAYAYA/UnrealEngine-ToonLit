// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Elements/PCGActorSelector.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "PCGGetActorProperty.generated.h"

class UActorComponent;

/**
* Extract a property value from an actor/component into a ParamData.
*/
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGGetActorPropertySettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetActorProperty")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGPropertyToParamDataSettings", "NodeTitle", "Get Actor Property"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual void GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
	virtual bool CanDynamicallyTrackKeys() const override { return true; }
#endif

	virtual FString GetAdditionalTitleInformation() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }

protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override { return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic; }
#endif
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
	FPCGActorSelectorSettings ActorSelector;

	/** Allow to look for an actor component instead of an actor. It will need to be attached to the found actor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bSelectComponent = false;

	/** If we are looking for an actor component, the class can be specified here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bSelectComponent", EditConditionHides))
	TSubclassOf<UActorComponent> ComponentClass;

	/** Property name to extract. Can only extract properties that are compatible with metadata types. If None, extract the actor/component directly.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName PropertyName = NAME_None;

	/** If the property is a struct/object supported by metadata, this option can be toggled to force extracting all (compatible) properties contained in this property. Automatically true if unsupported by metadata. For now, only supports direct child properties (and not deeper). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bForceObjectAndStructExtraction = false;

	/** By default, attribute name will be None, but it can be overridden by this name. Use @SourceName to use the property name (only works when not extracting). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bForceObjectAndStructExtraction", EditConditionHides))
	FName OutputAttributeName = NAME_None;

	/** If this is true, we will never put this element in cache, and will always try to re-query the actors and read the latest properties from them. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAlwaysRequeryActors = true;

#if WITH_EDITORONLY_DATA
	/** If this is checked, found actors that are outside component bounds will not trigger a refresh. Only works for tags for now in editor. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings)
	bool bTrackActorsOnlyWithinBounds = false;
#endif // WITH_EDITORONLY_DATA

private:
	UPROPERTY()
	EPCGActorSelection ActorSelection_DEPRECATED;

	UPROPERTY()
	FName ActorSelectionTag_DEPRECATED;

	UPROPERTY()
	FName ActorSelectionName_DEPRECATED;

	UPROPERTY()
	TSubclassOf<AActor> ActorSelectionClass_DEPRECATED;

	UPROPERTY()
	EPCGActorFilter ActorFilter_DEPRECATED = EPCGActorFilter::Self;

	UPROPERTY()
	bool bIncludeChildren_DEPRECATED = false;
};

class FPCGGetActorPropertyElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return !CastChecked<UPCGGetActorPropertySettings>(InSettings)->bAlwaysRequeryActors; }
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
