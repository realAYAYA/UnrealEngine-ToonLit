// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelingToolsManagerActions.h"
#include "ModelingToolsEditorModeStyle.h"
#include "ModelingToolsEditorModeSettings.h"
#include "Styling/ISlateStyle.h"

#define LOCTEXT_NAMESPACE "ModelingToolsManagerCommands"



FModelingToolsManagerCommands::FModelingToolsManagerCommands() :
	TCommands<FModelingToolsManagerCommands>(
		"ModelingToolsManagerCommands", // Context name for fast lookup
		NSLOCTEXT("Contexts", "ModelingToolsToolCommands", "Modeling Mode - Tools"), // Localized context name for displaying
		NAME_None, // Parent
		FModelingToolsEditorModeStyle::Get()->GetStyleSetName() // Icon Style Set
		)
{
}



TSharedPtr<FUICommandInfo> FModelingToolsManagerCommands::FindToolByName(FString Name, bool& bFound) const
{
	bFound = false;
	for (const FStartToolCommand& Command : RegisteredTools)
	{
		if (Command.ToolUIName.Equals(Name, ESearchCase::IgnoreCase)
		 || (Command.ToolCommand.IsValid() && Command.ToolCommand->GetLabel().ToString().Equals(Name, ESearchCase::IgnoreCase)))
		{
			bFound = true;
			return Command.ToolCommand;
		}
	}
	return TSharedPtr<FUICommandInfo>();
}


