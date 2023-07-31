// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedStructDetails.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailCustomNodeBuilder.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Modules/ModuleManager.h"
#include "StructViewerModule.h"
#include "StructViewerFilter.h"
#include "Styling/SlateIconFinder.h"
#include "Editor.h"
#include "Engine/UserDefinedStruct.h"
#include "InstancedStruct.h"

#define LOCTEXT_NAMESPACE "StructUtilsEditor"

class FInstancedStructFilter : public IStructViewerFilter
{
public:
	/** The base struct for the property that classes must be a child-of. */
	const UScriptStruct* BaseStruct = nullptr;

	// A flag controlling whether we allow UserDefinedStructs
	bool bAllowUserDefinedStructs = false;

	// A flag controlling whether we allow to select the BaseStruct
	bool bAllowBaseStruct = true;

	virtual bool IsStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const UScriptStruct* InStruct, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override
	{
		if (InStruct->IsA<UUserDefinedStruct>())
		{
			return bAllowUserDefinedStructs;
		}

		if (InStruct == BaseStruct)
		{
			return bAllowBaseStruct;
		}

		if (InStruct->HasMetaData(TEXT("Hidden")))
		{
			return false;
		}

		// Query the native struct to see if it has the correct parent type (if any)
		return !BaseStruct || InStruct->IsChildOf(BaseStruct);
	}

	virtual bool IsUnloadedStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const FSoftObjectPath& InStructPath, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override
	{
		// User Defined Structs don't support inheritance, so only include them requested
		return bAllowUserDefinedStructs;
	}
};

namespace UE::StructUtils::Private
{

const UScriptStruct* GetCommonScriptStruct(TSharedPtr<IPropertyHandle> StructProperty)
{
	const UScriptStruct* CommonStructType = nullptr;

	StructProperty->EnumerateConstRawData([&CommonStructType](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (RawData)
		{
			const FInstancedStruct* InstancedStruct = static_cast<const FInstancedStruct*>(RawData);

			const UScriptStruct* StructTypePtr = InstancedStruct->GetScriptStruct();
			if (CommonStructType && CommonStructType != StructTypePtr)
			{
				// Multiple struct types on the sources - show nothing set
				CommonStructType = nullptr;
				return false;
			}
			CommonStructType = StructTypePtr;
		}

		return true;
	});

	return CommonStructType;
}

void SetInstancedStructProperty(TSharedPtr<IPropertyHandle> StructProperty, const FInstancedStruct& InstancedStructToSet, const bool bAllowStructMismatch)
{
	// Note: We use the ExportText/SetPerObjectValues flow here (rather then CopyScriptStruct) 
	// so that PropagatePropertyChange is called for the underlying FInstancedStruct property
	TArray<FString> NewInstancedStructValues;
	StructProperty->EnumerateRawData([bAllowStructMismatch, &InstancedStructToSet, &NewInstancedStructValues](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (RawData)
		{
			const FInstancedStruct* InstancedStruct = static_cast<const FInstancedStruct*>(RawData);

			// Only copy the data if this source is still using the expected struct type or we allow a mismatch
			const UScriptStruct* StructTypePtr = InstancedStruct->GetScriptStruct();
			if (bAllowStructMismatch || StructTypePtr == InstancedStructToSet.GetScriptStruct())
			{
				FString& NewInstancedStructValue = NewInstancedStructValues.AddDefaulted_GetRef();
				FInstancedStruct::StaticStruct()->ExportText(NewInstancedStructValue, &InstancedStructToSet, &InstancedStructToSet, nullptr, PPF_None, nullptr);
			}
			else
			{
				FString& NewInstancedStructValue = NewInstancedStructValues.AddDefaulted_GetRef();
				FInstancedStruct::StaticStruct()->ExportText(NewInstancedStructValue, InstancedStruct, InstancedStruct, nullptr, PPF_None, nullptr);
			}
		}
		else
		{
			NewInstancedStructValues.AddDefaulted();
		}
		return true;
	});

	StructProperty->SetPerObjectValues(NewInstancedStructValues);
}

}

////////////////////////////////////

