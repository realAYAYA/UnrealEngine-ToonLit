// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "InheritableComponentHandler.generated.h"

class UActorComponent;
class UBlueprint;
class USCS_Node;
struct FUCSComponentId;

USTRUCT()
struct ENGINE_API FComponentKey
{
	GENERATED_USTRUCT_BODY()

	FComponentKey()
		: OwnerClass(nullptr)
	{}

	FComponentKey(const USCS_Node* SCSNode);
#if WITH_EDITOR
	FComponentKey(UBlueprint* Blueprint, const FUCSComponentId& UCSComponentID);
#endif 

	bool Match(const FComponentKey& OtherKey) const;

	bool IsSCSKey() const
	{
		return (SCSVariableName != NAME_None) && AssociatedGuid.IsValid();
	}

	bool IsUCSKey() const
	{
		return AssociatedGuid.IsValid() && (SCSVariableName == NAME_None);
	}

	bool IsValid() const
	{
		return OwnerClass && AssociatedGuid.IsValid() && (!IsSCSKey() || (SCSVariableName != NAME_None));
	}

	USCS_Node* FindSCSNode() const;
	UActorComponent* GetOriginalTemplate(const FName& TemplateName = NAME_None) const;
	bool RefreshVariableName();

	UClass* GetComponentOwner()  const { return OwnerClass; }
	FName   GetSCSVariableName() const { return SCSVariableName; }
	FGuid   GetAssociatedGuid()  const { return AssociatedGuid; }

private: 
	UPROPERTY()
	TObjectPtr<UClass> OwnerClass;

	UPROPERTY()
	FName SCSVariableName;

	UPROPERTY()
	FGuid AssociatedGuid;
};

USTRUCT()
struct FComponentOverrideRecord
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TObjectPtr<UClass> ComponentClass;

	UPROPERTY()
	TObjectPtr<UActorComponent> ComponentTemplate;

	UPROPERTY()
	FComponentKey ComponentKey;

	UPROPERTY()
	FBlueprintCookedComponentInstancingData CookedComponentInstancingData;

	FComponentOverrideRecord()
		: ComponentClass(nullptr)
		, ComponentTemplate(nullptr)
	{}
};

UCLASS()
class ENGINE_API UInheritableComponentHandler : public UObject
{
	GENERATED_BODY()

private:

	/* Template name prefix for SCS DefaultSceneRootNode overrides */
	static const FString SCSDefaultSceneRootOverrideNamePrefix;

#if WITH_EDITOR

	bool IsRecordValid(const FComponentOverrideRecord& Record) const;
	bool IsRecordNecessary(const FComponentOverrideRecord& Record) const;

public:

	UActorComponent* CreateOverridenComponentTemplate(const FComponentKey& Key);
	void RemoveOverridenComponentTemplate(const FComponentKey& Key);
	void UpdateOwnerClass(UBlueprintGeneratedClass* OwnerClass);
	void ValidateTemplates();
	bool IsValid() const;
	UActorComponent* FindBestArchetype(const FComponentKey& Key, const FName& TemplateName = NAME_None) const;

	bool IsEmpty() const
	{
		return 0 == Records.Num();
	}

	bool RefreshTemplateName(const FComponentKey& OldKey);

	FComponentKey FindKey(UActorComponent* ComponentTemplate) const;
#endif

public:

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif	// WITH_EDITOR
	//~ End UObject Interface

	void PreloadAllTemplates();
	void PreloadAll();

	FComponentKey FindKey(const FName VariableName) const;

	UActorComponent* GetOverridenComponentTemplate(const FComponentKey& Key) const;
	const FBlueprintCookedComponentInstancingData* GetOverridenComponentTemplateData(const FComponentKey& Key) const;

	TArray<FComponentOverrideRecord>::TIterator CreateRecordIterator()
	{
		return Records.CreateIterator();
	}

	void GetAllTemplates(TArray<UActorComponent*>& OutArray, bool bIncludeTransientTemplates = false) const
	{
		OutArray.Reserve(OutArray.Num() + Records.Num() + (bIncludeTransientTemplates ? UnnecessaryComponents.Num() : 0));
		for (const FComponentOverrideRecord& Record : Records)
		{
			OutArray.Add(Record.ComponentTemplate);
		}

		if (bIncludeTransientTemplates)
		{
			OutArray.Append(UnnecessaryComponents);
		}
	}

private:
	const FComponentOverrideRecord* FindRecord(const FComponentKey& Key) const;

	/** Helper method used to assist with fixing up component template names at load time. */
	void FixComponentTemplateName(UActorComponent* ComponentTemplate, FName NewName);
	
	/** All component records */
	UPROPERTY()
	TArray<FComponentOverrideRecord> Records;

	/** List of components that were marked unnecessary, need to keep these around so it doesn't regenerate them when a child asks for one */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> UnnecessaryComponents;
};