void FModelingToolsManagerCommands::RegisterCommands()
{
	const UModelingToolsEditorModeSettings* Settings = GetDefault<UModelingToolsEditorModeSettings>();
	const UModelingToolsModeCustomizationSettings* UISettings = GetDefault<UModelingToolsModeCustomizationSettings>();
	bool bUseShortNames = UISettings->bUseLegacyModelingPalette;

	// this has to be done with a compile-time macro because UI_COMMAND expands to LOCTEXT macros
#define REGISTER_MODELING_TOOL_COMMAND(ToolCommandInfo, ToolName, ToolTip) \
		UI_COMMAND(ToolCommandInfo, ToolName, ToolTip, EUserInterfaceActionType::ToggleButton, FInputChord()); \
		RegisteredTools.Add(FStartToolCommand{ ToolName, ToolCommandInfo });

	// this has to be done with a compile-time macro because UI_COMMAND expands to LOCTEXT macros
#define REGISTER_MODELING_ACTION_COMMAND(ToolCommandInfo, ToolName, ToolTip) \
		UI_COMMAND(ToolCommandInfo, ToolName, ToolTip, EUserInterfaceActionType::Button, FInputChord()); \
		RegisteredTools.Add(FStartToolCommand{ ToolName, ToolCommandInfo });

	// Shapes
	REGISTER_MODELING_TOOL_COMMAND(BeginAddBoxPrimitiveTool, "Box", "Create new box objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddSpherePrimitiveTool, "Sphere", "Create new sphere objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddConePrimitiveTool, "Cone", "Create new cone objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddTorusPrimitiveTool, "Torus", "Create new torus objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddArrowPrimitiveTool, "Arrow", "Create new arrow objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddDiscPrimitiveTool, "Disc", "Create new disc objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddStairsPrimitiveTool, "Stairs", "Create new stairs objects");
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginAddCylinderPrimitiveTool, "Cyl", "Create new cylinder objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginAddRectanglePrimitiveTool, "Rect", "Create new rectangle objects");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginAddCylinderPrimitiveTool, "Cylinder", "Create new cylinder objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginAddRectanglePrimitiveTool, "Rectangle", "Create new rectangle objects");
	}


	// Create
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginDrawPolygonTool, "PolyExt", "Draw and extrude 2D Polygons to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginDrawPolyPathTool, "PathExt", "Draw and extrude 2D Paths to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginDrawAndRevolveTool, "PathRev", "Draw and revolve 2D Paths to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginRevolveSplineTool, "SplnRev", "Revolve splines to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginRevolveBoundaryTool, "BdryRev", "Revolve Mesh boundary loops to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginCombineMeshesTool, "Merge", "Merge multiple Meshes to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginDuplicateMeshesTool, "Dupe", "Duplicate single Meshes to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginDrawSplineTool, "DrwSpln", "Draw a spline in the level");
		REGISTER_MODELING_TOOL_COMMAND(BeginTriangulateSplinesTool, "MshSpln", "Triangulate the Spline Components of selected actors to create new objects");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginDrawPolygonTool, "Extrude Polygon", "Draw and extrude 2D Polygons to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginDrawPolyPathTool, "Extrude Path", "Draw and extrude 2D Paths to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginDrawAndRevolveTool, "Revolve Path", "Draw and revolve 2D Paths to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginRevolveSplineTool, "Revolve Spline", "Revolve splines to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginRevolveBoundaryTool, "Revolve Boundary", "Revolve Mesh boundary loops to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginCombineMeshesTool, "Merge", "Merge multiple Meshes to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginDuplicateMeshesTool, "Duplicate", "Duplicate single Meshes to create new objects");
		REGISTER_MODELING_TOOL_COMMAND(BeginDrawSplineTool, "Draw Spline", "Draw a spline in the level");
		REGISTER_MODELING_TOOL_COMMAND(BeginTriangulateSplinesTool, "Mesh Splines", "Triangulate the Spline Components of selected actors to create new objects");
	}

	// PolyModel
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyEditTool, "PolyEd", "Edit Meshes via PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyDeformTool, "PolyDef", "Deform Meshes via PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginCubeGridTool, "CubeGr", "Create block out Meshes using a repositionable grid");
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshBooleanTool, "MshBool", "Apply Boolean operations to the two selected Meshes");
		REGISTER_MODELING_TOOL_COMMAND(BeginCutMeshWithMeshTool, "MshCut", "Split one Mesh into parts using a second Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginSubdividePolyTool, "SubDiv", "Subdivide the selected Mesh via PolyGroups or triangles");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyEditTool, "PolyGroup Edit", "Edit Meshes via PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyDeformTool, "Deform PolyGroups", "Deform Meshes via PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginCubeGridTool, "CubeGrid", "Create block out Meshes using a repositionable grid");
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshBooleanTool, "Boolean", "Apply Boolean operations to the two selected Meshes");
		REGISTER_MODELING_TOOL_COMMAND(BeginCutMeshWithMeshTool, "Mesh Cut", "Split one Mesh into parts using a second Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginSubdividePolyTool, "Subdivide", "Subdivide the selected Mesh via PolyGroups or triangles");
	}

	// TriModel
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshSelectionTool, "TriSel", "Select and edit Mesh triangles with a brush interface");
		REGISTER_MODELING_TOOL_COMMAND(BeginTriEditTool, "TriEd", "Select and Edit the Mesh vertices, edges, and triangles");
		REGISTER_MODELING_TOOL_COMMAND(BeginHoleFillTool, "HFill", "Fill holes in the selected Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginPlaneCutTool, "PlnCut", "Cut the selected Meshes with a 3D plane");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshSelectionTool, "Tri Select", "Select and edit Mesh triangles with a brush interface");
		REGISTER_MODELING_TOOL_COMMAND(BeginTriEditTool, "Triangle Edit", "Select and Edit the Mesh vertices, edges, and triangles");
		REGISTER_MODELING_TOOL_COMMAND(BeginHoleFillTool, "Fill Holes", "Fill holes in the selected Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginPlaneCutTool, "Plane Cut", "Cut the selected Meshes with a 3D plane");
	}
	REGISTER_MODELING_TOOL_COMMAND(BeginMirrorTool, "Mirror", "Mirror the selected Meshes");
	REGISTER_MODELING_TOOL_COMMAND(BeginPolygonCutTool, "PolyCut", "Cut the selected Mesh with an extruded polygon");
	REGISTER_MODELING_TOOL_COMMAND(BeginMeshTrimTool, "Trim", "Trim/Cut the selected Mesh with the second selected Mesh");

	// Deform
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginSculptMeshTool, "VSclpt", "Vertex sculpting");
		REGISTER_MODELING_TOOL_COMMAND(BeginRemeshSculptMeshTool, "DSclpt", "Dynamic mesh sculpting");
		REGISTER_MODELING_TOOL_COMMAND(BeginDisplaceMeshTool, "Displce", "Tessellate and Displace the selected Mesh");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginSculptMeshTool, "Vertex Sculpt", "Vertex sculpting");
		REGISTER_MODELING_TOOL_COMMAND(BeginRemeshSculptMeshTool, "Dynamic Sculpt", "Dynamic mesh sculpting");
		REGISTER_MODELING_TOOL_COMMAND(BeginDisplaceMeshTool, "Displace", "Tessellate and Displace the selected Mesh");
	}
	REGISTER_MODELING_TOOL_COMMAND(BeginSmoothMeshTool, "Smooth", "Smooth the shape of the selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND(BeginOffsetMeshTool, "Offset", "Offset the surface of the selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND(BeginMeshSpaceDeformerTool, "Warp", "Reshape the selected Mesh using space deformers");
	REGISTER_MODELING_TOOL_COMMAND(BeginLatticeDeformerTool, "Lattice", "Deform the selected Mesh using a 3D lattice/grid");

	// Transform
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginTransformMeshesTool, "XForm", "Transform the selected Meshes");
		REGISTER_MODELING_TOOL_COMMAND(BeginEditPivotTool, "Pivot", "Edit the pivot points of the selected Meshes");
		REGISTER_MODELING_TOOL_COMMAND(BeginAddPivotActorTool, "PivotAct", "Add actor to act as a pivot for child components");
		REGISTER_MODELING_TOOL_COMMAND(BeginBakeTransformTool, "BakeRS", "Bake rotation and scale into the selected Meshes");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginTransformMeshesTool, "Transform", "Transform the selected Meshes");
		REGISTER_MODELING_TOOL_COMMAND(BeginEditPivotTool, "Edit Pivot", "Edit the pivot points of the selected Meshes");
		REGISTER_MODELING_TOOL_COMMAND(BeginAddPivotActorTool, "Pivot Actor", "Add actor to act as a pivot for child components");
		REGISTER_MODELING_TOOL_COMMAND(BeginBakeTransformTool, "Bake Transform", "Bake rotation and scale into the selected Meshes");
	}
	REGISTER_MODELING_TOOL_COMMAND(BeginAlignObjectsTool, "Align", "Align the selected Objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginTransferMeshTool, "Transfer", "Copy the first selected Mesh to the second selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND(BeginConvertMeshesTool, "Convert", "Convert the selected Meshes to a different type of Mesh Object");
	REGISTER_MODELING_TOOL_COMMAND(BeginSplitMeshesTool, "Split", "Split the selected Meshes into separate parts based on connectivity");
	REGISTER_MODELING_TOOL_COMMAND(BeginPatternTool, "Pattern", "Create patterns of Meshes");
	REGISTER_MODELING_TOOL_COMMAND(BeginHarvestInstancesTool, "Harvest Instances", "Extract a set of InstancedStaticMeshComponents from a set of Actors");
	

	// MeshOps
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginSimplifyMeshTool, "Simplfy", "Simplify the selected Mesh");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginSimplifyMeshTool, "Simplify", "Simplify the selected Mesh");
	}
	REGISTER_MODELING_TOOL_COMMAND(BeginRemeshMeshTool, "Remesh", "Re-triangulate the selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND(BeginWeldEdgesTool, "Weld", "Weld overlapping Mesh edges");
	REGISTER_MODELING_TOOL_COMMAND(BeginRemoveOccludedTrianglesTool, "Jacket", "Remove hidden triangles from the selected Meshes");
	REGISTER_MODELING_TOOL_COMMAND(BeginSelfUnionTool, "Union", "Boolean Union the selected Meshes, including Self-Union to resolve self-intersections");
	REGISTER_MODELING_TOOL_COMMAND(BeginProjectToTargetTool, "Project", "Map/re-mesh the first selected Mesh onto the second selected Mesh");


	// VoxOps
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginVoxelSolidifyTool, "VoxWrap", "Wrap the selected Meshes using SDFs/voxels");
		REGISTER_MODELING_TOOL_COMMAND(BeginVoxelBlendTool, "VoxBlnd", "Blend the selected Meshes using SDFs/voxels");
		REGISTER_MODELING_TOOL_COMMAND(BeginVoxelMorphologyTool, "VoxMrph", "Offset/Inset the selected Meshes using SDFs/voxels");
