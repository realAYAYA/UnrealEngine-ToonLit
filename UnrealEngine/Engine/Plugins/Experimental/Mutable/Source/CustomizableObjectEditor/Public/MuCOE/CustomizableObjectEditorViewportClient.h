// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Editor/UnrealEdTypes.h"
#include "EditorViewportClient.h"
#include "Math/Axis.h"
#include "Math/BoxSphereBounds.h"
#include "Math/Color.h"
#include "Math/Matrix.h"
#include "Math/Rotator.h"
#include "Math/Sphere.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UnrealWidgetFwd.h"

class FCanvas;
class FCustomizableObjectWidget;
class FMaterialRenderProxy;
class FPreviewScene;
class FPrimitiveDrawInterface;
class FReferenceCollector;
class FSceneView;
class FViewport;
class ICustomizableObjectInstanceEditor;
class UAnimationAsset;
class UCustomizableObject;
class UCustomizableObjectNodeMeshClipMorph;
class UCustomizableObjectNodeMeshClipWithMesh;
class UCustomizableObjectNodeProjectorConstant;
class UCustomizableObjectNodeProjectorParameter;
class UDebugSkelMeshComponent;
class UMaterial;
class UMaterialInterface;
class UObject;
class UStaticMesh;
class UStaticMeshComponent;
struct FInputKeyEventArgs;


extern void RemoveRestrictedChars(FString& String);

/**
* Class used to store all the possible gizmo edition options for projectors
* (constant projector node, parameter projector node, and projector
* parameter from a Customizable Object Instance
*/
class GizmoRTSProxy
{
public:
	GizmoRTSProxy();

	/** Updates the origin data of this gizmo proxy with the information
    * present in GizmoRTSProxy::Value */
    void CopyTransformFromOriginData();

	/** Updates the origin data of this gizmo proxy with the information
	* present in GizmoRTSProxy::Value */
	bool UpdateOriginData();

	/** Call to the corresponding Modify method when DataOriginParameter
	* or DataOriginConstant is not nullptr */
	bool Modify();

	/** Call to the corresponding graph Modify method when DataOriginParameter
	* or DataOriginConstant is not nullptr */
	bool ModifyGraph();

	/** Reinitializes the values of DataOriginParameter, DataOriginConstant
	* and ProjectorParameterName and ProjectorRangeIndex */
	void CleanOriginData();

	/** Returns true if the values in the variable Parameter given has the ones assigned
	* in the FCustomizableObjectProjector constructor (to identify uninitialized
	* parameter proyector data outside the graph editor and initialize it) */
	bool ProjectorHasInitialValues(FCustomizableObjectProjector& Parameter);

	/** Returns the initial transform data for the projector */
	FCustomizableObjectProjector SetProjectorInitialValue(TArray<TWeakObjectPtr<class UDebugSkelMeshComponent>>& SkeletalMeshComponents, float TotalLength);

	/** Returns true if a projector parameter is selected in the viewport */
	bool IsProjectorParameterSelected();

	/** To notify of changes in any parameter projector node property in case is the one being used, currently
	* just the projector type parameter is taken into account */
	void ProjectorParameterChanged(UCustomizableObjectNodeProjectorParameter* Node);

	/** To notify of changes in any parameter projector node property in case is the one being used, currently
	* just the projector type parameter is taken into account */
	void ProjectorParameterChanged(UCustomizableObjectNodeProjectorConstant* Node);

	/** Setter for CallUpdateSkeletalMesh */
	void SetCallUpdateSkeletalMesh(bool Value);

	/** Set Projector updated in viewport to skip rebuilding the Instance Properties panel */
	void SetProjectorUpdatedInViewport(bool Value);

	/** Getter for ProjectorParameterName */
	FString GetProjectorParameterName();

	/** Getter for ProjectorParameterNameWithIndex */
	FString GetProjectorParameterNameWithIndex();

	/** Getter for HasAssignedData */
	bool GetHasAssignedData();

	/** Getter for AssignedDataIsFromNode */
	bool GetAssignedDataIsFromNode();

