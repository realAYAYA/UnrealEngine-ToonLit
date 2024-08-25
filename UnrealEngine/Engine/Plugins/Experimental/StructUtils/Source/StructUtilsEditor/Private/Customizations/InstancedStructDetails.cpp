// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedStructDetails.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "UObject/Package.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "ScopedTransaction.h"
#include "Modules/ModuleManager.h"
#include "StructViewerModule.h"
#include "Styling/SlateIconFinder.h"
#include "Engine/UserDefinedStruct.h"
#include "InstancedStruct.h"
#include "Widgets/Layout/SBox.h"
#include "IStructureDataProvider.h"
#include "GameFramework/Actor.h"
#include "Misc/ConfigCacheIni.h"
#include "StructUtilsDelegates.h"

#define LOCTEXT_NAMESPACE "StructUtilsEditor"

////////////////////////////////////

class FInstancedStructProvider : public IStructureDataProvider
{
public:
	FInstancedStructProvider() = default;
	
	explicit FInstancedStructProvider(const TSharedPtr<IPropertyHandle>& InStructProperty)
		: StructProperty(InStructProperty)
	{
	}
	
	virtual ~FInstancedStructProvider() override {}

	void Reset()
	{
		StructProperty = nullptr;
	}
	
	virtual bool IsValid() const override
	{
		bool bHasValidData = false;
		EnumerateInstances([&bHasValidData](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
		{
			if (ScriptStruct && Memory)
			{
				bHasValidData = true;
				return false; // Stop
			}
			return true; // Continue
		});

		return bHasValidData;
	}
	
	virtual const UStruct* GetBaseStructure() const override
	{
		// Taken from UClass::FindCommonBase
		auto FindCommonBaseStruct = [](const UScriptStruct* StructA, const UScriptStruct* StructB)
		{
			const UScriptStruct* CommonBaseStruct = StructA;
			while (CommonBaseStruct && StructB && !StructB->IsChildOf(CommonBaseStruct))
			{
				CommonBaseStruct = Cast<UScriptStruct>(CommonBaseStruct->GetSuperStruct());
			}
			return CommonBaseStruct;
		};

		const UScriptStruct* CommonStruct = nullptr;
		EnumerateInstances([&CommonStruct, &FindCommonBaseStruct](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
		{
			if (ScriptStruct)
			{
				CommonStruct = FindCommonBaseStruct(ScriptStruct, CommonStruct);
			}
			return true; // Continue
		});

		return CommonStruct;
	}

	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override
	{
		// The returned instances need to be compatible with base structure.
		// This function returns empty instances in case they are not compatible, with the idea that we have as many instances as we have outer objects.
		EnumerateInstances([&OutInstances, ExpectedBaseStructure](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
		{
			TSharedPtr<FStructOnScope> Result;
			
			if (ExpectedBaseStructure && ScriptStruct && ScriptStruct->IsChildOf(ExpectedBaseStructure))
			{
				Result = MakeShared<FStructOnScope>(ScriptStruct, Memory);
				Result->SetPackage(Package);
			}

			OutInstances.Add(Result);

			return true; // Continue
		});
	}

	virtual bool IsPropertyIndirection() const override
	{
		return true;
	}

	virtual uint8* GetValueBaseAddress(uint8* ParentValueAddress, const UStruct* ExpectedBaseStructure) const override
	{
		if (!ParentValueAddress)
		{
			return nullptr;
		}

		FInstancedStruct& InstancedStruct = *reinterpret_cast<FInstancedStruct*>(ParentValueAddress);
		if (ExpectedBaseStructure && InstancedStruct.GetScriptStruct() && InstancedStruct.GetScriptStruct()->IsChildOf(ExpectedBaseStructure))
		{
			return InstancedStruct.GetMutableMemory();
		}
		
		return nullptr;
	}
	
protected:

	void EnumerateInstances(TFunctionRef<bool(const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)> InFunc) const
	{
		if (!StructProperty.IsValid())
		{
			return;
		}
		
		TArray<UPackage*> Packages;
		StructProperty->GetOuterPackages(Packages);

		StructProperty->EnumerateRawData([&InFunc, &Packages](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
		{
			const UScriptStruct* ScriptStruct = nullptr;
			uint8* Memory = nullptr;
			UPackage* Package = nullptr;
			if (FInstancedStruct* InstancedStruct = static_cast<FInstancedStruct*>(RawData))
			{
				ScriptStruct = InstancedStruct->GetScriptStruct();
				Memory = InstancedStruct->GetMutableMemory();
				if (ensureMsgf(Packages.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
				{
					Package = Packages[DataIndex];
				}
			}

			return InFunc(ScriptStruct, Memory, Package);
		});
	}
	
	TSharedPtr<IPropertyHandle> StructProperty;
};

////////////////////////////////////

bool FInstancedStructFilter::IsStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const UScriptStruct* InStruct, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs)
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

bool FInstancedStructFilter::IsUnloadedStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const FSoftObjectPath& InStructPath, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs)
{
	// User Defined Structs don't support inheritance, so only include them requested
	return bAllowUserDefinedStructs;
}

////////////////////////////////////

namespace UE::StructUtils::Private
{

FPropertyAccess::Result GetCommonScriptStruct(TSharedPtr<IPropertyHandle> StructProperty, const UScriptStruct*& OutCommonStruct)
{
	bool bHasResult = false;
	bool bHasMultipleValues = false;
	
	StructProperty->EnumerateConstRawData([&OutCommonStruct, &bHasResult, &bHasMultipleValues](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (const FInstancedStruct* InstancedStruct = static_cast<const FInstancedStruct*>(RawData))
		{
			const UScriptStruct* Struct = InstancedStruct->GetScriptStruct();

			if (!bHasResult)
			{
				OutCommonStruct = Struct;
			}
			else if (OutCommonStruct != Struct)
			{
				bHasMultipleValues = true;
			}

			bHasResult = true;
		}

		return true;
	});

	if (bHasMultipleValues)
	{
		return FPropertyAccess::MultipleValues;
	}
	
	return bHasResult ? FPropertyAccess::Success : FPropertyAccess::Fail;
}

} // UE::StructUtils::Private

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

FInstancedStructDataDetails::~FInstancedStructDataDetails()
{
	UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.Remove(UserDefinedStructReinstancedHandle);
}

void FInstancedStructDataDetails::OnUserDefinedStructReinstancedHandle(const UUserDefinedStruct& Struct)
{
	OnStructLayoutChanges();
}

void FInstancedStructDataDetails::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	OnRegenerateChildren = InOnRegenerateChildren;
}

TArray<TWeakObjectPtr<const UStruct>> FInstancedStructDataDetails::GetInstanceTypes() const
{
	TArray<TWeakObjectPtr<const UStruct>> Result;
	
	StructProperty->EnumerateConstRawData([&Result](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		TWeakObjectPtr<const UStruct>& Type = Result.AddDefaulted_GetRef();
		if (const FInstancedStruct* InstancedStruct = static_cast<const FInstancedStruct*>(RawData))
		{
			Result.Add(InstancedStruct->GetScriptStruct());
		}
		else
		{
			Result.Add(nullptr);
		}
		return true;
	});

	return Result;
}

void FInstancedStructDataDetails::OnStructLayoutChanges()
{
	if (StructProvider.IsValid())
	{
		// Reset the struct provider immediately, some update functions might get called with the old struct.
		StructProvider->Reset();
	}
	OnRegenerateChildren.ExecuteIfBound();
}

void FInstancedStructDataDetails::OnStructHandlePostChange()
{
	if (StructProvider.IsValid())
	{
		TArray<TWeakObjectPtr<const UStruct>> InstanceTypes = GetInstanceTypes();
		if (InstanceTypes != CachedInstanceTypes)
		{
			OnRegenerateChildren.ExecuteIfBound();
		}
	}
}

void FInstancedStructDataDetails::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	StructProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FInstancedStructDataDetails::OnStructHandlePostChange));
	if (!UserDefinedStructReinstancedHandle.IsValid())
	{
		UserDefinedStructReinstancedHandle = UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.AddSP(this, &FInstancedStructDataDetails::OnUserDefinedStructReinstancedHandle);
	}
}

