// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaEaseCurveTool.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "DetailLayoutBuilder.h"
#include "EaseCurveTool/AvaEaseCurvePreset.h"
#include "EaseCurveTool/AvaEaseCurveTool.h"
#include "EaseCurveTool/AvaEaseCurveToolCommands.h"
#include "EaseCurveTool/AvaEaseCurveToolSettings.h"
#include "EaseCurveTool/AvaEaseCurveSubsystem.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurveEditor.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurvePreset.h"
#include "Editor.h"
#include "EngineAnalytics.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IAvaSequencer.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/MessageDialog.h"
#include "SCurveEditor.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SAvaEaseCurveTool"

namespace UE::AvaSequencer
{
	template<typename NumericType>
	void ExtractNumericMetadata(FProperty* InProperty
		, TOptional<NumericType>& OutMinValue
		, TOptional<NumericType>& OutMaxValue
		, TOptional<NumericType>& OutSliderMinValue
		, TOptional<NumericType>& OutSliderMaxValue
		, NumericType& OutSliderExponent
		, NumericType& OutDelta
		, float& OutShiftMultiplier
		, float& OutCtrlMultiplier
		, bool& OutSupportDynamicSliderMaxValue
		, bool& OutSupportDynamicSliderMinValue)
	{
		const FString& MetaUIMinString = InProperty->GetMetaData(TEXT("UIMin"));
		const FString& MetaUIMaxString = InProperty->GetMetaData(TEXT("UIMax"));
		const FString& SliderExponentString = InProperty->GetMetaData(TEXT("SliderExponent"));
		const FString& DeltaString = InProperty->GetMetaData(TEXT("Delta"));
		const FString& ShiftMultiplierString = InProperty->GetMetaData(TEXT("ShiftMultiplier"));
		const FString& CtrlMultiplierString = InProperty->GetMetaData(TEXT("CtrlMultiplier"));
		const FString& SupportDynamicSliderMaxValueString = InProperty->GetMetaData(TEXT("SupportDynamicSliderMaxValue"));
		const FString& SupportDynamicSliderMinValueString = InProperty->GetMetaData(TEXT("SupportDynamicSliderMinValue"));
		const FString& ClampMinString = InProperty->GetMetaData(TEXT("ClampMin"));
		const FString& ClampMaxString = InProperty->GetMetaData(TEXT("ClampMax"));

		// If no UIMin/Max was specified then use the clamp string
		const FString& UIMinString = MetaUIMinString.Len() ? MetaUIMinString : ClampMinString;
		const FString& UIMaxString = MetaUIMaxString.Len() ? MetaUIMaxString : ClampMaxString;

		NumericType ClampMin = TNumericLimits<NumericType>::Lowest();
		NumericType ClampMax = TNumericLimits<NumericType>::Max();

		if (!ClampMinString.IsEmpty())
		{
			TTypeFromString<NumericType>::FromString(ClampMin, *ClampMinString);
		}

		if (!ClampMaxString.IsEmpty())
		{
			TTypeFromString<NumericType>::FromString(ClampMax, *ClampMaxString);
		}

		NumericType UIMin = TNumericLimits<NumericType>::Lowest();
		NumericType UIMax = TNumericLimits<NumericType>::Max();
		TTypeFromString<NumericType>::FromString(UIMin, *UIMinString);
		TTypeFromString<NumericType>::FromString(UIMax, *UIMaxString);

		OutSliderExponent = NumericType(1);

		if (SliderExponentString.Len())
		{
			TTypeFromString<NumericType>::FromString(OutSliderExponent, *SliderExponentString);
		}

		OutDelta = NumericType(0);

		if (DeltaString.Len())
		{
			TTypeFromString<NumericType>::FromString(OutDelta, *DeltaString);
		}

		OutShiftMultiplier = 10.f;
		if (ShiftMultiplierString.Len())
		{
			TTypeFromString<float>::FromString(OutShiftMultiplier, *ShiftMultiplierString);
		}

		OutCtrlMultiplier = 0.1f;
		if (CtrlMultiplierString.Len())
		{
			TTypeFromString<float>::FromString(OutCtrlMultiplier, *CtrlMultiplierString);
		}

		const NumericType ActualUIMin = FMath::Max(UIMin, ClampMin);
		const NumericType ActualUIMax = FMath::Min(UIMax, ClampMax);

		OutMinValue = ClampMinString.Len() ? ClampMin : TOptional<NumericType>();
		OutMaxValue = ClampMaxString.Len() ? ClampMax : TOptional<NumericType>();
		OutSliderMinValue = (UIMinString.Len()) ? ActualUIMin : TOptional<NumericType>();
		OutSliderMaxValue = (UIMaxString.Len()) ? ActualUIMax : TOptional<NumericType>();

		OutSupportDynamicSliderMaxValue = SupportDynamicSliderMaxValueString.Len() > 0 && SupportDynamicSliderMaxValueString.ToBool();
		OutSupportDynamicSliderMinValue = SupportDynamicSliderMinValueString.Len() > 0 && SupportDynamicSliderMinValueString.ToBool();
	}
}

