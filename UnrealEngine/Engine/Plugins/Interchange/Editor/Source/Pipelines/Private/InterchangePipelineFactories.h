// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions/AssetTypeActions_Blueprint.h"
#include "AssetTypeActions_Base.h"
#include "ClassViewerFilter.h"
#include "Factories/Factory.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangePipelineFactories.generated.h"

class FInterchangePipelineBaseFilterViewer : public IClassViewerFilter
{
public:
	TSet<const UClass*> AllowedChildrenOfClasses;
	TSet<const UClass*> DisallowedChildrenOfClasses;
	EClassFlags DisallowedClassFlags = EClassFlags::CLASS_None;
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<class FClassViewerFilterFuncs> InFilterFuncs) override
	{
		if (InClass->HasAnyClassFlags(DisallowedClassFlags))
		{
			return false;
		}
		
		return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed
			&& InFilterFuncs->IfInChildOfClassesSet(DisallowedChildrenOfClasses, InClass) == EFilterReturn::Failed;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		if (InUnloadedClassData->HasAnyClassFlags(DisallowedClassFlags))
		{
			return false;
		}

		return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed
			&& InFilterFuncs->IfInChildOfClassesSet(DisallowedChildrenOfClasses, InUnloadedClassData) == EFilterReturn::Failed;
	}
};

UCLASS(hidecategories = Object, collapsecategories)
class UInterchangeBlueprintPipelineBaseFactory : public UFactory
{
	GENERATED_BODY()

public:
	UInterchangeBlueprintPipelineBaseFactory();

	// The type of blueprint that will be created
	UPROPERTY(EditAnywhere, Category = InterchangeBlueprintPipelineBaseFactory)
	TEnumAsByte<enum EBlueprintType> BlueprintType;

	// The parent class of the created blueprint
	UPROPERTY(EditAnywhere, Category = InterchangeBlueprintPipelineBaseFactory)
	TSubclassOf<class UInterchangePipelineBase> ParentClass;

	//Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//End UFactory Interface	
};

class FAssetTypeActions_InterchangeBlueprintPipelineBase : public FAssetTypeActions_Blueprint
{
public:
	// FAssetTypeActions_Blueprint interface
	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("FAssetTypeActions_InterchangeGraphInspectorPipeline", "InterchangeGraphInspectorPipelineName", "Interchange Blueprint Pipeline Base"); }
	virtual FColor GetTypeColor() const override { return FColor(10, 25, 175); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Blueprint | EAssetTypeCategories::Misc; }
};

UCLASS(hidecategories = Object, collapsecategories)
class UInterchangePipelineBaseFactory : public UFactory
{
	GENERATED_BODY()

public:
	UInterchangePipelineBaseFactory();

	// UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	virtual FText GetDisplayName() const override;
	virtual bool ConfigureProperties() override;
	// End of UFactory Interface
private:
	UClass* PipelineClass = nullptr;
};



class FAssetTypeActions_InterchangePipelineBase : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("FAssetTypeActions_InterchangePipelineBase", "InterchangePipelineBaseName", "Interchange Pipeline"); }
	virtual FColor GetTypeColor() const override { return FColor(135, 200, 25); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
};

UCLASS(hidecategories = Object, collapsecategories)
class UInterchangePythonPipelineAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UInterchangePythonPipelineAssetFactory();

	// UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	virtual FText GetDisplayName() const override;
	virtual bool ConfigureProperties() override;
	// End of UFactory Interface
private:
	UClass* PythonClass = nullptr;
};

class FAssetTypeActions_InterchangePythonPipelineBase : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("InterchangePipelineFactories", "FAssetTypeActions_InterchangePythonPipeline", "Interchange Python Pipeline"); }
	virtual FColor GetTypeColor() const override { return FColor(135, 200, 25); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
};