	// Projector information for edition
	FCustomizableObjectProjector Value;

	// True if there's a gizmo to be shown, false otherwise
	bool AnyGizmoSelected;

	// Pointer to the Customizable Object Node Projector Parameter
	// in case this proxy is using gizmo data from an instance of this type
	UCustomizableObjectNodeProjectorParameter* DataOriginParameter;

	// Pointer to the Customizable Object Node Projector Constant
	// in case this proxy is using gizmo data from an instance of this type
	UCustomizableObjectNodeProjectorConstant* DataOriginConstant;

	// Name of the projecto parameter in case this proxy is using gizmo
	// data from a Customizable Object Instance Projector parameter data
	// (an element of CustomizableObjectInstance::ProjectorParameters)
	FString ProjectorParameterName;
	FString ProjectorParameterNameWithIndex;
	int32 ProjectorRangeIndex;

	/** Pointer back to the editor tool that owns the FCustomizableObjectEditorViewportClient instance */
	TWeakPtr<ICustomizableObjectInstanceEditor> CustomizableObjectEditorPtr;

	/** Simple flag to cache when to update the original data source of the gizmo */
	bool HasAssignedData;

	/** Simple flag to when the gizmo assigned data is from a node */
	bool AssignedDataIsFromNode;

	/** Index of the projector parameter in the Customizable Object Instance projector
	* parameter array in the case of editing a projector parameter */
	int32 ProjectorParameterIndex;

	/** To track edition of the projector gizmo */
	bool ProjectorGizmoEdited;

	/** In the case of selecting another parameter projector, avoid re-generating the skeletal mesh since no change is done in the selection event */
	bool CallUpdateSkeletalMesh;

	bool bManipulatingGizmo;

	// Projection type for the associated texture.
	ECustomizableObjectProjectorType ProjectionType;
};


/** Viewport Client for the preview viewport */
class FCustomizableObjectEditorViewportClient : public FEditorViewportClient
{
public:
	FCustomizableObjectEditorViewportClient(TWeakPtr<class ICustomizableObjectInstanceEditor> InCustomizableObjectEditor, FPreviewScene* InPreviewScene);
	~FCustomizableObjectEditorViewportClient();

	/** persona config options **/
	class UPersonaOptions* ConfigOption;

	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;

	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual bool InputWidgetDelta(FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale) override;
	virtual void TrackingStarted(const struct FInputEventState& InInputState, bool bIsDragging, bool bNudge) override;
	virtual void TrackingStopped() override;
	virtual UE::Widget::EWidgetMode GetWidgetMode() const override;
	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override;
	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual ECoordSystem GetWidgetCoordSystemSpace() const override;
	virtual void SetWidgetCoordSystemSpace(ECoordSystem NewCoordSystem) override;
	virtual void SetViewportType(ELevelViewportType InViewportType) override;
	// End of FEditorViewportClient

	void UpdateCameraSetup();
	void UpdateFloor();

	/** */
	void SetPreviewComponent(UStaticMeshComponent* InPreviewStaticMeshComponent);
	void SetPreviewComponents(TArray<UDebugSkelMeshComponent*>& InPreviewSkeletalMeshComponents);

	/** Sets an informative message in the viewport warning the user that the CustomizableObject has no reference mesh */
	void SetReferenceMeshMissingWarningMessage(bool bVisible);

	/**
	 *	Draws the UV overlay for the current LOD.
	 *
	 *	@param	InViewport					The viewport to draw to
	 *	@param	InCanvas					The canvas to draw to
	 *	@param	InTextYPos					The Y-position to begin drawing the UV-Overlay at (due to text displayed above it).
	 */
	void DrawUVs(FViewport* InViewport, FCanvas* InCanvas, int32 InTextYPos, const FString& MaterialName);

	// Callback for baking the current instance.
	void BakeInstance();
	
	// Returns false if the resource has already been duplicated, otherwise, returns true and an unique ResourceName.
	bool GetUniqueResourceName(UObject* InResource, FString& InOutResourceName, TArray<UObject*>& InCachedObjects, TArray<FString>& InCachedObjectNames);
	