void SAvaEaseCurveTool::Construct(const FArguments& InArgs, const TSharedRef<FAvaEaseCurveTool>& InEaseCurveTool)
{
	ToolMode = InArgs._ToolMode;
	ToolOperation = InArgs._ToolOperation;

	EaseCurveTool = InEaseCurveTool;

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.f, 1.f, 0.f, 0.f)
			[
				SAssignNew(CurvePresetWidget, SAvaEaseCurvePreset)
				.OnPresetChanged(this, &SAvaEaseCurveTool::OnPresetChanged)
				.OnQuickPresetChanged(this, &SAvaEaseCurveTool::OnQuickPresetChanged)
				.OnGetNewPresetTangents_Lambda([this](FAvaEaseCurveTangents& OutTangents) -> bool
					{
						OutTangents = EaseCurveTool->GetEaseCurveTangents();
						return true;
					})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				ConstructCurveEditorPanel()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.f, 3.f, 0.f, 0.f)
			[
				ConstructInputBoxes()
			]
		];

	BindCommands();

	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

TSharedRef<SWidget> SAvaEaseCurveTool::ConstructCurveEditorPanel()
{
	CurrentGraphSize = GetDefault<UAvaEaseCurveToolSettings>()->GetGraphSize();

	const TSharedRef<FAvaEaseCurveTool> EaseCurveToolRef = EaseCurveTool.ToSharedRef();

	return SNew(SBorder)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(CurveEaseEditorWidget, SAvaEaseCurveEditor, EaseCurveTool->GetToolCurve())
				.DisplayRate(EaseCurveToolRef, &FAvaEaseCurveTool::GetDisplayRate)
				.Operation(EaseCurveToolRef, &FAvaEaseCurveTool::GetOperation)
				.DesiredSize_Lambda([this]() -> FVector2D
					{
						return FVector2D(CurrentGraphSize);
					})
				.OnTangentsChanged(this, &SAvaEaseCurveTool::HandleEditorTangentsChanged)
				.GridSnap_UObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetGridSnap)
				.GridSize_UObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetGridSize)
				.GetContextMenuContent(this, &SAvaEaseCurveTool::CreateContextMenuContent)
				.StartText(this, &SAvaEaseCurveTool::GetStartText)
				.StartTooltipText(this, &SAvaEaseCurveTool::GetStartTooltipText)
				.EndText(this, &SAvaEaseCurveTool::GetEndText)
				.EndTooltipText(this, &SAvaEaseCurveTool::GetEndTooltipText)
				.OnKeyDown(this, &SAvaEaseCurveTool::OnKeyDown)
				.OnDragStart(this, &SAvaEaseCurveTool::OnEditorDragStart)
				.OnDragEnd(this, &SAvaEaseCurveTool::OnEditorDragEnd)
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox").TopShadowBrush))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox").BottomShadowBrush))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox").LeftShadowBrush))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox").RightShadowBrush))
			]
		];
}

TSharedRef<SWidget> SAvaEaseCurveTool::ConstructInputBoxes()
{
	constexpr float WrapSize = 120.f;

	constexpr float MinTangent = -180.0f;
	constexpr float MaxTangent = 180.0f;
	constexpr float MinWeight = 0.0f;
	constexpr float MaxWeight = 10.0f;

	return SNew(SWrapBox)
		.UseAllottedSize(true)
		.HAlign(HAlign_Center)
		+ SWrapBox::Slot()
		.FillLineWhenSizeLessThan(WrapSize)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 1.f, 0.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(26.f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("OutLabel", "Out"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				ConstructTangentNumBox(LOCTEXT("OutTangentWeightLabel", "W")
					, LOCTEXT("OutTangentWeightToolTip", "Out Tangent Weight")
					, TAttribute<float>::CreateSP(this, &SAvaEaseCurveTool::GetStartTangentWeight)
					, SNumericEntryBox<float>::FOnValueChanged::CreateSP(this, &SAvaEaseCurveTool::OnStartTangentWeightSpinBoxChanged)
					, MinWeight, MaxWeight)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f, 0.f, 0.f)
			[
				ConstructTangentNumBox(LOCTEXT("OutTangentLabel", "T")
					, LOCTEXT("OutTangentToolTip", "Out Tangent")
					, TAttribute<float>::CreateSP(this, &SAvaEaseCurveTool::GetStartTangent)
					, SNumericEntryBox<float>::FOnValueChanged::CreateSP(this, &SAvaEaseCurveTool::OnStartTangentSpinBoxChanged)
					, MinTangent, MaxTangent)
			]
		]
		+ SWrapBox::Slot()
		.FillLineWhenSizeLessThan(WrapSize)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 1.f, 0.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(26.f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("InLabel", "In"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				ConstructTangentNumBox(LOCTEXT("InTangentWeightLabel", "W")
					, LOCTEXT("InTangentWeightToolTip", "In Tangent Weight")
					, TAttribute<float>::CreateSP(this, &SAvaEaseCurveTool::GetEndTangentWeight)
					, SNumericEntryBox<float>::FOnValueChanged::CreateSP(this, &SAvaEaseCurveTool::OnEndTangentWeightSpinBoxChanged)
					, MinWeight, MaxWeight)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f, 0.f, 0.f)
			[
				ConstructTangentNumBox(LOCTEXT("InTangentLabel", "T")
					, LOCTEXT("InTangentToolTip", "In Tangent")
					, TAttribute<float>::CreateSP(this, &SAvaEaseCurveTool::GetEndTangent)
					, SNumericEntryBox<float>::FOnValueChanged::CreateSP(this, &SAvaEaseCurveTool::OnEndTangentSpinBoxChanged)
					, MinTangent, MaxTangent)
			]
		];
}

