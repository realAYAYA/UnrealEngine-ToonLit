// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailCustomization_Layers.h"
#include "IDetailChildrenBuilder.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Brushes/SlateColorBrush.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "LandscapeEditorDetailCustomization_LayersBrushStack.h" // FLandscapeBrushDragDropOp
#include "LandscapeEditorModule.h"
#include "LandscapeEditorObject.h"
#include "Landscape.h"
#include "Styling/AppStyle.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"

#include "SLandscapeEditor.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "DesktopPlatformModule.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "LandscapeRender.h"
#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"
#include "LandscapeEdit.h"
#include "IDetailGroup.h"
#include "Widgets/SBoxPanel.h"
#include "LandscapeEditorDetailCustomization_TargetLayers.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "LandscapeEditorCommands.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.Layers"

TSharedRef<IDetailCustomization> FLandscapeEditorDetailCustomization_Layers::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorDetailCustomization_Layers);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorDetailCustomization_Layers::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& LayerCategory = DetailBuilder.EditCategory("Edit Layers");

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolMode != nullptr)
	{
		if (LandscapeEdMode->GetLandscape())
		{
			LayerCategory.AddCustomBuilder(MakeShareable(new FLandscapeEditorCustomNodeBuilder_Layers(DetailBuilder.GetThumbnailPool().ToSharedRef())));

			LayerCategory.AddCustomRow(FText())
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([]() { return ShoudShowLayersErrorMessageTip() ? EVisibility::Visible : EVisibility::Collapsed; })))
				[
					SNew(SMultiLineEditableTextBox)
					.IsReadOnly(true)
					.Font(DetailBuilder.GetDetailFontBold())
					.BackgroundColor(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([]() { return FAppStyle::GetColor("ErrorReporting.WarningBackgroundColor"); })))
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&FLandscapeEditorDetailCustomization_Layers::GetLayersErrorMessageText)))
					.AutoWrapText(true)
				];
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool FLandscapeEditorDetailCustomization_Layers::ShoudShowLayersErrorMessageTip()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->DoesCurrentToolAffectEditLayers())
	{
		return !LandscapeEdMode->CanEditLayer();
	}
	return false;
}

FText FLandscapeEditorDetailCustomization_Layers::GetLayersErrorMessageText()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	FText Reason;
	if (LandscapeEdMode && !LandscapeEdMode->CanEditLayer(&Reason))
	{
		return Reason;
	}
	return FText::GetEmpty();
}

//////////////////////////////////////////////////////////////////////////

FEdModeLandscape* FLandscapeEditorCustomNodeBuilder_Layers::GetEditorMode()
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

FLandscapeEditorCustomNodeBuilder_Layers::FLandscapeEditorCustomNodeBuilder_Layers(TSharedRef<FAssetThumbnailPool> InThumbnailPool)
	: ThumbnailPool(InThumbnailPool)
	, CurrentEditingInlineTextBlock(INDEX_NONE)
	, CurrentSlider(INDEX_NONE)
{
}

FLandscapeEditorCustomNodeBuilder_Layers::~FLandscapeEditorCustomNodeBuilder_Layers()
{
}

void FLandscapeEditorCustomNodeBuilder_Layers::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
}

