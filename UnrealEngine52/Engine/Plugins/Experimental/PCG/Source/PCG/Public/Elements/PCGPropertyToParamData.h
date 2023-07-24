// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGPin.h"
#include "PCGSettings.h"
#include "Elements/PCGActorSelector.h"

#include "PCGPropertyToParamData.generated.h"

class UActorComponent;

/**
* Extract a property value from an actor/component into a ParamData.
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGPropertyToParamDataSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetActorProperty")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGPropertyToParamDataSettings", "NodeTitle", "Get Actor Property"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual void GetTrackedActorTags(FPCGTagToSettingsMap& OutTagToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }

protected:
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

	/** Property name to extract. Can only extract properties that are compatible with metadata types. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName PropertyName = NAME_None;

	/** If the property is a struct/object unsupported by metadata, this option can be toggled to extract all (compatible) properties contained in this property. For now, only supports direct child properties (and not deeper). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bExtractObjectAndStruct = false;

	/** By default, attribute name will be the property name, but it can be overridden by this name. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bExtractObjectAndStruct", EditConditionHides))
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

class FPCGPropertyToParamDataElement : public FSimplePCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return !CastChecked<UPCGPropertyToParamDataSettings>(InSettings)->bAlwaysRequeryActors; }
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const;
};