#if WITH_PROXYLOD
		// The ProxyLOD plugin is currently only available on Windows. Without it, the following tools do not work as expected.
		REGISTER_MODELING_TOOL_COMMAND(BeginVoxelBooleanTool, "VoxBool", "Boolean the selected Meshes using SDFs/voxels");
		REGISTER_MODELING_TOOL_COMMAND(BeginVoxelMergeTool, "VoxMrg", "Merge the selected Meshes using SDFs/voxels");
#endif	// WITH_PROXYLOD
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginVoxelSolidifyTool, "Voxel Wrap", "Wrap the selected Meshes using SDFs/voxels");
		REGISTER_MODELING_TOOL_COMMAND(BeginVoxelBlendTool, "Voxel Blend", "Blend the selected Meshes using SDFs/voxels");
		REGISTER_MODELING_TOOL_COMMAND(BeginVoxelMorphologyTool, "Voxel Offset", "Offset/Inset the selected Meshes using SDFs/voxels");
#if WITH_PROXYLOD
		// The ProxyLOD plugin is currently only available on Windows. Without it, the following tools do not work as expected.
		REGISTER_MODELING_TOOL_COMMAND(BeginVoxelBooleanTool, "Voxel Boolean", "Boolean the selected Meshes using SDFs/voxels");
		REGISTER_MODELING_TOOL_COMMAND(BeginVoxelMergeTool, "Voxel Merge", "Merge the selected Meshes using SDFs/voxels");
