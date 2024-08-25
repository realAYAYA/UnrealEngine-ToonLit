// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Animation/AnimSequence.h"
#include "Editor.h"
#include "PersonaDelegates.h"
#include "Factories/FbxImportUI.h"
#include "SPersonaToolBox.h"

class FBlueprintEditor;
class IDetailsView;
class IEditableSkeleton;
class IPersonaPreviewScene;
class IPersonaToolkit;
class UAnimBlueprint;
class USkeletalMeshComponent;
class UPhysicsAsset;
class IPinnedCommandList;
class FWorkflowAllowedTabSet;
class IAssetFamily;
class FWorkflowTabFactory;
class UBlendSpace;
class IAnimSequenceCurveEditor;
class IAnimationEditor;
class IDetailLayoutBuilder;
class FPreviewSceneDescriptionCustomization;
struct FAnimAssetFindReplaceConfig;

extern const FName PersonaAppName;

// Editor mode constants
struct PERSONA_API FPersonaEditModes
{
	/** Selection/manipulation of bones & sockets */
	const static FEditorModeID SkeletonSelection;
};

DECLARE_DELEGATE_TwoParams(FIsRecordingActive, USkeletalMeshComponent* /*Component*/, bool& /* bIsRecording */);
DECLARE_DELEGATE_OneParam(FRecord, USkeletalMeshComponent* /*Component*/);
DECLARE_DELEGATE_OneParam(FStopRecording, USkeletalMeshComponent* /*Component*/);
DECLARE_DELEGATE_TwoParams(FGetCurrentRecording, USkeletalMeshComponent* /*Component*/, class UAnimSequence*& /* OutRecording */);
DECLARE_DELEGATE_TwoParams(FGetCurrentRecordingTime, USkeletalMeshComponent* /*Component*/, float& /* OutTime */);
DECLARE_DELEGATE_TwoParams(FTickRecording, USkeletalMeshComponent* /*Component*/, float /* DeltaSeconds */);

/** Called back when a viewport is created */
DECLARE_DELEGATE_OneParam(FOnViewportCreated, const TSharedRef<class IPersonaViewport>&);

/** Called back when a details panel is created */
DECLARE_DELEGATE_OneParam(FOnDetailsCreated, const TSharedRef<IDetailsView>&);

/** Called back when an anim sequence browser is created */
DECLARE_DELEGATE_OneParam(FOnAnimationSequenceBrowserCreated, const TSharedRef<IAnimationSequenceBrowser>&);

/** Called back when a Persona preview scene is created */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreviewSceneCreated, const TSharedRef<IPersonaPreviewScene>&);

/** Called back when a Persona preview scene settings are customized */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreviewSceneSettingsCustomized, IDetailLayoutBuilder& DetailBuilder);

/** Called back to register tabs */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRegisterTabs, FWorkflowAllowedTabSet&, TSharedPtr<FAssetEditorToolkit>);

/** Called back to register common layout extensions */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRegisterLayoutExtensions, FLayoutExtender&);

/** Initialization parameters for persona toolkits */
struct FPersonaToolkitArgs
{
	/** 
	 * Delegate called when the preview scene is created, used to setup the scene 
	 * If this is not set, then a default scene will be set up.
	 */
	FOnPreviewSceneCreated::FDelegate OnPreviewSceneCreated;

	/** Whether to create a preview scene */
	bool bCreatePreviewScene = true;

	/** 
	 * Delegate called when the preview scene settings are being customized, supplies the IDetailLayoutBuilder
	 * for the user to customize the layout however they wish. */
	FOnPreviewSceneSettingsCustomized::FDelegate OnPreviewSceneSettingsCustomized;

	/**
	 * Set to true if the preview mesh can be associated with a skeleton different from the one being inspected
	 * by Persona. Used for editors that are mostly skeleton agnostic.
	 */
	bool bPreviewMeshCanUseDifferentSkeleton = false;

