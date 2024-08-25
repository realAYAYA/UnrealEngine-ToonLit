// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/SDMToolBar.h"
#include "AssetToolsModule.h"
#include "DMBlueprintFunctionLibrary.h"
#include "DMEDefs.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "GameFramework/Actor.h"
#include "IAssetTools.h"
#include "Model/DynamicMaterialModel.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMToolBar"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDMToolBar::Construct(const FArguments& InArgs)
{
	SetCanTick(true);

	MaterialActorWeak = InArgs._MaterialActor;
	MaterialModelWeak = InArgs._MaterialModel;
	OnSlotChanged = InArgs._OnSlotChanged;
	OnGetSettingsMenu = InArgs._OnGetSettingsMenu;
	
	if (MaterialActorWeak.IsValid())
	{
		SetMaterialActor(MaterialActorWeak.Get());
	}
	else
	{
		SetMaterialModel(MaterialModelWeak.Get());
	}
	
	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	[
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.BorderImage(FDynamicMaterialEditorStyle::GetBrush("Border.Bottom"))
		.BorderBackgroundColor(FLinearColor(1, 1, 1, 0.05f))
		[
			CreateToolBarEntries()
		]
	];
}

TSharedRef<SWidget> SDMToolBar::CreateToolBarEntries()
{
	return 
		SNew(SHorizontalBox)
		
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SNew(SWrapBox)
			.Orientation(Orient_Horizontal)
			.UseAllottedSize(true)
			.HAlign(HAlign_Left)
			.InnerSlotPadding(FVector2D(20.0f))
			+ SWrapBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MaterialDesignerInstanceActorLabel", "Actor"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(FDynamicMaterialEditorStyle::Get(), "ActorName")
					.Text(this, &SDMToolBar::GetSlotActorDisplayName)
				]
			]
			+ SWrapBox::Slot()
			.FillEmptySpace(true)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SDMToolBar::GetSlotsComboBoxWidgetVisibiltiy)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MaterialDesignerInstanceActorSlotLabel", "Property"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					CreateSlotsComboBoxWidget()
				]
			]
		]		
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.ContentPadding(GetDefaultToolBarButtonContentPadding())
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
			.ToolTipText(LOCTEXT("MaterialDesignerFollowSelectionTooltip", "Toggles whether the Material Designer display will change selecting new objects and actors."))
			.OnClicked(this, &SDMToolBar::OnFollowSelectionButtonClicked)
			[
				SNew(SImage)
				.Image(this, &SDMToolBar::GetFollowSelectionBrush)
				.DesiredSizeOverride(GetDefaultToolBarButtonSize())
				.ColorAndOpacity(this, &SDMToolBar::GetFollowSelectionColor)
			]
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SComboButton)
			.HasDownArrow(false)
			.IsFocusable(true)
			.ContentPadding(GetDefaultToolBarButtonContentPadding())
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
			.ToolTipText(LOCTEXT("MaterialDesignerSettingsTooltip", "Material Designer Settings"))
			.OnGetMenuContent(OnGetSettingsMenu)
			.ButtonContent()
			[
				SNew(SImage)
				.Image(FDynamicMaterialEditorStyle::GetBrush("Icons.Menu.Dropdown"))
				.DesiredSizeOverride(GetDefaultToolBarButtonSize())
			]
		];
}

TSharedRef<SWidget> SDMToolBar::CreateToolBarButton(TAttribute<const FSlateBrush*> InImageBrush, const TAttribute<FText>& InTooltipText, FOnClicked InOnClicked)
{
	return 
		SNew(SButton)
		.ContentPadding(GetDefaultToolBarButtonContentPadding())
		.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText(InTooltipText)
		.OnClicked(InOnClicked)
		[
			SNew(SImage)
			.Image(InImageBrush)
			.DesiredSizeOverride(GetDefaultToolBarButtonSize())
		];
}

