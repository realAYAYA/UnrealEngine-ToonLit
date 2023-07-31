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
#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "HairStrandsInterface.h"


class UGroomComponent;
class SDockTab;

/**
 * Material Editor Preview viewport widget
 */
class SGroomEditorViewport : public SEditorViewport, public FGCObject, public ICommonEditorViewportToolbarInfoProvider
{
public:

	void Construct(const FArguments& InArgs);
	~SGroomEditorViewport();
	
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SGroomEditorViewport");
	}

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
										
	/** Event handlers */
	void TogglePreviewGrid();
	bool IsTogglePreviewGridChecked() const;
	
	// ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	// End of ICommonEditorViewportToolbarInfoProvider interface

	// Set the component to preview
	void SetGroomComponent(UGroomComponent* GroomComponent);

	// set the mesh on which we are grooming
	void SetStaticMeshComponent(UStaticMeshComponent *StaticGroomTarget);

	// set the mesh on which we are grooming
	void SetSkeletalMeshComponent(USkeletalMeshComponent *SkeletalGroomTarget);

	int32 GetLODSelection() const;
	int32 GetLODModelCount() const;
	bool IsLODModelSelected(int32 InLODSelection) const;

	void OnSetLODModel(int32 InLODSelection);

	void OnViewMode(EHairStrandsDebugMode Mode);
	bool CanViewMode(bool bGuideMode) const;

	void OnCardsGuides();
	bool CanCardsGuides() const;

protected:

	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual EVisibility OnGetViewportContentVisibility() const override;
	virtual void BindCommands() override;
	virtual void OnFocusViewportToSelection() override;
	virtual void PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay) override;

private:

	bool IsVisible() const override;

	void RefreshViewport();

private:
	/** The parent tab where this viewport resides */
	TWeakPtr<SDockTab> ParentTab;

	/** Level viewport client */
	TSharedPtr<class FGroomEditorViewportClient> SystemViewportClient;

	/** Preview Scene - uses advanced preview settings */
	TSharedPtr<class FAdvancedPreviewScene> AdvancedPreviewScene;

	class UGroomComponent			*GroomComponent;	
	class UStaticMeshComponent		*StaticGroomTarget; 
	class USkeletalMeshComponent	*SkeletalGroomTarget;

	/** If true, render grid the preview scene. */
	bool bShowGrid;
	
};