FInstancedStructDataDetails::FInstancedStructDataDetails(TSharedPtr<IPropertyHandle> InStructProperty)
{
#if DO_CHECK
	FStructProperty* StructProp = CastFieldChecked<FStructProperty>(InStructProperty->GetProperty());
	check(StructProp);
	check(StructProp->Struct == FInstancedStruct::StaticStruct());
#endif

	StructProperty = InStructProperty;
}

void FInstancedStructDataDetails::OnStructValuePreChange()
{
	PreChangeOuterObjectNames.Reset();

	// Forward the change event to the real struct handle
	if (StructProperty && StructProperty->IsValidHandle())
	{
		StructProperty->NotifyPreChange();

		// Store the outer objects for the Actor Component special case (see FInstancedStructDataDetails::OnStructValuePostChange).
		TArray<UObject*> OuterObjects;
		StructProperty->GetOuterObjects(OuterObjects);
		
		for (UObject* Outer : OuterObjects)
		{
			if (Outer != nullptr)
			{
				PreChangeOuterObjectNames.Add(Outer->GetPathName());
			}
		}
	}
}

void FInstancedStructDataDetails::OnStructValuePostChange()
{
	// Forward the change event to the real struct handle
	if (StructProperty && StructProperty->IsValidHandle())
	{
		TGuardValue<bool> HandlingStructValuePostChangeGuard(bIsHandlingStructValuePostChange, true);

		// When an InstancedStruct is on an Actor Component, the details customization gets rebuild between
		// the OnStructValuePreChange() and OnStructValuePostChange() calls due to AActor::RerunConstructionScripts().
		// When the new details panel is build, it will clear the outer objects of the StructProperty property handle, and then setting the value will fail.
		// To overcome that, we restore the outer objects based on the objects stored in OnStructValuePreChange().
		if (StructProperty->GetNumOuterObjects() == 0 && PreChangeOuterObjectNames.Num() > 0)
		{
			TArray<UObject*> OuterObjects;
			for (const FString& ObjectPathName : PreChangeOuterObjectNames)
			{
				UObject* OuterObject = FindObject<UObject>(nullptr, *ObjectPathName);
				if (OuterObject)
				{
					OuterObjects.Add(OuterObject);
				}
			}
			
			StructProperty->ReplaceOuterObjects(OuterObjects);
		}
		
		// Copy the modified struct data back to the source instances
		{
			FInstancedStruct TmpInstancedStruct;
			if (StructInstanceData)
			{
				TmpInstancedStruct.InitializeAs(Cast<UScriptStruct>(StructInstanceData->GetStruct()), StructInstanceData->GetStructMemory());
			}
			UE::StructUtils::Private::SetInstancedStructProperty(StructProperty, TmpInstancedStruct, /*bAllowStructMismatch*/false);
		}

		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		StructProperty->NotifyFinishedChangingProperties();
	}
}

void FInstancedStructDataDetails::OnStructHandlePostChange()
{
	if (!bIsHandlingStructValuePostChange)
	{
		// External change; force a sync next Tick
		LastSyncEditableInstanceFromSourceSeconds = 0.0;
	}
}

void FInstancedStructDataDetails::SyncEditableInstanceFromSource(bool* OutStructMismatch)
{
	if (OutStructMismatch)
	{
		*OutStructMismatch = false;
	}

	if (StructProperty && StructProperty->IsValidHandle())
	{
		const UScriptStruct* ExpectedStructType = StructInstanceData ? Cast<UScriptStruct>(StructInstanceData->GetStruct()) : nullptr;
		StructProperty->EnumerateConstRawData([this, ExpectedStructType, OutStructMismatch](const void* RawData, const int32 /*DataIndex*/, const int32 NumDatas)
		{
			if (RawData && NumDatas == 1)
			{
				const FInstancedStruct* InstancedStruct = static_cast<const FInstancedStruct*>(RawData);

				// Only copy the data if this source is still using the expected struct type
				const UScriptStruct* StructTypePtr = InstancedStruct->GetScriptStruct();
				if (StructTypePtr == ExpectedStructType)
				{
					if (StructTypePtr)
					{
						StructTypePtr->CopyScriptStruct(StructInstanceData->GetStructMemory(), InstancedStruct->GetMemory());
					}
				}
				else if (OutStructMismatch)
				{
					*OutStructMismatch = true;
				}
			}
			return false;
		});
	}

	LastSyncEditableInstanceFromSourceSeconds = FPlatformTime::Seconds();
}