TSharedRef<SWidget> SDMToolBar::CreateSlotsComboBoxWidget()
{
	if (!MaterialActorWeak.IsValid() || !MaterialModelWeak.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	const TSharedPtr<FDMObjectMaterialProperty> InitiallySelectedItem = 
		ActorMaterialProperties.IsValidIndex(SelectedMaterialSlotIndex) ? ActorMaterialProperties[SelectedMaterialSlotIndex] : nullptr;

	return 
		SNew(SComboBox<TSharedPtr<FDMObjectMaterialProperty>>)
		.InitiallySelectedItem(InitiallySelectedItem)
		.OptionsSource(&ActorMaterialProperties)
		.OnGenerateWidget(this, &SDMToolBar::GenerateSelectedMaterialSlotRow)
		.OnSelectionChanged(this, &SDMToolBar::OnMaterialSlotChanged)
		[
			SNew(STextBlock)
			.MinDesiredWidth(100.0f)
			.Text(this, &SDMToolBar::GetSelectedMaterialSlotName)
		];
}

TSharedRef<SWidget> SDMToolBar::GenerateSelectedMaterialSlotRow(TSharedPtr<FDMObjectMaterialProperty> InSelectedSlot) const
{
	if (InSelectedSlot.IsValid())
	{
		return SNew(STextBlock)
			.MinDesiredWidth(100.f)
			.Text(this, &SDMToolBar::GetSlotDisplayName, InSelectedSlot);
	}
	return SNullWidget::NullWidget;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText SDMToolBar::GetSlotDisplayName(TSharedPtr<FDMObjectMaterialProperty> InSlot) const
{
	return InSlot->GetPropertyName(false);
}

FText SDMToolBar::GetSelectedMaterialSlotName() const
{
	if (ActorMaterialProperties.IsValidIndex(SelectedMaterialSlotIndex) && ActorMaterialProperties[SelectedMaterialSlotIndex].IsValid())
	{
		return GetSlotDisplayName(ActorMaterialProperties[SelectedMaterialSlotIndex]);
	}
	return FText::GetEmpty();
}

void SDMToolBar::OnMaterialSlotChanged(TSharedPtr<FDMObjectMaterialProperty> InSelectedSlot, ESelectInfo::Type InSelectInfoType)
{
	if (!InSelectedSlot.IsValid())
	{
		return;
	}

	UDynamicMaterialModel* SelectedMaterialModel = InSelectedSlot->GetMaterialModel();
	if (IsValid(SelectedMaterialModel))
	{
		SetMaterialModel(SelectedMaterialModel);
	}
	else if (InSelectedSlot->OuterWeak.IsValid())
	{
		UDMBlueprintFunctionLibrary::CreateDynamicMaterialInObject(*InSelectedSlot.Get());
	}

	OnSlotChanged.ExecuteIfBound(InSelectedSlot);
}

EVisibility SDMToolBar::GetSlotsComboBoxWidgetVisibiltiy() const
{
	return ActorMaterialProperties.Num() > 1 ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SDMToolBar::GetSlotActorDisplayName() const
{
	const AActor* const SlotActor = GetMaterialActor();
	return IsValid(SlotActor) ? FText::FromString(SlotActor->GetActorLabel()) : FText();
}

void SDMToolBar::SetMaterialProperties(const TArray<TSharedPtr<FDMObjectMaterialProperty>>& InActorMaterialProperties)
{
	ActorMaterialProperties = InActorMaterialProperties;
	SelectedMaterialSlotIndex = 0;
}

void SDMToolBar::SetMaterialModel(UDynamicMaterialModel* InModel)
{
	if (!IsValid(InModel))
	{
		return;
	}

	MaterialModelWeak = InModel;

	if (!MaterialActorWeak.IsValid())
	{
		MaterialActorWeak = MaterialModelWeak->GetTypedOuter<AActor>();
	}

	ActorMaterialProperties.Empty(0);
	SelectedMaterialSlotIndex = INDEX_NONE;

	if (!MaterialActorWeak.IsValid())
	{
		return;
	}

	const TArray<FDMObjectMaterialProperty> ActorDynamicMaterialSlots = UDMBlueprintFunctionLibrary::GetActorMaterialProperties(MaterialActorWeak.Get());
	ActorMaterialProperties.Reserve(ActorDynamicMaterialSlots.Num());

	for (const FDMObjectMaterialProperty& ActorDynamicMaterialSlot : ActorDynamicMaterialSlots)
	{
		ActorMaterialProperties.Add(MakeShared<FDMObjectMaterialProperty>(ActorDynamicMaterialSlot));
	}

	for (int32 SlotIndex = 0; SlotIndex < ActorMaterialProperties.Num(); ++SlotIndex)
	{
		FDMObjectMaterialProperty* ActorMaterialSlot = ActorMaterialProperties[SlotIndex].Get();
		if (ActorMaterialSlot && ActorMaterialSlot->GetMaterialModel() == MaterialModelWeak)
		{
			SelectedMaterialSlotIndex = SlotIndex;
			break;
		}
	}
}

void SDMToolBar::SetMaterialActor(AActor* InActor, const int32 InActiveSlotIndex)
{
	MaterialActorWeak = InActor;
	ActorMaterialProperties.Empty();
	SelectedMaterialSlotIndex = 0;

	TArray<FDMObjectMaterialProperty> ActorProperties = UDMBlueprintFunctionLibrary::GetActorMaterialProperties(InActor);
	UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get();

	for (int32 MaterialPropertyIdx = 0; MaterialPropertyIdx < ActorProperties.Num(); ++MaterialPropertyIdx)
	{
		const FDMObjectMaterialProperty& MaterialProperty = ActorProperties[MaterialPropertyIdx];

		ActorMaterialProperties.Add(MakeShared<FDMObjectMaterialProperty>(MaterialProperty));

		if (MaterialProperty.GetMaterialModel() == MaterialModel)
		{
			SelectedMaterialSlotIndex = MaterialPropertyIdx;
		}
	}
}

const FSlateBrush* SDMToolBar::GetFollowSelectionBrush() const
{
	static const FSlateBrush* Unlocked = FAppStyle::Get().GetBrush("Icons.Unlock");
	static const FSlateBrush* Locked = FAppStyle::Get().GetBrush("Icons.Lock");

	if (UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get())
	{
		if (!Settings->bFollowSelection)
		{
			return Locked;
		}
	}

	return Unlocked;
}

FSlateColor SDMToolBar::GetFollowSelectionColor() const
{
	// We want the icon to stand out when it's locked.
	static FSlateColor EnabledColor = FSlateColor(EStyleColor::AccentGray);
	static FSlateColor DisabledColor = FSlateColor(EStyleColor::Primary);

	bool bFollowingSelection = true;

	if (UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get())
	{
		bFollowingSelection = Settings->bFollowSelection;
	}

	return bFollowingSelection
		? EnabledColor
		: DisabledColor;
}

FReply SDMToolBar::OnFollowSelectionButtonClicked()
{
	if (UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get())
	{
		Settings->bFollowSelection = !Settings->bFollowSelection;
		Settings->SaveConfig();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
