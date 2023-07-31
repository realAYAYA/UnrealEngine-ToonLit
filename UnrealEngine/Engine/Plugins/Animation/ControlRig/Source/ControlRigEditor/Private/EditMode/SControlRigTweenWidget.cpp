// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditMode/SControlRigTweenWidget.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetRegistry/AssetData.h"
#include "Styling/AppStyle.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "ScopedTransaction.h"
#include "ControlRig.h"
#include "UnrealEdGlobals.h"
#include "Tools/ControlRigPose.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "ILevelSequenceEditorToolkit.h"
#include "Viewports/InViewportUIDragOperation.h"
#include "EditMode/ControlRigEditModeToolkit.h"

#define LOCTEXT_NAMESPACE "ControlRigTweenWidget"

void SControlRigTweenSlider::Construct(const FArguments& InArgs)
{

	PoseBlendValue = 0.0f;
	bIsBlending = false;
	bSliderStartedTransaction = false;
	AnimSlider = InArgs._InAnimSlider;
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[	
					SAssignNew(SpinBox,SSpinBox<double>)
					.PreventThrottling(true)
					.Value(this, &SControlRigTweenSlider::OnGetPoseBlendValue)
					.ToolTipText_Lambda([this]()
						{
							FText TooltipText = AnimSlider->GetTooltipText();
							return TooltipText;
						})
					.MinValue(-2.0)
					.MaxValue(2.0)
					.MinSliderValue(-1.0)
					.MaxSliderValue(1.0)
					.SliderExponent(1)
					.Delta(0.005)
					.MinDesiredWidth(100.0)
					.SupportDynamicSliderMinValue(true)
					.SupportDynamicSliderMaxValue(true)
					.ClearKeyboardFocusOnCommit(true)
					.SelectAllTextOnCommit(false)
					.OnValueChanged(this, &SControlRigTweenSlider::OnPoseBlendChanged)
					.OnValueCommitted(this, &SControlRigTweenSlider::OnPoseBlendCommited)
					.OnBeginSliderMovement(this, &SControlRigTweenSlider::OnBeginSliderMovement)
					.OnEndSliderMovement(this, &SControlRigTweenSlider::OnEndSliderMovement)
					
				]
			]
	];	
}

void SControlRigTweenSlider::OnPoseBlendChanged(double ChangedVal)
{
	if (WeakSequencer.IsValid() && bIsBlending)
	{
		PoseBlendValue = ChangedVal;
		AnimSlider->Blend(WeakSequencer, ChangedVal);
		WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}

}

void SControlRigTweenSlider::ResetAnimSlider()
{
	if (SpinBox.IsValid())
	{
		PoseBlendValue = 0.0;
	}
}

void SControlRigTweenSlider::DragAnimSliderTool(double Val)
{
	if (SpinBox.IsValid())
	{
		//if control is down then act like we are overriding the slider
		const bool bCtrlDown = FSlateApplication::Get().GetModifierKeys().IsControlDown();
		const double MinSliderVal = bCtrlDown ? SpinBox->GetMinValue() : SpinBox->GetMinSliderValue();
		const double MaxSliderVal = bCtrlDown ? SpinBox->GetMaxValue() : SpinBox->GetMaxSliderValue();

		double NewVal = Val;
		if (NewVal > MaxSliderVal)
		{
			NewVal = MaxSliderVal;
		}
		else if (NewVal < MinSliderVal)
		{
			NewVal = MinSliderVal;
		}
		Setup();
		bIsBlending = true;
		OnPoseBlendChanged(NewVal);
		bIsBlending = false;
	}
}

void SControlRigTweenSlider::OnBeginSliderMovement()
{
	if (bSliderStartedTransaction == false)
	{
		bIsBlending = bSliderStartedTransaction = Setup();
		if (bIsBlending)
		{
			GEditor->BeginTransaction(LOCTEXT("AnimSliderBlend", "AnimSlider Blend"));
		}
	}
}