TSharedRef<SWidget> SAvaEaseCurveTool::ConstructTangentNumBox(const FText& InLabel
	, const FText& InToolTip
	, const TAttribute<float>& InValue
	, const SNumericEntryBox<float>::FOnValueChanged& InOnValueChanged
	, const TOptional<float>& InMinSliderValue
	, const TOptional<float>& InMaxSliderValue) const
{
	return SNew(SBox)
		.MaxDesiredWidth(100.f)
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.MinDesiredWidth(8.f)
				.Margin(FMargin(2.f, 5.f, 2.f, 3.f))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(InLabel)
				.ToolTipText(InToolTip)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SSpinBox<float>)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.MinSliderValue(InMinSliderValue)
				.MaxSliderValue(InMaxSliderValue)
				.Delta(0.00001f)
				.WheelStep(0.001f)
				.MinFractionalDigits(4)
				.MaxFractionalDigits(6)
				.MinDesiredWidth(70.f)
				.Value(InValue)
				.OnBeginSliderMovement_Lambda([this]()
					{
						EaseCurveTool->BeginTransaction(LOCTEXT("SliderDragStartLabel", "Ease Curve Slider Drag"));
					})
				.OnEndSliderMovement_Lambda([this](const float InNewValue)
					{
						EaseCurveTool->EndTransaction();
					})
				.OnValueChanged_Lambda([this, InOnValueChanged](const float InNewValue)
					{
						InOnValueChanged.ExecuteIfBound(InNewValue);
					})
				.OnValueCommitted_Lambda([InOnValueChanged](const float InNewValue, ETextCommit::Type InCommitType)
					{
						InOnValueChanged.ExecuteIfBound(InNewValue);
					})
			]
		];
}

void SAvaEaseCurveTool::HandleEditorTangentsChanged(const FAvaEaseCurveTangents& InTangents) const
{
	SetTangents(InTangents, ToolOperation.Get(), true, true, true);
}

void SAvaEaseCurveTool::OnEditorDragStart() const
{
	EaseCurveTool->BeginTransaction(LOCTEXT("EditorDragStartLabel", "Ease Curve Graph Drag"));
}

void SAvaEaseCurveTool::OnEditorDragEnd() const
{
	EaseCurveTool->EndTransaction();
}

void SAvaEaseCurveTool::SetTangents(const FAvaEaseCurveTangents& InTangents, FAvaEaseCurveTool::EOperation InOperation
	, const bool bInSetEaseCurve, const bool bInBroadcastUpdate, const bool bInSetSequencerTangents) const
{
	if (CurvePresetWidget.IsValid())
	{
		if (!CurvePresetWidget->SetSelectedItem(InTangents))
		{
			CurvePresetWidget->ClearSelection();
		}
	}

	// To change the graph UI tangents, we need to change the ease curve object tangents and the graph will reflect.
	if (bInSetEaseCurve && EaseCurveTool.IsValid())
	{
		EaseCurveTool->SetEaseCurveTangents(InTangents, InOperation, bInBroadcastUpdate, bInSetSequencerTangents);
	}

	if (GetDefault<UAvaEaseCurveToolSettings>()->GetAutoZoomToFit())
	{
		ZoomToFit();
	}
}

float SAvaEaseCurveTool::GetStartTangent() const
{
	return EaseCurveTool->GetEaseCurveTangents().Start;
}

float SAvaEaseCurveTool::GetStartTangentWeight() const
{
	return EaseCurveTool->GetEaseCurveTangents().StartWeight;
}

float SAvaEaseCurveTool::GetEndTangent() const
{
	return EaseCurveTool->GetEaseCurveTangents().End;
}

float SAvaEaseCurveTool::GetEndTangentWeight() const
{
	return EaseCurveTool->GetEaseCurveTangents().EndWeight;
}

void SAvaEaseCurveTool::OnStartTangentSpinBoxChanged(const float InNewValue) const
{
	FAvaEaseCurveTangents NewTangents = EaseCurveTool->GetEaseCurveTangents();
	NewTangents.Start = InNewValue;
	SetTangents(NewTangents, ToolOperation.Get(), true, true, true);
}

