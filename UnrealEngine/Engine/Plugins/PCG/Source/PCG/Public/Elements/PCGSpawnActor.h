// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSubgraph.h"
#include "Metadata/PCGObjectPropertyOverride.h"

#include "PCGSpawnActor.generated.h"

class AActor;
class UPCGGraphInterface;
class UPCGPointData;

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

/*
* PCG settings class that allows spawning actors with some options to perform the work more efficiently.
* Note that depending on the options, any PCG components on the spawned actors can be also generated,
* which is why this class derives from UPCGBaseSubgraphSettings - it has similar inner-workings to the subgraph node
* as far as data passing and dispatch go.
* Note that at this point in time, results from the underlying graphs being generated is not propagated back as results of this node.
*/
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGSpawnActorSettings : public UPCGBaseSubgraphSettings
{
	GENERATED_BODY()

public:
	UPCGSpawnActorSettings(const FObjectInitializer& ObjectInitializer);

	/** 
	 * Can specify a list of functions from the template class to be called on each actor spawned, in order. Need to have "CallInEditor" flag enabled
	 * and have either no parameters or exactly the parameters PCGPoint and PCGMetadata
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option!=EPCGSpawnActorOption::CollapseActors", EditConditionHides))
	TArray<FName> PostSpawnFunctionNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGSpawnActorOption Option = EPCGSpawnActorOption::CollapseActors;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option==EPCGSpawnActorOption::NoMerging", EditConditionHides))
	bool bForceDisableActorParsing = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option==EPCGSpawnActorOption::NoMerging", EditConditionHides))
	EPCGSpawnActorGenerationTrigger GenerationTrigger = EPCGSpawnActorGenerationTrigger::Default;

	/** Warning: inheriting parent actor tags work only in non-collapsed actor hierarchies */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option!=EPCGSpawnActorOption::CollapseActors", EditConditionHides))
	bool bInheritActorTags = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option!=EPCGSpawnActorOption::CollapseActors", EditConditionHides))
	TArray<FName> TagsToAddOnActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = Settings, meta = (ShowInnerProperties, EditCondition = "bAllowTemplateActorEditing && Option != EPCGSpawnActorOption::CollapseActors", EditConditionHides))
	TObjectPtr<AActor> TemplateActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "Option != EPCGSpawnActorOption::CollapseActors", EditConditionHides))
	TArray<FPCGObjectPropertyOverrideDescription> SpawnedActorPropertyOverrideDescriptions;

	UPROPERTY(meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> RootActor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option != EPCGSpawnActorOption::CollapseActors", EditConditionHides))
	EPCGAttachOptions AttachOptions = EPCGAttachOptions::Attached; // Note that this is no longer the default value for new nodes, it is now EPCGAttachOptions::InFolder

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bSpawnByAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bSpawnByAttribute"))
	FName SpawnAttribute = NAME_None;

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (OnlyPlaceable, DisallowCreateNew))
	TSubclassOf<AActor> TemplateActorClass = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Option != EPCGSpawnActorOption::CollapseActors"))
	bool bAllowTemplateActorEditing = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	EPCGSpawnActorGenerationTrigger bGenerationTrigger_DEPRECATED = EPCGSpawnActorGenerationTrigger::Default;

	UPROPERTY()
	TArray<FPCGActorPropertyOverride> ActorOverrides_DEPRECATED;
#endif

public:
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	void OnBlueprintChanged(UBlueprint* Blueprint);
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR

	//~Begin UPCGSettings interface
	virtual UPCGNode* CreateNode() const override;

	PCG_API void SetTemplateActorClass(const TSubclassOf<AActor>& InTemplateActorClass);
	PCG_API void SetAllowTemplateActorEditing(bool bInAllowTemplateActorEditing);
	const TSubclassOf<AActor>& GetTemplateActorClass() const { return TemplateActorClass; }
	bool GetAllowTemplateActorEditing() const { return bAllowTemplateActorEditing; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SpawnActor")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSpawnActorSettings", "NodeTitle", "Spawn Actor"); }
	virtual EPCGSettingsType GetType() const override;
#endif

protected:
#if WITH_EDITOR	
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override;
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	//~Begin UPCGBaseSubgraphSettings interface
	// When using spawn by attribute, the potential execution of subgraphs will be done in a dynamic manner
	virtual bool IsDynamicGraph() const { return bSpawnByAttribute; }
	virtual UPCGGraphInterface* GetSubgraphInterface() const override;

	static UPCGGraphInterface* GetGraphInterfaceFromActorSubclass(TSubclassOf<AActor> InTemplateActorClass);

protected:
#if WITH_EDITOR
	//~End UPCGBaseSubgraphSettings interface

	void SetupBlueprintEvent();
	void TeardownBlueprintEvent();

private:
	void RefreshTemplateActor();
#endif

	friend class FPCGSpawnActorElement;
};

UCLASS(ClassGroup = (Procedural))
class UPCGSpawnActorNode : public UPCGBaseSubgraphNode
{
	GENERATED_BODY()
public:
	/** ~Begin UPCGBaseSubgraphNode interface */
	virtual TObjectPtr<UPCGGraphInterface> GetSubgraphInterface() const override;
	/** ~End UPCGBaseSubgraphNode interface */
};

class FPCGSpawnActorElement : public FPCGSubgraphElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool IsPassthrough(const UPCGSettings* InSettings) const override { return !InSettings; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	bool SpawnAndPrepareSubgraphs(FPCGSubgraphContext* Context, const UPCGSpawnActorSettings* Settings) const;

	TArray<FName> GetNewActorTags(FPCGContext* Context, AActor* TargetActor, bool bInheritActorTags, const TArray<FName>& AdditionalTags) const;

	void CollapseIntoTargetActor(FPCGSubgraphContext* Context, AActor* TargetActor, TSubclassOf<AActor> TemplateActorClass, const UPCGPointData* PointData) const;
	void SpawnActors(FPCGSubgraphContext* Context, AActor* TargetActor, TSubclassOf<AActor> TemplateActorClass, AActor* TemplateActor, FPCGTaggedData& Output, const UPCGPointData* PointData, UPCGPointData* OutPointData) const;
};