bool SControlRigTweenSlider::Setup()
{
	//if getting sequencer from level sequence need to use the current(leader), not the focused
	ULevelSequence* LevelSequence = ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence();
	IAssetEditorInstance* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(LevelSequence, false);
	ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);
	WeakSequencer = LevelSequenceEditor ? LevelSequenceEditor->GetSequencer() : nullptr;

	return AnimSlider->Setup(WeakSequencer);
	
}

void SControlRigTweenSlider::OnEndSliderMovement(double NewValue)
{
	if (bSliderStartedTransaction)
	{
		GEditor->EndTransaction();
		bSliderStartedTransaction = false;
		bIsBlending = false;
		PoseBlendValue = 0.0f;
	}
	// Set focus back to the parent widget for users focusing the slider
	TSharedRef<SWidget> ThisRef = AsShared();
	FSlateApplication::Get().ForEachUser([&ThisRef](FSlateUser& User) {
		if (User.HasFocusedDescendants(ThisRef) && ThisRef->IsParentValid())
		{
			User.SetFocus(ThisRef->GetParentWidget().ToSharedRef());
		}
	});
	WeakSequencer = nullptr;
}


FReply SControlRigTweenWidget::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Need to remember where within a tab we grabbed
	const FVector2D TabGrabScreenSpaceOffset = MouseEvent.GetScreenSpacePosition() - MyGeometry.GetAbsolutePosition();

	FOnInViewportUIDropped OnUIDropped = FOnInViewportUIDropped::CreateSP(this, &SControlRigTweenWidget::FinishDraggingWidget);
	// Start dragging.
	TSharedRef<FInViewportUIDragOperation> DragDropOperation =
		FInViewportUIDragOperation::New(
			SharedThis(this),
			TabGrabScreenSpaceOffset,
			GetDesiredSize(),
			OnUIDropped
		);

	if (OwningToolkit.IsValid())
	{
		OwningToolkit.Pin()->TryRemoveTweenOverlay();
	}
	return FReply::Handled().BeginDragDrop(DragDropOperation);
}

void SControlRigTweenWidget::FinishDraggingWidget(const FVector2D InLocation)
{
	if (OwningToolkit.IsValid())
	{
		OwningToolkit.Pin()->UpdateTweenWidgetLocation(InLocation);
		OwningToolkit.Pin()->TryShowTweenOverlay();
	}
}

void SControlRigTweenSlider::OnPoseBlendCommited(double ChangedVal, ETextCommit::Type Type)
{
	if (SpinBox.IsValid() && SpinBox->HasKeyboardFocus())
	{
		if (bIsBlending == false)
		{
			bIsBlending = Setup();

		}
		if (bIsBlending)
		{
			FScopedTransaction ScopedTransaction(LOCTEXT("TweenTransaction", "Tween"));
			PoseBlendValue = ChangedVal;
			OnPoseBlendChanged(ChangedVal);
			bIsBlending = false;
			PoseBlendValue = 0.0f;
		}
	}
}


void SControlRigTweenWidget::OnSelectSliderTool(int32 Index)
{
	ActiveSlider = Index;
	if (ActiveSlider >= 0 && ActiveSlider < AnimBlendTools.GetAnimSliders().Num())
	{
		SliderWidget->SetAnimSlider(AnimBlendTools.GetAnimSliders()[ActiveSlider]);
	}
	
}
FText SControlRigTweenWidget::GetActiveSliderName() const
{
	if (ActiveSlider >= 0 && ActiveSlider < AnimBlendTools.GetAnimSliders().Num())
	{
		return AnimBlendTools.GetAnimSliders()[ActiveSlider].Get()->GetText();
	}
	return FText();
}

