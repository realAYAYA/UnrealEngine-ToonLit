// Copyright Epic Games, Inc. All Rights Reserved.


#include "InstanceDataObjectFixupDetailCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "InstanceDataObjectFixupPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UObject/PropertyBagRepository.h"

#define LOCTEXT_NAMESPACE "InstanceDataObjectFixupDetails"

/////////////////////////////////////////////////////////////////
// FInstanceDataObjectFixupDetailCustomization
/////////////////////////////////////////////////////////////////

FInstanceDataObjectFixupDetailCustomization::FInstanceDataObjectFixupDetailCustomization(const TSharedRef<FInstanceDataObjectFixupPanel>& InDiffPanel)
	: DiffPanel(InDiffPanel)
{
	
}

FInstanceDataObjectFixupDetailCustomization::~FInstanceDataObjectFixupDetailCustomization()
{
}

void FInstanceDataObjectFixupDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const TSharedPtr<FInstanceDataObjectFixupPanel> Panel = DiffPanel.Pin();
	if (!Panel)
	{
		return;
	}
	
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.IsEmpty())
	{
		return;
	}
	TArray<FName> CategoryNames;
	DetailBuilder.GetCategoryNames(CategoryNames);
	for (const FName CategoryName : CategoryNames)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName);
		TArray<TSharedRef<IPropertyHandle>> Handles;
		Category.GetDefaultProperties(Handles, true, true);
		for (const TSharedRef<IPropertyHandle>& Handle : Handles)
		{
			CustomizeHandle(Handle, DetailBuilder);
		}
	}
}

bool FInstanceDataObjectFixupDetailCustomization::IsHidden(const TSharedPtr<IPropertyHandle>& PropertyHandle) const
{
	const TSharedPtr<FInstanceDataObjectFixupPanel> Panel = DiffPanel.Pin();
	if (!Panel)
	{
		return true;
	}

	if (const FProperty* Property = PropertyHandle->GetProperty())
	{
		if (Panel->HasViewFlag(FInstanceDataObjectFixupPanel::EViewFlags::HideLooseProperties) &&
			Property->GetBoolMetaData(TEXT("isLoose")))
		{
			return true;
		}

		if (Panel->HasViewFlag(FInstanceDataObjectFixupPanel::EViewFlags::IncludeOnlySetBySerialization))
		{
			if (!Panel->IsInRedirectedPropertyTree(*PropertyHandle->CreateFPropertyPath()))
			{
				return true;
			}
		}
	}
	return false;
}

void FInstanceDataObjectFixupDetailCustomization::CustomizeHandle(const TSharedRef<IPropertyHandle>& Handle, IDetailLayoutBuilder& DetailBuilder)
{
	if (IsHidden(Handle))
	{
		DetailBuilder.HideProperty(Handle);
	}
	else
	{
		// recurse into children
		uint32 NumChildren = 0;
		Handle->GetNumChildren(NumChildren);
		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			CustomizeHandle(Handle->GetChildHandle(ChildIndex).ToSharedRef(), DetailBuilder);
		}
	}
}


/////////////////////////////////////////////////////////////////
// FInstanceDataObjectNameWidgetOverride
/////////////////////////////////////////////////////////////////

FInstanceDataObjectNameWidgetOverride::FInstanceDataObjectNameWidgetOverride(const TSharedRef<FInstanceDataObjectFixupPanel>& InDiffPanel)
	: DiffPanel(InDiffPanel)
{
}

