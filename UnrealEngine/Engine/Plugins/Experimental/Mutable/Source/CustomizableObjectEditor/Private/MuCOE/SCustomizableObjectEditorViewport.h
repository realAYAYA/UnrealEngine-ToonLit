// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Engine/EngineBaseTypes.h"
#include "Input/Reply.h"
#include "Math/BoxSphereBounds.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "SEditorViewport.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "UObject/GCObject.h"
#include "UnrealWidgetFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FDragDropEvent;
class FEditorViewportClient;
class FReferenceCollector;
class FSceneViewport;
class FUICommandList;
class STextComboBox;
class SWidget;
class UCustomizableObject;
class UCustomizableObjectNodeProjectorConstant;
class UCustomizableObjectNodeProjectorParameter;
class UDebugSkelMeshComponent;
class UStaticMesh;
struct FCustomizableObjectProjector;
struct FGeometry;


struct FCustomizableObjectEditorViewportRequiredArgs
{
	FCustomizableObjectEditorViewportRequiredArgs(
		//const TSharedRef<class ISkeletonTree>& InSkeletonTree, 
		const TSharedRef<class FCustomizableObjectPreviewScene>& InPreviewScene,
		TSharedRef<class SCustomizableObjectEditorViewportTabBody> InTabBody 
		//TSharedRef<class FAssetEditorToolkit> InAssetEditorToolkit, 
		//FSimpleMulticastDelegate& InOnPostUndo
	)
//		: SkeletonTree(InSkeletonTree)
		: PreviewScene(InPreviewScene)
		, TabBody(InTabBody)
//		, AssetEditorToolkit(InAssetEditorToolkit)
//		, OnPostUndo(InOnPostUndo)
	{}

	//TSharedRef<class ISkeletonTree> SkeletonTree;

	TSharedRef<class FCustomizableObjectPreviewScene> PreviewScene;

	TSharedRef<class SCustomizableObjectEditorViewportTabBody> TabBody;

	//TSharedRef<class FAssetEditorToolkit> AssetEditorToolkit;

	//FSimpleMulticastDelegate& OnPostUndo;
};


//////////////////////////////////////////////////////////////////////////
// SCustomizableObjectEditorViewport: the pure viewport widget

class SCustomizableObjectEditorViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SCustomizableObjectEditorViewport)
		: _ShowStats(true)
	{}

		SLATE_ARGUMENT(bool, ShowStats)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FCustomizableObjectEditorViewportRequiredArgs& InRequiredArgs);

	TSharedPtr<FSceneViewport>& GetSceneViewport();

protected:
	// SEditorViewport interface
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	// End of SEditorViewport interface

	/**  Handle undo/redo by refreshing the viewport */
	void OnUndoRedo();

protected:
	// Viewport client
	TSharedPtr<class FCustomizableObjectEditorViewportClient> LevelViewportClient;

	// Pointer to the compound widget that owns this viewport widget
	TWeakPtr<class SCustomizableObjectEditorViewportTabBody> TabBodyPtr;

	// The preview scene that we are viewing
	TWeakPtr<class FCustomizableObjectPreviewScene> PreviewScenePtr;

	TWeakPtr<class SCustomizableObjectViewportToolBar> ToolbarPtr;

	// The skeleton tree we are editing
	//TWeakPtr<class ISkeletonTree> SkeletonTreePtr;

	// The asset editor we are embedded in
	//TWeakPtr<class FAssetEditorToolkit> AssetEditorToolkitPtr;

	/** Whether we should show stats for this viewport */
	bool bShowStats;
};


/**
 * CustomizableObject Editor Preview viewport widget wwith toolbars, etc.
 */