void FLandscapeEditorCustomNodeBuilder_Layers::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{

}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorCustomNodeBuilder_Layers::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		TSharedPtr<SDragAndDropVerticalBox> LayerList = SNew(SDragAndDropVerticalBox)
			.OnCanAcceptDrop(this, &FLandscapeEditorCustomNodeBuilder_Layers::HandleCanAcceptDrop)
			.OnAcceptDrop(this, &FLandscapeEditorCustomNodeBuilder_Layers::HandleAcceptDrop)
			.OnDragDetected(this, &FLandscapeEditorCustomNodeBuilder_Layers::HandleDragDetected);

		LayerList->SetDropIndicator_Above(*FAppStyle::GetBrush("LandscapeEditor.TargetList.DropZone.Above"));
		LayerList->SetDropIndicator_Below(*FAppStyle::GetBrush("LandscapeEditor.TargetList.DropZone.Below"));

		ChildrenBuilder.AddCustomRow(FText::FromString(FString(TEXT("Edit Layers"))))
			.Visibility(EVisibility::Visible)
			[
				LayerList.ToSharedRef()
			];

		InlineTextBlocks.Empty();
		InlineTextBlocks.AddDefaulted(LandscapeEdMode->GetLayerCount());
		// Slots are displayed in the opposite order of LandscapeLayers
		for (int32 i = LandscapeEdMode->GetLayerCount()-1; i >= 0 ; --i)
		{
			TSharedPtr<SWidget> GeneratedRowWidget = GenerateRow(i);

			if (GeneratedRowWidget.IsValid())
			{
				LayerList->AddSlot()
					.AutoHeight()
					[
						GeneratedRowWidget.ToSharedRef()
					];
			}
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedPtr<SWidget> FLandscapeEditorCustomNodeBuilder_Layers::GenerateRow(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	TSharedPtr<SWidget> RowWidget = SNew(SLandscapeEditorSelectableBorder)
		.Padding(FMargin(8.f, 0.f))
		.VAlign(VAlign_Center)
		.OnContextMenuOpening(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnLayerContextMenuOpening, InLayerIndex)
		.OnSelected(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnLayerSelectionChanged, InLayerIndex)
		.IsSelected(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_Layers::IsLayerSelected, InLayerIndex)))
		.Visibility(EVisibility::Visible)
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.OnClicked(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnToggleLock, InLayerIndex)
				.ToolTipText(LOCTEXT("LandscapeLayerLock", "Locks the current layer"))
				[
					SNew(SImage)
					.Image(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLockBrushForLayer, InLayerIndex)
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 0)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0.0f)
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.OnClicked(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnToggleVisibility, InLayerIndex)
				.ToolTipText(LOCTEXT("LandscapeLayerVisibility", "Toggle Layer Visibility"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Content()
				[
					SNew(SImage)
					.Image(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetVisibilityBrushForLayer, InLayerIndex)
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0)
			.VAlign(VAlign_Center)
			.Padding(4, 0)
			[
				SAssignNew(InlineTextBlocks[InLayerIndex], SInlineEditableTextBlock)
				.IsEnabled(this, &FLandscapeEditorCustomNodeBuilder_Layers::IsLayerEditionEnabled, InLayerIndex)
				.Text(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLayerDisplayName, InLayerIndex)
				.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLayerTextColor, InLayerIndex)))
				.ToolTipText(LOCTEXT("LandscapeLayers_tooltip", "Name of the Layer"))
				.OnVerifyTextChanged(FOnVerifyTextChanged::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_Layers::CanRenameLayerTo, InLayerIndex))
				.OnEnterEditingMode(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnBeginNameTextEdit)
				.OnExitEditingMode(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnEndNameTextEdit)
				.OnTextCommitted(FOnTextCommitted::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_Layers::SetLayerName, InLayerIndex))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(0, 2)
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0)
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.IsEnabled(this, &FLandscapeEditorCustomNodeBuilder_Layers::IsLayerEditionEnabled, InLayerIndex)
					.Visibility(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLayerAlphaVisibility, InLayerIndex)
					.Text(LOCTEXT("LandscapeLayerAlpha", "Alpha"))
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLayerTextColor, InLayerIndex)))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(0, 2)
				.HAlign(HAlign_Left)
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.MinValue(-1.0f)
					.MaxValue(1.0f)
					.MinSliderValue(-1.0f)
					.MaxSliderValue(1.0f)
					.Delta(0.01f)
					.MinDesiredValueWidth(60.0f)
					.IsEnabled(this, &FLandscapeEditorCustomNodeBuilder_Layers::IsLayerEditionEnabled, InLayerIndex)
					.Visibility(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLayerAlphaVisibility, InLayerIndex)
					.Value(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLayerAlpha, InLayerIndex)
					.OnValueChanged_Lambda([this, InLayerIndex](float InValue) { SetLayerAlpha(InValue, InLayerIndex, false); })
					.OnValueCommitted_Lambda([this, InLayerIndex](float InValue, ETextCommit::Type InCommitType) { SetLayerAlpha(InValue, InLayerIndex, true); })
					.OnBeginSliderMovement_Lambda([this, InLayerIndex]()
					{
						CurrentSlider = InLayerIndex;
						GEditor->BeginTransaction(LOCTEXT("Landscape_Layers_SetAlpha", "Set Layer Alpha"));
					})
					.OnEndSliderMovement_Lambda([this](double)
					{
						GEditor->EndTransaction();
						CurrentSlider = INDEX_NONE;
					})
				]		
			]
		];	

	return RowWidget;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText FLandscapeEditorCustomNodeBuilder_Layers::GetLayerDisplayName(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (LandscapeEdMode && Landscape)
	{
		const FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(InLayerIndex);
		const FLandscapeLayer* ReservedLayer = Landscape->GetLandscapeSplinesReservedLayer();
		bool bIsReserved = Layer && Layer == ReservedLayer && InLayerIndex != CurrentEditingInlineTextBlock;
		return FText::Format(LOCTEXT("LayerDisplayName", "{0}{1}"), FText::FromName(LandscapeEdMode->GetLayerName(InLayerIndex)), bIsReserved ? LOCTEXT("ReservedForSplines", " (Reserved for Splines)") : FText::GetEmpty());
	}

	return FText::FromString(TEXT("None"));
}

bool FLandscapeEditorCustomNodeBuilder_Layers::IsLayerSelected(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		return LandscapeEdMode->GetCurrentLayerIndex() == InLayerIndex;
	}

	return false;
}

void FLandscapeEditorCustomNodeBuilder_Layers::OnBeginNameTextEdit()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	CurrentEditingInlineTextBlock = LandscapeEdMode ? LandscapeEdMode->GetCurrentLayerIndex() : INDEX_NONE;
}

