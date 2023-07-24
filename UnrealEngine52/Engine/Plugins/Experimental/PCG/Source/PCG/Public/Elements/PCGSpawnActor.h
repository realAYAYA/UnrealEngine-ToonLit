// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGPin.h"
#include "PCGSubgraph.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGSpawnActor.generated.h"

class AActor;
class UPCGGraphInterface;

UENUM()
enum class EPCGSpawnActorOption : uint8
{
	CollapseActors,
	MergePCGOnly,
	NoMerging
};

UENUM()
enum class EPCGSpawnActorGenerationTrigger : uint8
{
	Default, // Generate if the component has "Generate On Load"
	ForceGenerate, // Generate in all cases
	DoNotGenerateInEditor, // Does not call generate in editor, but decays to Default otherwise
	DoNotGenerate // Does not call generate
};

USTRUCT(BlueprintType)
struct FPCGActorPropertyOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGAttributePropertySelector InputSource;

	UPROPERTY(EditAnywhere, Category = Settings)
	FString PropertyTarget;
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSpawnActorSettings : public UPCGBaseSubgraphSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (OnlyPlaceable, DisallowCreateNew))
	TSubclassOf<AActor> TemplateActorClass = nullptr;

	/* Can specify a list of functions from the template class to be called on each actor spawned, in order. Need to be parameter-less and with "CallInEditor" flag enabled.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option!=EPCGSpawnActorOption::CollapseActors", EditConditionHides))
	TArray<FName> PostSpawnFunctionNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGSpawnActorOption Option = EPCGSpawnActorOption::CollapseActors;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option==EPCGSpawnActorOption::NoMerging", EditConditionHides))
	bool bForceDisableActorParsing = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option==EPCGSpawnActorOption::NoMerging", EditConditionHides))
	EPCGSpawnActorGenerationTrigger bGenerationTrigger = EPCGSpawnActorGenerationTrigger::Default;

	/** Warning: inheriting parent actor tags work only in non-collapsed actor hierarchies */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option!=EPCGSpawnActorOption::CollapseActors", EditConditionHides))
	bool bInheritActorTags = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option!=EPCGSpawnActorOption::CollapseActors", EditConditionHides))
	TArray<FName> TagsToAddOnActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = Settings, meta = (ShowInnerProperties, EditCondition = "Option != EPCGSpawnActorOption::CollapseActors", EditConditionHides))
	TObjectPtr<AActor> TemplateActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "Option != EPCGSpawnActorOption::CollapseActors", EditConditionHides))
	TArray<FPCGActorPropertyOverride> ActorOverrides;

	virtual void PostLoad() override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	void OnBlueprintChanged(UBlueprint* Blueprint);
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR

	//~Begin UCPGSettings interface
	virtual UPCGNode* CreateNode() const override;

#if WITH_EDITOR	
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SpawnActor")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSpawnActorSettings", "NodeTitle", "Spawn Actor"); }
	virtual EPCGSettingsType GetType() const override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	//~Begin UPCGBaseSubgraphSettings interface
	virtual UPCGGraphInterface* GetSubgraphInterface() const override;

protected:
#if WITH_EDITOR
	virtual bool IsStructuralProperty(const FName& InPropertyName) const override;
	//~End UPCGBaseSubgraphSettings interface

	void SetupBlueprintEvent();
	void TeardownBlueprintEvent();
#endif

private:
	void RefreshTemplateActor();
};

UCLASS(ClassGroup = (Procedural))
class PCG_API UPCGSpawnActorNode : public UPCGBaseSubgraphNode
{
	GENERATED_BODY()
public:
	/** ~Begin UPCGBaseSubgraphNode interface */
	virtual TObjectPtr<UPCGGraphInterface> GetSubgraphInterface() const override;
	/** ~End UPCGBaseSubgraphNode interface */
};

class FPCGSpawnActorElement : public FSimplePCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool IsPassthrough(const UPCGSettings* InSettings) const override { return !InSettings || InSettings->bEnabled; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	TArray<FName> GetNewActorTags(FPCGContext* Context, AActor* TargetActor, bool bInheritActorTags, const TArray<FName>& AdditionalTags) const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