	FPersonaToolkitArgs() = default;
};

struct FAnimDocumentArgs
{
	FAnimDocumentArgs(const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, const TSharedRef<class IPersonaToolkit>& InPersonaToolkit, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnSectionsChanged)
		: PreviewScene(InPreviewScene)
		, PersonaToolkit(InPersonaToolkit)
		, EditableSkeleton(InEditableSkeleton)
		, OnSectionsChanged(InOnSectionsChanged)
	{}

	/** Required args */
	TWeakPtr<class IPersonaPreviewScene> PreviewScene;
	TWeakPtr<class IPersonaToolkit> PersonaToolkit;
	TWeakPtr<class IEditableSkeleton> EditableSkeleton;
	FSimpleMulticastDelegate& OnSectionsChanged;

	/** Optional args */
	FOnObjectsSelected OnDespatchObjectsSelected;
	FOnInvokeTab OnDespatchInvokeTab;
	FSimpleDelegate OnDespatchSectionsChanged;
};

struct FBlendSpaceEditorArgs
{
	// Called when a blendspace sample point is removed
	FOnBlendSpaceSampleRemoved OnBlendSpaceSampleRemoved;

	// Called when a blendspace sample point is added
	FOnBlendSpaceSampleAdded OnBlendSpaceSampleAdded;
	
	// Called when a blendspace sample point is replaced
	FOnBlendSpaceSampleReplaced OnBlendSpaceSampleReplaced;

	// Called when the blendspace canvas is double clicked
	FOnBlendSpaceNavigateUp OnBlendSpaceNavigateUp;

	// Called when the blendspace canvas is double clicked
	FOnBlendSpaceNavigateDown OnBlendSpaceNavigateDown;

	// Called when the blendspace canvas is double clicked
	FOnBlendSpaceCanvasDoubleClicked OnBlendSpaceCanvasDoubleClicked;

	// Called when a blendspace sample point is double clicked
	FOnBlendSpaceSampleDoubleClicked OnBlendSpaceSampleDoubleClicked;

	// Called to get the overridden name of a blend sample
	FOnGetBlendSpaceSampleName OnGetBlendSpaceSampleName;

	// Allows the target preview position to be programmatically driven
	TAttribute<FVector> PreviewPosition;

	// Allows the current position to be programmatically driven
	TAttribute<FVector> PreviewFilteredPosition;

	// Allows an external widget to be inserted into a sample's tooltip
	FOnExtendBlendSpaceSampleTooltip OnExtendSampleTooltip;

	// Allows preview position to drive external node
	FOnSetBlendSpacePreviewPosition OnSetPreviewPosition;

	// Status bar to display hint messages in
	FName StatusBarName = TEXT("AssetEditor.AnimationEditor.MainMenu");
};

struct FBlendSpacePreviewArgs
{
	TAttribute<const UBlendSpace*> PreviewBlendSpace;

	// Allows the target preview position to be programatically driven
	TAttribute<FVector> PreviewPosition;

	// Allows the current preview position to be programatically driven
	TAttribute<FVector> PreviewFilteredPosition;

	// Called to get the overridden name of a blend sample
	FOnGetBlendSpaceSampleName OnGetBlendSpaceSampleName;
};

/** Places that viewport text can be placed */
enum class EViewportCorner : uint8
{
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
};

/** Delegate used to provide custom text for the viewport corners */
DECLARE_DELEGATE_RetVal_OneParam(FText, FOnGetViewportText, EViewportCorner /*InViewportCorner*/);

/** Arguments used to create a persona viewport tab */
struct FPersonaViewportArgs
{
	FPersonaViewportArgs(const TSharedRef<class IPersonaPreviewScene>& InPreviewScene)
		: PreviewScene(InPreviewScene)
		, ContextName(NAME_None)
		, bShowShowMenu(true)
		, bShowLODMenu(true)
		, bShowPlaySpeedMenu(true)
		, bShowTimeline(true)
		, bShowStats(true)
		, bAlwaysShowTransformToolbar(false)
		, bShowFloorOptions(true)
		, bShowTurnTable(true)
		, bShowPhysicsMenu(false)
	{}

