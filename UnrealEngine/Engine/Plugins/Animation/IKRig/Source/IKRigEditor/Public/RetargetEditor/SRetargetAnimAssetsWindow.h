// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorAnimUtils.h"
#include "IKRetargetBatchOperation.h"
#include "SEditorViewport.h"
#include "Settings/SkeletalMeshEditorSettings.h"
#include "PreviewScene.h"

class UIKRetargeter;

//** Dialog to select path to export to */
class SSelectExportPathDialog: public SWindow
{
public:
	SLATE_BEGIN_ARGS(SSelectExportPathDialog){}
	SLATE_ARGUMENT(FText, DefaultAssetPath)
	SLATE_END_ARGS()

	SSelectExportPathDialog() : UserResponse(EAppReturnType::Cancel)
	{
	}

	void Construct(const FArguments& InArgs);

public:
	/** Displays the dialog in a blocking fashion */
	EAppReturnType::Type ShowModal();

	/** Gets the resulting asset path */
	FString GetAssetPath();

protected:
	void OnPathChange(const FString& NewPath);
	FReply OnButtonClick(EAppReturnType::Type ButtonID);

	EAppReturnType::Type UserResponse;
	FText AssetPath;
};

//** Viewport in retarget dialog that shows source / target retarget pose */
class SRetargetPoseViewport: public SEditorViewport
{
	
public:
	
	SLATE_BEGIN_ARGS(SRetargetPoseViewport)
	{}
	SLATE_ARGUMENT(USkeletalMesh*, SkeletalMesh)
	SLATE_END_ARGS()

	SRetargetPoseViewport();

	void Construct(const FArguments& InArgs);
	
	void SetSkeletalMesh(USkeletalMesh* SkeletalMesh);

protected:
	
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	/** END SEditorViewport interface */

private:

	USkeletalMesh* Mesh;

	FPreviewScene PreviewScene;

	class UDebugSkelMeshComponent* PreviewComponent;

	virtual bool IsVisible() const override;
};

//** Client for SRetargetPoseViewport */
class FRetargetPoseViewportClient: public FEditorViewportClient
{
	
public:
	
	FRetargetPoseViewportClient(
		FPreviewScene& InPreviewScene,
		const TSharedRef<SRetargetPoseViewport>& InRetargetPoseViewport)
		: FEditorViewportClient(
			nullptr,
			&InPreviewScene,
			StaticCastSharedRef<SEditorViewport>(InRetargetPoseViewport))
	{
		SetViewMode(VMI_Lit);

		// Always composite editor objects after post processing in the editor
		EngineShowFlags.SetCompositeEditorPrimitives(true);
		EngineShowFlags.DisableAdvancedFeatures();

		UpdateLighting();

		// Setup defaults for the common draw helper.
		DrawHelper.bDrawPivot = false;
		DrawHelper.bDrawWorldBox = false;
		DrawHelper.bDrawKillZ = false;
		DrawHelper.bDrawGrid = true;
		DrawHelper.GridColorAxis = FColor(70, 70, 70);
		DrawHelper.GridColorMajor = FColor(40, 40, 40);
		DrawHelper.GridColorMinor =  FColor(20, 20, 20);
		DrawHelper.PerspectiveGridSize = UE_OLD_HALF_WORLD_MAX1;

		bDisableInput = true;
	}

	/** FEditorViewportClient interface */
	virtual void Tick(float DeltaTime) override
	{
		if (PreviewScene)
		{
			PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaTime);
		}
	}

	virtual FSceneInterface* GetScene() const override
	{
		return PreviewScene->GetScene();
	}

	virtual FLinearColor GetBackgroundColor() const override 
	{ 
		return FLinearColor::White; 
	}
	virtual void SetViewMode(EViewModeIndex Index) override final
	{
		FEditorViewportClient::SetViewMode(Index);
	}
	/** END FEditorViewportClient interface */

	void UpdateLighting() const
	{
		const USkeletalMeshEditorSettings* Options = GetDefault<USkeletalMeshEditorSettings>();

		PreviewScene->SetLightDirection(Options->AnimPreviewLightingDirection);
		PreviewScene->SetLightColor(Options->AnimPreviewDirectionalColor);
		PreviewScene->SetLightBrightness(Options->AnimPreviewLightBrightness);
	}
};

//** Window to display when configuring batch duplicate & retarget process */
class SRetargetAnimAssetsWindow : public SCompoundWidget
{
	
public:

	SLATE_BEGIN_ARGS(SRetargetAnimAssetsWindow)
		: _CurrentSkeletalMesh(nullptr)
		, _WidgetWindow(nullptr)
		, _ShowRemapOption(false)
	{
	}

	SLATE_ARGUMENT( USkeletalMesh*, CurrentSkeletalMesh )
	SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
	SLATE_ARGUMENT( bool, ShowRemapOption )
	SLATE_END_ARGS()	

	/** Constructs this widget for the window */
	void Construct( const FArguments& InArgs );

private:

	/** Retarget or Cancel buttons */
	bool CanApply() const;
	FReply OnApply();
	FReply OnCancel();
	void CloseWindow();

	/** Handler for dialog window close button */
	void OnDialogClosed(const TSharedRef<SWindow>& Window);

	/** Modifying Source Mesh */
	void SourceMeshAssigned(const FAssetData& InAssetData);
	FString GetCurrentSourceMeshPath() const;

	/** Modifying Target Mesh */
	void TargetMeshAssigned(const FAssetData& InAssetData);
	FString GetCurrentTargetMeshPath() const;

	/** Modifying Retargeter */
	FString GetCurrentRetargeterPath() const;
	void RetargeterAssigned(const FAssetData& InAssetData);
	
	/** Modifying "Remap Assets" checkbox */
	ECheckBoxState IsRemappingReferencedAssets() const;
	void OnRemappingReferencedAssetsChanged(ECheckBoxState InNewRadioState);

	/** Modifying Rename Prefix */
	FText GetPrefixName() const;
	void SetPrefixName(const FText &InText);

	/** Modifying Rename Suffix */
	FText GetSuffixName() const;
	void SetSuffixName(const FText &InText);

	/** Modifying Search/Replace text */
	FText GetReplaceFrom() const;
	void SetReplaceFrom(const FText &InText);
	FText GetReplaceTo() const;
	void SetReplaceTo(const FText &InText);

	/** Example rename text */
	FText GetExampleText() const;
	void UpdateExampleText();

	/** Modify folder output path */
	FText GetFolderPath() const;
	FReply GetExportFolder();

	/** Necessary data collected from UI to run retarget. */
	FIKRetargetBatchOperationContext BatchContext;

	/** The rename rule sample text */
	FText ExampleText;

	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;

	/** Viewport displaying SOURCE mesh in retarget pose */
	TSharedPtr<SRetargetPoseViewport> SourceViewport;

	/** Viewport displaying TARGET mesh in retarget pose */
	TSharedPtr<SRetargetPoseViewport> TargetViewport;

public:

	static void ShowWindow(TArray<UObject*> InAnimAssets);

	static TSharedPtr<SWindow> DialogWindow;
};