void FLandscapeEditorCustomNodeBuilder_Layers::OnEndNameTextEdit()
{
	CurrentEditingInlineTextBlock = INDEX_NONE;
}

bool FLandscapeEditorCustomNodeBuilder_Layers::CanRenameLayerTo(const FText& InNewText, FText& OutErrorMessage, int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		if (!LandscapeEdMode->CanRenameLayerTo(InLayerIndex, *InNewText.ToString()))
		{
			OutErrorMessage = LOCTEXT("Landscape_Layers_RenameFailed_AlreadyExists", "This layer already exists");
			return false;
		}
	}
	return true;
}

void FLandscapeEditorCustomNodeBuilder_Layers::SetLayerName(const FText& InText, ETextCommit::Type InCommitType, int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Rename", "Rename Layer"));
		LandscapeEdMode->SetLayerName(InLayerIndex, *InText.ToString());
		OnLayerSelectionChanged(InLayerIndex);
	}
}

FSlateColor FLandscapeEditorCustomNodeBuilder_Layers::GetLayerTextColor(int32 InLayerIndex) const
{
	return IsLayerSelected(InLayerIndex) ? FStyleColors::ForegroundHover : FSlateColor::UseForeground();
}

void FLandscapeEditorCustomNodeBuilder_Layers::FillAddBrushMenu(FMenuBuilder& MenuBuilder, TArray<ALandscapeBlueprintBrushBase*> Brushes)
{
	for (ALandscapeBlueprintBrushBase* Brush : Brushes)
	{
		TSharedRef<FLandscapeEditorCustomNodeBuilder_Layers> SharedThis = AsShared();
		FUIAction AddAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, Brush]() { SharedThis->AddBrushToCurrentLayer(Brush); }));
		MenuBuilder.AddMenuEntry(FText::FromString(Brush->GetActorLabel()), FText(), FSlateIcon(), AddAction);
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::FillClearPaintLayerMenu(FMenuBuilder& MenuBuilder, int32 InLayerIndex, TArray<ULandscapeLayerInfoObject*> InUsedLayerInfos)
{
	// Clear All Weightmap Data
	TSharedRef<FLandscapeEditorCustomNodeBuilder_Layers> SharedThis = AsShared();
	FUIAction ClearAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex]() { SharedThis->ClearLayer(InLayerIndex, ELandscapeClearMode::Clear_Weightmap); }));
	MenuBuilder.AddMenuEntry(LOCTEXT("LandscapeClearAllWeightmap", "All"), FText(), FSlateIcon(), ClearAction);
	MenuBuilder.AddMenuSeparator();

	// Clear Per LayerInfo
	for (ULandscapeLayerInfoObject* LayerInfo : InUsedLayerInfos)
	{
		FUIAction ClearLayerInfoAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex, LayerInfo]() { SharedThis->ClearPaintLayer(InLayerIndex, LayerInfo); }));
		MenuBuilder.AddMenuEntry(FText::FromName(LayerInfo->LayerName), FText(), FSlateIcon(), ClearLayerInfoAction);
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::FillClearLayerMenu(FMenuBuilder& MenuBuilder, int32 InLayerIndex)
{
	TSharedRef<FLandscapeEditorCustomNodeBuilder_Layers> SharedThis = AsShared();
	const UEnum* EnumPtr = FindObject<UEnum>(nullptr, TEXT("/Script/Landscape.ELandscapeClearMode"), true);
	if (ensure(EnumPtr != nullptr))
	{
		// NumEnums()-1 to exclude Enum Max Value
		for (int32 i = 0; i < EnumPtr->NumEnums()-1; ++i)
		{
			ELandscapeClearMode EnumValue = (ELandscapeClearMode)EnumPtr->GetValueByIndex(i);
			if(EnumValue == ELandscapeClearMode::Clear_Weightmap)
			{
				TArray<ULandscapeLayerInfoObject*> UsedLayerInfos;
				FEdModeLandscape* LandscapeEdMode = GetEditorMode();
				check(LandscapeEdMode);
				ALandscape* Landscape = LandscapeEdMode->GetLandscape();
				check(Landscape);

				Landscape->GetUsedPaintLayers(InLayerIndex, UsedLayerInfos);
				if (UsedLayerInfos.Num() > 0)
				{
					MenuBuilder.AddSubMenu(
						EnumPtr->GetDisplayNameTextByIndex(i),
						FText(),
						FNewMenuDelegate::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_Layers::FillClearPaintLayerMenu, InLayerIndex, UsedLayerInfos),
						false,
						FSlateIcon()
					);
				}
			}
			else
			{
				FUIAction ClearAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex, EnumValue]() { SharedThis->ClearLayer(InLayerIndex, EnumValue); }));
				MenuBuilder.AddMenuEntry(EnumPtr->GetDisplayNameTextByIndex(i), FText(), FSlateIcon(), ClearAction);
			}
		}
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::AddBrushToCurrentLayer(ALandscapeBlueprintBrushBase* Brush)
{
	const FScopedTransaction Transaction(LOCTEXT("LandscapeBrushAddToCurrentLayerTransaction", "Add brush to current layer"));
	GetEditorMode()->AddBrushToCurrentLayer(Brush);
}

