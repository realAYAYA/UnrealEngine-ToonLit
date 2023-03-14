// Copyright Epic Games, Inc. All Rights Reserved.
/**
* Hold the Slate Views for the different Control Rig Asset Views
* This is shown in the Bottom of The SControlRigBaseListWidget
*/
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Tools/ControlRigPose.h"
#include "Misc/NotifyHook.h"

class FAssetThumbnail;
class UControlRig;

//Class to Hold Statics that are shared and externally callable
class FControlRigView
{
public:
	static void CaptureThumbnail(UObject* Asset);
};

class SControlRigPoseView : public SCompoundWidget, public FNotifyHook
{
	SLATE_BEGIN_ARGS(SControlRigPoseView) {}
	SLATE_ARGUMENT(UControlRigPoseAsset*, PoseAsset)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SControlRigPoseView();

	//FNotifyHook
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

private:
	void UpdateStatusBlocks();

	/*
	* Delegates and Helpers
	*/
	ECheckBoxState IsKeyPoseChecked() const;
	void OnKeyPoseChecked(ECheckBoxState NewState);
	ECheckBoxState IsMirrorPoseChecked() const;
	void OnMirrorPoseChecked(ECheckBoxState NewState);
	bool IsMirrorEnabled() const;
	void OnPoseBlendChanged(float ChangedVal);
	void OnPoseBlendCommited(float ChangedVal, ETextCommit::Type Type);
	void OnBeginSliderMovement();
	void OnEndSliderMovement(float NewValue);
	float OnGetPoseBlendValue()const {return PoseBlendValue;}
	FReply OnPastePose();
	FReply OnSelectControls();
	FReply OnCaptureThumbnail();
	void HandleControlSelected(UControlRig* Subject, FRigControlElement* InControl, bool bSelected);
	void HandleControlAdded(UControlRig* ControlRig, bool bIsAdded);
	TArray<UControlRig*> GetControlRigs();

	static bool bIsKey;
	static bool bIsMirror;
	float PoseBlendValue;
	bool bIsBlending;
	bool bSliderStartedTransaction;
	FControlRigControlPose TempPose;
	TSharedPtr< FAssetThumbnail > Thumbnail;
	TSharedRef<SWidget> GetThumbnailWidget();

	TWeakObjectPtr<UControlRigPoseAsset> PoseAsset;
	TArray<TWeakObjectPtr<UControlRig>> CurrentControlRigs;

	TSharedPtr<STextBlock> TextStatusBlock1;
	TSharedPtr<STextBlock> TextStatusBlock2;


	/* Mirroring*/
	TSharedPtr<IDetailsView> MirrorDetailsView;

	TSharedRef<ITableRow> OnGenerateWidgetForList(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{

		return SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
			.Content()
			[
				SNew(SBox)
				.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString((*InItem)))
			]
			];
	}

public:
	static bool IsMirror() { return bIsMirror; }
};

