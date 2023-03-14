// Copyright Epic Games, Inc. All Rights Reserved.

#include "InputCustomizations.h"

#include "ActionMappingDetails.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "EnhancedActionKeyMapping.h"
#include "IDetailChildrenBuilder.h"
#include "InputMappingContext.h"
#include "PropertyCustomizationHelpers.h"
#include "EnhancedInputDeveloperSettings.h"
#include "InputEditorModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "InputCustomization"

//////////////////////////////////////////////////////////
// FInputContextDetails

TSharedRef<IDetailCustomization> FInputContextDetails::MakeInstance()
{
	return MakeShareable(new FInputContextDetails);
}

void FInputContextDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	// Custom Action Mappings
	const TSharedPtr<IPropertyHandle> ActionMappingsPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UInputMappingContext, Mappings));
	ActionMappingsPropertyHandle->MarkHiddenByCustomization();

	IDetailCategoryBuilder& MappingsDetailCategoryBuilder = DetailBuilder.EditCategory(ActionMappingsPropertyHandle->GetDefaultCategoryName());
	const TSharedRef<FActionMappingsNodeBuilderEx> ActionMappingsBuilder = MakeShareable(new FActionMappingsNodeBuilderEx(&DetailBuilder, ActionMappingsPropertyHandle));
	MappingsDetailCategoryBuilder.AddCustomBuilder(ActionMappingsBuilder);
}

//////////////////////////////////////////////////////////
// FEnhancedActionMappingCustomization

void FEnhancedActionMappingCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	MappingPropertyHandle = PropertyHandle;

	// Grab the FKey property
	TSharedPtr<IPropertyHandle> KeyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnhancedActionKeyMapping, Key));
	TSharedPtr<IPropertyHandle> TriggersHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnhancedActionKeyMapping, Triggers));

	TriggersHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FEnhancedActionMappingCustomization::OnTriggersChanged));

	TSharedRef<SWidget> RemoveButton = PropertyCustomizationHelpers::MakeDeleteButton(FSimpleDelegate::CreateSP(this, &FEnhancedActionMappingCustomization::RemoveMappingButton_OnClick),
		LOCTEXT("RemoveMappingToolTip", "Remove Mapping"));

	// Create  a new instance of the key customization.
	KeyStructInstance = FKeyStructCustomization::MakeInstance();

	// TODO: Use FDetailArrayBuilder?

	// Pass our header row into the key struct customizeheader method so it populates our row with the key struct header
	KeyStructCustomization = StaticCastSharedPtr<FKeyStructCustomization>(KeyStructInstance);
	const bool bContainsComboTrigger = DoesTriggerArrayContainCombo();
	KeyStructCustomization->SetDisplayIcon(bContainsComboTrigger);
	KeyStructCustomization->SetEnableKeySelector(!bContainsComboTrigger);
	KeyStructCustomization->CustomizeHeaderOnlyWithButton(KeyHandle.ToSharedRef(), HeaderRow, CustomizationUtils, RemoveButton);
}

void FEnhancedActionMappingCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyHandle> TriggersHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnhancedActionKeyMapping, Triggers));
	TSharedPtr<IPropertyHandle> ModifiersHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnhancedActionKeyMapping, Modifiers));
	TSharedPtr<IPropertyHandle> IsPlayerMappableHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnhancedActionKeyMapping, bIsPlayerMappable));
	TSharedPtr<IPropertyHandle> PlayerBindingOptions = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnhancedActionKeyMapping, PlayerMappableOptions));

	// TODO: ResetToDefault needs to be disabled for arrays
	ChildBuilder.AddProperty(TriggersHandle.ToSharedRef());
	ChildBuilder.AddProperty(ModifiersHandle.ToSharedRef());
	ChildBuilder.AddProperty(IsPlayerMappableHandle.ToSharedRef());
	ChildBuilder.AddProperty(PlayerBindingOptions.ToSharedRef());
}

void FEnhancedActionMappingCustomization::RemoveMappingButton_OnClick() const
{
	if (MappingPropertyHandle->IsValidHandle())
	{
		const TSharedPtr<IPropertyHandle> ParentHandle = MappingPropertyHandle->GetParentHandle();
		const TSharedPtr<IPropertyHandleArray> ParentArrayHandle = ParentHandle->AsArray();

		ParentArrayHandle->DeleteItem(MappingPropertyHandle->GetIndexInArray());
	}
}