TSharedRef<SWidget> FInstanceDataObjectNameWidgetOverride::CustomizeName(TSharedRef<SWidget> InnerNameContent, FPropertyPath& Path)
{
	const TSharedRef<SWidget> NameContent = SNew(SWidgetSwitcher)
		.WidgetIndex(this, &FInstanceDataObjectNameWidgetOverride::GetNameWidgetIndex, Path)
		+SWidgetSwitcher::Slot()
		[
			InnerNameContent
		]
		+SWidgetSwitcher::Slot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Visibility(this, &FInstanceDataObjectNameWidgetOverride::DeletionSymbolVisibility, Path)
				.Text(LOCTEXT("MarkedForDeletion", "❌"))
			]
			+SHorizontalBox::Slot()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AutoWidth()
			[
				SNew(SComboButton)
				.Visibility(EVisibility::Visible)
				.OnGetMenuContent_Raw(this, &FInstanceDataObjectNameWidgetOverride::GeneratePropertyRedirectMenu, Path)
				.ButtonContent()
				[
					InnerNameContent
				]
			]
		];
	return NameContent;
}


TSet<FPropertyPath> FInstanceDataObjectNameWidgetOverride::GetRedirectOptions(const UStruct* Struct, void* Value) const
{
	TSet<FPropertyPath> Result;
	GetRedirectOptions(Struct, Value, {}, Result);
	return Result;
}

void FInstanceDataObjectNameWidgetOverride::GetRedirectOptions(const UStruct* Struct, void* Value, const FPropertyPath& Path, TSet<FPropertyPath>& OutPaths) const
{
	for (FProperty* SubProperty : TFieldRange<FProperty>(Struct))
	{
		if (SubProperty->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}
		if (!SubProperty->HasAnyPropertyFlags(CPF_Edit | CPF_EditConst))
		{
			continue;
		}
		if (SubProperty->ArrayDim == 1)
		{
			const FPropertyPath SubPath = Path.ExtendPath(FPropertyInfo(SubProperty)).Get();
			GetRedirectOptions(SubProperty, SubProperty->ContainerPtrToValuePtr<void>(Value), SubPath, OutPaths);
		}
		else
		{
			for (int32 ArrayIndex = 0; ArrayIndex < SubProperty->ArrayDim; ++ArrayIndex)
			{
    			const FPropertyPath SubPath = Path.ExtendPath(FPropertyInfo(SubProperty, ArrayIndex)).Get();
    			GetRedirectOptions(SubProperty, SubProperty->ContainerPtrToValuePtr<void>(Value, ArrayIndex), SubPath, OutPaths);
			}
		}
    }
}

void FInstanceDataObjectNameWidgetOverride::GetRedirectOptions(const FProperty* Property, void* Value, const FPropertyPath& Path, TSet<FPropertyPath>& OutPaths) const
{
	if (Property->GetBoolMetaData(TEXT("isLoose")))
	{
		// don't include loose properties as options
		return;
	}

	if (!DiffPanel.Pin()->RedirectedPropertyTree->Find(Path))
	{
		OutPaths.Add(Path);
	}
	
	if (const FStructProperty* AsStructProperty = CastField<FStructProperty>(Property))
	{
		GetRedirectOptions(AsStructProperty->Struct, Value, Path, OutPaths);
	}
	if (const FObjectProperty* AsObjectProperty = CastField<FObjectProperty>(Property))
	{
		if (AsObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference))
		{
			TObjectPtr<UObject> Object = AsObjectProperty->GetObjectPropertyValue(Value);
			UE::FPropertyBagRepository& PropertyBagRepository = UE::FPropertyBagRepository::Get();
			if (UObject* Found = PropertyBagRepository.FindInstanceDataObject(Object))
			{
				Object = Found;
			}
			GetRedirectOptions(Object->GetClass(), Object, Path, OutPaths);
		}
	}
	else if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper Array(AsArrayProperty, Value);
		for (int32 ArrayIndex = 0; ArrayIndex < Array.Num(); ++ArrayIndex)
		{
			const FPropertyPath SubPath = Path.ExtendPath(FPropertyInfo(AsArrayProperty->Inner, ArrayIndex)).Get();
			GetRedirectOptions(AsArrayProperty->Inner, Array.GetElementPtr(ArrayIndex), SubPath, OutPaths);
		}
	}
	else if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
	{
		FScriptSetHelper Set(AsSetProperty, Value);
		for (FScriptSetHelper::FIterator Iterator = Set.CreateIterator(); Iterator; ++Iterator)
		{
			const FPropertyPath SubPath = Path.ExtendPath(FPropertyInfo(AsSetProperty->ElementProp, Iterator.GetLogicalIndex())).Get();
			GetRedirectOptions(AsSetProperty->ElementProp, Set.GetElementPtr(Iterator), SubPath, OutPaths);
		}
	}
	else if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper Map(AsMapProperty, Value);
		for (FScriptMapHelper::FIterator Iterator = Map.CreateIterator(); Iterator; ++Iterator)
		{
			const FPropertyPath SubPath = Path.ExtendPath(FPropertyInfo(AsMapProperty->ValueProp, Iterator.GetLogicalIndex())).Get();
			GetRedirectOptions(AsMapProperty->ValueProp, Map.GetValuePtr(Iterator), SubPath, OutPaths);
		}
	}
}

