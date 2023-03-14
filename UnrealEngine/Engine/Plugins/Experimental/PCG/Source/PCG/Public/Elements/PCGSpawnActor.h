// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "PCGSubgraph.h"
#include "Templates/SubclassOf.h"

#include "PCGSpawnActor.generated.h"

class AActor;

UENUM()
enum class EPCGSpawnActorOption : uint8
{
	CollapseActors,
	MergePCGOnly,
	NoMerging
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSpawnActorSettings : public UPCGBaseSubgraphSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TSubclassOf<AActor> TemplateActorClass = nullptr;

	/* Can specify a list of functions from the template class to be called on each actor spawned, in order. Need to be parameter-less and with "CallInEditor" flag enabled.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option!=EPCGSpawnActorOption::CollapseActors"))
	TArray<FName> PostSpawnFunctionNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGSpawnActorOption Option = EPCGSpawnActorOption::CollapseActors;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option==EPCGSpawnActorOption::NoMerging"))
	bool bForceDisableActorParsing = true;

	/** Warning: inheriting parent actor tags work only in non-collapsed actor hierarchies */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option!=EPCGSpawnActorOption::CollapseActors"))
	bool bInheritActorTags = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option!=EPCGSpawnActorOption::CollapseActors"))
	TArray<FName> TagsToAddOnActors;

	//~Begin UCPGSettings interface
	virtual UPCGNode* CreateNode() const override;

#if WITH_EDITOR	
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SpawnActorNode")); }
	virtual EPCGSettingsType GetType() const override;
#endif
protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	//~Begin UPCGBaseSubgraphSettings interface
	virtual UPCGGraph* GetSubgraph() const override;

protected:
#if WITH_EDITOR
	virtual bool IsStructuralProperty(const FName& InPropertyName) const override;
#endif
	//~End UPCGBaseSubgraphSettings interface
};

UCLASS(ClassGroup = (Procedural))
class PCG_API UPCGSpawnActorNode : public UPCGBaseSubgraphNode
{
	GENERATED_BODY()
public:
	/** ~Begin UPCGBaseSubgraphNode interface */
	virtual TObjectPtr<UPCGGraph> GetSubgraph() const override;
	/** ~End UPCGBaseSubgraphNode interface */
};

class FPCGSpawnActorElement : public FSimplePCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool IsPassthrough() const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	TArray<FName> GetNewActorTags(FPCGContext* Context, AActor* TargetActor, bool bInheritActorTags, const TArray<FName>& AdditionalTags) const;
};