void SAvaEaseCurveTool::OnStartTangentWeightSpinBoxChanged(const float InNewValue) const
{
	FAvaEaseCurveTangents NewTangents = EaseCurveTool->GetEaseCurveTangents();
	NewTangents.StartWeight = InNewValue;
	SetTangents(NewTangents, ToolOperation.Get(), true, true, true);
}

void SAvaEaseCurveTool::OnEndTangentSpinBoxChanged(const float InNewValue) const
{
	FAvaEaseCurveTangents NewTangents = EaseCurveTool->GetEaseCurveTangents();
	NewTangents.End = InNewValue;
	SetTangents(NewTangents, ToolOperation.Get(), true, true, true);
}

void SAvaEaseCurveTool::OnEndTangentWeightSpinBoxChanged(const float InNewValue) const
{
	FAvaEaseCurveTangents NewTangents = EaseCurveTool->GetEaseCurveTangents();
	NewTangents.EndWeight = InNewValue;
	SetTangents(NewTangents, ToolOperation.Get(), true, true, true);
}

void SAvaEaseCurveTool::OnPresetChanged(const TSharedPtr<FAvaEaseCurvePreset>& InPreset) const
{
	SetTangents(InPreset->Tangents, ToolOperation.Get(), true, true, true);

	FSlateApplication::Get().SetAllUserFocus(CurveEaseEditorWidget);

	if (FEngineAnalytics::IsAvailable())
	{
		// Only send analytics for default presets
		const TMap<FString, TArray<FString>>& DefaultPresetNames = UAvaEaseCurveSubsystem::GetDefaultCategoryPresetNames();
		if (DefaultPresetNames.Contains(InPreset->Category)
			&& DefaultPresetNames[InPreset->Category].Contains(InPreset->Name))
		{
			TArray<FAnalyticsEventAttribute> Attributes;
			Attributes.Emplace(TEXT("Category"), InPreset->Category);
			Attributes.Emplace(TEXT("Name"), InPreset->Name);

			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MotionDesign.EaseCurveTool.SetTangentsPreset"), Attributes);
		}
	}
}

void SAvaEaseCurveTool::OnQuickPresetChanged(const TSharedPtr<FAvaEaseCurvePreset>& InPreset) const
{
	FSlateApplication::Get().SetAllUserFocus(CurveEaseEditorWidget);
}

void SAvaEaseCurveTool::BindCommands()
{
	const FAvaEaseCurveToolCommands& EaseCurveToolCommands = FAvaEaseCurveToolCommands::Get();

	const TSharedRef<FAvaEaseCurveTool> EaseCurveToolRef = EaseCurveTool.ToSharedRef();

	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(FGenericCommands::Get().Undo, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::UndoAction));
	
	CommandList->MapAction(FGenericCommands::Get().Redo, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::RedoAction));

	CommandList->MapAction(EaseCurveToolCommands.OpenToolSettings, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::OpenToolSettings));

	CommandList->MapAction(EaseCurveToolCommands.ResetToDefaultPresets, FExecuteAction::CreateSP(this, &SAvaEaseCurveTool::ResetToDefaultPresets));

	CommandList->MapAction(EaseCurveToolCommands.Refresh, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::UpdateEaseCurveFromSequencerKeySelections));
	
	CommandList->MapAction(EaseCurveToolCommands.Apply, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::ApplyEaseCurveToSequencerKeySelections));

	CommandList->MapAction(EaseCurveToolCommands.ZoomToFit, FExecuteAction::CreateSP(this, &SAvaEaseCurveTool::ZoomToFit));

	CommandList->MapAction(EaseCurveToolCommands.ToggleGridSnap
		, FExecuteAction::CreateUObject(GetMutableDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::ToggleGridSnap)
		, FCanExecuteAction()
		, FIsActionChecked::CreateUObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetGridSnap));

	CommandList->MapAction(EaseCurveToolCommands.ToggleAutoFlipTangents
		, FExecuteAction::CreateUObject(GetMutableDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::ToggleAutoFlipTangents)
		, FCanExecuteAction()
		, FIsActionChecked::CreateUObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetAutoFlipTangents));

	CommandList->MapAction(EaseCurveToolCommands.ToggleAutoZoomToFit
		, FExecuteAction::CreateUObject(GetMutableDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::ToggleAutoZoomToFit)
		, FCanExecuteAction()
		, FIsActionChecked::CreateUObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetAutoZoomToFit));

	CommandList->MapAction(EaseCurveToolCommands.SelectNextChannelKey, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::SelectNextChannelKey));
	
	CommandList->MapAction(EaseCurveToolCommands.SelectPreviousChannelKey, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::SelectPreviousChannelKey));

	CommandList->MapAction(EaseCurveToolCommands.SetOperationToEaseOut
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::SetToolOperation, FAvaEaseCurveTool::EOperation::Out)
		, FCanExecuteAction()
		, FIsActionChecked::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::IsToolOperation, FAvaEaseCurveTool::EOperation::Out));

	CommandList->MapAction(EaseCurveToolCommands.SetOperationToEaseInOut
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::SetToolOperation, FAvaEaseCurveTool::EOperation::InOut)
		, FCanExecuteAction()
		, FIsActionChecked::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::IsToolOperation, FAvaEaseCurveTool::EOperation::InOut));

	CommandList->MapAction(EaseCurveToolCommands.SetOperationToEaseIn
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::SetToolOperation, FAvaEaseCurveTool::EOperation::In)
		, FCanExecuteAction()
		, FIsActionChecked::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::IsToolOperation, FAvaEaseCurveTool::EOperation::In));

	CommandList->MapAction(EaseCurveToolCommands.ResetTangents
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::ResetEaseCurveTangents, FAvaEaseCurveTool::EOperation::InOut));

	CommandList->MapAction(EaseCurveToolCommands.ResetStartTangent
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::ResetEaseCurveTangents, FAvaEaseCurveTool::EOperation::Out));

	CommandList->MapAction(EaseCurveToolCommands.ResetEndTangent
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::ResetEaseCurveTangents, FAvaEaseCurveTool::EOperation::In));

	CommandList->MapAction(EaseCurveToolCommands.FlattenTangents
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::FlattenOrStraightenTangents, FAvaEaseCurveTool::EOperation::InOut, true));

	CommandList->MapAction(EaseCurveToolCommands.FlattenStartTangent
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::FlattenOrStraightenTangents, FAvaEaseCurveTool::EOperation::Out, true));

	CommandList->MapAction(EaseCurveToolCommands.FlattenEndTangent
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::FlattenOrStraightenTangents, FAvaEaseCurveTool::EOperation::In, true));

	CommandList->MapAction(EaseCurveToolCommands.StraightenTangents
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::FlattenOrStraightenTangents, FAvaEaseCurveTool::EOperation::InOut, false));

	CommandList->MapAction(EaseCurveToolCommands.StraightenStartTangent
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::FlattenOrStraightenTangents, FAvaEaseCurveTool::EOperation::Out, false));

	CommandList->MapAction(EaseCurveToolCommands.StraightenEndTangent
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::FlattenOrStraightenTangents, FAvaEaseCurveTool::EOperation::In, false));

	CommandList->MapAction(EaseCurveToolCommands.CopyTangents
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::CopyTangentsToClipboard)
		, FCanExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::CanCopyTangentsToClipboard));

	CommandList->MapAction(EaseCurveToolCommands.PasteTangents
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::PasteTangentsFromClipboard)
		, FCanExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::CanPasteTangentsFromClipboard));

	CommandList->MapAction(EaseCurveToolCommands.CreateExternalCurveAsset
		, FExecuteAction::CreateLambda([this]()
			{
				EaseCurveTool->CreateCurveAsset();
			})
		, FCanExecuteAction());

	CommandList->MapAction(EaseCurveToolCommands.SetKeyInterpConstant,
		FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Constant, RCTM_Auto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Constant, RCTM_Auto));

	CommandList->MapAction(EaseCurveToolCommands.SetKeyInterpLinear,
		FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Linear, RCTM_Auto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Linear, RCTM_Auto));

	CommandList->MapAction(EaseCurveToolCommands.SetKeyInterpCubicAuto,
		FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Cubic, RCTM_Auto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Cubic, RCTM_Auto));

	CommandList->MapAction(EaseCurveToolCommands.SetKeyInterpCubicSmartAuto,
		FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Cubic, RCTM_SmartAuto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Cubic, RCTM_SmartAuto));

	CommandList->MapAction(EaseCurveToolCommands.SetKeyInterpCubicUser,
		FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Cubic, RCTM_User),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Cubic, RCTM_User));

	CommandList->MapAction(EaseCurveToolCommands.SetKeyInterpCubicBreak,
		FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Cubic, RCTM_Break),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Cubic, RCTM_Break));

	CommandList->MapAction(EaseCurveToolCommands.QuickEase
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::ApplyQuickEaseToSequencerKeySelections, FAvaEaseCurveTool::EOperation::InOut));

	CommandList->MapAction(EaseCurveToolCommands.QuickEaseIn
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::ApplyQuickEaseToSequencerKeySelections, FAvaEaseCurveTool::EOperation::In));

	CommandList->MapAction(EaseCurveToolCommands.QuickEaseOut
		, FExecuteAction::CreateSP(EaseCurveToolRef, &FAvaEaseCurveTool::ApplyQuickEaseToSequencerKeySelections, FAvaEaseCurveTool::EOperation::Out));
}

TSharedRef<SWidget> SAvaEaseCurveTool::CreateContextMenuContent()
{
	UToolMenus* const ToolMenus = UToolMenus::Get();
	if (!ToolMenus)
	{
		return SNullWidget::NullWidget;
	}

	constexpr const TCHAR* MenuName = TEXT("AvaEaseCurveToolMenu");

	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenu* const ToolMenu = ToolMenus->RegisterMenu(MenuName, NAME_None, EMultiBoxType::Menu);

		FToolMenuSection& Section = ToolMenu->FindOrAddSection(TEXT("EaseCurveTool"), LOCTEXT("EaseCurveToolActions", "Curve Ease Tool Actions"));

		const FAvaEaseCurveToolCommands& EaseCurveToolCommands = FAvaEaseCurveToolCommands::Get();

		Section.AddSubMenu(TEXT("Settings"),
			LOCTEXT("SettingsSubMenuLabel", "Settings"),
			LOCTEXT("SettingsSubMenuToolTip", ""),
			FNewToolMenuDelegate::CreateSP(this, &SAvaEaseCurveTool::MakeContextMenuSettings),
			false,
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), TEXT("Icons.Toolbar.Settings")));

		Section.AddSeparator(NAME_None);

		Section.AddMenuEntry(EaseCurveToolCommands.CreateExternalCurveAsset);

		Section.AddSeparator(NAME_None);

		Section.AddMenuEntry(EaseCurveToolCommands.CopyTangents);

		Section.AddMenuEntry(EaseCurveToolCommands.PasteTangents);

		Section.AddSeparator(NAME_None);

		Section.AddSubMenu(TEXT("StraightenTangents"),
			LOCTEXT("StraightenTangentsSubMenuLabel", "Straighten Tangents"),
			LOCTEXT("StraightenTangentsSubMenuToolTip", ""),
			FNewToolMenuDelegate::CreateLambda([&EaseCurveToolCommands](UToolMenu* InToolMenu)
				{
					FToolMenuSection& NewSection = InToolMenu->FindOrAddSection(TEXT("StraightenTangents"));
					NewSection.AddMenuEntry(EaseCurveToolCommands.StraightenTangents);
					NewSection.AddSeparator(NAME_None);
					NewSection.AddMenuEntry(EaseCurveToolCommands.StraightenStartTangent);
					NewSection.AddMenuEntry(EaseCurveToolCommands.StraightenEndTangent);
				}),
			false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), TEXT("GenericCurveEditor.StraightenTangents")));

		Section.AddSubMenu(TEXT("FlattenTangents"),
			LOCTEXT("FlattenTangentsSubMenuLabel", "Flatten Tangents"),
			LOCTEXT("FlattenTangentsSubMenuToolTip", ""),
			FNewToolMenuDelegate::CreateLambda([&EaseCurveToolCommands](UToolMenu* InToolMenu)
				{
					FToolMenuSection& NewSection = InToolMenu->FindOrAddSection(TEXT("FlattenTangents"));
					NewSection.AddMenuEntry(EaseCurveToolCommands.FlattenTangents);
					NewSection.AddSeparator(NAME_None);
					NewSection.AddMenuEntry(EaseCurveToolCommands.FlattenStartTangent);
					NewSection.AddMenuEntry(EaseCurveToolCommands.FlattenEndTangent);
				}), 
			false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), TEXT("GenericCurveEditor.FlattenTangents")));

		Section.AddSubMenu(TEXT("ResetTangents"),
			LOCTEXT("ResetTangentsSubMenuLabel", "Reset Tangents"),
			LOCTEXT("ResetTangentsSubMenuToolTip", ""),
			FNewToolMenuDelegate::CreateLambda([&EaseCurveToolCommands](UToolMenu* InToolMenu)
				{
					FToolMenuSection& NewSection = InToolMenu->FindOrAddSection(TEXT("ResetTangents"));
					NewSection.AddMenuEntry(EaseCurveToolCommands.ResetTangents);
					NewSection.AddSeparator(NAME_None);
					NewSection.AddMenuEntry(EaseCurveToolCommands.ResetStartTangent);
					NewSection.AddMenuEntry(EaseCurveToolCommands.ResetEndTangent);
				}),
			false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), TEXT("PropertyWindow.DiffersFromDefault")));

		Section.AddSeparator(NAME_None);

		/** @TODO: Only show these in single edit mode(?)
		Section.AddMenuEntry(EaseCurveToolCommands.SetKeyInterpConstant);

		Section.AddMenuEntry(EaseCurveToolCommands.SetKeyInterpLinear);

		Section.AddMenuEntry(EaseCurveToolCommands.SetKeyInterpCubicAuto);

		Section.AddMenuEntry(EaseCurveToolCommands.SetKeyInterpCubicSmartAuto);

		Section.AddMenuEntry(EaseCurveToolCommands.SetKeyInterpCubicUser);

		Section.AddMenuEntry(EaseCurveToolCommands.SetKeyInterpCubicBreak);

		Section.AddSeparator(NAME_None);*/

		Section.AddMenuEntry(EaseCurveToolCommands.SetOperationToEaseOut);

		Section.AddMenuEntry(EaseCurveToolCommands.SetOperationToEaseInOut);

		Section.AddMenuEntry(EaseCurveToolCommands.SetOperationToEaseIn);

		Section.AddSeparator(NAME_None);

		Section.AddMenuEntry(EaseCurveToolCommands.SelectPreviousChannelKey);

		Section.AddMenuEntry(EaseCurveToolCommands.SelectNextChannelKey);

		Section.AddSeparator(NAME_None);

		Section.AddMenuEntry(EaseCurveToolCommands.ToggleGridSnap);

		Section.AddMenuEntry(EaseCurveToolCommands.ZoomToFit);

		Section.AddSeparator(NAME_None);

		Section.AddMenuEntry(EaseCurveToolCommands.Refresh);

		Section.AddMenuEntry(EaseCurveToolCommands.Apply);
	}

	return ToolMenus->GenerateWidget(MenuName, FToolMenuContext(CommandList));
}

void SAvaEaseCurveTool::MakeContextMenuSettings(UToolMenu* const InToolMenu)
{
	if (!IsValid(InToolMenu))
	{
		return;
	}

	const FAvaEaseCurveToolCommands& EaseCurveToolCommands = FAvaEaseCurveToolCommands::Get();

	FToolMenuSection& Section = InToolMenu->FindOrAddSection(TEXT("EaseCurveToolSettings"), LOCTEXT("EaseCurveToolSettingsActions", "Settings"));

	Section.AddMenuEntry(EaseCurveToolCommands.OpenToolSettings);

	Section.AddSeparator(NAME_None);

	Section.AddMenuEntry(EaseCurveToolCommands.ToggleAutoFlipTangents);

	Section.AddSeparator(NAME_None);

	// Graph Size
	{
		FProperty* const GraphSizeProperty = UAvaEaseCurveToolSettings::StaticClass()->FindPropertyByName(TEXT("GraphSize"));
		check(GraphSizeProperty);

		TOptional<int32> MinValue, MaxValue, SliderMinValue, SliderMaxValue;
		int32 SliderExponent, Delta;
		float ShiftMultiplier = 10.f;
		float CtrlMultiplier = 0.1f;
		bool SupportDynamicSliderMaxValue = false;
		bool SupportDynamicSliderMinValue = false;
		UE::AvaSequencer::ExtractNumericMetadata(GraphSizeProperty
			, MinValue, MaxValue
			, SliderMinValue, SliderMaxValue
			, SliderExponent, Delta
			, ShiftMultiplier
			, CtrlMultiplier
			, SupportDynamicSliderMaxValue, SupportDynamicSliderMinValue);

		const TSharedRef<SNumericEntryBox<int32>> GraphSizeWidget = SNew(SNumericEntryBox<int32>)
			.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
			.AllowSpin(true)
			.MinValue(MinValue)
			.MaxValue(MaxValue)
			.MinSliderValue(SliderMinValue)
			.MaxSliderValue(SliderMaxValue)
			.SliderExponent(SliderExponent)
			.Delta(Delta)
			.ShiftMultiplier(ShiftMultiplier)
			.CtrlMultiplier(CtrlMultiplier)
			.SupportDynamicSliderMaxValue(SupportDynamicSliderMaxValue)
			.SupportDynamicSliderMinValue(SupportDynamicSliderMinValue)
			.Value_Lambda([this]()
				{
					return CurrentGraphSize;
				})
			.OnValueChanged_Lambda([this](const int32 InNewValue)
				{
					CurrentGraphSize = InNewValue;
				})
			.OnValueCommitted_Lambda([this](const int32 InNewValue, ETextCommit::Type InCommitType)
				{
					CurrentGraphSize = InNewValue;

					UAvaEaseCurveToolSettings* const EaseCurveToolSettings = GetMutableDefault<UAvaEaseCurveToolSettings>();
					check(EaseCurveToolSettings);
					EaseCurveToolSettings->SetGraphSize(CurrentGraphSize);
					EaseCurveToolSettings->SaveConfig();
				});

		Section.AddEntry(FToolMenuEntry::InitWidget(TEXT("ToolSize"), GraphSizeWidget, LOCTEXT("ToolSizeLabel", "Tool Size")));
	}

	// Grid Size
	{
		FProperty* const GridSizeProperty = UAvaEaseCurveToolSettings::StaticClass()->FindPropertyByName(TEXT("GridSize"));
		check(GridSizeProperty);

		TOptional<int32> MinValue, MaxValue, SliderMinValue, SliderMaxValue;
		int32 SliderExponent, Delta;
		float ShiftMultiplier = 10.f;
		float CtrlMultiplier = 0.1f;
		bool SupportDynamicSliderMaxValue = false;
		bool SupportDynamicSliderMinValue = false;
		UE::AvaSequencer::ExtractNumericMetadata(GridSizeProperty
			, MinValue, MaxValue
			, SliderMinValue, SliderMaxValue
			, SliderExponent, Delta
			, ShiftMultiplier
			, CtrlMultiplier
			, SupportDynamicSliderMaxValue, SupportDynamicSliderMinValue);

		auto SetEaseCurveToolGridSize = [](const int32 InNewValue)
			{
				UAvaEaseCurveToolSettings* const EaseCurveToolSettings = GetMutableDefault<UAvaEaseCurveToolSettings>();
				check(EaseCurveToolSettings);
				EaseCurveToolSettings->SetGridSize(InNewValue);
				EaseCurveToolSettings->SaveConfig();
			};

		const TSharedRef<SNumericEntryBox<int32>> GridSizeWidget = SNew(SNumericEntryBox<int32>)
			.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
			.AllowSpin(true)
			.MinValue(MinValue)
			.MaxValue(MaxValue)
			.MinSliderValue(SliderMinValue)
			.MaxSliderValue(SliderMaxValue)
			.SliderExponent(SliderExponent)
			.Delta(Delta)
			.ShiftMultiplier(ShiftMultiplier)
			.CtrlMultiplier(CtrlMultiplier)
			.SupportDynamicSliderMaxValue(SupportDynamicSliderMaxValue)
			.SupportDynamicSliderMinValue(SupportDynamicSliderMinValue)
			.Value_Lambda([this]()
				{
					return GetDefault<UAvaEaseCurveToolSettings>()->GetGridSize();
				})
			.OnValueChanged_Lambda(SetEaseCurveToolGridSize)
			.OnValueCommitted_Lambda([SetEaseCurveToolGridSize](const int32 InNewValue, ETextCommit::Type InCommitType)
				{
					SetEaseCurveToolGridSize(InNewValue);
				});

		Section.AddEntry(FToolMenuEntry::InitWidget(TEXT("GridSize"), GridSizeWidget, LOCTEXT("GridSizeLabel", "Grid Size")));
	}

	Section.AddSeparator(NAME_None);

	Section.AddMenuEntry(EaseCurveToolCommands.ToggleAutoZoomToFit);

	Section.AddSeparator(NAME_None);

	Section.AddMenuEntry(EaseCurveToolCommands.ResetToDefaultPresets);
}

void SAvaEaseCurveTool::UndoAction()
{
	if (GEditor)
	{
		GEditor->UndoTransaction();
	}
}

void SAvaEaseCurveTool::RedoAction()
{
	if (GEditor)
	{
		GEditor->RedoTransaction();
	}
}

void SAvaEaseCurveTool::ZoomToFit() const
{
	if (CurveEaseEditorWidget.IsValid())
	{
		CurveEaseEditorWidget->ZoomToFit();
	}
}

FKeyHandle SAvaEaseCurveTool::GetSelectedKeyHandle() const
{
	if (CurveEaseEditorWidget.IsValid())
	{
		return CurveEaseEditorWidget->GetSelectedKeyHandle();
	}
	return FKeyHandle::Invalid();
}

FReply SAvaEaseCurveTool::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FText SAvaEaseCurveTool::GetStartText() const
{
	return (ToolMode.Get(FAvaEaseCurveTool::EMode::DualKeyEdit) == FAvaEaseCurveTool::EMode::DualKeyEdit)
		? LOCTEXT("StartText", "Leave")
		: LOCTEXT("ArriveText", "Arrive");
}

FText SAvaEaseCurveTool::GetStartTooltipText() const
{
	return (ToolMode.Get(FAvaEaseCurveTool::EMode::DualKeyEdit) == FAvaEaseCurveTool::EMode::DualKeyEdit)
		? LOCTEXT("StartTooltipText", "Start: The selected key's leave tangent")
		: LOCTEXT("ArriveTooltipText", "Arrive");
}

FText SAvaEaseCurveTool::GetEndText() const
{
	return (ToolMode.Get(FAvaEaseCurveTool::EMode::DualKeyEdit) == FAvaEaseCurveTool::EMode::DualKeyEdit)
		? LOCTEXT("EndText", "Arrive")
		: LOCTEXT("LeaveText", "Leave");
}

FText SAvaEaseCurveTool::GetEndTooltipText() const
{
	return (ToolMode.Get(FAvaEaseCurveTool::EMode::DualKeyEdit) == FAvaEaseCurveTool::EMode::DualKeyEdit)
		? LOCTEXT("EndTooltipText", "End: The next key's arrive tangent")
		: LOCTEXT("LeaveTooltipText", "Leave");
}

void SAvaEaseCurveTool::ResetToDefaultPresets()
{
	const FText MessageBoxTitle = LOCTEXT("ResetToDefaultPresets", "Reset To Default Presets");
	const EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::YesNoCancel
		, LOCTEXT("ConfirmResetToDefaultPresets", "Are you sure you want to reset to default presets?\n\n"
			"*CAUTION* All directories and files inside '[Project]/Config/EaseCurves' will be lost!")
		, MessageBoxTitle);
	if (Response == EAppReturnType::Yes)
	{
		UAvaEaseCurveSubsystem::Get().ResetToDefaultPresets(false);
	}
}

#undef LOCTEXT_NAMESPACE
