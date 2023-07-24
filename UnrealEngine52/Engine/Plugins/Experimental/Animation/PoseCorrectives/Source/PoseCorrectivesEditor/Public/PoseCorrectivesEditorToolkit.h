// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IHasPersonaToolkit.h"
#include "IPersonaPreviewScene.h"
#include "PersonaAssetEditorToolkit.h"
#include "EditorUndoClient.h"
#include "PoseCorrectivesMode.h"

class UPoseCorrectivesAsset;
class FPoseCorrectivesEditorController;
class IAnimationSequenceBrowser;

namespace PoseCorrectivesEditorModes
{
	extern const FName PoseCorrectivesEditorMode;
}

class FPoseCorrectivesEditorToolkit :
	public FPersonaAssetEditorToolkit,
	public IHasPersonaToolkit,
	public FGCObject,
	public FSelfRegisteringEditorUndoClient,
	public FTickableEditorObject
{
public:

	FPoseCorrectivesEditorToolkit();
	virtual ~FPoseCorrectivesEditorToolkit();

	void InitAssetEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr< IToolkitHost >& InitToolkitHost,
		UPoseCorrectivesAsset* Asset);

	
	/** FAssetEditorToolkit interface */
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	/** END FAssetEditorToolkit interface */

	
	/** IHasPersonaToolkit interface */
	virtual TSharedRef<IPersonaToolkit> GetPersonaToolkit() const override;
	/** END IHasPersonaToolkit interface */

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	/** END FGCObject interface */

	//** FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override;
	//~ END FTickableEditorObject Interface

	TSharedRef<FPoseCorrectivesEditorController> GetController() const {return EditorController;};

	void HandleAssetDoubleClicked(UObject* InNewAsset);
	void HandleAnimationSequenceBrowserCreated(const TSharedRef<IAnimationSequenceBrowser>& InSequenceBrowser);

private:
	void HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene);
	void HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView);
	void OnFinishedChangingDetails(const FPropertyChangedEvent& PropertyChangedEvent);
	void SetupAnimInstance();
	void HandleCreateCorrectiveClicked();

	/** toolbar */
	void BindCommands();
	void ExtendToolbar();
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);
	/** END toolbar */

	TSharedRef<FPoseCorrectivesEditorController> EditorController;

	friend FPoseCorrectivesMode;
};