void FInstancedStructDataDetails::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	OnRegenerateChildren = InOnRegenerateChildren;
}

void FInstancedStructDataDetails::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	StructProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FInstancedStructDataDetails::OnStructHandlePostChange));
}

void FInstancedStructDataDetails::GenerateChildContent(IDetailChildrenBuilder& ChildBuilder)
{
	// Create a struct instance to edit, for the common struct type of the sources being edited
	StructInstanceData.Reset();
	if (const UScriptStruct* CommonStructType = UE::StructUtils::Private::GetCommonScriptStruct(StructProperty))
	{
		StructInstanceData = MakeShared<FStructOnScope>(CommonStructType);

		// Make sure the struct also has a valid package set, so that properties that rely on this (like FText) work correctly
		{
			TArray<UPackage*> OuterPackages;
			StructProperty->GetOuterPackages(OuterPackages);
			if (OuterPackages.Num() > 0)
			{
				StructInstanceData->SetPackage(OuterPackages[0]);
			}
		}

		bool bStructMismatch = false;
		SyncEditableInstanceFromSource(&bStructMismatch);
	}

	// Add the rows for the struct
	if (StructInstanceData)
	{
		FSimpleDelegate OnStructValuePreChangeDelegate = FSimpleDelegate::CreateSP(this, &FInstancedStructDataDetails::OnStructValuePreChange);
		FSimpleDelegate OnStructValuePostChangeDelegate = FSimpleDelegate::CreateSP(this, &FInstancedStructDataDetails::OnStructValuePostChange);

		TArray<TSharedPtr<IPropertyHandle>> ChildProperties = StructProperty->AddChildStructure(StructInstanceData.ToSharedRef());
		for (TSharedPtr<IPropertyHandle> ChildHandle : ChildProperties)
		{
			ChildHandle->SetOnPropertyValuePreChange(OnStructValuePreChangeDelegate);
			ChildHandle->SetOnChildPropertyValuePreChange(OnStructValuePreChangeDelegate);
			ChildHandle->SetOnPropertyValueChanged(OnStructValuePostChangeDelegate);
			ChildHandle->SetOnChildPropertyValueChanged(OnStructValuePostChangeDelegate);

			IDetailPropertyRow& Row = ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
			OnChildRowAdded(Row);
		}
	}
}

void FInstancedStructDataDetails::Tick(float DeltaTime)
{
	if (LastSyncEditableInstanceFromSourceSeconds + 0.1 < FPlatformTime::Seconds())
	{
		bool bStructMismatch = false;
		SyncEditableInstanceFromSource(&bStructMismatch);

		if (bStructMismatch)
		{
			// If the editable struct no longer has the same struct type as the underlying source, 
			// then we need to refresh to update the child property rows for the new type
			OnRegenerateChildren.ExecuteIfBound();
		}
	}
}

FName FInstancedStructDataDetails::GetName() const
{
	static const FName Name("InstancedStructDataDetails");
	return Name;
}

void FInstancedStructDataDetails::PostUndo(bool bSuccess)
{
	// Undo; force a sync next Tick
	LastSyncEditableInstanceFromSourceSeconds = 0.0;
}

void FInstancedStructDataDetails::PostRedo(bool bSuccess)
{
	// Redo; force a sync next Tick
	LastSyncEditableInstanceFromSourceSeconds = 0.0;
}

////////////////////////////////////


TSharedRef<IPropertyTypeCustomization> FInstancedStructDetails::MakeInstance()
{
	return MakeShared<FInstancedStructDetails>();
}

void FInstancedStructDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	static const FName NAME_BaseStruct = "BaseStruct";
	static const FName NAME_StructTypeConst = "StructTypeConst";

	StructProperty = StructPropertyHandle;

	const FProperty* MetaDataProperty = StructProperty->GetMetaDataProperty();

	const bool bEnableStructSelection = !MetaDataProperty->HasMetaData(NAME_StructTypeConst);

	BaseScriptStruct = nullptr;
	{
		const FString& BaseStructName = MetaDataProperty->GetMetaData(NAME_BaseStruct);
		if (!BaseStructName.IsEmpty())
		{
			BaseScriptStruct = UClass::TryFindTypeSlow<UScriptStruct>(BaseStructName);
			if (!BaseScriptStruct)
			{
				BaseScriptStruct = LoadObject<UScriptStruct>(nullptr, *BaseStructName);
			}
		}
	}

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ComboButton, SComboButton)
			.OnGetMenuContent(this, &FInstancedStructDetails::GenerateStructPicker)
			.ContentPadding(0)
			.IsEnabled(bEnableStructSelection)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SImage)
					.Image(this, &FInstancedStructDetails::GetDisplayValueIcon)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FInstancedStructDetails::GetDisplayValueString)
					.ToolTipText(this, &FInstancedStructDetails::GetDisplayValueString)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];
}

void FInstancedStructDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<FInstancedStructDataDetails> DataDetails = MakeShared<FInstancedStructDataDetails>(StructProperty);
	StructBuilder.AddCustomBuilder(DataDetails);
}

FText FInstancedStructDetails::GetDisplayValueString() const
{
	const UScriptStruct* ScriptStruct = UE::StructUtils::Private::GetCommonScriptStruct(StructProperty);
	if (ScriptStruct)
	{
		return ScriptStruct->GetDisplayNameText();
	}
	return LOCTEXT("NullScriptStruct", "None");
}

const FSlateBrush* FInstancedStructDetails::GetDisplayValueIcon() const
{
	return FSlateIconFinder::FindIconBrushForClass(UScriptStruct::StaticClass());
}

TSharedRef<SWidget> FInstancedStructDetails::GenerateStructPicker()
{
	static const FName NAME_ExcludeBaseStruct = "ExcludeBaseStruct";
	static const FName NAME_HideViewOptions = "HideViewOptions";
	static const FName NAME_ShowTreeView = "ShowTreeView";

	const FProperty* MetaDataProperty = StructProperty->GetMetaDataProperty();

	const bool bExcludeBaseStruct = MetaDataProperty->HasMetaData(NAME_ExcludeBaseStruct);
	const bool bAllowNone = !(MetaDataProperty->PropertyFlags & CPF_NoClear);
	const bool bHideViewOptions = MetaDataProperty->HasMetaData(NAME_HideViewOptions);
	const bool bShowTreeView = MetaDataProperty->HasMetaData(NAME_ShowTreeView);

	TSharedRef<FInstancedStructFilter> StructFilter = MakeShared<FInstancedStructFilter>();
	StructFilter->BaseStruct = BaseScriptStruct;
	StructFilter->bAllowUserDefinedStructs = false;
	StructFilter->bAllowBaseStruct = !bExcludeBaseStruct;

	FStructViewerInitializationOptions Options;
	Options.bShowNoneOption = bAllowNone;
	Options.StructFilter = StructFilter;
	Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
	Options.DisplayMode = bShowTreeView ? EStructViewerDisplayMode::TreeView : EStructViewerDisplayMode::ListView;
	Options.bAllowViewOptions = !bHideViewOptions;

	FOnStructPicked OnPicked(FOnStructPicked::CreateRaw(this, &FInstancedStructDetails::OnStructPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, OnPicked)
			]
		];
}

void FInstancedStructDetails::OnStructPicked(const UScriptStruct* InStruct)
{
	if (StructProperty && StructProperty->IsValidHandle())
	{
		FScopedTransaction Transaction(LOCTEXT("OnStructPicked", "Set Struct"));

		StructProperty->NotifyPreChange();

		// Copy the modified struct data back to the source instances
		{
			FInstancedStruct TmpInstancedStruct;
			TmpInstancedStruct.InitializeAs(InStruct);
			UE::StructUtils::Private::SetInstancedStructProperty(StructProperty, TmpInstancedStruct, /*bAllowStructMismatch*/true);
		}

		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		StructProperty->NotifyFinishedChangingProperties();
	}

	ComboButton->SetIsOpen(false);
}

#undef LOCTEXT_NAMESPACE
