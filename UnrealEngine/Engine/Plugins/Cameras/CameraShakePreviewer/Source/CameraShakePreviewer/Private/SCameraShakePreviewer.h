// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "Containers/Array.h"
#include "EditorUndoClient.h"
#include "Engine/Scene.h"
#include "Templates/SharedPointer.h"
#include "TickableEditorObject.h"
#include "UObject/GCObject.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "SCameraShakePreviewer.generated.h"

class AActor;
class ACameraActor;
class FCameraShakePreviewUpdater;
class FCameraShakePreviewerLevelEditorViewportClient;
class FCameraShakePreviewerModule;
class FLevelEditorViewportClient;
class FSceneView;
class ITableRow;
class STableViewBase;
class UCameraAnimInst;
class UCameraShakeSourceComponent;
class ULevel;
class UWorld;
struct FCameraShakeData;
struct FEditorViewportViewModifierParams;
struct FTogglePreviewCameraShakesParams;

UCLASS()
class APreviewPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	void ResetPostProcessSettings()
	{
		ClearCachedPPBlends();
	}

	void MergePostProcessSettings(TArray<FPostProcessSettings>& InSettings, TArray<float>& InBlendWeights)
	{
		InSettings.Append(PostProcessBlendCache);
		InBlendWeights.Append(PostProcessBlendCacheWeights);
	}
};

class FCameraShakePreviewUpdater : public FTickableEditorObject, public FGCObject
{
public:
	FCameraShakePreviewUpdater();
	virtual ~FCameraShakePreviewUpdater();

	// FTickableObject Interface
	virtual ETickableTickType GetTickableTickType() const { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FCameraShakePreviewUpdater, STATGROUP_Tickables); }
	virtual void Tick(float DeltaTime) override;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SCameraShakePreviewer"); }

	UCameraShakeBase* AddCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, const FAddCameraShakeParams& Params);
	void RemoveAllCameraShakesFromSource(const UCameraShakeSourceComponent* SourceComponent);
	void GetActiveCameraShakes(TArray<FActiveCameraShakeInfo>& ActiveCameraShakes) const;
	void RemoveCameraShake(UCameraShakeBase* ShakeInstance);
	void RemoveAllCameraShakes();

	void ModifyCamera(FEditorViewportViewModifierParams& Params);

private:
	void AddPostProcessBlend(const FPostProcessSettings& Settings, float Weight);

	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);

private:
	APreviewPlayerCameraManager* PreviewCamera;
	UCameraModifier_CameraShake* PreviewCameraShake;

	TOptional<float> LastDeltaTime;

	FVector LastLocationModifier;
	FRotator LastRotationModifier;
	float LastFOVModifier;

	TArray<FPostProcessSettings> LastPostProcessSettings;
	TArray<float> LastPostProcessBlendWeights;
};

/**
 * Camera shake preview panel.
 */
class SCameraShakePreviewer : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SCameraShakePreviewer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SCameraShakePreviewer();

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

private:
	void Populate();
	bool FindCurrentWorld();
	void Refresh();

	void OnTogglePreviewCameraShakes(const FTogglePreviewCameraShakesParams& Params);

	TSharedRef<ITableRow> OnCameraShakesListGenerateRowWidget(TSharedPtr<FCameraShakeData> Entry, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnCameraShakesListSectionChanged(TSharedPtr<FCameraShakeData> Entry, ESelectInfo::Type SelectInfo) const;
	FText GetActiveViewportName() const;
	FText GetActiveViewportWarnings() const;

	FReply OnPlayStopAllShakes();
	FReply OnPlayStopSelectedShake();
	void PlayCameraShake(TSharedPtr<FCameraShakeData> CameraShake);

	void OnLevelViewportClientListChanged();
	void OnLevelAdded(ULevel* InLevel, UWorld* InWorld);
	void OnLevelRemoved(ULevel* InLevel, UWorld* InWorld);
	void OnLevelActorsAdded(AActor* InActor);
	void OnLevelActorsRemoved(AActor* InActor);
	void OnLevelActorListChanged();
	void OnMapChange(uint32 MapFlags);
	void OnNewCurrentLevel();
	void OnMapLoaded(const FString&  Filename, bool bAsTemplate);

	void OnModifyView(FEditorViewportViewModifierParams& Params);

private:
	TArray<TSharedPtr<FCameraShakeData>> CameraShakes;
	TUniquePtr<FCameraShakePreviewUpdater> CameraShakePreviewUpdater;

	TSharedPtr<SListView<TSharedPtr<FCameraShakeData>>> CameraShakesListView;
	TSharedPtr<SButton> PlayStopSelectedButton;

	FCameraShakePreviewerModule* CameraShakePreviewerModule;
	FLevelEditorViewportClient* ActiveViewportClient;
	int ActiveViewportIndex;

	TWeakObjectPtr<UWorld> CurrentWorld;
	bool bNeedsRefresh;
};
