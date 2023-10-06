// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "IPersonaPreviewScene.h"
	
#include "SPoseAssetNameWidget.h"

class IEditableSkeleton;
struct FAssetData;

class SRetargetSources : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SRetargetSources )
	{}

	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo);

private:

	FReply OnViewRetargetBasePose() const;
	FReply OnModifyPose();
	FReply OnSaveRetargetBasePose();

	/** The editable skeleton */
	TWeakPtr<class IEditableSkeleton> EditableSkeletonPtr;

	/** The preview scene  */
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;

	
	/** Delegate for undo/redo transaction **/
	void PostUndo();
	FText GetToggleRetargetBasePose() const;	
	void ResetRetargetBasePose();
	void UseCurrentPose();
	void ImportPose(const UPoseAsset* PoseAsset, const FName& PoseName);

	TSharedRef<SWidget> OnModifyPoseContextMenu();

	void SetSelectedPose(const FAssetData& InAssetData);
	FString GetSelectedPose() const;
	bool ShouldFilterAsset(const FAssetData& InAssetData);

	TSharedPtr<SPoseAssetNameWidget> PoseAssetNameWidget;
	TWeakObjectPtr<UPoseAsset> SelectedPoseAsset;
	FString SelectedPoseName;

	void SetPoseName(TSharedPtr<FString> PoseName, ESelectInfo::Type SelectionType);
	FReply OnImportPose();

	void TurnOnPreviewRetargetBasePose();
	bool CanImportPose() const;
};