TSharedPtr<SWidget> FLandscapeEditorCustomNodeBuilder_Layers::OnLayerContextMenuOpening(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape && LandscapeEdMode && LandscapeEdMode->DoesCurrentToolAffectEditLayers())
	{
		FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(InLayerIndex);
		TSharedRef<FLandscapeEditorCustomNodeBuilder_Layers> SharedThis = AsShared();
		FMenuBuilder MenuBuilder(true, nullptr);
		MenuBuilder.BeginSection("LandscapeEditorLayerActions", LOCTEXT("LandscapeEditorLayerActions.Heading", "Edit Layers"));
		{
			// Create Layer
			FUIAction CreateLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis] { SharedThis->CreateLayer(); }), FCanExecuteAction::CreateLambda([Landscape] { return !Landscape->IsMaxLayersReached(); }));
			TAttribute<FText> CreateLayerText(Landscape->IsMaxLayersReached() ? LOCTEXT("MaxLayersReached", "Create (Max layers reached)") : LOCTEXT("CreateLayer", "Create"));
			MenuBuilder.AddMenuEntry(CreateLayerText, LOCTEXT("CreateLayerTooltip", "Create Layer"), FSlateIcon(), CreateLayerAction);
	
			if (Layer)
			{
				// Rename Layer
				FUIAction RenameLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->RenameLayer(InLayerIndex); }));
				MenuBuilder.AddMenuEntry(LOCTEXT("RenameLayer", "Rename..."), LOCTEXT("RenameLayerTooltip", "Rename Layer"), FSlateIcon(), RenameLayerAction);

				if (!Layer->bLocked)
				{
					// Clear Layer
					MenuBuilder.AddSubMenu(
						LOCTEXT("LandscapeEditorClearLayerSubMenu", "Clear"),
						FText(),
						FNewMenuDelegate::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_Layers::FillClearLayerMenu, InLayerIndex),
						false,
						FSlateIcon()
					);

					// Delete Layer
					FUIAction DeleteLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->DeleteLayer(InLayerIndex); }), FCanExecuteAction::CreateLambda([Landscape] { return Landscape->LandscapeLayers.Num() > 1; }));
					TAttribute<FText> DeleteLayerText(Landscape->LandscapeLayers.Num() == 1 ? LOCTEXT("CantDeleteLastLayer", "Delete (Last layer)") : LOCTEXT("DeleteLayer", "Delete..."));
					MenuBuilder.AddMenuEntry(DeleteLayerText, LOCTEXT("DeleteLayerTooltip", "Delete Layer"), FSlateIcon(), DeleteLayerAction);

					// Collapse Layer
					if (LandscapeEdMode->CurrentToolMode->ToolModeName == "ToolMode_Manage")
					{
						FUIAction CollapseLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->CollapseLayer(InLayerIndex); }));
						MenuBuilder.AddMenuEntry(LOCTEXT("CollapseLayer", "Collapse..."), LOCTEXT("CollapseLayerTooltip", "Collapse layer into top neighbor layer"), FSlateIcon(), CollapseLayerAction);
					}
				}

				if (Landscape->GetLandscapeSplinesReservedLayer() != Landscape->GetLayer(InLayerIndex))
				{
					// Reserve for Landscape Splines
					FUIAction ReserveLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->SetLandscapeSplinesReservedLayer(InLayerIndex); }));
					MenuBuilder.AddMenuEntry(LOCTEXT("ReserveLayerForSplines", "Reserve for Splines"), LOCTEXT("ReserveLayerForSplinesTooltip", "Reserve Layer for Landscape Splines"), FSlateIcon(), ReserveLayerAction);
				}
				else
				{
					FUIAction RemoveReserveLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->SetLandscapeSplinesReservedLayer(INDEX_NONE); }));
					MenuBuilder.AddMenuEntry(LOCTEXT("RemoveReserveLayerForSplines", "Remove Reserve for Splines"), LOCTEXT("RemoveReserveLayerForSplinesTooltip", "Remove reservation of Layer for Landscape Splines"), FSlateIcon(), RemoveReserveLayerAction);

					FUIAction ForceUpdateSplinesAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->ForceUpdateSplines(); }));
					MenuBuilder.AddMenuEntry(LOCTEXT("UpdateSplines", "Update Splines"), LOCTEXT("UpdateSplinesTooltip", "Update Landscape Splines"), FSlateIcon(), ForceUpdateSplinesAction);
				}
			}
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("LandscapeEditorLayerVisibility", LOCTEXT("LandscapeEditorLayerVisibility.Heading", "Visibility"));
		{
			if (Layer)
			{
				if (Layer->bVisible)
				{
					// Hide Selected Layer
					FUIAction HideSelectedLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->OnToggleVisibility(InLayerIndex); }));
					MenuBuilder.AddMenuEntry(LOCTEXT("HideSelected", "Hide Selected"), LOCTEXT("HideSelectedLayerTooltip", "Hide Selected Layer"), FSlateIcon(), HideSelectedLayerAction);
				}
				else
				{
					// Show Selected Layer
					FUIAction ShowSelectedLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->OnToggleVisibility(InLayerIndex); }));
					MenuBuilder.AddMenuEntry(LOCTEXT("ShowSelected", "Show Selected"), LOCTEXT("ShowSelectedLayerTooltip", "Show Selected Layer"), FSlateIcon(), ShowSelectedLayerAction);
				}

				// Show Only Selected Layer
				FUIAction ShowOnlySelectedLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->ShowOnlySelectedLayer(InLayerIndex); }));
				MenuBuilder.AddMenuEntry(LOCTEXT("ShowOnlySelected", "Show Only Selected"), LOCTEXT("ShowOnlySelectedLayerTooltip", "Show Only Selected Layer"), FSlateIcon(), ShowOnlySelectedLayerAction);
			}

			// Show All Layers
			FUIAction ShowAllLayersAction = FUIAction(FExecuteAction::CreateLambda([SharedThis] { SharedThis->ShowAllLayers(); }));
			MenuBuilder.AddMenuEntry(LOCTEXT("ShowAllLayers", "Show All Layers"), LOCTEXT("ShowAllLayersTooltip", "Show All Layers"), FSlateIcon(), ShowAllLayersAction);
		}
		MenuBuilder.EndSection();

		const TArray<ALandscapeBlueprintBrushBase*>& Brushes = LandscapeEdMode->GetBrushList();
		TArray<ALandscapeBlueprintBrushBase*> FilteredBrushes = Brushes.FilterByPredicate([](ALandscapeBlueprintBrushBase* Brush) { return Brush->GetOwningLandscape() == nullptr; });
		if (FilteredBrushes.Num())
		{
			MenuBuilder.BeginSection("LandscapeEditorBrushActions", LOCTEXT("LandscapeEditorBrushActions.Heading", "Brushes"));
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("LandscaeEditorBrushAddSubMenu", "Add"),
					LOCTEXT("LandscaeEditorBrushAddSubMenuToolTip", "Add brush to current layer"),
					FNewMenuDelegate::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_Layers::FillAddBrushMenu, FilteredBrushes),
					false,
					FSlateIcon()
				);
			}
			MenuBuilder.EndSection();
		}

		return MenuBuilder.MakeWidget();
	}
	return nullptr;
}

