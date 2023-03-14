// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDStageViewModel.h"
#include "UsdWrappers/UsdStage.h"

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class AUsdStageActor;
class FLevelCollectionModel;
class FMenuBuilder;
class ISceneOutliner;
enum class EMapChangeType : uint8;
enum class EUsdInitialLoadSet : uint8;
struct FSlateBrush;

#if USE_USD_SDK

class SUsdStage : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SUsdStage ) {}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	virtual ~SUsdStage();

protected:
	void SetupStageActorDelegates();
	void ClearStageActorDelegates();

	TSharedRef< SWidget > MakeMainMenu();
	TSharedRef< SWidget > MakeActorPickerMenu();
	TSharedRef< SWidget > MakeActorPickerMenuContent();
	TSharedRef< SWidget > MakeIsolateWarningButton();
	void FillFileMenu( FMenuBuilder& MenuBuilder );
	void FillActionsMenu( FMenuBuilder& MenuBuilder );
	void FillOptionsMenu( FMenuBuilder& MenuBuilder );
	void FillExportSubMenu( FMenuBuilder& MenuBuilder );
	void FillPayloadsSubMenu( FMenuBuilder& MenuBuilder );
	void FillPurposesToLoadSubMenu( FMenuBuilder& MenuBuilder );
	void FillRenderContextSubMenu( FMenuBuilder& MenuBuilder );
	void FillMaterialPurposeSubMenu( FMenuBuilder& MenuBuilder );
	void FillRootMotionSubMenu( FMenuBuilder& MenuBuilder );
	void FillCollapsingSubMenu( FMenuBuilder& MenuBuilder );
	void FillInterpolationTypeSubMenu( FMenuBuilder& MenuBuilder );
	void FillSelectionSubMenu( FMenuBuilder& MenuBuilder );
	void FillNaniteThresholdSubMenu( FMenuBuilder& MenuBuilder );

	void OnNew();
	void OnOpen();
	void OnSave();
	void OnExportAll();
	void OnExportFlattened();
	void OnReloadStage();
	void OnResetStage();
	void OnClose();

	void OnLayerIsolated( const UE::FSdfLayer& IsolatedLayer );

	void OnImport();

	void OnPrimSelectionChanged( const TArray<FString>& PrimPath );

	void OpenStage( const TCHAR* FilePath );

	void SetActor( AUsdStageActor* InUsdStageActor );

	void Refresh();

	void OnStageActorLoaded( AUsdStageActor* InUsdStageActor );

	void OnViewportSelectionChanged( UObject* NewSelection );

	int32 GetNaniteTriangleThresholdValue() const;
	void OnNaniteTriangleThresholdValueChanged( int32 InValue );
	void OnNaniteTriangleThresholdValueCommitted( int32 InValue, ETextCommit::Type InCommitType );

	UE::FUsdStageWeak GetCurrentStage() const;

protected:
	TSharedPtr< class SUsdStageTreeView > UsdStageTreeView;
	TSharedPtr< class SUsdPrimInfo > UsdPrimInfoWidget;
	TSharedPtr< class SUsdLayersTreeView > UsdLayersTreeView;

	FDelegateHandle OnActorLoadedHandle;
	FDelegateHandle OnActorDestroyedHandle;
	FDelegateHandle OnStageActorPropertyChangedHandle;
	FDelegateHandle OnStageChangedHandle;
	FDelegateHandle OnStageEditTargetChangedHandle;
	FDelegateHandle OnPrimChangedHandle;
	FDelegateHandle OnLayersChangedHandle;

	FDelegateHandle OnViewportSelectionChangedHandle;

	FString SelectedPrimPath;

	FUsdStageViewModel ViewModel;

	// True while we're in the middle of setting the viewport selection from the prim selection
	bool bUpdatingViewportSelection;

	// True while we're in the middle of updating the prim selection from the viewport selection
	bool bUpdatingPrimSelection;

	// We keep this menu alive instead of recreating it every time because it doesn't expose
	// setting/getting/responding to the picked world, which resets every time it is reconstructed (see UE-127879)
	// By keeping it alive it will keep its own state and we just do a full refresh when needed
	TSharedPtr<ISceneOutliner> ActorPickerMenu;

	int32 CurrentNaniteThreshold = INT32_MAX;

	TArray<TSharedPtr<FString>> MaterialPurposes;
};

#endif // #if USE_USD_SDK