void FEnhancedActionMappingCustomization::OnTriggersChanged() const
{
	const bool bContainsComboTrigger = DoesTriggerArrayContainCombo();
	KeyStructCustomization->SetDisplayIcon(bContainsComboTrigger);
    KeyStructCustomization->SetEnableKeySelector(!bContainsComboTrigger);
}

bool FEnhancedActionMappingCustomization::DoesTriggerArrayContainCombo() const
{
	if (MappingPropertyHandle)
	{
		if (TSharedPtr<IPropertyHandle> TriggersHandle = MappingPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnhancedActionKeyMapping, Triggers)))
		{
			// getting data for the trigger array
			void* Data = nullptr;
			TriggersHandle->GetValueData(Data);
			if (Data)
			{
				FProperty* TriggersProperty = TriggersHandle->GetProperty();
				FArrayProperty* TriggersArrayProperty = CastField<FArrayProperty>(TriggersProperty);
				FScriptArrayHelper ArrayHelper(TriggersArrayProperty, Data);
	
				for (int32 i = 0; i < ArrayHelper.Num(); i++)
				{
					// Make sure we can cast this to a input trigger and if it's a combo we can return true
					if (UInputTrigger** InputComboTrigger = reinterpret_cast<UInputTrigger**>(ArrayHelper.GetRawPtr(i)))
					{
						if ((*InputComboTrigger) && (*InputComboTrigger)->IsA(UInputTriggerCombo::StaticClass()))
						{
							return true;
						}
					}
				}
			}
		}
	}
	
	return false;
}

//////////////////////////////////////////////////////////
// FEnhancedInputDeveloperSettingsCustomization

TSet<FName> FEnhancedInputDeveloperSettingsCustomization::ExcludedAssetNames;

FEnhancedInputDeveloperSettingsCustomization::~FEnhancedInputDeveloperSettingsCustomization()
{
	// Unregister settings panel listeners
	if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		IAssetRegistry* AssetRegistry = AssetRegistryModule->TryGet();
		if (AssetRegistry)
		{
			AssetRegistry->OnAssetAdded().RemoveAll(this);
			AssetRegistry->OnAssetRemoved().RemoveAll(this);
			AssetRegistry->OnAssetRenamed().RemoveAll(this);
		}
	}
	
	CachedDetailBuilder = nullptr;
}

void FEnhancedInputDeveloperSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Note: The details view for the UEnhancedInputDeveloperSettings object will be on by default
	// This 'EditCategory' call will ensure that the developer settings are displayed on top of the trigger/modifier default values
	DetailBuilder.EditCategory(UEnhancedInputDeveloperSettings::StaticClass()->GetFName(), FText::GetEmpty(), ECategoryPriority::Important);

	static const FName TriggerCategoryName = TEXT("Trigger Default Values");
	static const FName ModifierCategoryName = TEXT("Modifier Default Values");

	TArray<UObject*> ModifierCDOs = GatherClassDetailsCDOs(UInputModifier::StaticClass());
	TArray<UObject*> TriggerCDOs = GatherClassDetailsCDOs(UInputTrigger::StaticClass());
	ExcludedAssetNames.Reset();
	
	// Add The modifier/trigger defaults that are generated via CDO to the details builder
	CustomizeCDOValues(DetailBuilder, ModifierCategoryName, ModifierCDOs);
	CustomizeCDOValues(DetailBuilder, TriggerCategoryName, TriggerCDOs);
	
	// Support for updating blueprint based triggers and modifiers in the settings panel
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	if (!AssetRegistry.OnAssetAdded().IsBoundToObject(this))
	{
		AssetRegistry.OnAssetAdded().AddRaw(this, &FEnhancedInputDeveloperSettingsCustomization::OnAssetAdded);
		AssetRegistry.OnAssetRemoved().AddRaw(this, &FEnhancedInputDeveloperSettingsCustomization::OnAssetRemoved);
		AssetRegistry.OnAssetRenamed().AddRaw(this, &FEnhancedInputDeveloperSettingsCustomization::OnAssetRenamed);
	}
}