FText SControlRigTweenWidget::GetActiveSliderTooltip() const
{
	if (ActiveSlider >= 0 && ActiveSlider < AnimBlendTools.GetAnimSliders().Num())
	{
		return AnimBlendTools.GetAnimSliders()[ActiveSlider].Get()->GetTooltipText();
	}
	return FText();
}

int32 SControlRigTweenWidget::ActiveSlider = 0;


void SControlRigTweenWidget::Construct(const FArguments& InArgs)
{

	TSharedPtr<FBaseAnimSlider> BlendNeighborPtr = MakeShareable(new FBlendNeighborSlider());
	AnimBlendTools.RegisterAnimSlider(BlendNeighborPtr);
	TSharedPtr<FBaseAnimSlider> PushPullPtr = MakeShareable(new FPushPullSlider());
	AnimBlendTools.RegisterAnimSlider(PushPullPtr);
	OwningToolkit = InArgs._InOwningToolkit;
	TSharedPtr<FBaseAnimSlider> TweenPtr = MakeShareable(new FControlsToTween());
	AnimBlendTools.RegisterAnimSlider(TweenPtr);


	// Combo Button to swap sliders 
	TSharedRef<SComboButton> SliderComoboBtn = SNew(SComboButton)
		.OnGetMenuContent_Lambda([this]()
			{

				FMenuBuilder MenuBuilder(true, NULL); //maybe todo look at settting these up with commands

				MenuBuilder.BeginSection("Anim Sliders");

				int32 Index = 0;
				for (const TSharedPtr<FBaseAnimSlider>& SliderPtr : AnimBlendTools.GetAnimSliders())
				{
					FUIAction ItemAction(FExecuteAction::CreateSP(this, &SControlRigTweenWidget::OnSelectSliderTool, Index));
					MenuBuilder.AddMenuEntry(SliderPtr.Get()->GetText(), TAttribute<FText>(), FSlateIcon(), ItemAction);
					++Index;
				}

				MenuBuilder.EndSection();

				return MenuBuilder.MakeWidget();
			})
			.ButtonContent()
				[
					SNew(SHorizontalBox)
					/*  todo add an icon
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
					[
						SNew(SBox)
						.WidthOverride(16.f)
						.HeightOverride(16.f)
						[
							SNew(SImage)
							.Image_Static(&FLevelEditorToolBar::GetActiveModeIcon, LevelEditorPtr)
						]

					]
					*/
				+SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
						{
							return GetActiveSliderName();
						})
					.ToolTipText_Lambda([this]()
					{
						return GetActiveSliderTooltip();
					})
				]

			];

		TSharedRef<SHorizontalBox> MainBox = SNew(SHorizontalBox);
		TSharedPtr<FBaseAnimSlider> SliderPtr = AnimBlendTools.GetAnimSliders()[ActiveSlider];
		MainBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(2.0f, 2.0f, 2.0f, 2.0f)
		[
			SliderComoboBtn
		];
		MainBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(2.0f, 2.0f, 2.0f, 2.0f)
		[
			SAssignNew(SliderWidget,SControlRigTweenSlider).InAnimSlider(SliderPtr)
		];
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
		[
			MainBox
		]
	];
}

void SControlRigTweenWidget::GetToNextActiveSlider()
{
	int32 Index = ActiveSlider < (AnimBlendTools.GetAnimSliders().Num() - 1) ? ActiveSlider + 1 : 0;
	OnSelectSliderTool(Index);
}

void SControlRigTweenWidget::DragAnimSliderTool(double Val)
{
	if (SliderWidget.IsValid())
	{
		SliderWidget->DragAnimSliderTool(Val);
	}
}

void SControlRigTweenWidget::ResetAnimSlider()
{
	if (SliderWidget.IsValid())
	{
		SliderWidget->ResetAnimSlider();
	}
}

void SControlRigTweenWidget::StartAnimSliderTool()
{
	if (SliderWidget.IsValid())
	{
		SliderWidget->Setup();
	}
}

#undef LOCTEXT_NAMESPACE