	/** Required args */
	TSharedRef<class IPersonaPreviewScene> PreviewScene;

	/** Optional blueprint editor that we can be embedded in */
	TSharedPtr<class FBlueprintEditor> BlueprintEditor;

	/** Delegate fired when the viewport is created */
	FOnViewportCreated OnViewportCreated;
	
	/** Menu extenders */
	TArray<TSharedPtr<FExtender>> Extenders;

	/** Delegate used to customize viewport corner text */
	FOnGetViewportText OnGetViewportText;

	/** The context in which we are constructed. Used to persist various settings. */
	FName ContextName;

	/** Whether to show the 'Show' menu */
	bool bShowShowMenu;

	/** Whether to show the 'LOD' menu */
	bool bShowLODMenu;

	/** Whether to show the 'Play Speed' menu */
	bool bShowPlaySpeedMenu;

	/** Whether to show the animation timeline */
	bool bShowTimeline;

	/** Whether to show in-viewport stats */
	bool bShowStats;

	/** Whether we should always show the transform toolbar for this viewport */
	bool bAlwaysShowTransformToolbar;

	/** Whether to show options relating to floor height */
	bool bShowFloorOptions;

	/** Whether to show options relating to turntable */
	bool bShowTurnTable;

	/** Whether to show options relating to physics */
	bool bShowPhysicsMenu;
};

/**
 * Persona module manages the lifetime of all instances of Persona editors.
 */