#endif	// WITH_PROXYLOD
	}

	// Attributes
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshInspectorTool, "Inspct", "Inspect Mesh attributes");
		REGISTER_MODELING_TOOL_COMMAND(BeginEditNormalsTool, "Nrmls", "Recompute or Repair Normals");
		REGISTER_MODELING_TOOL_COMMAND(BeginEditTangentsTool, "Tngnts", "Recompute Tangents");
		REGISTER_MODELING_TOOL_COMMAND(BeginAttributeEditorTool, "AttrEd", "Edit/configure Mesh attributes");
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyGroupsTool, "GrpGen", "Generate new PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshGroupPaintTool, "GrpPnt", "Paint new PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshVertexPaintTool, "VtxPnt", "Paint Mesh Vertices");
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshAttributePaintTool, "MapPnt", "Paint attribute maps");
		REGISTER_MODELING_TOOL_COMMAND(BeginEditMeshMaterialsTool, "MatEd", "Assign materials to selected triangles");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshInspectorTool, "Inspect", "Inspect Mesh attributes");
		REGISTER_MODELING_TOOL_COMMAND(BeginEditNormalsTool, "Normals", "Recompute or Repair Normals");
		REGISTER_MODELING_TOOL_COMMAND(BeginEditTangentsTool, "Tangents", "Recompute Tangents");
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyGroupsTool, "Generate PolyGroups", "Generate new PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshGroupPaintTool, "Paint PolyGroups", "Paint new PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshVertexPaintTool, "Paint Vertex Colors", "Paint Mesh Vertex Colors");
		REGISTER_MODELING_TOOL_COMMAND(BeginAttributeEditorTool, "Edit Attributes", "Edit/configure Mesh attributes");
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshAttributePaintTool, "Paint Maps", "Paint attribute maps");
		REGISTER_MODELING_TOOL_COMMAND(BeginEditMeshMaterialsTool, "Edit Materials", "Assign materials to selected triangles");
	}

	// UVs
	REGISTER_MODELING_TOOL_COMMAND(BeginGlobalUVGenerateTool, "AutoUV", "Automatically unwrap and pack UVs");
	// This is done directly, not with the REGISTER_ macro, since we don't want it added to the tool list or use a toggle button
	UI_COMMAND(LaunchUVEditor, "UVEditor", "Launch UV asset editor", EUserInterfaceActionType::Button, FInputChord());
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginGroupUVGenerateTool, "Unwrap", "Recompute UVs for existing UV islands or PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginUVProjectionTool, "Project", "Compute UVs via projecting to simple shapes");
		REGISTER_MODELING_TOOL_COMMAND(BeginUVSeamEditTool, "SeamEd", "Add UV seams");
		REGISTER_MODELING_TOOL_COMMAND(BeginTransformUVIslandsTool, "XFormUV", "Transform UV islands in UV space");
		REGISTER_MODELING_TOOL_COMMAND(BeginUVLayoutTool, "Layout", "Transform and Repack existing UVs");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginGroupUVGenerateTool, "UV Unwrap", "Recompute UVs for existing UV islands or PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginUVProjectionTool, "Project UVs", "Compute UVs via projecting to simple shapes");
		REGISTER_MODELING_TOOL_COMMAND(BeginUVSeamEditTool, "Edit UV Seams", "Add UV seams");
		REGISTER_MODELING_TOOL_COMMAND(BeginTransformUVIslandsTool, "Transform UVs", "Transform UV islands in UV space");
		REGISTER_MODELING_TOOL_COMMAND(BeginUVLayoutTool, "Layout UVs", "Transform and Repack existing UVs");
	}

	// Baking
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginBakeMeshAttributeMapsTool, "BakeTx", "Bake textures for a target Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginBakeMultiMeshAttributeMapsTool, "BakeAll", "Bake textures for a target Mesh from multiple source Meshes");
		REGISTER_MODELING_TOOL_COMMAND(BeginBakeMeshAttributeVertexTool, "BakeVtx", "Bake vertex colors for a target Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginBakeRenderCaptureTool, "BakeRC", "Bake renders into new textures for a target Mesh from multiple source Meshes");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginBakeMeshAttributeMapsTool, "Bake Textures", "Bake textures for a target Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginBakeMultiMeshAttributeMapsTool, "Bake All", "Bake textures for a target Mesh from multiple source Meshes");
		REGISTER_MODELING_TOOL_COMMAND(BeginBakeMeshAttributeVertexTool, "Bake Vertex Colors", "Bake vertex colors for a target Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginBakeRenderCaptureTool, "Bake RC", "Bake renders into new textures for a target Mesh from multiple source Meshes");
	}

	// Volumes
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginVolumeToMeshTool, "Vol2Msh", "Convert a Volume to a new Mesh Object");
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshToVolumeTool, "Msh2Vol", "Convert a Mesh to a Volume");
		if (!Settings->InRestrictiveMode())
		{
			REGISTER_MODELING_TOOL_COMMAND(BeginBspConversionTool, "BSPConv", "Convert BSP to a new Mesh Object");
		}
		REGISTER_MODELING_TOOL_COMMAND(BeginPhysicsInspectorTool, "PInspct", "Inspect the physics/collision geometry for selected Meshes");
		REGISTER_MODELING_TOOL_COMMAND(BeginSimpleCollisionEditorTool, "SCollEdit", "Edit the simple collision shapes for the selected Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginSetCollisionGeometryTool, "Msh2Coll", "Convert selected Meshes to Simple Collision Geometry (for last selected)");
		REGISTER_MODELING_TOOL_COMMAND(BeginExtractCollisionGeometryTool, "Coll2Msh", "Convert Simple Collision Geometry to a new Mesh Object");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginVolumeToMeshTool, "Volume To Mesh", "Convert a Volume to a new Mesh Object");
		REGISTER_MODELING_TOOL_COMMAND(BeginMeshToVolumeTool, "Mesh To Volume", "Convert a Mesh to a Volume");
		if (!Settings->InRestrictiveMode())
		{
			REGISTER_MODELING_TOOL_COMMAND(BeginBspConversionTool, "Convert BSPs", "Convert BSP to a new Mesh Object");
		}
		REGISTER_MODELING_TOOL_COMMAND(BeginPhysicsInspectorTool, "Inspect Collision", "Inspect the physics/collision geometry for selected Meshes");
		REGISTER_MODELING_TOOL_COMMAND(BeginSimpleCollisionEditorTool, "Simple Collision Editor", "Edit the simple collision shapes for the selected Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginSetCollisionGeometryTool, "Mesh To Collision", "Convert selected Meshes to Simple Collision Geometry (for last selected)");
		REGISTER_MODELING_TOOL_COMMAND(BeginExtractCollisionGeometryTool, "Collision To Mesh", "Convert Simple Collision Geometry to a new Mesh Object");
	}

	// LODs
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginLODManagerTool, "LODMgr", "Inspect the LODs of a Static Mesh Asset");
		REGISTER_MODELING_TOOL_COMMAND(BeginGenerateStaticMeshLODAssetTool, "AutoLOD", "Automatically generate a simplified LOD with baked Textures/Materials for a Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginISMEditorTool, "ISMEd", "Edit the Instances of Instanced Static Mesh Components");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginLODManagerTool, "LOD Manager", "Inspect the LODs of a Static Mesh Asset");
		REGISTER_MODELING_TOOL_COMMAND(BeginGenerateStaticMeshLODAssetTool, "AutoLOD", "Automatically generate a simplified LOD with baked Textures/Materials for a Mesh");
		REGISTER_MODELING_TOOL_COMMAND(BeginISMEditorTool, "ISM Editor", "Edit the Instances of Instanced Static Mesh Components");
	}

	REGISTER_MODELING_TOOL_COMMAND(BeginAddPatchTool, "Patch", "Add Patch");
	REGISTER_MODELING_TOOL_COMMAND(BeginShapeSprayTool, "Spray", "Shape Spray");
	REGISTER_MODELING_TOOL_COMMAND(BeginEditCollisionGeometryTool, "EditPhys", "Edit Simple Collision Geometry for selected Mesh");
	
	REGISTER_MODELING_TOOL_COMMAND(BeginSkinWeightsPaintTool, "Edit Weights", "Tune the per-vertex skin weights.");
	REGISTER_MODELING_TOOL_COMMAND(BeginSkinWeightsBindingTool, "Bind Skin", "Create default weights by binding the skin to bones.");

	REGISTER_MODELING_TOOL_COMMAND(BeginSkeletonEditingTool, "Edit Skeleton", "Add, Remove, Reparent, Move and Rename bones.");

	REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_Inset, "Inset", "Inset the current Selected Faces");
	REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_Outset, "Outset", "Outset the current Selected Faces");
	REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_CutFaces, "Cut", "Cut the current Selected Faces");
	if (bUseShortNames)
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_ExtrudeEdges, "ExtEdg", "Extrude selected boundary edges.");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_ExtrudeEdges, "Extrude Edges", "Extrude selected boundary edges.");
	}
	REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_PushPull, "PushPull", "Push/Pull the current Selected Faces");
	REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_Bevel, "Bevel", "Bevel the current Mesh Selection (Edges or Faces)");
	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_InsertEdgeLoop, "ELoop", "Insert Edge Loops into the Selected Mesh");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_InsertEdgeLoop, "Insert Loops", "Insert Edge Loops into the Selected Mesh");
	}

	if ( bUseShortNames )
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_PolyEd, "PolyEd", "Select / Edit the current Mesh via PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_TriSel, "TriSel", "Select / Edit the current Mesh via Triangles");
	}
	else
	{
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_PolyEd, "PolyGroup Edit", "Select / Edit the current Mesh via PolyGroups");
		REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_TriSel, "Triangle Edit", "Select / Edit the current Mesh via Triangles");
	}


	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_NoSelection, "None", "Disable Mesh Element Selection (Select Objects Only)");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_MeshTriangles, "Tris", "Select Mesh Triangles");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_MeshVertices, "Verts", "Select Mesh Vertices");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_MeshEdges, "Edges", "Select Mesh Edges");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_GroupFaces, "Groups", "Select Mesh PolyGroups");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_GroupCorners, "Corners", "Select Mesh PolyGroup Corners/Vertices");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_GroupEdges, "Borders", "Select Mesh PolyGroup Borders/Edges");

	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Delete, "Delete", "Delete the current Mesh Selection");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Retriangulate, "Clean", "Retriangulate the current Mesh or Mesh Selection");
	if ( bUseShortNames )
	{
		REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Disconnect, "Discon", "Disconnect the current Mesh Selection");
	}
	else
	{
		REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Disconnect, "Disconnect", "Disconnect the current Mesh Selection");
	}

	REGISTER_MODELING_TOOL_COMMAND(BeginSelectionAction_Extrude, "Extrude", "Extrude the current Mesh Selection");
	REGISTER_MODELING_TOOL_COMMAND(BeginSelectionAction_Offset, "Offset", "Offset the current Mesh Selection");

	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_SelectAll, "Select All", "Select All Elements");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_ExpandToConnected, "Expand To Connected", "Expand Selection to Geometrically-Connected Elements");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Invert, "Invert Selection", "Invert the current Selection");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_InvertConnected, "Invert Connected", "Invert the current Selection to Geometrically-Connected Elements");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Expand, "Expand Selection", "Expand the current Selection by a ring of elements");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Contract, "Contract Selection", "Contract the current Selection by a ring of elements");



	UI_COMMAND(AddToFavorites, "Add to Favorites", "Add to Favorites", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RemoveFromFavorites, "Remove from Favorites", "Remove from Favorites", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(LoadFavoritesTools, "Faves", "Favorites", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadSelectionTools, "Select", "Edit Mesh Selections", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadShapesTools, "Shapes", "Shapes", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadCreateTools, "Create", "Create New Shapes", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadPolyTools, "Model", "Shape Modeling", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadTriTools, "Process", "Mesh Processing", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadDeformTools, "Deform", "Deformations", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadTransformTools, "XForm", "Transforms & Conversion", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadMeshOpsTools, "Mesh", "Mesh Processing", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadVoxOpsTools, "Voxel", "Voxel Processing", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadAttributesTools, "Attribs", "Mesh Attributes", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadUVsTools, "UVs", "Create & Edit UVs", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadBakingTools, "Bake", "Bake Textures & Colors", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadVolumeTools, "Volumes", "Volumes", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadLodsTools, "Misc", "Additional Utility Tools", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadSkinTools, "Skin", "Edit Skin Weights", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadSkeletonTools, "Skeleton", "Edit Bones", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(AcceptActiveTool, "Accept", "Accept the active Tool", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CancelActiveTool, "Cancel", "Cancel the active Tool", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CompleteActiveTool, "Done", "Complete the active Tool", EUserInterfaceActionType::Button, FInputChord());

	// Note that passing a chord into one of these calls hooks the key press to the respective action. 
	UI_COMMAND(AcceptOrCompleteActiveTool, "Accept or Complete", "Accept or Complete the active Tool", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter));
	UI_COMMAND(CancelOrCompleteActiveTool, "Cancel or Complete", "Cancel or Complete the active Tool", EUserInterfaceActionType::Button, FInputChord(EKeys::Escape));
	
#undef REGISTER_MODELING_TOOL_COMMAND
}


