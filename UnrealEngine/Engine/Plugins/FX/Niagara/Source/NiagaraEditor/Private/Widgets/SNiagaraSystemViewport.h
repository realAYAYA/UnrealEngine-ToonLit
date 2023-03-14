// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/GCObject.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "PreviewScene.h"
#include "Framework/Commands/UICommandList.h"
#include "EditorViewportClient.h"
#include "ISequencerModule.h"
#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "Particles/ParticlePerfStatsManager.h"
#include "NiagaraPerfBaseline.h"

class UNiagaraComponent;
class FNiagaraSystemEditorViewportClient;
class FNiagaraSystemInstance;
class UNiagaraEffectType;

/**
 * Niagara Editor Preview viewport widget
 */
class SNiagaraSystemViewport : public SEditorViewport, public FGCObject, public ICommonEditorViewportToolbarInfoProvider
{
public:
	DECLARE_DELEGATE_OneParam(FOnThumbnailCaptured, UTexture2D*);

public:
	SLATE_BEGIN_ARGS( SNiagaraSystemViewport ){}
		SLATE_EVENT(FOnThumbnailCaptured, OnThumbnailCaptured)
		/** So we can retrieve different data from sequencer to display in the viewport */
		SLATE_ARGUMENT(TWeakPtr<ISequencer>, Sequencer)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);
	~SNiagaraSystemViewport();
	
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SNiagaraSystemViewport");
	}

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	void RefreshViewport();
	
	void SetPreviewComponent(UNiagaraComponent* NiagaraComponent);
	
	void ToggleRealtime();
	
	/** @return The list of commands known by the niagara editor */
	TSharedRef<FUICommandList> GetNiagaraSystemEditorCommands() const;
	
	/** If true, render background object in the preview scene. */
	bool bShowBackground;

	TSharedRef<class FAdvancedPreviewScene> GetPreviewScene() { return AdvancedPreviewScene.ToSharedRef(); }

	/** The material editor has been added to a tab */
	void OnAddedToTab( const TSharedRef<SDockTab>& OwnerTab );
	
	/** Event handlers */
	void TogglePreviewGrid();
	bool IsTogglePreviewGridChecked() const;
	void TogglePreviewBackground();
	bool IsTogglePreviewBackgroundChecked() const;
	class UNiagaraComponent *GetPreviewComponent()	{ return PreviewComponent;  }

	// ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	// End of ICommonEditorViewportToolbarInfoProvider interface

	/** Draw flag types */
	enum EDrawElements
	{
		Bounds = 0x020,
		InstructionCounts = 0x040,
		ParticleCounts = 0x080,
		EmitterExecutionOrder = 0x100,
		GpuTickInformation = 0x200,
	};

	bool GetDrawElement(EDrawElements Element) const;
	void ToggleDrawElement(EDrawElements Element);
	void CreateThumbnail(UObject* InScreenShotOwner);


	bool IsToggleOrbitChecked() const;
	void ToggleOrbit();

protected:
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual EVisibility OnGetViewportContentVisibility() const override;
	virtual void BindCommands() override;
	virtual void OnFocusViewportToSelection() override;
	virtual void PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay) override;
	FText GetViewportCompileStatusText() const;

private:
	bool IsVisible() const override;

	void OnScreenShotCaptured(UTexture2D* ScreenShot);

private:
	/** The parent tab where this viewport resides */
	TWeakPtr<SDockTab> ParentTab;

	TWeakPtr<ISequencer> Sequencer = nullptr;
	
	/** Preview Scene - uses advanced preview settings */
	TSharedPtr<class FAdvancedPreviewScene> AdvancedPreviewScene;

	TSharedPtr<STextBlock> CompileText;
	
	/** Pointer back to the material editor tool that owns us */
	//TWeakPtr<INiagaraSystemEditor> SystemEditorPtr;
	
	class UNiagaraComponent* PreviewComponent;
	
	/** Level viewport client */
	TSharedPtr<class FNiagaraSystemViewportClient> SystemViewportClient;

	uint32 DrawFlags;

	FOnThumbnailCaptured OnThumbnailCaptured;

	/** Used on tick to determine if a view transition was active so we can restore view settings at the end of it */
	bool bIsViewTransitioning = false;

	/** True if orbit mode was active before we started a view transition. Used to restore orbit mode at the the end of the transition */
	bool bShouldActivateOrbitAfterTransitioning = false;
};

#if NIAGARA_PERF_BASELINES

/** Niagara Baseline Display Viewport */
class SNiagaraBaselineViewport : public SEditorViewport, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SNiagaraBaselineViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SNiagaraBaselineViewport();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SNiagaraBaselineViewport");
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void RefreshViewport();

	TSharedRef<class FAdvancedPreviewScene> GetPreviewScene() { return AdvancedPreviewScene.ToSharedRef(); }

	void Init(TSharedPtr<SWindow>& InOwnerWindow);

	bool AddBaseline(UNiagaraEffectType* EffectType);

protected:
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual EVisibility OnGetViewportContentVisibility() const override;
	virtual void BindCommands() override;
	virtual void OnFocusViewportToSelection() override;
	virtual void PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay) override;
	EVisibility OnGetViewportCompileTextVisibility() const;

private:
	bool IsVisible() const override;

	/** Preview Scene - uses advanced preview settings */
	TSharedPtr<class FAdvancedPreviewScene> AdvancedPreviewScene;

	/** Level viewport client */
	TSharedPtr<class FNiagaraBaselineViewportClient> SystemViewportClient;

	TSharedPtr<SWindow> OwnerWindow;
};

#endif