class FPersonaModule : public IModuleInterface,
	public IHasMenuExtensibility
{
public:
	/**
	 * Called right after the module's DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule();

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule();

	/** Create a re-usable toolkit that multiple asset editors that are concerned with USkeleton-related data can use */
	virtual TSharedRef<IPersonaToolkit> CreatePersonaToolkit(UObject* InAsset, const FPersonaToolkitArgs& PersonaToolkitArgs, USkeleton* InSkeleton = nullptr) const;
	virtual TSharedRef<IPersonaToolkit> CreatePersonaToolkit(USkeleton* InSkeleton, const FPersonaToolkitArgs& PersonaToolkitArgs) const;
	virtual TSharedRef<IPersonaToolkit> CreatePersonaToolkit(UAnimationAsset* InAnimationAsset, const FPersonaToolkitArgs& PersonaToolkitArgs) const;
	virtual TSharedRef<IPersonaToolkit> CreatePersonaToolkit(USkeletalMesh* InSkeletalMesh, const FPersonaToolkitArgs& PersonaToolkitArgs) const;
	virtual TSharedRef<IPersonaToolkit> CreatePersonaToolkit(UAnimBlueprint* InAnimBlueprint, const FPersonaToolkitArgs& PersonaToolkitArgs) const;
	virtual TSharedRef<IPersonaToolkit> CreatePersonaToolkit(UPhysicsAsset* InPhysicsAsset, const FPersonaToolkitArgs& PersonaToolkitArgs) const;

	/** Create an asset family for the supplied persona asset */
	virtual TSharedRef<IAssetFamily> CreatePersonaAssetFamily(const UObject* InAsset) const;

	/** Broadcast event that all asset families need to change */
	virtual void BroadcastAssetFamilyChange() const;

	/** Record that an asset was opened (forward to relevant asset families) */
	virtual void RecordAssetOpened(const FAssetData& InAssetData) const;
	
	/** Create a shortcut widget for an asset family */
	virtual TSharedRef<SWidget> CreateAssetFamilyShortcutWidget(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IAssetFamily>& InAssetFamily) const;

	/** Create a details panel tab factory */
	virtual TSharedRef<FWorkflowTabFactory> CreateDetailsTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, FOnDetailsCreated InOnDetailsCreated) const;

	/** Create a persona viewport tab factory */
	virtual TSharedRef<FWorkflowTabFactory> CreatePersonaViewportTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const FPersonaViewportArgs& InArgs) const;

	/** Register 4 Persona viewport tab factories */
	virtual void RegisterPersonaViewportTabFactories(FWorkflowAllowedTabSet& TabSet, const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const FPersonaViewportArgs& InArgs) const;

	/** Create an anim notifies tab factory */
	virtual TSharedRef<FWorkflowTabFactory> CreateAnimNotifiesTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FOnObjectsSelected InOnObjectsSelected) const;

	UE_DEPRECATED(5.0, "Please use the overload that does not take a post-undo delegate")
	virtual TSharedRef<FWorkflowTabFactory> CreateCurveViewerTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedPtr<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectsSelected InOnObjectsSelected) const;

	/** Create a skeleton curve viewer tab factory */
	virtual TSharedRef<FWorkflowTabFactory> CreateCurveViewerTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedPtr<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FOnObjectsSelected InOnObjectsSelected) const;

	/** Create a skeleton curve metadata editor tab factory */
	virtual TSharedRef<FWorkflowTabFactory> CreateCurveMetadataEditorTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, UObject* InMetadataHost, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FOnObjectsSelected InOnObjectsSelected) const;

	/** Create a retarget sources tab factory */
	virtual TSharedRef<FWorkflowTabFactory> CreateRetargetSourcesTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo) const;

	/** Create a tab factory used to configure preview scene settings */
	virtual TSharedRef<FWorkflowTabFactory> CreateAdvancedPreviewSceneTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene) const;

	/** Create a tab factory for the animation asset browser */
	virtual TSharedRef<FWorkflowTabFactory> CreateAnimationAssetBrowserTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaToolkit>& InPersonaToolkit, FOnOpenNewAsset InOnOpenNewAsset, FOnAnimationSequenceBrowserCreated InOnAnimationSequenceBrowserCreated, bool bInShowHistory) const;

	/** Create a tab factory for editing a single object (like an animation asset) */
	virtual TSharedRef<FWorkflowTabFactory> CreateAssetDetailsTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, FOnGetAsset InOnGetAsset, FOnDetailsCreated InOnDetailsCreated) const;

	/** Create a tab factory for for previewing morph targets */
	virtual TSharedRef<FWorkflowTabFactory> CreateMorphTargetTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& OnPostUndo) const;

	/** Create a tab factory for editing anim blueprint preview & defaults */
	virtual TSharedRef<FWorkflowTabFactory> CreateAnimBlueprintPreviewTabFactory(const TSharedRef<class FBlueprintEditor>& InBlueprintEditor, const TSharedRef<IPersonaPreviewScene>& InPreviewScene) const;

	/** Create a tab factory for the pose watch manager */
	virtual TSharedRef<FWorkflowTabFactory> CreatePoseWatchTabFactory(const TSharedRef<class FBlueprintEditor>& InBlueprintEditor) const;

	/** Create a tab factory for editing anim blueprint parent overrides */
	virtual TSharedRef<FWorkflowTabFactory> CreateAnimBlueprintAssetOverridesTabFactory(const TSharedRef<class FBlueprintEditor>& InBlueprintEditor, UAnimBlueprint* InAnimBlueprint, FSimpleMulticastDelegate& InOnPostUndo) const;

	UE_DEPRECATED(5.0, "Please use the overload that does not take a post-undo delegate")
	virtual TSharedRef<FWorkflowTabFactory> CreateSkeletonSlotNamesTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectSelected InOnObjectSelected) const;

	/** Create a tab factory for editing slot names and groups */
	virtual TSharedRef<FWorkflowTabFactory> CreateSkeletonSlotNamesTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FOnObjectSelected InOnObjectSelected) const;

	/** Create a toolbox tab factory */
	virtual TSharedRef<FWorkflowTabFactory> CreatePersonaToolboxTabFactory(const TSharedRef<class FPersonaAssetEditorToolkit>& InHostingApp) const;

	/** Deprecated */
	UE_DEPRECATED(5.0, "Please use the overload that takes a FBlendSpacePreviewArgs struct")
	virtual TSharedRef<SWidget> CreateBlendSpacePreviewWidget(TAttribute<const UBlendSpace*> InBlendSpace, TAttribute<FVector> InBlendPosition, TAttribute<FVector> InFilteredBlendPosition) const;

	/** Create a widget to preview a blendspace */
	virtual TSharedRef<SWidget> CreateBlendSpacePreviewWidget(const FBlendSpacePreviewArgs& InArgs) const;

	/** Create a widget to edit a blendspace */
	virtual TSharedRef<SWidget> CreateBlendSpaceEditWidget(UBlendSpace* InBlendSpace, const FBlendSpaceEditorArgs& InArgs) const;

	/** Create a tab factory for editing montage sections */
	virtual TSharedRef<FWorkflowTabFactory> CreateAnimMontageSectionsTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaToolkit>& InPersonaToolkit, FSimpleMulticastDelegate& InOnSectionsChanged) const;

	/** Create a tab factory for finding and replacing in anim data */
	virtual TSharedRef<FWorkflowTabFactory> CreateAnimAssetFindReplaceTabFactory(const TSharedRef<FWorkflowCentricApplication>& InHostingApp, const FAnimAssetFindReplaceConfig& InConfig) const;
	
	/** Create a widget for finding and replacing in anim data */
	virtual TSharedRef<SWidget> CreateFindReplaceWidget(const FAnimAssetFindReplaceConfig& InConfig) const;
	
	/** Create a widget that acts as a document for an animation asset */
	virtual TSharedRef<SWidget> CreateEditorWidgetForAnimDocument(const TSharedRef<IAnimationEditor>& InHostingApp, UObject* InAnimAsset, const FAnimDocumentArgs& InArgs, FString& OutDocumentLink);

	/** Create a widget that acts as a curve document for an animation asset */
	virtual TSharedRef<IAnimSequenceCurveEditor> CreateCurveWidgetForAnimDocument(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, UAnimSequenceBase* InAnimSequence, const TSharedPtr<ITimeSliderController>& InExternalTimeSliderController, const TSharedPtr<FTabManager>& InTabManager);

	/** Customize a skeletal mesh details panel */
	virtual void CustomizeMeshDetails(const TSharedRef<IDetailsView>& InDetailsView, const TSharedRef<IPersonaToolkit>& InPersonaToolkit);

	/** Gets the extensibility managers for outside entities to extend persona editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() {return ToolBarExtensibilityManager;}

	/** Import a new asset using the supplied skeleton */
	virtual void ImportNewAsset(USkeleton* InSkeleton, EFBXImportType DefaultImportType);

	UE_DEPRECATED(5.3, "Please use TestSkeletonCurveMetaDataForUse")
	virtual void TestSkeletonCurveNamesForUse(const TSharedRef<IEditableSkeleton>& InEditableSkeleton) const { TestSkeletonCurveMetaDataForUse(InEditableSkeleton); }

	/** Check all animations & skeletal meshes for curve metadata usage */
	virtual void TestSkeletonCurveMetaDataForUse(const TSharedRef<IEditableSkeleton>& InEditableSkeleton) const;

	/** Apply Compression to list of animations and optionally asks to pick an overrides to the bone compression settings */
	virtual void ApplyCompression(TArray<TWeakObjectPtr<class UAnimSequence>>& AnimSequences, bool bPickBoneSettingsOverride);

	/** Export to FBX files of the list of animations */
	virtual bool ExportToFBX(TArray<TWeakObjectPtr<class UAnimSequence>>& AnimSequences, USkeletalMesh* SkeletalMesh);

	/** Add looping interpolation to the list of animations */
	virtual void AddLoopingInterpolation(TArray<TWeakObjectPtr<class UAnimSequence>>& AnimSequences);

	UE_DEPRECATED(4.24, "Function renamed, please use CustomizeBlueprintEditorDetails")
	virtual void CustomizeSlotNodeDetails(const TSharedRef<class IDetailsView>& InDetailsView, FOnInvokeTab InOnInvokeTab) { CustomizeBlueprintEditorDetails(InDetailsView, InOnInvokeTab); }

	/** Customize the details of a slot node for the specified details view */
	virtual void CustomizeBlueprintEditorDetails(const TSharedRef<class IDetailsView>& InDetailsView, FOnInvokeTab InOnInvokeTab);

	/** Create a Persona editor mode manager. Should be destroyed using plain ol' delete. Note: Only FPersonaEditMode-derived modes should be used with this manager! */
	virtual class IPersonaEditorModeManager* CreatePersonaEditorModeManager();

	/** Delegate used to query whether recording is active */
	virtual FIsRecordingActive& OnIsRecordingActive() { return IsRecordingActiveDelegate; }

	/** Delegate used to start recording animation */
	virtual FRecord& OnRecord() { return RecordDelegate; }

	/** Delegate used to stop recording animation */
	virtual FStopRecording& OnStopRecording() { return StopRecordingDelegate; }

	/** Delegate used to get the currently recording animation */
	virtual FGetCurrentRecording& OnGetCurrentRecording() { return GetCurrentRecordingDelegate; }

	/** Delegate used to get the currently recording animation time */
	virtual FGetCurrentRecordingTime& OnGetCurrentRecordingTime() { return GetCurrentRecordingTimeDelegate; }

	/** Delegate broadcast when a preview scene is created */
	virtual FOnPreviewSceneCreated& OnPreviewSceneCreated() { return OnPreviewSceneCreatedDelegate; }

	/** Settings for AddCommonToolbarExtensions */
	struct FCommonToolMenuExtensionArgs
	{
		FCommonToolMenuExtensionArgs()
			: bPreviewMesh(true)
			, bPreviewAnimation(true)
			, bReferencePose(false)
			, bCreateAsset(true)
		{}

		/** Adds a shortcut to setup a preview mesh to override the current display */
		bool bPreviewMesh;

		/** Adds a shortcut to setup a preview animation to override the current display */
		bool bPreviewAnimation;

		/** Adds a shortcut to set the character back to reference pose (also clears all bone modifications) */
		bool bReferencePose;

		/** Adds a combo menu to allow other anim assets to be created */
		bool bCreateAsset;
	};

	typedef FCommonToolMenuExtensionArgs FCommonToolbarExtensionArgs;

	/** Add common menu extensions */
	virtual void AddCommonMenuExtensions(UToolMenu* InToolMenu, const FCommonToolMenuExtensionArgs& InArgs = FCommonToolMenuExtensionArgs());

	/** Add common toolbar extensions */
	virtual void AddCommonToolbarExtensions(UToolMenu* InToolMenu, const FCommonToolMenuExtensionArgs& InArgs = FCommonToolMenuExtensionArgs());

	/** Add common toobar extensions (legacy support) - DEPRECATED */
	virtual void AddCommonToolbarExtensions(FToolBarBuilder& InToolbarBuilder, TSharedRef<IPersonaToolkit> PersonaToolkit, const FCommonToolbarExtensionArgs& InArgs = FCommonToolbarExtensionArgs());

	/** Register common layout extensions */
	virtual FOnRegisterLayoutExtensions& OnRegisterLayoutExtensions() { return OnRegisterLayoutExtensionsDelegate; }

	/** Register common tabs */
	virtual FOnRegisterTabs& OnRegisterTabs() { return OnRegisterTabsDelegate; }

	/** Create a widget that can choose a curve name. Derives available names from the asset registry list of assets that use the specified skeleton. */
	virtual TSharedRef<SWidget> CreateCurvePicker(const USkeleton* InSkeleton, FOnCurvePicked InOnCurvePicked, FIsCurveNameMarkedForExclusion InIsCurveNameMarkedForExclusion = FIsCurveNameMarkedForExclusion());

	UE_DEPRECATED(5.3, "Please use CreateCurvePicker that takes a const USkeleton*")
	virtual TSharedRef<SWidget> CreateCurvePicker(TSharedRef<IEditableSkeleton> InEditableSkeleton, FOnCurvePicked InOnCurvePicked, FIsCurveNameMarkedForExclusion InIsCurveNameMarkedForExclusion = FIsCurveNameMarkedForExclusion());
	
