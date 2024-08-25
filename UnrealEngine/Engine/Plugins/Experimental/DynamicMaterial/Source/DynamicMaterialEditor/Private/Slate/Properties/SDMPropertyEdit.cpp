// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/SDMPropertyEdit.h"
#include "Components/DMMaterialValue.h"
#include "DMWorldSubsystem.h"
#include "DetailLayoutBuilder.h"
#include "DetailRowMenuContext.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailKeyframeHandler.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Slate/SDMEditor.h"
#include "Slate/SDMSlot.h"
#include "Slate/SDMComponentEdit.h"
#include "ToolMenuContext.h"
#include "ToolMenus.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEdit"

void SDMPropertyEdit::Construct(const FArguments& InArgs)
{
	ComponentEditWidgetWeak = InArgs._ComponentEditWidget;
	PropertyHandle = InArgs._PropertyHandle;
	PropertyMaterialValue = InArgs._PropertyMaterialValue;

	if (PropertyHandle && PropertyHandle->IsValidHandle())
	{
		Property = PropertyHandle->GetProperty();
	}
	else if (PropertyMaterialValue.IsValid())
	{
		Property = PropertyMaterialValue->GetClass()->FindPropertyByName("Value");
	}
 
	TSharedRef<SHorizontalBox> ComponentBox = SNew(SHorizontalBox);

	if (ensure(Property))
	{
		for (int32 ComponentIndex = 0; ComponentIndex < InArgs._InputCount; ++ComponentIndex)
		{
			static const FMargin FullMargin(2.f, 0.f, 2.f, 0.f);
			static const FMargin PartialMargin(0.f, 0.f, 2.f, 0.f);

			const FMargin SlotPadding = ComponentIndex == 0 ? FullMargin : PartialMargin;
			const FMargin ComponentPadding = InArgs._InputCount > 1 ? SlotPadding : FullMargin;
			const float MaxWidth = GetMaxWidthForWidget(ComponentIndex);

			if (MaxWidth <= KINDA_SMALL_NUMBER)
			{
				ComponentBox->AddSlot()
					.FillWidth(1.0f)			
					.Padding(ComponentPadding)
					[
						GetComponentWidget(ComponentIndex)
					];			
			}
			else
			{
				ComponentBox->AddSlot()
					.FillWidth(1.0f)
					.Padding(ComponentPadding)
					[
						SNew(SBox)
						.MinDesiredWidth(MaxWidth)
						.MaxDesiredWidth(MaxWidth)
						[
							GetComponentWidget(ComponentIndex)
						]
					];
			}
		}
	}

	const FMargin ParentPadding = FMargin(0.0f, 1.0f);
 
	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(ParentPadding)
		[
			ComponentBox
		]
	];
}

UDMMaterialValue* SDMPropertyEdit::GetValue() const
{
	return PropertyMaterialValue.Get();
}

TSharedPtr<IPropertyHandle> SDMPropertyEdit::GetPropertyHandle() const
{
	return PropertyHandle;
}

FReply SDMPropertyEdit::CreateRightClickDetailsMenu(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent,
	TWeakPtr<SDMPropertyEdit> InPropertyEditorWeak)
{
	if (InPointerEvent.GetEffectingButton() != EKeys::RightMouseButton)
	{
		return FReply::Unhandled();
	}

	TSharedPtr<SDMPropertyEdit> PropertyEditor = InPropertyEditorWeak.Pin();

	if (!PropertyEditor.IsValid() || !PropertyEditor->IsEnabled())
	{
		return FReply::Unhandled();
	}

	UToolMenus* Menus = UToolMenus::Get();
	check(Menus);

	static const FName DetailViewContextMenuName = UE::PropertyEditor::RowContextMenuName;

	if (!Menus->IsMenuRegistered(DetailViewContextMenuName))
	{
		return FReply::Unhandled();
	}

	const TSharedPtr<IPropertyHandle> RowPropertyHandle = PropertyEditor->GetPropertyHandle();

	if (!RowPropertyHandle.IsValid())
	{
		return FReply::Unhandled();
	}

	UDetailRowMenuContext* RowMenuContext = NewObject<UDetailRowMenuContext>();
	RowMenuContext->PropertyHandles.Add(RowPropertyHandle);

	const FToolMenuContext ToolMenuContext(RowMenuContext);

	FSlateApplication::Get().PushMenu(
		PropertyEditor.ToSharedRef(),
		FWidgetPath(),
		Menus->GenerateWidget(DetailViewContextMenuName, ToolMenuContext),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect::ContextMenu
	);

	return FReply::Handled();
}