class SCustomizableObjectEditorViewportTabBody : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SCustomizableObjectEditorViewportTabBody){}
		SLATE_ARGUMENT(TWeakPtr<class ICustomizableObjectInstanceEditor>, CustomizableObjectEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCustomizableObjectEditorViewportTabBody();

	void SetAnimation(class UAnimationAsset* Animation, EAnimationMode::Type AnimationType);

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	
	// FSerializableObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SCustomizableObjectEditorViewportTabBody");
	}
	// End of FSerializableObject interface

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	// End of SWidget interface

	/**
	* @return The list of commands on the viewport that are bound to delegates
	*/
	const TSharedPtr<FUICommandList>& GetCommandList() const { return UICommandList; }


	void SetClipMorphPlaneVisibility(bool bVisible, const FVector& Origin = FVector::ZeroVector, const FVector& Normal = FVector::ZeroVector, float MorphLength = 0.f, const FBoxSphereBounds& Bounds = FBoxSphereBounds(), float Radius1 = 3.f, float Radius2 = 2.f, float RotationAngle = 0.f);
	void SetClipMorphPlaneVisibility(bool bVisible, class UCustomizableObjectNodeMeshClipMorph* NodeMorphClipMesh);
	void SetClipMeshVisibility(bool bVisible, UStaticMesh* ClipMesh, class UCustomizableObjectNodeMeshClipWithMesh* ClipMeshNode);
	void SetProjectorVisibility(bool bVisible, FString ProjectorParameterName, FString ProjectorParameterNameWithIndex, int32 RangeIndex, const FCustomizableObjectProjector& Data, int32 ProjectorParameterIndex);
	void SetProjectorVisibility(bool bVisible, class UCustomizableObjectNodeProjectorConstant* Projector = nullptr);
	void SetProjectorParameterVisibility(bool bVisible, class UCustomizableObjectNodeProjectorParameter* ProjectorParameter = nullptr);
	void ResetProjectorVisibility(bool OnlyNonNodeProjector);
	void SetProjectorType(bool bVisible, FString ProjectorParameterName, FString ProjectorParameterNameWithIndex, int32 RangeIndex, const FCustomizableObjectProjector& Data, int32 ProjectorParameterIndex);

	/** Binds commands associated with the viewport client. */
	void BindCommands();
	TSharedRef<SWidget> BuildToolBar();

	TSharedRef<SWidget> GenerateUVMaterialOptionsMenuContent();

	/** Callback to show / hide the state and rutime parameter test in the viewport */
	TSharedRef<SWidget> ShowStateTestData();

	/** Retrieves the skeletal mesh component. */
	const TArray<UDebugSkelMeshComponent*>& GetSkeletalMeshComponents() const;
	
	void SetPreviewComponents(TArray<UDebugSkelMeshComponent*>& InSkeletalMeshComponents);
	
	/** Function to get viewport's current background color */
	FLinearColor GetViewportBackgroundColor() const;

	/** Function to get the number of LOD models associated with the preview skeletal mesh*/
	int32 GetLODModelCount() const;

	/** LOD model selection checking function*/
	bool IsLODModelSelected(int32 LODSelectionType) const;
	int32 GetLODSelection() const { return LODSelection; };

	/** Callback for when the projector checkbox changes */
	void ProjectorCheckboxStateChanged(UE::Widget::EWidgetMode InNewMode);

	/** Called when the rotation grid snap is toggled off and on */
	void RotationGridSnapClicked();

	/** Returns whether or not rotation grid snap is enabled */
	bool RotationGridSnapIsChecked() const;

	/** Sets the rotation grid size */
	static void SetRotationGridSize(int32 InIndex, ERotationGridMode InGridMode);

	/** Checks to see if the specified rotation grid angle is the current rotation grid angle */
	static bool IsRotationGridSizeChecked(int32 GridSizeIndex, ERotationGridMode GridMode);

	/** Function to set LOD model selection*/
	void OnSetLODModel(int32 LODSelectionType);

	// Getter of PreviewScenePtr
	TSharedPtr<class FCustomizableObjectPreviewScene> GetPreviewScene() { return PreviewScenePtr; }

	//
	TSharedPtr<class FCustomizableObjectEditorViewportClient> GetViewportClient() { return LevelViewportClient; }

	// Pointer back to the editor tool that owns us.
	TWeakPtr<class ICustomizableObjectInstanceEditor> CustomizableObjectEditorPtr;

	//void SetEditorTransformViewportToolbar(TWeakPtr<class SCustomizableObjectEditorTransformViewportToolbar> EditorTransformViewportToolbarParam);
	void SetViewportToolbarTransformWidget(TWeakPtr<class SWidget> TransformWidget);

	/** Update the current gizmo data to its corresponding data origin */
	void UpdateGizmoDataToOrigin();

	/** Setter for LevelViewportClient::GizmoProxy::CallUpdateSkeletalMesh */
	void SetGizmoCallUpdateSkeletalMesh(bool Value);

	/** Getter for LevelViewportClient::GizmoProxy::ProjectorParameterName */
	FString GetGizmoProjectorParameterName();

	/** Getter for LevelViewportClient::GizmoProxy::ProjectorParameterNameWithIndex */
	FString GetGizmoProjectorParameterNameWithIndex();

	/** Getter for LevelViewportClient::GizmoProxy::HasAssignedData */
	bool GetGizmoHasAssignedData();

	/** Getter for LevelViewportClient::GizmoProxy::AssignedDataIsFromNode */
	bool GetGizmoAssignedDataIsFromNode();

	void CopyTransformFromOriginData();

	/** Returns true if any projector node is currently selected (in the case of the Customizable Object Editor) */
	bool AnyProjectorNodeSelected();

	/** To notify of changes in any parameter projector node property in case is the one being used, currently
	* just the projector type parameter is taken into account */
	void ProjectorParameterChanged(UCustomizableObjectNodeProjectorParameter* Node);

	/** To notify of changes in any parameter projector node property in case is the one being used, currently
	* just the projector type parameter is taken into account */
	void ProjectorParameterChanged(UCustomizableObjectNodeProjectorConstant* Node);
	
	/** Setter of CustomizableObject */
	void SetCustomizableObject(UCustomizableObject* CustomizableObjectParameter);

	/** Setter of AssetRegistryLoaded and LevelViewportClient::AssetRegistryLoaded */
	void SetAssetRegistryLoaded(bool Value);

	/** Called to bring up the screenshot UI */
	void OnTakeHighResScreenshot();

	/** Getter for LevelViewportClient::bManipulating */
	bool GetIsManipulatingGizmo();

	/** Sets camera mode of the LevelViewportClient::bActivateOrbitalCamera */
	void SetCameraMode(bool Value);

	/** Returns whether or not the a camera mode is active (Orbital or Free) */
	bool IsCameraModeActive(int Value);

	/** Sets the first material of the instance as the default section to draw in the UVs Overlay (when there is no material selected in the combobox)*/
	void SetDrawDefaultUVMaterial();

