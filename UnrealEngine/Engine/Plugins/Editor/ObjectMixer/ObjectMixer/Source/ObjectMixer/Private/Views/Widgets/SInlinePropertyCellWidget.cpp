// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/Widgets/SInlinePropertyCellWidget.h"

#include "Views/List/ObjectMixerEditorListRowData.h"
#include "Views/List/ObjectMixerUtils.h"

#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ISinglePropertyView.h"
#include "Layout/WidgetPath.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "ObjectMixerEditor"
	
void SInlinePropertyCellWidget::Construct(
	const FArguments& InArgs, const FName InColumnName, const TSharedRef<ISceneOutlinerTreeItem> RowPtr)
{
	const bool bGetHybridComponent = true;
	UObject* ObjectRef = FObjectMixerUtils::GetRowObject(RowPtr, bGetHybridComponent);
	if (!ObjectRef || InColumnName.IsEqual(NAME_None))
	{
		return;
	}

	ColumnName = InColumnName;
	WeakRowPtr = RowPtr;
	
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FSinglePropertyParams Params;
	Params.NamePlacement = EPropertyNamePlacement::Hidden;
	
	const TSharedPtr<ISinglePropertyView> SinglePropertyView =
		PropertyEditorModule.CreateSingleProperty(ObjectRef, InColumnName, Params
	);

	if (SinglePropertyView)
	{
		if (const TSharedPtr<IPropertyHandle> Handle = SinglePropertyView->GetPropertyHandle())
		{
			FObjectMixerUtils::GetRowData(RowPtr)->PropertyNamesToHandles.Add(InColumnName, Handle);
				
			// Simultaneously edit all selected rows with a similar property
			if (InArgs._OnPropertyValueChanged.IsBound())
			{
				Handle->SetOnPropertyValueChangedWithData(InArgs._OnPropertyValueChanged);
				Handle->SetOnChildPropertyValueChangedWithData(InArgs._OnPropertyValueChanged);
			}

			ChildSlot
			[
				SNew(SBox)
				.Visibility(EVisibility::Visible)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SinglePropertyView.ToSharedRef()
				]
			];
		}
	}
}

FReply SInlinePropertyCellWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() ==  EKeys::RightMouseButton)
	{
		return MakePropertyContextMenu(MouseEvent);
	}
	
	return SCompoundWidget::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SInlinePropertyCellWidget::MakePropertyContextMenu(const FPointerEvent& MouseEvent)
{
	FMenuBuilder MenuBuilder(true, nullptr, nullptr, true);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CopyProperty", "Copy"),
		LOCTEXT("CopyProperty_ToolTip", "Copy this property value"),
		FSlateIcon(FCoreStyle::Get().GetStyleSetName(), "GenericCommands.Copy"),
		FExecuteAction::CreateRaw(this, &SInlinePropertyCellWidget::CopyPropertyValue));

	FUIAction PasteAction = 
		FUIAction(
			FExecuteAction::CreateRaw(this, &SInlinePropertyCellWidget::PastePropertyValue),
			FCanExecuteAction::CreateRaw(this, &SInlinePropertyCellWidget::CanPaste)
		)
	;

	MenuBuilder.AddMenuEntry(
		LOCTEXT("PasteProperty", "Paste"),
		LOCTEXT("PasteProperty_ToolTip", "Paste the copied value here"),
		FSlateIcon(FCoreStyle::Get().GetStyleSetName(), "GenericCommands.Paste"),
		PasteAction);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CopyPropertyDisplayName", "Copy Display Name"),
		LOCTEXT("CopyPropertyDisplayName_ToolTip", "Copy this property display name"),
		FSlateIcon(FCoreStyle::Get().GetStyleSetName(), "GenericCommands.Copy"),
		FExecuteAction::CreateRaw(this, &SInlinePropertyCellWidget::CopyPropertyName));

	const FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

	FSlateApplication::Get().PushMenu(
		AsShared(), WidgetPath, MenuBuilder.MakeWidget(),
		MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect::ContextMenu
	);

	return FReply::Handled();
}

void SInlinePropertyCellWidget::CopyPropertyValue() const
{
	check(WeakRowPtr.IsValid());
			
	const TWeakPtr<IPropertyHandle>* PropertyHandle = FObjectMixerUtils::GetRowData(WeakRowPtr.Pin())->PropertyNamesToHandles.Find(ColumnName);
	if (PropertyHandle && PropertyHandle->IsValid())
	{
		FString Value;
		const TSharedPtr<IPropertyHandle> PinnedHandle = PropertyHandle->Pin();
		if (PinnedHandle->GetValueAsFormattedString(Value, PPF_Copy) == FPropertyAccess::Success)
		{
			FPlatformApplicationMisc::ClipboardCopy(*Value);
		}
	}
}

void SInlinePropertyCellWidget::PastePropertyValue() const
{
	check(WeakRowPtr.IsValid());
			
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
			
	const TWeakPtr<IPropertyHandle>* PropertyHandle = FObjectMixerUtils::GetRowData(WeakRowPtr.Pin())->PropertyNamesToHandles.Find(ColumnName);
	if (!ClipboardContent.IsEmpty() && PropertyHandle && PropertyHandle->IsValid())
	{
		(*PropertyHandle).Pin()->SetValueFromFormattedString(ClipboardContent, EPropertyValueSetFlags::InstanceObjects);
	}
}

bool SInlinePropertyCellWidget::CanPaste() const
{
	check(WeakRowPtr.IsValid());
			
	FString ClipboardContent;
			
	const TWeakPtr<IPropertyHandle>* PropertyHandle = FObjectMixerUtils::GetRowData(WeakRowPtr.Pin())->PropertyNamesToHandles.Find(ColumnName);
	if (PropertyHandle && PropertyHandle->IsValid())
	{
		if ((*PropertyHandle).Pin()->IsEditConst())
		{
			return false;
		}
	}

	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
	return !ClipboardContent.IsEmpty();
}

void SInlinePropertyCellWidget::CopyPropertyName() const
{
	check(WeakRowPtr.IsValid());
			
	const TWeakPtr<IPropertyHandle>* PropertyHandle = FObjectMixerUtils::GetRowData(WeakRowPtr.Pin())->PropertyNamesToHandles.Find(ColumnName);
	if (PropertyHandle && PropertyHandle->IsValid())
	{
		const FString Value = (*PropertyHandle).Pin()->GetPropertyDisplayName().ToString();
		if (!Value.IsEmpty())
		{
			FPlatformApplicationMisc::ClipboardCopy(*Value);
		}
	}
}

#undef LOCTEXT_NAMESPACE