	// If baking files that already exist, ask the user for permission to overwirte them
	bool ManageBakingAction(const FString& Path, const FString& Name);

	// Callback to show / hide instance geometry data
	void StateChangeShowGeometryData();

	/** Callback for toggling the UV overlay show flag. */
	void SetDrawUVOverlay();
	
	/** Callback for checking the UV overlay show flag. */
	bool IsSetDrawUVOverlayChecked() const;

	// Set the material index whose UVs will be drawn.
	void SetDrawUVOverlayMaterial(const FString& MaterialName, FString UVChannel);

	/** Callback for toggling the grid show flag. */
	void SetShowGrid();
	
	/** Callback for checking the grid show flag. */
	bool IsSetShowGridChecked() const;

	/** Callback for toggling the sky show flag. */
	void SetShowSky();

	/** Callback for checking the sky show flag. */
	bool IsSetShowSkyChecked() const;

	/** Callback for toggling the bounds show flag. */
	void SetShowBounds();
	
	/** Callback for checking the bounds show flag. */
	bool IsSetShowBoundsChecked() const;

	/** Callback for toggling the collision geometry show flag. */
	void SetShowCollision();
	
	/** Callback for checking the collision geometry show flag. */
	bool IsSetShowCollisionChecked() const;

	/** Callback for toggling the pivot show flag. */
	void SetShowPivot();

	/** Callback for checking the pivot show flag. */
	bool IsSetShowPivotChecked() const;
	
	/** Returns the desired target of the camera */
	FSphere GetCameraTarget();

	/** Point the camera to the Skeletal Mesh Components. */
	void ResetCamera();
	
	/* Returns the floor height offset */	
	float GetFloorOffset() const;

	/* Sets the floor height offset, saves it to config and invalidates the viewport so it shows up immediately */
	void SetFloorOffset(float NewValue);

	void SetClipMorphPlaneVisibility(bool bVisible, const FVector& Origin, const FVector& Normal, float MorphLength, const FBoxSphereBounds& Bounds, float InRadius1, float InRadius2, float InRotationAngle);
	void SetClipMorphPlaneVisibility(bool bVisible, UCustomizableObjectNodeMeshClipMorph* ClipPlainNode);
	void SetClipMeshVisibility(bool bVisible, UStaticMesh* ClipMesh, UCustomizableObjectNodeMeshClipWithMesh* ClipMeshNode);
	void SetProjectorVisibility(bool bVisible, FString ProjectorParameterName, FString ProjectorParameterNameWithIndex, int32 ProjectorRangeIndex, const FCustomizableObjectProjector& Data, int32 ProjectorParameterIndex);
	void SetProjectorType(bool bVisible, FString ProjectorParameterName, FString ProjectorParameterNameWithIndex, int32 ProjectorRangeIndex, const FCustomizableObjectProjector& Data, int32 ProjectorParameterIndex);
	void SetProjectorVisibility(bool bVisible, UCustomizableObjectNodeProjectorConstant* Projector);
	void SetProjectorParameterVisibility(bool bVisible, UCustomizableObjectNodeProjectorParameter* InProjectorParameter);
	void ResetProjectorVisibility();
	void SetVisibilityForWireframeMode(bool bIsWireframeMode);

	void SetAnimation(class UAnimationAsset* Animation, EAnimationMode::Type AnimationType);

	/** Set again the animation saved in AnimationBeingPlayed (if any) */
	void ReSetAnimation();

	/** Add Light Component to the scene */
	void AddLightToScene(class ULightComponent* AddedLight);

	/** Remove Light component from the scene. use destroy component instead? */
	void RemoveLightFromScene(class ULightComponent* RemovedLight);

	/** Select/Unselect lights, selected lights can be edited using gizmos */
	void SetSelectedLight(class ULightComponent* SelectedLight);

	void SetProjectorWidgetMode(UE::Widget::EWidgetMode InMode);

	const GizmoRTSProxy& GetGizmoProxy();