TSharedRef<SHorizontalBox> SDMPropertyEdit::AddWidgetLabel(const FText& InLabel, TSharedRef<SWidget> ValueWidget, TSharedPtr<SHorizontalBox> AddOnToWidget)
{
	static const FMargin FirstSlotPadding = FMargin(2.0f);
	static const FMargin OtherSlotPadding = FMargin(10.0f, 2.0f, 2.0f, 2.0f);
	static const FSlateFontInfo DetailsFontInfo = IDetailLayoutBuilder::GetDetailFont();

	TSharedRef<SHorizontalBox> ReturnWidget = AddOnToWidget.IsValid() ? AddOnToWidget.ToSharedRef() : SNew(SHorizontalBox);

	ReturnWidget->AddSlot()
		.AutoWidth()
		.Padding(AddOnToWidget.IsValid() ? OtherSlotPadding : FirstSlotPadding)
		[
			SNew(SBox)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InLabel)
				.Font(DetailsFontInfo)
			]
		];

	ReturnWidget->AddSlot()
		.FillWidth(1.0f)
		.Padding(OtherSlotPadding)
		[
			ValueWidget
		];

	return ReturnWidget;
}

void SDMPropertyEdit::CreateKey(TWeakObjectPtr<UDMMaterialValue> InValueWeak)
{
	if (!InValueWeak.IsValid())
	{
		return;
	}
 
	TSharedPtr<IPropertyHandle> PropertyHandle = InValueWeak->GetPropertyHandle();
	if (!PropertyHandle.IsValid())
	{
		return;
	}
 
	const UWorld* World = InValueWeak->GetWorld();
	if (!World)
	{
		return;
	}
 
	const UDMWorldSubsystem* WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>();
	if (!WorldSubsystem)
	{
		return;
	}
 
	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler = WorldSubsystem->GetKeyframeHandler();
	if (!KeyframeHandler.IsValid())
	{
		return;
	}
 
	if (!KeyframeHandler->IsPropertyKeyable(InValueWeak->GetClass(), *PropertyHandle))
	{
		return;
	}
 
	KeyframeHandler->OnKeyPropertyClicked(*PropertyHandle);
}
 
TSharedRef<SWidget> SDMPropertyEdit::GetComponentWidget(int32 InIndex)
{
	return SNullWidget::NullWidget;
}

float SDMPropertyEdit::GetMaxWidthForWidget(int32 InIndex) const
{
	return 0.f;
}
 
TSharedRef<SWidget> SDMPropertyEdit::CreateCheckbox(const TAttribute<ECheckBoxState>& InValueAttr, const FOnCheckStateChanged& InChangeFunc)
{
	return SNew(SCheckBox)
		.IsChecked(InValueAttr)
		.OnCheckStateChanged(InChangeFunc);
}
 
void SDMPropertyEdit::OnSpinBoxStartScrubbing(FText InTransactionDescription)
{
	StartTransaction(InTransactionDescription);
}
 
void SDMPropertyEdit::OnSpinBoxEndScrubbing(float InValue)
{
	EndTransaction();
}
 
void SDMPropertyEdit::OnSpinBoxValueChange(float InValue, const SSpinBox<float>::FOnValueChanged InChangeFunc)
{
	if (ScrubbingTransaction.IsValid())
	{
		InChangeFunc.ExecuteIfBound(InValue);
	}
}
 
void SDMPropertyEdit::OnSpinBoxCommit(float InValue, ETextCommit::Type InCommitType, const SSpinBox<float>::FOnValueChanged InChangeFunc,
	FText InTransactionDescription)
{
	if (!ScrubbingTransaction.IsValid())
	{
		StartTransaction(InTransactionDescription);
		InChangeFunc.ExecuteIfBound(InValue);
		EndTransaction();
	}
}
 
TSharedRef<SSpinBox<float>> SDMPropertyEdit::CreateSpinBox(const TAttribute<float>& InValue, const SSpinBox<float>::FOnValueChanged& InOnValueChanged,
	const FText& InTransactionDescription, const FFloatInterval* InValueRange)
{
	return
		SNew(SSpinBox<float>)
		.ClearKeyboardFocusOnCommit(false)
		.Justification(ETextJustify::Right)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.MaxFractionalDigits(3)
		.MinDesiredWidth(50.f)
		.OnBeginSliderMovement(this, &SDMPropertyEdit::OnSpinBoxStartScrubbing, InTransactionDescription)
		.OnEndSliderMovement(this, &SDMPropertyEdit::OnSpinBoxEndScrubbing)
		.OnValueChanged(this, &SDMPropertyEdit::OnSpinBoxValueChange, InOnValueChanged)
		.OnValueCommitted(this, &SDMPropertyEdit::OnSpinBoxCommit, InOnValueChanged, InTransactionDescription)
		.Value(InValue)
		.MinValue(InValueRange ? TOptional<float>(InValueRange->Min) : TOptional<float>())
		.MinSliderValue(InValueRange ? TOptional<float>(InValueRange->Min) : TOptional<float>())
		.MaxValue(InValueRange ? TOptional<float>(InValueRange->Max) : TOptional<float>())
		.MaxSliderValue(InValueRange ? TOptional<float>(InValueRange->Max) : TOptional<float>());
}
 
void SDMPropertyEdit::OnColorPickedCommitted(FLinearColor InNewColor, const FOnLinearColorValueChanged InChangeFunc)
{
	InChangeFunc.ExecuteIfBound(InNewColor);
}
 
void SDMPropertyEdit::OnColorPickerCancelled(FLinearColor InOldColor, const FOnLinearColorValueChanged InChangeFunc)
{
	InChangeFunc.ExecuteIfBound(InOldColor);
}
 
void SDMPropertyEdit::OnColorPickerClosed(const TSharedRef<SWindow>& InClosedWindow)
{
	EndTransaction();
}
 
TSharedRef<SWidget> SDMPropertyEdit::CreateColorPicker(bool bInUseAlpha, const TAttribute<FLinearColor>& InValueAttr,
	const FOnLinearColorValueChanged& InChangeFunc, const FText& InPickerTransationDescription, 
	const FFloatInterval* InValueRange)
{
	// InValueRange is not currently supported
	return 
		SNew(SBox)
		.WidthOverride(36.f)
		.HeightOverride(18.f)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SColorBlock)
			.Color(InValueAttr)
			.Size(FVector2D(36.f, 18.f))
			.ShowBackgroundForAlpha(true)
			.OnMouseButtonDown(this, &SDMPropertyEdit::OnClickColorBlock, bInUseAlpha, InValueAttr, InChangeFunc, InPickerTransationDescription)
		];
}

TSharedRef<SWidget> SDMPropertyEdit::CreateEnum(const FOnGetPropertyComboBoxValue& InValueString, const FOnGetPropertyComboBoxStrings& InGetStrings,
	const FOnPropertyComboBoxValueSelected& InValueSet)
{
	FPropertyComboBoxArgs Args;
	Args.PropertyHandle = nullptr;
	Args.OnGetValue = InValueString;
	Args.OnGetStrings = InGetStrings;
	Args.OnValueSelected = InValueSet;

	return PropertyCustomizationHelpers::MakePropertyComboBox(Args);
}

TSharedRef<SWidget> SDMPropertyEdit::CreateAssetPicker(UClass* InAllowedClass, const TAttribute<FString>& InPathAttr, 
	const FOnSetObject& InChangeFunc)
{
	return 
		SNew(SObjectPropertyEntryBox)
		.AllowClear(true)
		.AllowedClass(InAllowedClass)
		.DisplayBrowse(true)
		.DisplayThumbnail(true)
		.DisplayCompactSize(true)
		.DisplayUseSelected(true)
		.ThumbnailPool(SDMEditor::GetThumbnailPool())
		.EnableContentPicker(true)
		.ObjectPath(InPathAttr)
		.OnObjectChanged(InChangeFunc)
		.OnShouldSetAsset(FOnShouldSetAsset::CreateLambda([](const FAssetData& InAssetData) {return false;}));
}
 
FReply SDMPropertyEdit::OnClickColorBlock(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bInUseAlpha,
	TAttribute<FLinearColor> InValueAttr, FOnLinearColorValueChanged InChangeFunc, FText InPickerTransactionDescription)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
 
	FColorPickerArgs PickerArgs;
	{
		PickerArgs.bOnlyRefreshOnOk = true;
		PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SDMPropertyEdit::OnColorPickedCommitted, InChangeFunc);
		PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP(this, &SDMPropertyEdit::OnColorPickerCancelled, InChangeFunc);
		PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateSP(this, &SDMPropertyEdit::OnColorPickerClosed);
		PickerArgs.bUseAlpha = bInUseAlpha;
		PickerArgs.InitialColor = InValueAttr.Get(FLinearColor::White);
		PickerArgs.bOnlyRefreshOnMouseUp = false;
		PickerArgs.bOnlyRefreshOnOk = false;
	}
 
	StartTransaction(InPickerTransactionDescription);
	OpenColorPicker(PickerArgs);
 
	return FReply::Handled();
}
 
 
void SDMPropertyEdit::StartTransaction(FText InDescription)
{
	if (ScrubbingTransaction.IsValid())
	{
		return;
	}
 
	UObject* Object = nullptr;

	if (PropertyMaterialValue.IsValid())
	{
		Object = PropertyMaterialValue.Get();
	}
	else if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		TArray<UObject*> Outers;
		PropertyHandle->GetOuterObjects(Outers);

		if (Outers.IsEmpty() == false)
		{
			Object = Outers[0];
		}
	}

	ScrubbingTransaction = MakeShared<FScopedTransaction>(InDescription);

	if (IsValid(Object))
	{
		Object->Modify();
	}
}
 
void SDMPropertyEdit::EndTransaction()
{
	ScrubbingTransaction.Reset();
}
 
TSharedRef<SWidget> SDMPropertyEdit::MakeCopyPasteMenu(const TSharedPtr<IPropertyHandle>& InPropertyHandle) const
{
	if (!InPropertyHandle.IsValid() || !InPropertyHandle->IsValidHandle())
	{
		return SNullWidget::NullWidget;
	}

	const FName MenuName = "CopyPasteMenu";

	UToolMenu* NewMenu = UToolMenus::Get()->RegisterMenu(MenuName, NAME_None, EMultiBoxType::Menu, false);
	if (!IsValid(NewMenu))
	{
		return SNullWidget::NullWidget;
	}

	NewMenu->bToolBarForceSmallIcons = true;
	NewMenu->bShouldCloseWindowAfterMenuSelection = true;
	NewMenu->bCloseSelfOnly = true;

	FToolMenuSection& NewSection = NewMenu->AddDynamicSection(NAME_None, 
		FNewToolMenuDelegate::CreateLambda([InPropertyHandle](UToolMenu* InMenu)
		{
			FUIAction CopyAction;
			FUIAction PasteAction;
			InPropertyHandle->CreateDefaultPropertyCopyPasteActions(CopyAction, PasteAction);

			FToolMenuSection& NewSection = InMenu->AddSection("Edit", LOCTEXT("EditSection", "Edit"));
			NewSection.AddMenuEntry(NAME_None,
				LOCTEXT("CopyProperty", "Copy"),
				LOCTEXT("CopyPropertyToolTip", "Copy this property value"),
				FSlateIcon(FCoreStyle::Get().GetStyleSetName(), "GenericCommands.Copy"),
				CopyAction);
			NewSection.AddMenuEntry(NAME_None,
				LOCTEXT("PasteProperty", "Paste"),
				LOCTEXT("PastePropertyToolTip", "Paste the copied value here"),
				FSlateIcon(FCoreStyle::Get().GetStyleSetName(), "GenericCommands.Paste"),
				PasteAction);
		}));

	return UToolMenus::Get()->GenerateWidget(MenuName, FToolMenuContext());
}
 
#undef LOCTEXT_NAMESPACE