private:
	/** When a new anim notify blueprint is created, this will handle post creation work such as adding non-event default nodes */
	void HandleNewAnimNotifyBlueprintCreated(UBlueprint* InBlueprint);

	/** When a new anim notify state blueprint is created, this will handle post creation work such as adding non-event default nodes */
	void HandleNewAnimNotifyStateBlueprintCreated(UBlueprint* InBlueprint);

	/** Options for asset creation */
	enum class EPoseSourceOption : uint8
	{
		ReferencePose,
		CurrentPose,
		CurrentAnimation_AnimData,
		CurrentAnimation_PreviewMesh,
		Max
	};

	static TSharedRef< SWidget > GenerateCreateAssetMenu(TWeakPtr<IPersonaToolkit> InWeakPersonaToolkit);

	static void FillCreateAnimationMenu(FMenuBuilder& MenuBuilder, TWeakPtr<IPersonaToolkit> InWeakPersonaToolkit);

	static void FillCreateAnimationFromCurrentAnimationMenu(FMenuBuilder& MenuBuilder, TWeakPtr<IPersonaToolkit> InWeakPersonaToolkit);

	static void FillCreatePoseAssetMenu(FMenuBuilder& MenuBuilder, TWeakPtr<IPersonaToolkit> InWeakPersonaToolkit);

	static void FillInsertPoseMenu(FMenuBuilder& MenuBuilder, TWeakPtr<IPersonaToolkit> InWeakPersonaToolkit);

	static void InsertCurrentPoseToAsset(const FAssetData& NewPoseAssetData, TWeakPtr<IPersonaToolkit> InWeakPersonaToolkit);

	static bool CreateAnimation(const TArray<UObject*> NewAssets, const EPoseSourceOption Option, TWeakPtr<IPersonaToolkit> InWeakPersonaToolkit);

	static bool CreatePoseAsset(const TArray<UObject*> NewAssets, const EPoseSourceOption Option, TWeakPtr<IPersonaToolkit> InWeakPersonaToolkit);
	
	static bool HandleAssetCreated(const TArray<UObject*> NewAssets);

	void RegisterToolMenuExtensions();
private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	/** Delegate used to query whether recording is active */
	FIsRecordingActive IsRecordingActiveDelegate;

	/** Delegate used to start recording animation */
	FRecord RecordDelegate;

	/** Delegate used to stop recording animation */
	FStopRecording StopRecordingDelegate;

	/** Delegate used to get the currently recording animation */
	FGetCurrentRecording GetCurrentRecordingDelegate;

	/** Delegate used to get the currently recording animation time */
	FGetCurrentRecordingTime GetCurrentRecordingTimeDelegate;

	/** Delegate used to tick the skelmesh component recording */
	FTickRecording TickRecordingDelegate;

	/** Delegate broadcast when a preview scene is created */
	FOnPreviewSceneCreated OnPreviewSceneCreatedDelegate;

	/** Delegate broadcast to register common layout extensions */
	FOnRegisterLayoutExtensions OnRegisterLayoutExtensionsDelegate;

	/** Delegate broadcast to register common tabs */
	FOnRegisterTabs OnRegisterTabsDelegate;
};