	/** Getter of GizmoRTSProxy::IsProjectorParameterSelected */
	bool GetGizmoIsProjectorParameterSelected();

	/** Getter for bManipulating */
	bool GetIsManipulating();

	/** Getter for WidgetVisibility */
	bool GetWidgetVisibility();

	/** Update the current gizmo data to its corresponding data origin */
	void UpdateGizmoDataToOrigin();

	/** Updates the origin data of this gizmo proxy with the information
    * present in GizmoRTSProxy::Value */
    void CopyTransformFromOriginData();

	/** Returns true if any projector node is currently selected (in the case of the Customizable Object Editor) */
	bool AnyProjectorNodeSelected();

	/** Returns in ArrayBoxSize and ArrayBoxColor the data for drawing the visual clue of the performance for the state enter and update test */
	void BuildMeanTimesBoxSizes(float MaxWidth, TArray<float>& ArrayData, TArray<float>& ArrayBoxSize, TArray<FLinearColor>& ArrayBoxColor);

	/** Returns in BoxSize and BoxColor the data for drawing the visual clue of the performance test for state enter and parameter update */
	void BuildMeanTimesBoxSize(const float MinValue, const float MaxValue, const float MeanValue, const float MaxWidth, const float Data, float& BoxSize, FLinearColor& BoxColor) const;

	/** Setter of CustomizableObject */
	void SetCustomizableObject(UCustomizableObject* CustomizableObjectParameter);

	/** Setter for StateChangeShowDataFlag */
	void SetStateChangeShowDataFlag(bool Value);

	/** Getter for StateChangeShowDataFlag */
	bool GetStateChangeShowDataFlag();

	/** Helper method to draw shadowed string on viewport */
	void DrawShadowedString(FCanvas* Canvas, float StartX, float StartY, const FLinearColor& Color, float TextScale, FString String);

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FCustomizableObjectEditorViewportClient");
	}
	// End of FSerializableObject interface

	/** Setter of AssetRegistryLoaded */
	void SetAssetRegistryLoaded(bool Value);

	/** Show pero LOD geometric information of the instance */
	void ShowInstanceGeometryInformation(FCanvas* InCanvas);

	/** Sets up the ShowFlag according to the current preview scene profile */
	void SetAdvancedShowFlagsForScene(const bool bAdvancedShowFlags);

	/** Delegate for preview profile is changed (used for updating show flags) */
	void OnAssetViewerSettingsChanged(const FName& InPropertyName);

	/** To notify of changes in any parameter projector node property in case is the one being used, currently
	* just the projector type parameter is taken into account */
	void ProjectorParameterChanged(UCustomizableObjectNodeProjectorParameter* Node);

	/** To notify of changes in any parameter projector node property in case is the one being used, currently
	* just the projector type parameter is taken into account */
	void ProjectorParameterChanged(UCustomizableObjectNodeProjectorConstant* Node);

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

	/** */
	bool IsNodeMeshClipMorphSelected();

	/** Debug draw a partial cylinder, given by MaxAngle in [0, 2PI] */
	void DrawCylinderArc(FPrimitiveDrawInterface* PDI, const FMatrix& CylToWorld, const FVector& Base, const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis, float Radius, float HalfHeight, uint32 Sides, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority, FColor Color, float MaxAngle);

	/** Getter of viewport's floor visibility */
	bool GetFloorVisibility();

	/** Setter of viewport's floor visibility */
	void SetFloorVisibility(bool Value);

	/** Getter of viewport's grid visibility */
	bool GetGridVisibility();

	/** Getter of viewport's environment visibility */
	bool GetEnvironmentMeshVisibility();

	/** Setter of viewport's environment visibility */
	void SetEnvironmentMeshVisibility(uint32 Value);

	/** Get associated texture parameter names for the material given as parameter */
	TArray<FName> GetTextureParameterNames(UMaterial* Material);

	/** Returns camera mode */
	bool IsOrbitalCameraActive() const; 

	/** Sets camera mode */
	void SetCameraMode(bool Value);

	/** Getter of the Material name being drawn in the UV viewer */
	FString GetMaterialToDrawInUVs()
	{
		return FullNameMaterialToDrawInUVs;
	}


