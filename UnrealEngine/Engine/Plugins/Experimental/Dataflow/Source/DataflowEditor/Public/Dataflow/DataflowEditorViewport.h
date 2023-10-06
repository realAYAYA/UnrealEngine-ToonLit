// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CanvasTypes.h"
#include "CoreMinimal.h"
#include "SAssetEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

class FDataflowEditorToolkit;
class FAdvancedPreviewScene;
class FEditorViewportClient;
class FDataflowEditorViewportClient;
class SEditorViewport;
class ADataflowActor;

// ----------------------------------------------------------------------------------

class SDataflowEditorViewport : public SAssetEditorViewport, public ICommonEditorViewportToolbarInfoProvider, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SDataflowEditorViewport) {}
		SLATE_ARGUMENT(TWeakPtr<FDataflowEditorToolkit>, DataflowEditorToolkit)
	SLATE_END_ARGS()

	SDataflowEditorViewport();

	void Construct(const FArguments& InArgs);

	//~ ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;

	//~ FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override{return TEXT("SDataflowEditorViewport");}


protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
private:

	virtual void BindCommands() override;


	/// The scene for this viewport. 
	TSharedPtr<FAdvancedPreviewScene> PreviewScene;

	/// Editor viewport client 
	TSharedPtr<FDataflowEditorViewportClient> ViewportClient;

	TWeakPtr<FDataflowEditorToolkit> DataflowEditorToolkitPtr;
	TObjectPtr<ADataflowActor> CustomDataflowActor = nullptr;
};
