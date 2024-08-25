// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/AssetEditorToolkit.h"

class UPointLightComponent;
class UCustomizableObjectNodeMeshClipWithMesh;
class UCustomizableObjectNodeProjectorParameter;
class UCustomizableObjectNodeProjectorConstant;
class SCustomizableObjectEditorViewportTabBody;
class UCustomizableObject;
class UCustomizableObjectInstance;
class UProjectorParameter;
class UCustomSettings;
class UCustomizableObjectNodeMeshClipMorph;
class UCustomizableObjectNodeMeshClipWithMesh;
class ULightComponent;
class UPoseAsset;

/**
 * Public interface to Customizable Object Instance Editor
 */
class ICustomizableObjectInstanceEditor : public FAssetEditorToolkit
{
public:
	virtual UCustomizableObjectInstance* GetPreviewInstance() = 0;

	/** Refreshes the Customizable Object Instance Editor's viewport. */
	virtual TSharedPtr<SCustomizableObjectEditorViewportTabBody> GetViewport() = 0;

	/** Refreshes everything in the Customizable Object Instance Editor. */
	virtual void RefreshTool() = 0;
	
	virtual void SetPoseAsset(UPoseAsset* PoseAssetParameter) {}

	/** Return the selected projector in the viewport/editor. */
	virtual UProjectorParameter* GetProjectorParameter() = 0;

	virtual UCustomSettings* GetCustomSettings() = 0;

	/** Hide the currently selected gizmo. Notice that only a single gizmo can be shown at the same time.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void HideGizmo() = 0;

	/** Show the default projector value gizmo of the given NodeProjectorConstant.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void ShowGizmoProjectorNodeProjectorConstant(UCustomizableObjectNodeProjectorConstant& Node) {}

	/** Hide the default projector value gizmo of the given NodeProjectorConstant.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void HideGizmoProjectorNodeProjectorConstant() {}
	
	/** Show the default projector value gizmo of the given NodeProjectorParameter.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void ShowGizmoProjectorNodeProjectorParameter(UCustomizableObjectNodeProjectorParameter& Node) {}

	/** Hide the default projector value gizmo of the given NodeProjectorParameter.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void HideGizmoProjectorNodeProjectorParameter() {}
	
	/** Show the projector value gizmo of the given parameter.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void ShowGizmoProjectorParameter(const FString& ParamName, int32 RangeIndex = -1) = 0;

	/** Hide the projector value gizmo of the given parameter.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void HideGizmoProjectorParameter() = 0;
	
	/** Show the clip morph plane gizmo of the NodeClipMorph.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void ShowGizmoClipMorph(UCustomizableObjectNodeMeshClipMorph& ClipPlainNode) {}

	/** Hide the clip morph plane gizmo of the NodeClipMorph.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void HideGizmoClipMorph() {}
	
	/** Show the clipping mesh gizmo from the NodeMeshClipWithMesh.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void ShowGizmoClipMesh(UCustomizableObjectNodeMeshClipWithMesh& ClipMeshNode) {}

	/** Hide the clipping mesh gizmo from the NodeMeshClipWithMesh.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void HideGizmoClipMesh() {}
	
	/** Show the light gizmo of the light.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void ShowGizmoLight(ULightComponent& SelectedLight) {}

	/** Hide the light gizmo of the light.
	 * Synchronizes all editor widgets so that the selection and the widgets is always consistent. */
	virtual void HideGizmoLight() {}
};



#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Toolkits/IToolkitHost.h"
#endif