void FLandscapeEditorCustomNodeBuilder_Layers::ForceUpdateSplines()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape && Landscape->HasLayersContent() && Landscape->GetLandscapeSplinesReservedLayer())
	{
		const bool bUpdateOnlySelection = false;
		const bool bForceUpdate = true;
		Landscape->UpdateLandscapeSplines(FGuid(), bUpdateOnlySelection, bForceUpdate);
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::SetLandscapeSplinesReservedLayer(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape)
	{
		const FLandscapeLayer* CurrenReservedLayer = Landscape->GetLandscapeSplinesReservedLayer();
		const FLandscapeLayer* NewReservedLayer = Landscape->GetLayer(InLayerIndex);
		if (NewReservedLayer != CurrenReservedLayer)
		{
			EAppReturnType::Type Result = EAppReturnType::No;
			if (NewReservedLayer)
			{
				Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("Landscape_SetReservedForSplines_Message", "Reserving layer {0} for landscape splines will clear it from its content and no editing will be allowed.  Continue?"), FText::FromName(NewReservedLayer->Name)));
			}
			else if (CurrenReservedLayer)
			{
				Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("Landscape_UnsetReservedForSplines_Message", "Removing reservation of layer {0} for landscape splines will clear its content.  Continue?"), FText::FromName(CurrenReservedLayer->Name)));
			}

			if (Result == EAppReturnType::Yes)
			{
				const FScopedTransaction Transaction(LOCTEXT("ReserveForSplines", "Reserve Landscape Layer for Splines"));
				Landscape->SetLandscapeSplinesReservedLayer(InLayerIndex);
				LandscapeEdMode->RefreshDetailPanel();
				LandscapeEdMode->AutoUpdateDirtyLandscapeSplines();
				OnLayerSelectionChanged(InLayerIndex);
			}
		}
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::RenameLayer(int32 InLayerIndex)
{
	if (InlineTextBlocks.IsValidIndex(InLayerIndex) && InlineTextBlocks[InLayerIndex].IsValid())
	{
		InlineTextBlocks[InLayerIndex]->EnterEditingMode();
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::ClearPaintLayer(int32 InLayerIndex, ULandscapeLayerInfoObject* InLayerInfo)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape)
	{
		FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(InLayerIndex);
		if (Layer)
		{
			EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("Landscape_ClearPaintLayer_Message", "The layer {0} : {1} content will be completely cleared.  Continue?"), FText::FromName(Layer->Name), FText::FromName(InLayerInfo->LayerName)));
			if (Result == EAppReturnType::Yes)
			{
				const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_PaintClear", "Clear Paint Layer"));
				Landscape->ClearPaintLayer(InLayerIndex, InLayerInfo);
				LandscapeEdMode->RequestUpdateShownLayerList();
			}
		}
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::ClearLayer(int32 InLayerIndex, ELandscapeClearMode InClearMode)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape)
	{
		FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(InLayerIndex);
		if (Layer)
		{
			EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("Landscape_ClearLayer_Message", "The layer {0} content will be completely cleared.  Continue?"), FText::FromName(Layer->Name)));
			if (Result == EAppReturnType::Yes)
			{
				const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Clean", "Clear Layer"));
				Landscape->ClearLayer(InLayerIndex, nullptr, InClearMode);
				OnLayerSelectionChanged(InLayerIndex);
				if (InClearMode & ELandscapeClearMode::Clear_Weightmap)
				{
					LandscapeEdMode->RequestUpdateShownLayerList();
				}
			}
		}
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::DeleteLayer(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape && Landscape->LandscapeLayers.Num() > 1)
	{
		FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(InLayerIndex);
		if (Layer)
		{
			EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("Landscape_DeleteLayer_Message", "The layer {0} will be deleted.  Continue?"), FText::FromName(Layer->Name)));
			if (Result == EAppReturnType::Yes)
			{
				const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Delete", "Delete Layer"));
				Landscape->DeleteLayer(InLayerIndex);
				int32 NewLayerSelectionIndex = Landscape->GetLayer(InLayerIndex) ? InLayerIndex : 0;
				OnLayerSelectionChanged(NewLayerSelectionIndex);
				LandscapeEdMode->RefreshDetailPanel();
			}
		}
	}
}