private:
	/** Component for the static/skeletal mesh. */
	TWeakObjectPtr<class UStaticMeshComponent> StaticMeshComponent;
	TArray<TWeakObjectPtr<class UDebugSkelMeshComponent>> SkeletalMeshComponents;
		
	TMap< int, TSharedPtr<FCustomizableObjectWidget> > ProjectorWidgets;

	/** Last hit proxy data. */
	bool bManipulating;
	EAxis::Type LastHitProxyAxis;
	FCustomizableObjectWidget* LastHitProxyWidget;

	/** Drag values for handling moving and rotating widgets. */
	FVector LocalManDir;
	FVector WorldManDir;
	float DragDirX;
	float DragDirY;

	/** Pointer back to the editor tool that owns us */
	TWeakPtr<ICustomizableObjectInstanceEditor> CustomizableObjectEditorPtr;

	/** Flags for various options in the editor. */
	bool bCameraMove;
	bool bDrawUVs;
	bool bShowSockets;
	bool bDrawNormals;
	bool bDrawTangents;
	bool bDrawBinormals;
	bool bShowPivot;
	bool bDrawSky;

	// Material index to draw in the uv preview.
	FString MaterialToDrawInUVs;
	FString FullNameMaterialToDrawInUVs;
	int32 MaterialToDrawInUVsLOD;
	int32 MaterialToDrawInUVsIndex;
	int32 UVChannelToDrawInUVs;
	int32 MaterialToDrawInUVsComponent;

	bool bReferenceMeshMissingWarningMessageVisible;

	UCustomizableObjectNodeMeshClipMorph* ClipMorphNode;
	UMaterial* ClipMorphMaterial;
	bool bClipMorphVisible;
	bool bClipMorphLocalStartOffset;
	FVector ClipMorphOrigin;
	FVector ClipMorphOffset;
	FVector ClipMorphLocalOffset;
	FVector ClipMorphNormal;
	FVector ClipMorphXAxis;
	FVector ClipMorphYAxis;
	float MorphLength;
	FBoxSphereBounds MorphBounds;
	UCustomizableObjectNodeMeshClipWithMesh* ClipMeshNode;
	UStaticMeshComponent* ClipMeshComp;
	float Radius1;
	float Radius2;
	float RotationAngle;

	UE::Widget::EWidgetMode WidgetMode;
	FSphere BoundSphere;

	/** Light beeing edited */
	class ULightComponent* SelectedLightComponent;

	bool bIsEditingLightEnabled;

	/** Proxy class to store the gizmo information needed for gizmo edition for a projector
	* (constant node, parameter node or Customizable Object Instance projector parameter */
	GizmoRTSProxy GizmoProxy;

	/** To track Widget::bDefaultVisibility value (there's no getter and FWidget::bDefaultVisibility is private) */
	bool WidgetVisibility;

	/** To know if an animation ia being played by the Customizable Object and restre it after compilation */
	bool IsPlayingAnimation;

	/** Animation being played by the Customizable Object, if any */
	UAnimationAsset* AnimationBeingPlayed;

	/** Customizable object being used */
	UCustomizableObject* CustomizableObject;

	/** Flag to control whther to show / hide the instance geometry information data */
	bool StateChangeShowGeometryDataFlag;

	/** To know if the user has given permission to overwrite files when baking if already present */
	bool BakingOverwritePermission;

	/** Flag to know when the asset registry initial loading has completed, value set by the Customizable Object / Customizable Object Instance editor */
	bool AssetRegistryLoaded;

	/** To know if the orbital camera is being used or not */
	bool bActivateOrbitalCamera;

	/** Material for cylinder arc solid render */
	UMaterialInterface* TransparentPlaneMaterialXY;

	/** bool to return the Camera mode to Orbital when changing the Camera view to Perspective */
	bool bSetOrbitalOnPerspectiveMode;
};