// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "SEditorViewport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class NIAGARAEDITOR_API SNiagaraAssetBrowserPreview : public SEditorViewport, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SNiagaraAssetBrowserPreview)
		{
		}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	virtual ~SNiagaraAssetBrowserPreview() override;

	void SetEmitter(UNiagaraEmitter& Emitter);
	void SetSystem(UNiagaraSystem& System);
	void ResetAsset();
	
	UNiagaraComponent* GetPreviewComponent() { return PreviewComponent; }
private:
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SNiagaraAssetBrowserPreview");
	}

private:
	TObjectPtr<UObject> AssetToDisplay;
	TObjectPtr<UNiagaraComponent> PreviewComponent;
	TObjectPtr<UNiagaraSystem> SystemForEmitterDisplay;

	/** Preview Scene - uses advanced preview settings */
	TSharedPtr<class FAdvancedPreviewScene> AdvancedPreviewScene;
	TSharedPtr<class FNiagaraAssetPreviewViewportClient> AssetPreviewViewportClient;

protected:
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
};