bool FLandscapeEditorCustomNodeBuilder_Layers::CanCollapseLayer(int32 InLayerIndex, FText& OutReason) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (!Landscape || Landscape->LandscapeLayers.Num() <= 1)
	{
		OutReason = LOCTEXT("Landscape_CollapseLayer_Reason_NotEnoughLayersToCollapse", "Not enough layers to do Collapse.");
		return false;
	}

	if (InLayerIndex < 1)
	{
		OutReason = LOCTEXT("Landscape_CollapseLayer_Reason_CantCollapseBaseLayer", "Can't Collapse first layer.");
		return false;
	}
	
	FLandscapeLayer* SplineLayer = Landscape->GetLandscapeSplinesReservedLayer();
	if (SplineLayer == LandscapeEdMode->GetLayer(InLayerIndex))
	{
		OutReason = LOCTEXT("Landscape_CollapseLayer_Reason_CantCollapseSplineLayer", "Can't Collapse reserved spline layer.");
		return false;
	}

	FLandscapeLayer* BaseLayer = LandscapeEdMode->GetLayer(InLayerIndex - 1);
	if (!BaseLayer)
	{
		OutReason = LOCTEXT("Landscape_CollapseLayer_Reason_InvalidBaseLayer", "Invalid base layer.");
		return false;
	}

	if (SplineLayer == BaseLayer)
	{
		OutReason = LOCTEXT("Landscape_CollapseLayer_Reason_CantCollapseOnSplineLayer", "Can't Collapse on reserved spline layer.");
		return false;
	}
	
	// Can't collapse on layer that has a Brush because result will change...
	if (BaseLayer->Brushes.Num() > 0)
	{
		OutReason = FText::Format(LOCTEXT("Landscape_CollapseLayer_Reason_CantCollapseOnLayerWithBrush", "Can't Collapse because base layer '{0}' contains Brush(es)."), FText::FromName(BaseLayer->Name));
		return false;
	}
	

	return true;
}

