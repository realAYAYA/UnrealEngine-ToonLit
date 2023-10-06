// Copyright Epic Games, Inc. All Rights Reserved.

#include "StructUtilsEditorModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyBagDetails.h"
#include "PropertyEditorModule.h"
#include "StructUtilsTypes.h"
#include "Engine/UserDefinedStruct.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "CoreGlobals.h"
#include "GameFramework/Actor.h"
#include "StructUtilsDelegates.h"
#include "InstancedStruct.h"
#include "InstancedStructContainer.h"
#include "PropertyBag.h"

#define LOCTEXT_NAMESPACE "StructUtilsEditor"

IMPLEMENT_MODULE(FStructUtilsEditorModule, StructUtilsEditor)

void FStructUtilsEditorModule::StartupModule()
{
	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("InstancedStruct", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInstancedStructDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("InstancedPropertyBag", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPropertyBagDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FStructUtilsEditorModule::ShutdownModule()
{
	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("InstancedStruct");
		PropertyModule.UnregisterCustomPropertyTypeLayout("InstancedPropertyBag");
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}

namespace UE::StructUtils::Private
{

static void VisitReferencedObjects(const UUserDefinedStruct* StructToReinstance)
{
	// Helper preference collector, does not collect anything, but makes sure AddStructReferencedObjects() gets called e.g. on instanced struct. 
	class FVisitorReferenceCollector : public FReferenceCollector
	{
	public:
		virtual bool IsIgnoringArchetypeRef() const override { return false; }
		virtual bool IsIgnoringTransient() const override { return false; }

		virtual void HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const FProperty* ReferencingProperty) override
		{
			// Empty
		}
	};

	FVisitorReferenceCollector Collector;

	// This sets global variable which read in the AddStructReferencedObjects().
	UE::StructUtils::Private::FStructureToReinstanceScope StructureToReinstanceScope(StructToReinstance);

	for (TObjectIterator<UObject> ObjectIt; ObjectIt; ++ObjectIt)
	{
		UObject* Object = *ObjectIt;
		
		// This sets global variable which read in the AddStructReferencedObjects().
		UE::StructUtils::Private::FCurrentReinstanceOuterObjectScope CurrentReinstanceOuterObjectScope(Object);
		
		Collector.AddPropertyReferencesWithStructARO(Object->GetClass(), Object);
	}

	// Handle CDOs and sparse class data 
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
	
		// Handle Sparse class data
		if (void* SparseData = const_cast<void*>(Class->GetSparseClassData(EGetSparseClassDataMethod::ReturnIfNull)))
		{
			UE::StructUtils::Private::FCurrentReinstanceOuterObjectScope CurrentReinstanceOuterObjectScope(Class);
			const UScriptStruct* SparseDataStruct = Class->GetSparseClassDataStruct();
			Collector.AddPropertyReferencesWithStructARO(SparseDataStruct, SparseData);
		}

		// Handle CDO
		if (UObject* CDO = Class->GetDefaultObject())
		{
			UE::StructUtils::Private::FCurrentReinstanceOuterObjectScope CurrentReinstanceOuterObjectScope(CDO);
			Collector.AddPropertyReferencesWithStructARO(Class, CDO);
		}
	}
};

}; // UE::StructUtils::Private

void FStructUtilsEditorModule::PreChange(const UUserDefinedStruct* StructToReinstance, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
	if (!StructToReinstance)
	{
		return;
	}

	// Make a duplicate of the existing struct, and point all instances of the struct to point to the duplicate.
	// This is done because the original struct will be changed.
	UUserDefinedStruct* DuplicatedStruct = nullptr;
	{
		const FString ReinstancedName = FString::Printf(TEXT("STRUCT_REINST_%s"), *StructToReinstance->GetName());
		const FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), UUserDefinedStruct::StaticClass(), FName(*ReinstancedName));

		TGuardValue<FIsDuplicatingClassForReinstancing, bool> IsDuplicatingClassForReinstancing(GIsDuplicatingClassForReinstancing, true);
		DuplicatedStruct = (UUserDefinedStruct*)StaticDuplicateObject(StructToReinstance, GetTransientPackage(), UniqueName, ~RF_Transactional); 

		DuplicatedStruct->Guid = StructToReinstance->Guid;
		DuplicatedStruct->Bind();
		DuplicatedStruct->StaticLink(true);
		DuplicatedStruct->PrimaryStruct = const_cast<UUserDefinedStruct*>(StructToReinstance);
		DuplicatedStruct->Status = EUserDefinedStructureStatus::UDSS_Duplicate;
		DuplicatedStruct->SetFlags(RF_Transient);
		DuplicatedStruct->AddToRoot();
	}

	UUserDefinedStructEditorData* DuplicatedEditorData = CastChecked<UUserDefinedStructEditorData>(DuplicatedStruct->EditorData);
	DuplicatedEditorData->RecreateDefaultInstance();

	UE::StructUtils::Private::VisitReferencedObjects(DuplicatedStruct);
	
	DuplicatedStruct->RemoveFromRoot();
}

void FStructUtilsEditorModule::PostChange(const UUserDefinedStruct* StructToReinstance, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
	if (!StructToReinstance)
	{
		return;
	}

	UE::StructUtils::Private::VisitReferencedObjects(StructToReinstance);

	if (UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.IsBound())
	{
		UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.Broadcast(*StructToReinstance);
	}
}

#undef LOCTEXT_NAMESPACE