int32 FInstanceDataObjectNameWidgetOverride::GetNameWidgetIndex(FPropertyPath Path) const
{
	if (const TSharedPtr<FInstanceDataObjectFixupPanel> Panel = DiffPanel.Pin())
	{
		if (Panel->HasViewFlag(FInstanceDataObjectFixupPanel::EViewFlags::AllowRemapLooseProperties))
		{
			if (Panel->RevertInfo.Contains(Path))
			{
				return DisplayRedirectMenu;
			}
			if (const FProperty* Property = Path.GetLeafMostProperty().Property.Get())
			{
				if (Property->GetBoolMetaData(TEXT("isLoose")))
				{
					return DisplayRedirectMenu;
				}
			}
		}
	}
	return DisplayRegularName;
}

TSharedRef<SWidget> FInstanceDataObjectNameWidgetOverride::GeneratePropertyRedirectMenu(FPropertyPath Path) const
{
	FMenuBuilder MenuBuilder(true, nullptr);
	
	const TSharedPtr<FInstanceDataObjectFixupPanel> Panel = DiffPanel.Pin();
	if (!Panel)
	{
		return MenuBuilder.MakeWidget();
	}
	const FPropertyPath& OriginalPath = Panel->GetOriginalPath(Path);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ResetRedirect", "Reset"));
	if (OriginalPath != Path || Panel->MarkedForDelete.Contains(OriginalPath))
	{
		FText OriginalPathText = FText::FromString(OriginalPath.ToString());
		FText Tooltip = FText::Format(LOCTEXT("ResetTooltip", "Reset back to {0}"), OriginalPathText);
		MenuBuilder.AddMenuEntry(OriginalPathText, Tooltip, FSlateIcon()
						, FUIAction(FExecuteAction::CreateSP(Panel.Get(), &FInstanceDataObjectFixupPanel::OnRedirectProperty, Path, OriginalPath))
						, NAME_None
						, EUserInterfaceActionType::RadioButton);
	}
	MenuBuilder.EndSection();
	
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("Delete", "Delete"));
	{
		if (!Panel->MarkedForDelete.Contains(OriginalPath)) // check that it wasn't already marked as deleted
		{
			FText DisplayName = LOCTEXT("MarkForDeletion", "Mark For Deletion");
			FText Tooltip = LOCTEXT("MarkForDeletionTooltip", "Mark this property for deletion");
			MenuBuilder.AddMenuEntry(DisplayName, Tooltip, FSlateIcon()
						, FUIAction(FExecuteAction::CreateSP(Panel.Get(), &FInstanceDataObjectFixupPanel::OnMarkForDelete, Path))
						, NAME_None
						, EUserInterfaceActionType::RadioButton);
		}
	}
	MenuBuilder.EndSection();

	UObject* FirstInstanceDataObject = Panel->Instances[0];
	TSet<FPropertyPath> RedirectOptions = GetRedirectOptions(FirstInstanceDataObject->GetClass(), FirstInstanceDataObject);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("MoveProperty", "Move"));
	{
		for (const FPropertyPath& Option : RedirectOptions)
		{
			FProperty* ThisProperty = Path.GetLeafMostProperty().Property.Get();
			FProperty* OptionProperty = Option.GetLeafMostProperty().Property.Get();
			if (ThisProperty->GetFName() == OptionProperty->GetFName())
			{
				if (OptionProperty->SameType(ThisProperty))
				{
					FText DisplayName = FText::FromString(Option.ToString());
					FText Tooltip = FText::Format(LOCTEXT("MovePropertyTooltip", "Move property to '{0}'"), DisplayName);
					MenuBuilder.AddMenuEntry(DisplayName, Tooltip, FSlateIcon()
					, FUIAction(FExecuteAction::CreateSP(Panel.Get(), &FInstanceDataObjectFixupPanel::OnRedirectProperty, Path, Option))
					, NAME_None
					, EUserInterfaceActionType::RadioButton);
				}
			}
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("RenameProperty", "Rename"));
	{
		for (const FPropertyPath& Option : RedirectOptions)
		{
			FProperty* ThisProperty = Path.GetLeafMostProperty().Property.Get();
			FProperty* OptionProperty = Option.GetLeafMostProperty().Property.Get();
			if (ThisProperty->GetFName() == OptionProperty->GetFName())
			{
				continue; // handled in the "move" category
			}
			if (OptionProperty->SameType(ThisProperty))
			{
				FText DisplayName = FText::FromString(Option.ToString());
				FText Tooltip = FText::Format(LOCTEXT("RenamePropertyTooltip", "Rename property to '{0}'"), DisplayName);
				MenuBuilder.AddMenuEntry(DisplayName, Tooltip, FSlateIcon()
				, FUIAction(FExecuteAction::CreateSP(Panel.Get(), &FInstanceDataObjectFixupPanel::OnRedirectProperty, Path, Option))
				, NAME_None
				, EUserInterfaceActionType::RadioButton);
			}
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ChangeToDifferingTypeConversion", "Convert Type"));
	{
		for (const FPropertyPath& Option : RedirectOptions)
		{
			FProperty* ThisProperty = Path.GetLeafMostProperty().Property.Get();
			FProperty* OptionProperty = Option.GetLeafMostProperty().Property.Get();
			if (OptionProperty->SameType(ThisProperty))
			{
				continue; // same type handled above
			}
			if (ThisProperty->GetFName() != OptionProperty->GetFName())
			{
				continue; // renames to handled below
			}
			if (FInstanceDataObjectFixupPanel::FTypeConverter Converter = Panel->CreateTypeConverter(Path, Option))
			{
				FText DisplayName = FText::FromString(Option.ToString());
				FText TypeName = FText::FromName(Option.GetLeafMostProperty().Property->GetID());
				FText Warning = Converter.GetWarning();
				FText Tooltip = FText::Format(LOCTEXT("ConvertTypeTooltip", "Change type to {0}"), TypeName);
				if (!Warning.IsEmpty())
				{
					DisplayName = FText::Format(LOCTEXT("ConvertTypeDisplayNameWithWarning", "⚠{0}"), DisplayName);
					Tooltip = FText::Format(LOCTEXT("ConvertTypeTooltipWithWarning", "⚠{0}\n Warning: {1}"), Tooltip, Warning);
				}
				MenuBuilder.AddMenuEntry(DisplayName, Tooltip, FSlateIcon()
				, FUIAction(FExecuteAction::CreateSP(Panel.Get(), &FInstanceDataObjectFixupPanel::OnRedirectProperty, Path, Option, Converter))
				, NAME_None
				, EUserInterfaceActionType::RadioButton);
			}
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("RenameToDifferingTypeConversion", "Convert Type and Rename"));
	{
		for (const FPropertyPath& Option : RedirectOptions)
		{
			FProperty* ThisProperty = Path.GetLeafMostProperty().Property.Get();
			FProperty* OptionProperty = Option.GetLeafMostProperty().Property.Get();
			if (OptionProperty->SameType(ThisProperty))
			{
				continue; // same type handled above
			}
			if (ThisProperty->GetFName() == OptionProperty->GetFName())
			{
				continue; // renames to handled below
			}
			if (FInstanceDataObjectFixupPanel::FTypeConverter Converter = Panel->CreateTypeConverter(Path, Option))
			{
				FText DisplayName = FText::FromString(Option.ToString());
				FText PropDisplayName = Option.GetLeafMostProperty().Property->GetDisplayNameText();
				FText TypeName = FText::FromName(Option.GetLeafMostProperty().Property->GetID());
				FText Warning = Converter.GetWarning();
				FText Tooltip = FText::Format(LOCTEXT("ConvertTypeAndRenameTooltip", "Change type to {0} and rename to {1}"), TypeName, PropDisplayName);
				if (!Warning.IsEmpty())
				{
					DisplayName = FText::Format(LOCTEXT("ConvertTypeAndRenameDisplayNameWithWarning", "⚠{0}"), DisplayName);
					Tooltip = FText::Format(LOCTEXT("ConvertTypeAndRenameTooltipWithWarning", "⚠{0}\n Warning: {1}"), Tooltip, Warning);
				}
				MenuBuilder.AddMenuEntry(DisplayName, Tooltip, FSlateIcon()
				, FUIAction(FExecuteAction::CreateSP(Panel.Get(), &FInstanceDataObjectFixupPanel::OnRedirectProperty, Path, Option, Converter))
				, NAME_None
				, EUserInterfaceActionType::RadioButton);
			}
		}
	}
	MenuBuilder.EndSection();
	
	return MenuBuilder.MakeWidget();
}

EVisibility FInstanceDataObjectNameWidgetOverride::DeletionSymbolVisibility(FPropertyPath Path) const
{
	if (const TSharedPtr<FInstanceDataObjectFixupPanel> Panel = DiffPanel.Pin())
	{
		return Panel->MarkedForDelete.Contains(Path) ? EVisibility::Visible :  EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

EVisibility FInstanceDataObjectNameWidgetOverride::ValueContentVisibility(FPropertyPath Path) const
{
	if (const TSharedPtr<FInstanceDataObjectFixupPanel> Panel = DiffPanel.Pin())
	{
		return Panel->MarkedForDelete.Contains(Path) ? EVisibility::Collapsed :  EVisibility::Visible;
	}
	return EVisibility::Visible;
}

/////////////////////////////////////////////////////////////////
// FHideLoosePropertiesCustomization
/////////////////////////////////////////////////////////////////

FHideLoosePropertiesCustomization::~FHideLoosePropertiesCustomization()
{
}

void FHideLoosePropertiesCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.IsEmpty())
	{
		return;
	}
	TArray<FName> CategoryNames;
	DetailBuilder.GetCategoryNames(CategoryNames);
	for (const FName CategoryName : CategoryNames)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategoryAllowNone(CategoryName);
		TArray<TSharedRef<IPropertyHandle>> Handles;
		Category.GetDefaultProperties(Handles, true, true);
		for (const TSharedRef<IPropertyHandle>& Handle : Handles)
		{
			CustomizeHandle(Handle, DetailBuilder);
		}
	}
}

void FHideLoosePropertiesCustomization::CustomizeHandle(const TSharedRef<IPropertyHandle>& Handle, IDetailLayoutBuilder& DetailBuilder)
{
	if (Handle->GetBoolMetaData(TEXT("isLoose")))
	{
		DetailBuilder.HideProperty(Handle);
	}
	else
	{
		// recurse into children
		uint32 NumChildren = 0;
		Handle->GetNumChildren(NumChildren);
		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			CustomizeHandle(Handle->GetChildHandle(ChildIndex).ToSharedRef(), DetailBuilder);
		}
	}
}

#undef LOCTEXT_NAMESPACE