void FLandscapeEditorCustomNodeBuilder_Layers::CollapseLayer(int32 InLayerIndex)
{
	FText Reason;
	if (CanCollapseLayer(InLayerIndex, Reason))
	{
		if (FEdModeLandscape* LandscapeEdMode = GetEditorMode())
		{
			ALandscape* Landscape = LandscapeEdMode->GetLandscape();
			FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(InLayerIndex);
			FLandscapeLayer* BaseLayer = LandscapeEdMode->GetLayer(InLayerIndex - 1);
			if (Landscape && Layer && BaseLayer)
			{
				EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("Landscape_CollapseLayer_Message", "The layer {0} will be collapsed into layer {1}.  Continue?"), FText::FromName(Layer->Name), FText::FromName(BaseLayer->Name)));
				if (Result == EAppReturnType::Yes)
				{
					const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Collapse", "Collapse Layer"));
					Landscape->CollapseLayer(InLayerIndex);
					OnLayerSelectionChanged(InLayerIndex - 1);
					LandscapeEdMode->RefreshDetailPanel();
				}
			}
		}
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, Reason);
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::ShowOnlySelectedLayer(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape)
	{
		const FScopedTransaction Transaction(LOCTEXT("ShowOnlySelectedLayer", "Show Only Selected Layer"));
		Landscape->ShowOnlySelectedLayer(InLayerIndex);
		OnLayerSelectionChanged(InLayerIndex);
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::ShowAllLayers()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape)
	{
		const FScopedTransaction Transaction(LOCTEXT("ShowAllLayers", "Show All Layers"));
		Landscape->ShowAllLayers();
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::CreateLayer()
{
FEdModeLandscape* LandscapeEdMode = GetEditorMode();
ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
if (Landscape)
{
	{
		const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Create", "Create Layer"));
		Landscape->CreateLayer();
		OnLayerSelectionChanged(Landscape->GetLayerCount() - 1);
	}
	LandscapeEdMode->RefreshDetailPanel();
}
}

void FLandscapeEditorCustomNodeBuilder_Layers::OnLayerSelectionChanged(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->GetCurrentLayerIndex() != InLayerIndex)
	{
		FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_SetCurrentLayer", "Set Current Layer"));
		LandscapeEdMode->SetCurrentLayer(InLayerIndex);
		LandscapeEdMode->UpdateTargetList();
	}
}

TOptional<float> FLandscapeEditorCustomNodeBuilder_Layers::GetLayerAlpha(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode)
	{
		return LandscapeEdMode->GetLayerAlpha(InLayerIndex);
	}

	return 1.0f;
}

void FLandscapeEditorCustomNodeBuilder_Layers::SetLayerAlpha(float InAlpha, int32 InLayerIndex, bool bCommit)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		// We get multiple commits when editing through the text box
		if (LandscapeEdMode->GetLayerAlpha(InLayerIndex) == LandscapeEdMode->GetClampedLayerAlpha(InAlpha))
		{
			return;
		}

		FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_SetAlpha", "Set Layer Alpha"), CurrentSlider == INDEX_NONE && bCommit);
		// Set Value when using slider or when committing text
		LandscapeEdMode->SetLayerAlpha(InLayerIndex, InAlpha);
	}
}

FReply FLandscapeEditorCustomNodeBuilder_Layers::OnToggleVisibility(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_SetVisibility", "Set Layer Visibility"));
		LandscapeEdMode->SetLayerVisibility(!LandscapeEdMode->IsLayerVisible(InLayerIndex), InLayerIndex);
		if (LandscapeEdMode->IsLayerVisible(InLayerIndex))
		{
			OnLayerSelectionChanged(InLayerIndex);
		}
	}
	return FReply::Handled();
}

const FSlateBrush* FLandscapeEditorCustomNodeBuilder_Layers::GetVisibilityBrushForLayer(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	bool bIsVisible = LandscapeEdMode && LandscapeEdMode->IsLayerVisible(InLayerIndex);
	return bIsVisible ? FAppStyle::GetBrush("Level.VisibleIcon16x") : FAppStyle::GetBrush("Level.NotVisibleIcon16x");
}

FReply FLandscapeEditorCustomNodeBuilder_Layers::OnToggleLock(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Locked", "Set Layer Locked"));
		LandscapeEdMode->SetLayerLocked(InLayerIndex, !LandscapeEdMode->IsLayerLocked(InLayerIndex));
	}
	return FReply::Handled();
}

EVisibility FLandscapeEditorCustomNodeBuilder_Layers::GetLayerAlphaVisibility(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	bool bIsVisible = LandscapeEdMode && LandscapeEdMode->IsLayerAlphaVisible(InLayerIndex);
	return bIsVisible ? EVisibility::Visible : EVisibility::Hidden;
}

bool FLandscapeEditorCustomNodeBuilder_Layers::IsLayerEditionEnabled(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	const FLandscapeLayer* Layer = LandscapeEdMode ? LandscapeEdMode->GetLayer(InLayerIndex) : nullptr;
	const FLandscapeLayer* LayerReservedForSplines = Landscape ? Landscape->GetLandscapeSplinesReservedLayer() : nullptr;
	return Layer && !Layer->bLocked && (Layer != LayerReservedForSplines) && LandscapeEdMode->DoesCurrentToolAffectEditLayers();
}

const FSlateBrush* FLandscapeEditorCustomNodeBuilder_Layers::GetLockBrushForLayer(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	bool bIsLocked = LandscapeEdMode && LandscapeEdMode->IsLayerLocked(InLayerIndex);
	return bIsLocked ? FAppStyle::GetBrush(TEXT("PropertyWindow.Locked")) : FAppStyle::GetBrush(TEXT("PropertyWindow.Unlocked"));
}

int32 FLandscapeEditorCustomNodeBuilder_Layers::SlotIndexToLayerIndex(int32 SlotIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (!Landscape)
	{
		return INDEX_NONE;
	}
	
	check(Landscape->LandscapeLayers.IsValidIndex(SlotIndex));
	return Landscape->LandscapeLayers.Num() - SlotIndex - 1;
}

FReply FLandscapeEditorCustomNodeBuilder_Layers::HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 SlotIndex, SVerticalBox::FSlot* Slot)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->DoesCurrentToolAffectEditLayers())
	{
		int32 LayerIndex = SlotIndexToLayerIndex(SlotIndex);
		FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(LayerIndex);
		if (Layer && !Layer->bLocked)
		{
			TSharedPtr<SWidget> Row = GenerateRow(LayerIndex);
			if (Row.IsValid())
			{
				return FReply::Handled().BeginDragDrop(FLandscapeListElementDragDropOp::New(SlotIndex, Slot, Row));
			}
		}
	}
	return FReply::Unhandled();
}

TOptional<SDragAndDropVerticalBox::EItemDropZone> FLandscapeEditorCustomNodeBuilder_Layers::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, SVerticalBox::FSlot* Slot)
{
	TSharedPtr<FLandscapeListElementDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FLandscapeListElementDragDropOp>();
	if (DragDropOperation.IsValid())
	{
		return DropZone;
	}
	return TOptional<SDragAndDropVerticalBox::EItemDropZone>();
}

FReply FLandscapeEditorCustomNodeBuilder_Layers::HandleAcceptDrop(FDragDropEvent const& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, int32 SlotIndex, SVerticalBox::FSlot* Slot)
{
	TSharedPtr<FLandscapeListElementDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FLandscapeListElementDragDropOp>();

	if (!DragDropOperation.IsValid())
	{
		return FReply::Unhandled();
	}

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (!Landscape)
	{
		return FReply::Unhandled();
	}

	// See if we're actually getting a drag from the blueprint brush list, rather than
	// from the edit layer list
	if (DragDropOperation->IsOfType<FLandscapeBrushDragDropOp>())
	{
		int32 StartingBrushIndex = DragDropOperation->SlotIndexBeingDragged;
		int32 StartingLayerIndex = LandscapeEdMode->GetCurrentLayerIndex();
		int32 DestinationLayerIndex = SlotIndexToLayerIndex(SlotIndex);

		if (StartingLayerIndex == DestinationLayerIndex)
		{
			// See comment further below about not returning Handled()
			return FReply::Unhandled();
		}

		ALandscapeBlueprintBrushBase* Brush = Landscape->GetBrushForLayer(StartingLayerIndex, StartingBrushIndex);
		if (!ensure(Brush))
		{
			return FReply::Unhandled();
		}

		const FScopedTransaction Transaction(LOCTEXT("Landscape_LayerBrushes_MoveLayers", "Move Brush to Layer"));
		Landscape->RemoveBrushFromLayer(StartingLayerIndex, StartingBrushIndex);
		Landscape->AddBrushToLayer(DestinationLayerIndex, Brush);

		LandscapeEdMode->SetCurrentLayer(DestinationLayerIndex);
		LandscapeEdMode->RefreshDetailPanel();

		// HACK: We don't return FReply::Handled() here because otherwise, SDragAndDropVerticalBox::OnDrop
		// will apply UI slot reordering after we return. Properly speaking, we should have a way to signal 
		// that the operation was handled yet that it is not one that SDragAndDropVerticalBox should deal with.
		// For now, however, just make sure to return Unhandled.
		return FReply::Unhandled();
	}

	// This must be a drag from our own list.
	int32 StartingLayerIndex = SlotIndexToLayerIndex(DragDropOperation->SlotIndexBeingDragged);
	int32 DestinationLayerIndex = SlotIndexToLayerIndex(SlotIndex);
	const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Reorder", "Reorder Layer"));
	if (Landscape->ReorderLayer(StartingLayerIndex, DestinationLayerIndex))
	{
		LandscapeEdMode->SetCurrentLayer(DestinationLayerIndex);
		LandscapeEdMode->RefreshDetailPanel();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<FLandscapeListElementDragDropOp> FLandscapeListElementDragDropOp::New(int32 InSlotIndexBeingDragged, SVerticalBox::FSlot* InSlotBeingDragged, TSharedPtr<SWidget> WidgetToShow)
{
	TSharedRef<FLandscapeListElementDragDropOp> Operation = MakeShareable(new FLandscapeListElementDragDropOp);

	Operation->MouseCursor = EMouseCursor::GrabHandClosed;
	Operation->SlotIndexBeingDragged = InSlotIndexBeingDragged;
	Operation->SlotBeingDragged = InSlotBeingDragged;
	Operation->WidgetToShow = WidgetToShow;

	Operation->Construct();

	return Operation;
}

TSharedPtr<SWidget> FLandscapeListElementDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ContentBrowser.AssetDragDropTooltipBackground"))
		.Content()
		[
			WidgetToShow.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE