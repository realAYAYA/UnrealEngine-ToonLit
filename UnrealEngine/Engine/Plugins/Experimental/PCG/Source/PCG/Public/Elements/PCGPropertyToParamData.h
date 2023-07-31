// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"

#include "Templates/SubclassOf.h"

#include "PCGPropertyToParamData.generated.h"

class AActor;
class UActorComponent;

UENUM()
enum class EPCGActorSelection : uint8
{
	ByTag,
	ByName,
	ByClass
};

UENUM()
enum class EPCGActorFilter : uint8
{
	Self,
	Parent,
	Root,
	AllWorldActors
	// TODO
	// TrackedActors
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGPropertyToParamDataSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PropertyToParamDataNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGActorSelection ActorSelection;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ActorSelection==EPCGActorSelection::ByTag", EditConditionHides))
	FName ActorSelectionTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ActorSelection==EPCGActorSelection::ByName", EditConditionHides))
	FName ActorSelectionName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ActorSelection==EPCGActorSelection::ByClass", EditConditionHides))
	TSubclassOf<AActor> ActorSelectionClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGActorFilter ActorFilter = EPCGActorFilter::Self;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ActorFilter!=EPCGActorFilter::AllWorldActors", EditConditionHides))
	bool bIncludeChildren = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bSelectComponent = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bSelectComponent", EditConditionHides))
	TSubclassOf<UActorComponent> ComponentClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName PropertyName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName OutputAttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAlwaysRequeryActors = true;
};

class FPCGPropertyToParamDataElement : public FSimplePCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return !CastChecked<UPCGPropertyToParamDataSettings>(InSettings)->bAlwaysRequeryActors; }
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const;
};