TSharedPtr<FUICommandInfo> FModelingToolsManagerCommands::RegisterExtensionPaletteCommand(
	FName Name,
	const FText& Label,
	const FText& Tooltip,
	const FSlateIcon& Icon)
{
	if (Instance.IsValid())
	{
		TSharedPtr<FModelingToolsManagerCommands> Commands = Instance.Pin();

		for (FDynamicExtensionCommand& ExtensionCommand : Commands->ExtensionPaletteCommands)
		{
			if (ExtensionCommand.RegistrationName == Name)
			{
				return ExtensionCommand.Command;
			}
		}

		TSharedPtr<FUICommandInfo> NewCommandInfo;

		FUICommandInfo::MakeCommandInfo(
			Commands->AsShared(),
			NewCommandInfo,
			Name, Label, Tooltip, Icon,
			EUserInterfaceActionType::RadioButton,
			FInputChord() );

		FDynamicExtensionCommand NewExtensionCommand;
		NewExtensionCommand.RegistrationName = Name;
		NewExtensionCommand.Command = NewCommandInfo;
		Commands->ExtensionPaletteCommands.Add(NewExtensionCommand);
		return NewCommandInfo;
	};

	return TSharedPtr<FUICommandInfo>();
}



#undef LOCTEXT_NAMESPACE