private:
	/** Determines the visibility of the viewport. */
	bool IsVisible() const;

	EActiveTimerReturnType HandleActiveTimer(double InCurrentTime, float InDeltaTime);

	
	// Components for the preview mesh.
	TArray<UDebugSkelMeshComponent*> PreviewSkeletalMeshComponents;

	// The scene for this viewport.
	TSharedPtr<class FCustomizableObjectPreviewScene> PreviewScenePtr;

	// Level viewport client
	TSharedPtr<class FCustomizableObjectEditorViewportClient> LevelViewportClient;

	// Viewport widget
	TSharedPtr<class SCustomizableObjectEditorViewport> ViewportWidget;
	TSharedPtr<class SCustomizableObjectEditorToolBar> ViewportToolbar;

	/** Commands that are bound to delegates*/
	TSharedPtr<FUICommandList> UICommandList;

	// The currently selected view mode.
	EViewModeIndex CurrentViewMode;

	// Selection for the material to filter when showing UVs
	TArray<TSharedPtr<FString>> ArrayUVMaterialOptionString;

	// Combo box for UV material selection
	TSharedPtr< STextComboBox> UVMaterialOptionCombo;

	// Selection for the channel to filter when showing UVs
	TArray<TSharedPtr<FString>> ArrayUVChannelOptionString;

	// Combo box for UV channel selection
	TSharedPtr< STextComboBox> UVChannelOptionCombo;

	// Generates the Combobox options for each UV Channel
	void GenerateUVMaterialOptions();

	// Generates the Combobox options for each UV Channel
	void GenerateUVChannelOptions(bool bReset);

	// Selected option of the UV Material ComboBox
	TSharedPtr<FString> SelectedUVMaterial;
	
	// Selected option of the UV Channel ComboBox
	TSharedPtr<FString> SelectedUVChannel;

	// Options for the above combo.
	TArray< TSharedPtr< FString > > MaterialNames;

	// Slate callbacks to manage the material combo
	bool IsMaterialsComboEnabled() const;
	void OnMaterialChanged(TSharedPtr<FString> Selected, ESelectInfo::Type SelectInfo);
	void OnUVChannelChanged(TSharedPtr<FString> Selected, ESelectInfo::Type SelectInfo);

	/** Current LOD selection*/
	int32 LODSelection;

	/** To be able to show and hide the constant projector RTS controls */
	//TWeakPtr<class SCustomizableObjectEditorTransformViewportToolbar> EditorTransformViewportToolbar;
	TWeakPtr<class SWidget> ViewportToolbarTransformWidget;

	union ViewportToolbarTransformWidgetVisibility
	{
		uint32 Value;
		struct 
		{
			uint32 bProjectorVisible      : 1;
			uint32 bClipMeshVisible       : 1;
			uint32 bClipMorphPlaneVisible : 1;
		} State;
	} TransformWidgetVisibility {0};

	/** Flag to know when the asset registry initial loading has completed, value set by the Customizable Object / Customizable Object Instance editor */
	bool AssetRegistryLoaded;

	/** Weak pointer to the highres screenshot dialog if it's open. Will become invalid if UI is closed whilst the viewport is open */
	TWeakPtr<class SWindow> CustomizableObjectHighresScreenshot;
};
