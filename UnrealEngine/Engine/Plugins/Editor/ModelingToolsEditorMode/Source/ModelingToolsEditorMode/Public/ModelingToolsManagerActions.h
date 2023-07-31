// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"


/**
 * TInteractiveToolCommands implementation for Modeling Mode Tools
 */
class MODELINGTOOLSEDITORMODE_API FModelingToolsManagerCommands : public TCommands<FModelingToolsManagerCommands>
{
public:
	FModelingToolsManagerCommands();

protected:
	struct FStartToolCommand
	{
		FString ToolUIName;
		TSharedPtr<FUICommandInfo> ToolCommand;
	};
	TArray<FStartToolCommand> RegisteredTools;		// Tool start-commands listed below are stored in this list

public:
	/**
	 * Find Tool start-command below by registered name (tool icon name in Mode palette)
	 */
	TSharedPtr<FUICommandInfo> FindToolByName(FString Name, bool& bFound) const;
	
	//
	// These commands are set up to launch registered Tools via the ToolManager in ModelingToolsEditorMode.cpp
	//

	TSharedPtr<FUICommandInfo> BeginAddBoxPrimitiveTool;
	TSharedPtr<FUICommandInfo> BeginAddCylinderPrimitiveTool;
	TSharedPtr<FUICommandInfo> BeginAddConePrimitiveTool;
	TSharedPtr<FUICommandInfo> BeginAddArrowPrimitiveTool;
	TSharedPtr<FUICommandInfo> BeginAddRectanglePrimitiveTool;
	TSharedPtr<FUICommandInfo> BeginAddDiscPrimitiveTool;
	TSharedPtr<FUICommandInfo> BeginAddTorusPrimitiveTool;
	TSharedPtr<FUICommandInfo> BeginAddSpherePrimitiveTool;
	TSharedPtr<FUICommandInfo> BeginAddStairsPrimitiveTool;

	TSharedPtr<FUICommandInfo> BeginAddPatchTool;
	TSharedPtr<FUICommandInfo> BeginRevolveBoundaryTool;
	TSharedPtr<FUICommandInfo> BeginDrawPolygonTool;
	TSharedPtr<FUICommandInfo> BeginDrawPolyPathTool;
	TSharedPtr<FUICommandInfo> BeginDrawAndRevolveTool;
	TSharedPtr<FUICommandInfo> BeginShapeSprayTool;

	TSharedPtr<FUICommandInfo> BeginSculptMeshTool;
	TSharedPtr<FUICommandInfo> BeginCubeGridTool;
	TSharedPtr<FUICommandInfo> BeginPolyEditTool;
	TSharedPtr<FUICommandInfo> BeginSubdividePolyTool;
	TSharedPtr<FUICommandInfo> BeginTriEditTool;
	TSharedPtr<FUICommandInfo> BeginPolyDeformTool;
	TSharedPtr<FUICommandInfo> BeginSmoothMeshTool;
	TSharedPtr<FUICommandInfo> BeginOffsetMeshTool;
	TSharedPtr<FUICommandInfo> BeginDisplaceMeshTool;
	TSharedPtr<FUICommandInfo> BeginMeshSpaceDeformerTool;
	TSharedPtr<FUICommandInfo> BeginTransformMeshesTool;
	TSharedPtr<FUICommandInfo> BeginEditPivotTool;
	TSharedPtr<FUICommandInfo> BeginAddPivotActorTool;
	TSharedPtr<FUICommandInfo> BeginBakeTransformTool;
	TSharedPtr<FUICommandInfo> BeginCombineMeshesTool;
	TSharedPtr<FUICommandInfo> BeginDuplicateMeshesTool;
	TSharedPtr<FUICommandInfo> BeginAlignObjectsTool;
	TSharedPtr<FUICommandInfo> BeginTransferMeshTool;
	TSharedPtr<FUICommandInfo> BeginConvertMeshesTool;
	TSharedPtr<FUICommandInfo> BeginSplitMeshesTool;
	TSharedPtr<FUICommandInfo> BeginPatternTool;


	TSharedPtr<FUICommandInfo> BeginRemeshSculptMeshTool;
	TSharedPtr<FUICommandInfo> BeginRemeshMeshTool;
	TSharedPtr<FUICommandInfo> BeginProjectToTargetTool;
	TSharedPtr<FUICommandInfo> BeginSimplifyMeshTool;
	TSharedPtr<FUICommandInfo> BeginEditNormalsTool;
	TSharedPtr<FUICommandInfo> BeginEditTangentsTool;
	TSharedPtr<FUICommandInfo> BeginRemoveOccludedTrianglesTool;
	TSharedPtr<FUICommandInfo> BeginUVProjectionTool;
	TSharedPtr<FUICommandInfo> BeginUVLayoutTool;
	TSharedPtr<FUICommandInfo> BeginPlaneCutTool;
	TSharedPtr<FUICommandInfo> BeginMirrorTool;
	TSharedPtr<FUICommandInfo> BeginHoleFillTool;
	TSharedPtr<FUICommandInfo> BeginLatticeDeformerTool;
	TSharedPtr<FUICommandInfo> BeginPolygonCutTool;

#if WITH_PROXYLOD
	TSharedPtr<FUICommandInfo> BeginVoxelMergeTool;
	TSharedPtr<FUICommandInfo> BeginVoxelBooleanTool;
#endif	// WITH_PROXYLOD

	TSharedPtr<FUICommandInfo> BeginVoxelSolidifyTool;
	TSharedPtr<FUICommandInfo> BeginVoxelBlendTool;
	TSharedPtr<FUICommandInfo> BeginVoxelMorphologyTool;
	TSharedPtr<FUICommandInfo> BeginSelfUnionTool;
	TSharedPtr<FUICommandInfo> BeginMeshBooleanTool;
	TSharedPtr<FUICommandInfo> BeginMeshTrimTool;
	TSharedPtr<FUICommandInfo> BeginCutMeshWithMeshTool;
	TSharedPtr<FUICommandInfo> BeginMeshSelectionTool;
	TSharedPtr<FUICommandInfo> BeginBspConversionTool;
	TSharedPtr<FUICommandInfo> BeginMeshToVolumeTool;
	TSharedPtr<FUICommandInfo> BeginVolumeToMeshTool;
	TSharedPtr<FUICommandInfo> BeginMeshGroupPaintTool;

	TSharedPtr<FUICommandInfo> BeginPhysicsInspectorTool;
	TSharedPtr<FUICommandInfo> BeginSetCollisionGeometryTool;
	TSharedPtr<FUICommandInfo> BeginEditCollisionGeometryTool;
	TSharedPtr<FUICommandInfo> BeginExtractCollisionGeometryTool;

	TSharedPtr<FUICommandInfo> BeginMeshInspectorTool;
	TSharedPtr<FUICommandInfo> BeginGlobalUVGenerateTool;
	TSharedPtr<FUICommandInfo> BeginGroupUVGenerateTool;
	TSharedPtr<FUICommandInfo> BeginWeldEdgesTool;
	TSharedPtr<FUICommandInfo> BeginPolyGroupsTool;
	TSharedPtr<FUICommandInfo> BeginEditMeshMaterialsTool;
	TSharedPtr<FUICommandInfo> BeginTransformUVIslandsTool;
	TSharedPtr<FUICommandInfo> BeginMeshAttributePaintTool;
	TSharedPtr<FUICommandInfo> BeginAttributeEditorTool;
	TSharedPtr<FUICommandInfo> BeginBakeMeshAttributeMapsTool;
	TSharedPtr<FUICommandInfo> BeginBakeMultiMeshAttributeMapsTool;
	TSharedPtr<FUICommandInfo> BeginBakeRenderCaptureTool;
	TSharedPtr<FUICommandInfo> BeginBakeMeshAttributeVertexTool;
	TSharedPtr<FUICommandInfo> BeginUVSeamEditTool;

	TSharedPtr<FUICommandInfo> BeginSkinWeightsPaintTool;
	TSharedPtr<FUICommandInfo> BeginSkinWeightsBindingTool;
	
	TSharedPtr<FUICommandInfo> BeginLODManagerTool;
	TSharedPtr<FUICommandInfo> BeginGenerateStaticMeshLODAssetTool;
	TSharedPtr<FUICommandInfo> BeginISMEditorTool;

	TSharedPtr<FUICommandInfo> BeginPolyModelTool_FaceSelect;
	TSharedPtr<FUICommandInfo> BeginPolyModelTool_EdgeSelect;
	TSharedPtr<FUICommandInfo> BeginPolyModelTool_VertexSelect;
	TSharedPtr<FUICommandInfo> BeginPolyModelTool_AllSelect;
	TSharedPtr<FUICommandInfo> BeginPolyModelTool_LoopSelect;
	TSharedPtr<FUICommandInfo> BeginPolyModelTool_RingSelect;

	TSharedPtr<FUICommandInfo> BeginPolyModelTool_Extrude;
	TSharedPtr<FUICommandInfo> BeginPolyModelTool_Inset;
	TSharedPtr<FUICommandInfo> BeginPolyModelTool_Outset;
	TSharedPtr<FUICommandInfo> BeginPolyModelTool_CutFaces;


	TSharedPtr<FUICommandInfo> BeginSelectionAction_ToVertexType;
	TSharedPtr<FUICommandInfo> BeginSelectionAction_ToEdgeType;
	TSharedPtr<FUICommandInfo> BeginSelectionAction_ToFaceType;

	TSharedPtr<FUICommandInfo> BeginSelectionAction_ToObjectType;
	TSharedPtr<FUICommandInfo> BeginSelectionAction_ToTriangleType;
	TSharedPtr<FUICommandInfo> BeginSelectionAction_ToPolygroupType;

	TSharedPtr<FUICommandInfo> BeginSelectionAction_Delete;

	// For connecting ModelingMode with UVEditor plugin
	TSharedPtr<FUICommandInfo> LaunchUVEditor;

	//
	// Accept/Cancel/Complete commands are used to end the active Tool via ToolManager
	//

	TSharedPtr<FUICommandInfo> AcceptActiveTool;
	TSharedPtr<FUICommandInfo> CancelActiveTool;
	TSharedPtr<FUICommandInfo> CompleteActiveTool;

	TSharedPtr<FUICommandInfo> AcceptOrCompleteActiveTool;
	TSharedPtr<FUICommandInfo> CancelOrCompleteActiveTool;

	/**
	 * Initialize above commands
	 */
	virtual void RegisterCommands() override;
};