void FEnhancedInputDeveloperSettingsCustomization::CustomizeCDOValues(IDetailLayoutBuilder& DetailBuilder, const FName CategoryName, const TArray<UObject*>& ObjectsToCustomize)
{
	IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(CategoryName);

	// All of the Objects in this array are the CDO, i.e. the class name starts with "Default__"
	for (UObject* CDO : ObjectsToCustomize)
	{
		if (!ensure(CDO && CDO->IsTemplate()))
		{
			continue;
		}

		// Add the CDO as an external object reference to this customization
		IDetailPropertyRow* Row = CategoryBuilder.AddExternalObjects({ CDO }, EPropertyLocation::Default, FAddPropertyParams().UniqueId(CDO->GetClass()->GetFName()));

		if (Row)
		{
			// We need to add a custom "Name" widget here, otherwise all the categories will just say "Object"
			Row->CustomWidget()
			.NameContent()
			[
				SNew(STextBlock)
					.Text(CDO->GetClass()->GetDisplayNameText())
			];	
		}
	}
}

void FEnhancedInputDeveloperSettingsCustomization::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	CachedDetailBuilder = DetailBuilder;
	CustomizeDetails(*DetailBuilder);
}

TArray<UObject*> FEnhancedInputDeveloperSettingsCustomization::GatherClassDetailsCDOs(UClass* Class)
{
	TArray<UObject*> CDOs;

	// Search native classes
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		if (!ClassIt->IsNative() || !ClassIt->IsChildOf(Class))
		{
			continue;
		}

		// Ignore abstract, hidedropdown, and deprecated.
		if (ClassIt->HasAnyClassFlags(CLASS_Abstract | CLASS_HideDropDown | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		CDOs.AddUnique(ClassIt->GetDefaultObject());
	}

	// Search BPs via asset registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> BlueprintAssetData;
	AssetRegistry.GetAssets(Filter, BlueprintAssetData);

	for (FAssetData& Asset : BlueprintAssetData)
	{
		FAssetDataTagMapSharedView::FFindTagResult Result = Asset.TagsAndValues.FindTag(TEXT("NativeParentClass"));
		if (Result.IsSet() && !ExcludedAssetNames.Contains(Asset.AssetName))
		{
			const FString ClassObjectPath = FPackageName::ExportTextPathToObjectPath(Result.GetValue());
			if (UClass* ParentClass = FindObjectSafe<UClass>(nullptr, *ClassObjectPath, true))
			{
				if (ParentClass->IsChildOf(Class))
				{
					// TODO: Forcibly loading these assets could cause problems on projects with a large number of them.
					UBlueprint* BP = CastChecked<UBlueprint>(Asset.GetAsset());
					CDOs.AddUnique(BP->GeneratedClass->GetDefaultObject());
				}
			}
		}
	}

	// Strip objects with no config stored properties
	CDOs.RemoveAll([Class](UObject* Object) {
		UClass* ObjectClass = Object->GetClass();
		if (ObjectClass->GetMetaData(TEXT("NotInputConfigurable")).ToBool())
		{
			return true;
		}
		while (ObjectClass)
		{
			for (FProperty* Property : TFieldRange<FProperty>(ObjectClass, EFieldIteratorFlags::ExcludeSuper, EFieldIteratorFlags::ExcludeDeprecated))
			{
				if (Property->HasAnyPropertyFlags(CPF_Config))
				{
					return false;
				}
			}

			// Stop searching at the base type. We don't care about configurable properties lower than that.
			ObjectClass = ObjectClass != Class ? ObjectClass->GetSuperClass() : nullptr;
		}
		return true;
	});

	return CDOs;
}

void FEnhancedInputDeveloperSettingsCustomization::RebuildDetailsViewForAsset(const FAssetData& AssetData, const bool bIsAssetBeingRemoved)
{
	// If the asset was a blueprint...
	if (AssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())
	{
		// With a native parent class...
		FAssetDataTagMapSharedView::FFindTagResult Result = AssetData.TagsAndValues.FindTag(FBlueprintTags::NativeParentClassPath);
		if (Result.IsSet())
		{
			// And the base class is UInputModifier or UInputTrigger, then we should rebuild the details
			const FString ClassObjectPath = FPackageName::ExportTextPathToObjectPath(Result.GetValue());
			if (UClass* ParentClass = FindObjectSafe<UClass>(nullptr, *ClassObjectPath, true))
			{
				if (ParentClass == UInputModifier::StaticClass() || ParentClass == UInputTrigger::StaticClass())
				{
					if (bIsAssetBeingRemoved)
					{
						ExcludedAssetNames.Add(AssetData.AssetName);
					}
					
					if (IDetailLayoutBuilder* DetailBuilder = CachedDetailBuilder.Pin().Get())
					{
						DetailBuilder->ForceRefreshDetails();
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE