// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimTimeline/AnimTimelineTrack_FloatCurve.h"
#include "CurveEditor.h"
#include "Animation/AnimSequenceBase.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AnimSequenceTimelineCommands.h"
#include "ScopedTransaction.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Animation/Skeleton.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Animation/AnimMontage.h"
#include "Widgets/Colors/SColorBlock.h"
#include "PersonaUtils.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "AnimTimeline/AnimModel_AnimSequenceBase.h"
#include "AnimTimelineClipboard.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "AnimTimeline/SAnimOutlinerItem.h"

#define LOCTEXT_NAMESPACE "FAnimTimelineTrack_FloatCurve"

ANIMTIMELINE_IMPLEMENT_TRACK(FAnimTimelineTrack_FloatCurve);

FAnimTimelineTrack_FloatCurve::FAnimTimelineTrack_FloatCurve(const FFloatCurve* InCurve, const TSharedRef<FAnimModel>& InModel)
	: FAnimTimelineTrack_Curve(&InCurve->FloatCurve, InCurve->Name, 0, ERawCurveTrackTypes::RCT_Float, FText::FromName(InCurve->Name.DisplayName), FText::FromName(InCurve->Name.DisplayName), InCurve->GetColor(), InCurve->GetColor(), InModel)
	, FloatCurve(InCurve)
	, CurveName(InCurve->Name)
	, CurveId(FAnimationCurveIdentifier(InCurve->Name, ERawCurveTrackTypes::RCT_Float))
{
	SetHeight(32.0f);
}

TSharedRef<SWidget> FAnimTimelineTrack_FloatCurve::MakeTimelineWidgetContainer()
{
	TSharedRef<SWidget> CurveWidget = MakeCurveWidget();

	// zoom to fit now we have a view
	CurveEditor->ZoomToFit(EAxisList::Y);

	auto ColorLambda = [this]()
	{
		if(GetModel()->IsTrackSelected(AsShared()))
		{
			return FAppStyle::GetSlateColor("SelectionColor").GetSpecifiedColor().CopyWithNewOpacity(0.75f);
		}
		else
		{
			return FloatCurve->GetCurveTypeFlag(AACF_Metadata) ? FloatCurve->GetColor().Desaturate(0.25f) : FloatCurve->GetColor().Desaturate(0.75f);
		}
	};

	return
		SAssignNew(TimelineWidgetContainer, SBorder)
		.Padding(0.0f)
		.BorderImage_Lambda([this](){ return FloatCurve->GetCurveTypeFlag(AACF_Metadata) ? FAppStyle::GetBrush("Sequencer.Section.SelectedSectionOverlay") : FAppStyle::GetBrush("AnimTimeline.Outliner.DefaultBorder"); })
		.BorderBackgroundColor_Lambda(ColorLambda)
		[
			CurveWidget
		];
}

TSharedRef<SWidget> FAnimTimelineTrack_FloatCurve::GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem>& InRow)
{
	TSharedPtr<SBorder> OuterBorder;
	TSharedPtr<SHorizontalBox> InnerHorizontalBox;
	TSharedRef<SWidget> OutlinerWidget = GenerateStandardOutlinerWidget(InRow, false, OuterBorder, InnerHorizontalBox);


	UAnimMontage* AnimMontage = Cast<UAnimMontage>(GetModel()->GetAnimSequenceBase());
	bool bChildAnimMontage = AnimMontage && AnimMontage->HasParentAsset();

	InnerHorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(2.0f, 1.0f)
		.FillWidth(1.0f)
		[
			SAssignNew(EditableTextLabel, SInlineEditableTextBlock)
			.IsReadOnly(bChildAnimMontage)
			.Text(this, &FAnimTimelineTrack_FloatCurve::GetLabel)
			.IsSelected_Lambda([this](){ return GetModel()->IsTrackSelected(SharedThis(this)); })
			.OnTextCommitted(this, &FAnimTimelineTrack_FloatCurve::OnCommitCurveName)
			.HighlightText(InRow->GetHighlightText())
		];

	if(!bChildAnimMontage)
	{
		OuterBorder->SetOnMouseDoubleClick(FPointerEventHandler::CreateSP(this, &FAnimTimelineTrack_FloatCurve::HandleDoubleClicked));
		AddCurveTrackButton(InnerHorizontalBox);
	}

	return OutlinerWidget;
}

TSharedRef<SWidget> FAnimTimelineTrack_FloatCurve::BuildCurveTrackMenu()
{
	FMenuBuilder MenuBuilder(true, GetModel()->GetCommandList());

	bool bIsMetadata = FloatCurve->GetCurveTypeFlag(AACF_Metadata);

	MenuBuilder.BeginSection("Curve", bIsMetadata ? LOCTEXT("CurveMetadataMenuSection", "Curve Metadata") : LOCTEXT("CurveMenuSection", "Curve"));
	{
		if(bIsMetadata)
		{
			MenuBuilder.AddMenuEntry(
				FAnimSequenceTimelineCommands::Get().ConvertMetaDataToCurve->GetLabel(),
				FAnimSequenceTimelineCommands::Get().ConvertMetaDataToCurve->GetDescription(),
				FAnimSequenceTimelineCommands::Get().ConvertMetaDataToCurve->GetIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAnimTimelineTrack_FloatCurve::ConvertMetaDataToCurve)
				)
			);
		}
		else
		{
			MenuBuilder.AddMenuEntry(
				FAnimSequenceTimelineCommands::Get().EditCurve->GetLabel(),
				FAnimSequenceTimelineCommands::Get().EditCurve->GetDescription(),
				FAnimSequenceTimelineCommands::Get().EditCurve->GetIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FAnimTimelineTrack_FloatCurve::HendleEditCurve)));

			MenuBuilder.AddMenuEntry(
				FAnimSequenceTimelineCommands::Get().ConvertCurveToMetaData->GetLabel(),
				FAnimSequenceTimelineCommands::Get().ConvertCurveToMetaData->GetDescription(),
				FAnimSequenceTimelineCommands::Get().ConvertCurveToMetaData->GetIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAnimTimelineTrack_FloatCurve::ConvertCurveToMetaData)
				)
			);
		}

		MenuBuilder.AddMenuEntry(
			FAnimSequenceTimelineCommands::Get().RemoveCurve->GetLabel(),
			FAnimSequenceTimelineCommands::Get().RemoveCurve->GetDescription(),
			FAnimSequenceTimelineCommands::Get().RemoveCurve->GetIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAnimTimelineTrack_FloatCurve::RemoveCurve)
			)
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FAnimTimelineTrack_FloatCurve::ConvertCurveToMetaData()
{
	UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();

	IAnimationDataController& Controller = AnimSequenceBase->GetController();
	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("ConvertCurveToMetaData_Bracket", "Converting curve to metadata"));
	Controller.SetCurveFlag(CurveId, AACF_Metadata, true);
	Controller.SetCurveKeys(CurveId, { FRichCurveKey(0.f, 1.f) });	

	ZoomToFit();
}

void FAnimTimelineTrack_FloatCurve::ConvertMetaDataToCurve()
{
	UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();

	IAnimationDataController& Controller = AnimSequenceBase->GetController();
	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("CurvePanel_ConvertMetaDataToCurve", "Convert metadata to curve"));
	Controller.SetCurveFlag(CurveId, AACF_Metadata, false);	
}

void FAnimTimelineTrack_FloatCurve::RemoveCurve()
{
	UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();

	if(AnimSequenceBase->GetDataModel()->FindFloatCurve(FAnimationCurveIdentifier(FloatCurve->Name, ERawCurveTrackTypes::RCT_Float)))
	{
		FSmartName TrackName;
		if (AnimSequenceBase->GetSkeleton()->GetSmartNameByUID(USkeleton::AnimCurveMappingName, FloatCurve->Name.UID, TrackName))
		{
			// Stop editing this curve in the external editor window
			IAnimationEditor::FCurveEditInfo EditInfo(CurveName, ERawCurveTrackTypes::RCT_Float, 0);
			
			AnimSequenceBase->Modify(true);

			IAnimationDataController& Controller = AnimSequenceBase->GetController();
			Controller.RemoveCurve(CurveId);

			AnimSequenceBase->PostEditChange();
		}
	}
}

void FAnimTimelineTrack_FloatCurve::OnCommitCurveName(const FText& InText, ETextCommit::Type CommitInfo)
{
	UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();

	if (USkeleton* Skeleton = AnimSequenceBase->GetSkeleton())
	{
		// only do this if the name isn't same
		FText CurrentCurveName = GetLabel();
		if (!CurrentCurveName.EqualToCaseIgnored(InText))
		{
			// Check that the name doesn't already exist
			const FName RequestedName = FName(*InText.ToString());

			const FSmartNameMapping* NameMapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

			FSmartName NewSmartName;
			if (NameMapping->FindSmartName(RequestedName, NewSmartName))
			{
				// Already in use in this sequence, and if it's not my UID
				const TArray<FFloatCurve>& FloatCurves = AnimSequenceBase->GetDataModel()->GetFloatCurves();
				if (NewSmartName.UID != FloatCurve->Name.UID && !FloatCurves.ContainsByPredicate([NewSmartName](FFloatCurve& Curve)
				{
					return Curve.Name == NewSmartName;
				}))
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("InvalidName"), FText::FromName(RequestedName));
					FNotificationInfo Info(FText::Format(LOCTEXT("AnimCurveRenamedInUse", "The name \"{InvalidName}\" is already used."), Args));

					Info.bUseLargeFont = false;
					Info.ExpireDuration = 5.0f;

					TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
					if (Notification.IsValid())
					{
						Notification->SetCompletionState(SNotificationItem::CS_Fail);
					}
					return;
				}
			}
			else
			{
				if(!Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, RequestedName, NewSmartName))
				{
					FNotificationInfo Info(LOCTEXT("AnimCurveRenamedError", "Failed to rename curve smart name, check the log for errors."));

					Info.bUseLargeFont = false;
					Info.ExpireDuration = 5.0f;

					TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
					if (Notification.IsValid())
					{
						Notification->SetCompletionState(SNotificationItem::CS_Fail);
					}
					return;
				}
			}

			IAnimationDataController& Controller = AnimSequenceBase->GetController();
			IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("CurveEditor_RenameCurve", "Renaming Curve"));
          
			FAnimationCurveIdentifier NewCurveId(NewSmartName, ERawCurveTrackTypes::RCT_Float);
			Controller.RenameCurve(CurveId, NewCurveId);
			Controller.RemoveBoneTracksMissingFromSkeleton(AnimSequenceBase->GetSkeleton());           
		}
	}
}

FText FAnimTimelineTrack_FloatCurve::GetLabel() const
{
	return FAnimTimelineTrack_FloatCurve::GetFloatCurveName(GetModel(), FloatCurve->Name);
}

void FAnimTimelineTrack_FloatCurve::Copy(UAnimTimelineClipboardContent* InOutClipboard) const
{
	check(InOutClipboard != nullptr)
	
	UFloatCurveCopyObject * CopyableCurve = UAnimCurveBaseCopyObject::Create<UFloatCurveCopyObject>();

	// Copy raw curve data
	CopyableCurve->Curve.Name = FloatCurve->Name;
	CopyableCurve->Curve.SetCurveTypeFlags(FloatCurve->GetCurveTypeFlags());
	CopyableCurve->Curve.CopyCurve(*FloatCurve);

	// Copy curve identifier data
	CopyableCurve->DisplayName = CurveName.DisplayName;
	CopyableCurve->UID = CurveName.UID;
	CopyableCurve->CurveType = ERawCurveTrackTypes::RCT_Float;
	CopyableCurve->Channel = CurveId.Channel;
	CopyableCurve->Axis = CurveId.Axis;

	// Origin data
	CopyableCurve->OriginName = GetModel()->GetAnimSequenceBase()->GetFName();
	
	InOutClipboard->Curves.Add(CopyableCurve);
}

FText FAnimTimelineTrack_FloatCurve::GetFloatCurveName(const TSharedRef<FAnimModel>& InModel, const FSmartName& InSmartName)
{
	const FSmartNameMapping* NameMapping = InModel->GetAnimSequenceBase()->GetSkeleton()->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
	if(NameMapping)
	{
		FName CurveName;
		if(NameMapping->GetName(InSmartName.UID, CurveName))
		{
			return FText::FromName(CurveName);
		}
	}

	return FText::FromName(InSmartName.DisplayName);
}

bool FAnimTimelineTrack_FloatCurve::CanEditCurve(int32 InCurveIndex) const
{
	return !FloatCurve->GetCurveTypeFlag(AACF_Metadata);
}

void FAnimTimelineTrack_FloatCurve::RequestRename()
{
	if(EditableTextLabel.IsValid())
	{
		EditableTextLabel->EnterEditingMode();
	}
}

void FAnimTimelineTrack_FloatCurve::AddCurveTrackButton(TSharedPtr<SHorizontalBox> InnerHorizontalBox)
{
	InnerHorizontalBox->AddSlot()
	.AutoWidth()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	.Padding(0.0f, 1.0f)
	[
		PersonaUtils::MakeTrackButton(LOCTEXT("EditCurveButtonText", "Curve"), FOnGetContent::CreateSP(this, &FAnimTimelineTrack_FloatCurve::BuildCurveTrackMenu), MakeAttributeSP(this, &FAnimTimelineTrack_FloatCurve::IsHovered))
	];

	auto GetValue = [this]()
	{
		return FloatCurve->Color;
	};

	auto SetValue = [this](FLinearColor InNewColor)
	{
		UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();
		AnimSequenceBase->GetController().SetCurveColor(CurveId, InNewColor);

		// Set display curves too
		for(const TPair<FCurveModelID, TUniquePtr<FCurveModel>>& CurvePair : CurveEditor->GetCurves())
		{
			CurvePair.Value->SetColor(InNewColor);
		}
	};

	auto OnGetMenuContent = [this, GetValue, SetValue]()
	{
		// Open a color picker
		return SNew(SColorPicker)
			.TargetColorAttribute_Lambda(GetValue)
			.UseAlpha(false)
			.DisplayInlineVersion(true)
			.OnColorCommitted_Lambda(SetValue)
			.OnInteractivePickBegin_Lambda([this]()
			{
				UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();
				AnimSequenceBase->GetController().OpenBracket(LOCTEXT("EditCurveColor_Bracket", "Editing Curve Color"));				
			})
			.OnInteractivePickEnd_Lambda([this]()
			{
				UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();
				AnimSequenceBase->GetController().CloseBracket();
			});
	};

	InnerHorizontalBox->AddSlot()
	.AutoWidth()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Fill)
	.Padding(2.0f, 0.0f, 0.0f, 0.0f)
	[
		SNew(SComboButton)
		.ToolTipText(LOCTEXT("EditCurveColor", "Edit Curve Color"))
		.ContentPadding(0.0f)
		.HasDownArrow(false)
		.ButtonStyle(FAppStyle::Get(), "Sequencer.AnimationOutliner.ColorStrip")
		.OnGetMenuContent_Lambda(OnGetMenuContent)
		.CollapseMenuOnParentFocus(true)
		.VAlign(VAlign_Fill)
		.ButtonContent()
		[
			SNew(SColorBlock)
			.Color_Lambda(GetValue)
			.ShowBackgroundForAlpha(false)
			.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
			.Size(FVector2D(OutlinerRightPadding - 2.0f, OutlinerRightPadding))
		]
	];
}

FLinearColor FAnimTimelineTrack_FloatCurve::GetCurveColor(int32 InCurveIndex) const
{ 
	return FloatCurve->Color; 
}

void FAnimTimelineTrack_FloatCurve::GetCurveEditInfo(int32 InCurveIndex, FSmartName& OutName, ERawCurveTrackTypes& OutType, int32& OutCurveIndex) const
{
	OutName = CurveName;
	OutType = ERawCurveTrackTypes::RCT_Float;
	OutCurveIndex = InCurveIndex;
}

#undef LOCTEXT_NAMESPACE