void FInstancedStructDataDetails::GenerateChildContent(IDetailChildrenBuilder& ChildBuilder)
{
	// Add the rows for the struct
	TSharedRef<FInstancedStructProvider> NewStructProvider = MakeShared<FInstancedStructProvider>(StructProperty);
	
	TArray<TSharedPtr<IPropertyHandle>> ChildProperties = StructProperty->AddChildStructure(NewStructProvider);
	for (TSharedPtr<IPropertyHandle> ChildHandle : ChildProperties)
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
		OnChildRowAdded(Row);
	}

	StructProvider = NewStructProvider;

	CachedInstanceTypes = GetInstanceTypes();
}

void FInstancedStructDataDetails::Tick(float DeltaTime)
{
	// If the instance types change (e.g. due to selecting new struct type), we'll need to update the layout.
	TArray<TWeakObjectPtr<const UStruct>> InstanceTypes = GetInstanceTypes();
	if (InstanceTypes != CachedInstanceTypes)
	{
		OnRegenerateChildren.ExecuteIfBound();
	}
}

FName FInstancedStructDataDetails::GetName() const
{
	static const FName Name("InstancedStructDataDetails");
	return Name;
}


////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FInstancedStructDetails::MakeInstance()
{
	return MakeShared<FInstancedStructDetails>();
}

FInstancedStructDetails::~FInstancedStructDetails()
{
	if (OnObjectsReinstancedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectsReinstanced.Remove(OnObjectsReinstancedHandle);
	}
}

void FInstancedStructDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	static const FName NAME_BaseStruct = "BaseStruct";
	static const FName NAME_StructTypeConst = "StructTypeConst";

	PropUtils = StructCustomizationUtils.GetPropertyUtilities();
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities();

	OnObjectsReinstancedHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddSP(this, &FInstancedStructDetails::OnObjectsReinstanced);
	
	const bool bEnableStructSelection = !StructProperty->HasMetaData(NAME_StructTypeConst);

	BaseScriptStruct = nullptr;
	{
		const FString& BaseStructName = StructProperty->GetMetaData(NAME_BaseStruct);
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
					.ToolTipText(this, &FInstancedStructDetails::GetTooltipText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];
}

void FInstancedStructDetails::OnObjectsReinstanced(const FReplacementObjectMap& ObjectMap)
{
	// Force update the details when BP is compiled, since we may cached hold references to the old object or class.
	if (!ObjectMap.IsEmpty() && PropUtils.IsValid())
	{
		PropUtils->RequestRefresh();
	}
}

void FInstancedStructDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<FInstancedStructDataDetails> DataDetails = MakeShared<FInstancedStructDataDetails>(StructProperty);
	StructBuilder.AddCustomBuilder(DataDetails);
}

FText FInstancedStructDetails::GetDisplayValueString() const
{
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = UE::StructUtils::Private::GetCommonScriptStruct(StructProperty, CommonStruct);
	
	if (Result == FPropertyAccess::Success)
	{
		if (CommonStruct)
		{
			return CommonStruct->GetDisplayNameText();
		}
		return LOCTEXT("NullScriptStruct", "None");
	}
	if (Result == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}
	
	return FText::GetEmpty();
}

FText FInstancedStructDetails::GetTooltipText() const
{
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = UE::StructUtils::Private::GetCommonScriptStruct(StructProperty, CommonStruct);
	
	if (CommonStruct && Result == FPropertyAccess::Success)
	{
		return CommonStruct->GetToolTipText();
	}
	
	return GetDisplayValueString();
}

const FSlateBrush* FInstancedStructDetails::GetDisplayValueIcon() const
{
	const UScriptStruct* CommonStruct = nullptr;
	if (UE::StructUtils::Private::GetCommonScriptStruct(StructProperty, CommonStruct) == FPropertyAccess::Success)
	{
		return FSlateIconFinder::FindIconBrushForClass(UScriptStruct::StaticClass());
	}
	
	return nullptr;
}

TSharedRef<SWidget> FInstancedStructDetails::GenerateStructPicker()
{
	static const FName NAME_ExcludeBaseStruct = "ExcludeBaseStruct";
	static const FName NAME_HideViewOptions = "HideViewOptions";
	static const FName NAME_ShowTreeView = "ShowTreeView";

	const bool bExcludeBaseStruct = StructProperty->HasMetaData(NAME_ExcludeBaseStruct);
	const bool bAllowNone = !(StructProperty->GetMetaDataProperty()->PropertyFlags & CPF_NoClear);
	const bool bHideViewOptions = StructProperty->HasMetaData(NAME_HideViewOptions);
	const bool bShowTreeView = StructProperty->HasMetaData(NAME_ShowTreeView);

	TSharedRef<FInstancedStructFilter> StructFilter = MakeShared<FInstancedStructFilter>();
	StructFilter->BaseStruct = BaseScriptStruct;
	StructFilter->bAllowUserDefinedStructs = BaseScriptStruct == nullptr; // Only allow user defined structs when BaseStruct is not set.
	StructFilter->bAllowBaseStruct = !bExcludeBaseStruct;

	const UScriptStruct* SelectedStruct = nullptr;
	const FPropertyAccess::Result Result = UE::StructUtils::Private::GetCommonScriptStruct(StructProperty, SelectedStruct);
	
	FStructViewerInitializationOptions Options;
	Options.bShowNoneOption = bAllowNone;
	Options.StructFilter = StructFilter;
	Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
	Options.DisplayMode = bShowTreeView ? EStructViewerDisplayMode::TreeView : EStructViewerDisplayMode::ListView;
	Options.bAllowViewOptions = !bHideViewOptions;
	Options.SelectedStruct = SelectedStruct;
	
	FOnStructPicked OnPicked(FOnStructPicked::CreateSP(this, &FInstancedStructDetails::OnStructPicked));

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

		StructProperty->EnumerateRawData([InStruct](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (FInstancedStruct* InstancedStruct = static_cast<FInstancedStruct*>(RawData))
			{
				InstancedStruct->InitializeAs(InStruct);
			}
			return true;
		});

		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		StructProperty->NotifyFinishedChangingProperties();

		// Property tree will be invalid after changing the struct type, force update.
		if (PropUtils.IsValid())
		{
			PropUtils->ForceRefresh();
		}
	}

	ComboButton->SetIsOpen(false);
}

#undef LOCTEXT_NAMESPACE
