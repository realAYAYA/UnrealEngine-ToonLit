// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClothEditorSimulationVisualization.h"
#include "ChaosClothAsset/ClothEditor3DViewportClient.h"
#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosCloth/ChaosClothVisualization.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Rendering/SkeletalMeshRenderData.h"


#define LOCTEXT_NAMESPACE "ClothEditorSimulationVisualization"

namespace UE::Chaos::ClothAsset
{
namespace Private
{
FText ConcatenateLine(const FText& InText, const FText& InNewLine)
{
	if (InText.IsEmpty())
	{
		return InNewLine;
	}
	return FText::Format(LOCTEXT("ViewportTextNewlineFormatter", "{0}\n{1}"), InText, InNewLine);
}

static FText GetSimulationStatisticsString(const FClothSimulationProxy* SimProxy)
{
	FText TextValue;
	if (SimProxy)
	{
		// Cloth stats
		if (const int32 NumActiveCloths = SimProxy->GetNumCloths())
		{
			TextValue = ConcatenateLine(TextValue, FText::Format(LOCTEXT("NumActiveCloths", "Active Cloths: {0}"), NumActiveCloths));
		}
		if (const int32 NumKinematicParticles = SimProxy->GetNumKinematicParticles())
		{
			TextValue = ConcatenateLine(TextValue, FText::Format(LOCTEXT("NumKinematicParticles", "Kinematic Particles: {0}"), NumKinematicParticles));
		}
		if (const int32 NumDynamicParticles = SimProxy->GetNumDynamicParticles())
		{
			TextValue = ConcatenateLine(TextValue, FText::Format(LOCTEXT("NumDynamicParticles", "Dynamic Particles: {0}"), NumDynamicParticles));
		}
		if (const int32 NumIterations = SimProxy->GetNumIterations())
		{
			TextValue = ConcatenateLine(TextValue, FText::Format(LOCTEXT("NumIterations", "Iterations: {0}"), NumIterations));
		}
		if (const int32 NumSubSteps = SimProxy->GetNumSubsteps())
		{
			TextValue = ConcatenateLine(TextValue, FText::Format(LOCTEXT("NumSubsteps", "Substeps: {0}"), NumSubSteps));
		}
		if (const int32 NumLinearSolveIterations = SimProxy->GetNumLinearSolveIterations())
		{
			TextValue = ConcatenateLine(TextValue, FText::Format(LOCTEXT("NumCGIterations", "CGIterations: {0}"), NumLinearSolveIterations));
		}
		if (const float LinearSolveError = SimProxy->GetLinearSolveError())
		{
			FNumberFormattingOptions NumberFormatOptions;
			NumberFormatOptions.AlwaysSign = false;
			NumberFormatOptions.UseGrouping = false;
			NumberFormatOptions.RoundingMode = ERoundingMode::HalfFromZero;
			NumberFormatOptions.MinimumIntegralDigits = 1;
			NumberFormatOptions.MaximumIntegralDigits = 6;
			NumberFormatOptions.MinimumFractionalDigits = 2;
			NumberFormatOptions.MaximumFractionalDigits = 6;
			TextValue = ConcatenateLine(TextValue, FText::Format(LOCTEXT("CGError", "CGError: {0}"), FText::AsNumber(LinearSolveError, &NumberFormatOptions)));
		}
		if (const float SimulationTime = SimProxy->GetSimulationTime())
		{
			FNumberFormattingOptions NumberFormatOptions;
			NumberFormatOptions.AlwaysSign = false;
			NumberFormatOptions.UseGrouping = false;
			NumberFormatOptions.RoundingMode = ERoundingMode::HalfFromZero;
			NumberFormatOptions.MinimumIntegralDigits = 1;
			NumberFormatOptions.MaximumIntegralDigits = 6;
			NumberFormatOptions.MinimumFractionalDigits = 2;
			NumberFormatOptions.MaximumFractionalDigits = 2;
			TextValue = ConcatenateLine(TextValue, FText::Format(LOCTEXT("SimulationTime", "Simulation Time: {0}ms"), FText::AsNumber(SimulationTime, &NumberFormatOptions)));
		}
		if (SimProxy->IsTeleported())
		{
			TextValue = ConcatenateLine(TextValue, LOCTEXT("IsTeleported", "Simulation Teleport Activated"));
		}
	}
	return TextValue;
}

struct FVisualizationOption
{
	// Actual option entries
	static const FVisualizationOption OptionData[];
	static const uint32 Count;

	// Chaos debug draw function
	typedef void (::Chaos::FClothVisualization::* FClothVisualizationDebugDrawFunction)(FPrimitiveDrawInterface*) const;
	typedef void (::Chaos::FClothVisualization::* FClothVisualizationDebugDrawTextsFunction)(FCanvas*, const FSceneView*) const;
	typedef FText (*FLocalDebugDisplayStringFunction)(const FClothSimulationProxy*);
	FClothVisualizationDebugDrawFunction ClothVisualizationDebugDrawFunction;
	FClothVisualizationDebugDrawTextsFunction ClothVisualizationDebugDrawTextsFunction;
	FLocalDebugDisplayStringFunction LocalDebugDisplayStringFunction;

	FText DisplayName;         // Text for menu entries.
	FText ToolTip;             // Text for menu tooltips.
	bool bDisablesSimulation;  // Whether or not this option requires the simulation to be disabled.
	bool bHidesClothSections;  // Hides the cloth section to avoid zfighting with the debug geometry.

	// Console override
	IConsoleVariable* const ConsoleVariable;

	FVisualizationOption(FClothVisualizationDebugDrawFunction InDebugDrawFunction, const TCHAR* ConsoleName, const FText& InDisplayName, const FText& InToolTip, bool bInDisablesSimulation = false, bool bInHidesClothSections = false)
		: ClothVisualizationDebugDrawFunction(InDebugDrawFunction)
		, DisplayName(InDisplayName)
		, ToolTip(InToolTip)
		, bDisablesSimulation(bInDisablesSimulation)
		, bHidesClothSections(bInHidesClothSections)
		, ConsoleVariable(IConsoleManager::Get().RegisterConsoleVariable(ConsoleName, false, *InToolTip.ToString(), 0))
	{}

	FVisualizationOption(FLocalDebugDisplayStringFunction InLocalDebugDisplayStringFunction, const TCHAR* ConsoleName, const FText& InDisplayName, const FText& InToolTip, bool bInDisablesSimulation = false, bool bInHidesClothSections = false)
		: LocalDebugDisplayStringFunction(InLocalDebugDisplayStringFunction)
		, DisplayName(InDisplayName)
		, ToolTip(InToolTip)
		, bDisablesSimulation(bInDisablesSimulation)
		, bHidesClothSections(bInHidesClothSections)
		, ConsoleVariable(IConsoleManager::Get().RegisterConsoleVariable(ConsoleName, false, *InToolTip.ToString(), 0))
	{}

	FVisualizationOption(FClothVisualizationDebugDrawTextsFunction InDebugDrawTextsFunction, const TCHAR * ConsoleName, const FText & InDisplayName, const FText & InToolTip, bool bInDisablesSimulation = false, bool bInHidesClothSections = false)
		: ClothVisualizationDebugDrawTextsFunction(InDebugDrawTextsFunction)
		, DisplayName(InDisplayName)
		, ToolTip(InToolTip)
		, bDisablesSimulation(bInDisablesSimulation)
		, bHidesClothSections(bInHidesClothSections)
		, ConsoleVariable(IConsoleManager::Get().RegisterConsoleVariable(ConsoleName, false, *InToolTip.ToString(), 0))
	{}

	~FVisualizationOption()
	{
		IConsoleManager::Get().UnregisterConsoleObject(ConsoleVariable);
	}

	bool IsConsoleVariableEnabled() const { return ConsoleVariable->AsVariableBool()->GetValueOnGameThread(); }
};

const FVisualizationOption FVisualizationOption::OptionData[] =
{
	FVisualizationOption(&::Chaos::FClothVisualization::DrawPhysMeshShaded, TEXT("p.ChaosClothAssetEditor.DebugDrawPhysMeshShaded"), LOCTEXT("ChaosVisName_PhysMesh", "Physical Mesh (Flat Shaded)"), LOCTEXT("ChaosVisName_PhysMeshShaded_ToolTip", "Draws the current physical result as a doubled sided flat shaded mesh"), /*bDisablesSimulation =*/false, /*bHidesClothSections=*/true),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawPhysMeshWired, TEXT("p.ChaosClothAssetEditor.DebugDrawPhysMeshWired"), LOCTEXT("ChaosVisName_PhysMeshWire", "Physical Mesh (Wireframe)"), LOCTEXT("ChaosVisName_PhysMeshWired_ToolTip", "Draws the current physical mesh result in wireframe")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawAnimMeshWired, TEXT("p.ChaosClothAssetEditor.DebugDrawAnimMeshWired"), LOCTEXT("ChaosVisName_AnimMeshWire", "Animated Mesh (Wireframe)"), LOCTEXT("ChaosVisName_AnimMeshWired_ToolTip", "Draws the current animated mesh input in wireframe")),
	FVisualizationOption((FLocalDebugDisplayStringFunction)nullptr, TEXT("p.ChaosClothAssetEditor.DebugDrawHideRenderMesh"), LOCTEXT("ChaosVisName_HideRenderMesh", "Hide Render Mesh"), LOCTEXT("ChaosVisName_HideRenderMesh_ToolTip", "Hide the render mesh."), /*bDisablesSimulation =*/false, /*bHidesClothSections=*/true),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawParticleIndices, TEXT("p.ChaosClothAssetEditor.DebugDrawParticleIndices"), LOCTEXT("ChaosVisName_ParticleIndices", "Particle Indices"), LOCTEXT("ChaosVisName_ParticleIndices_ToolTip", "Draws the particle indices as instantiated by the solver")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawElementIndices, TEXT("p.ChaosClothAssetEditor.DebugDrawElementIndices"), LOCTEXT("ChaosVisName_ElementIndices", "Element Indices"), LOCTEXT("ChaosVisName_ElementIndices_ToolTip", "Draws the element's (triangle or other) indices as instantiated by the solver")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawPointNormals , TEXT("p.ChaosClothAssetEditor.DebugDrawPointNormals"), LOCTEXT("ChaosVisName_PointNormals", "Physical Mesh Normals"), LOCTEXT("ChaosVisName_PointNormals_ToolTip", "Draws the current point normals for the simulation mesh")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawPointVelocities, TEXT("p.ChaosClothAssetEditor.DebugDrawPointVelocities"), LOCTEXT("ChaosVisName_PointVelocities", "Point Velocities"), LOCTEXT("ChaosVisName_PointVelocities_ToolTip", "Draws the current point velocities for the simulation mesh")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawAnimNormals, TEXT("p.ChaosClothAssetEditor.DebugDrawAnimNormals"), LOCTEXT("ChaosVisName_AnimNormals", "Animated Mesh Normals"), LOCTEXT("ChaosVisName_AnimNormals_ToolTip", "Draws the current point normals for the animated mesh")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawCollision, TEXT("p.ChaosClothAssetEditor.DebugDrawCollision"), LOCTEXT("ChaosVisName_Collision", "Collisions"), LOCTEXT("ChaosVisName_Collision_ToolTip", "Draws the collision bodies the simulation is currently using")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawBackstops , TEXT("p.ChaosClothAssetEditor.DebugDrawBackstops"), LOCTEXT("ChaosVisName_Backstop", "Backstops"), LOCTEXT("ChaosVisName_Backstop_ToolTip", "Draws the backstop radius and position for each simulation particle")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawBackstopDistances, TEXT("p.ChaosClothAssetEditor.DebugDrawBackstopDistances"), LOCTEXT("ChaosVisName_BackstopDistance", "Backstop Distances"), LOCTEXT("ChaosVisName_BackstopDistance_ToolTip", "Draws the backstop distance offset for each simulation particle"), /*bDisablesSimulation =*/true),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawMaxDistances, TEXT("p.ChaosClothAssetEditor.DebugDrawMaxDistances"), LOCTEXT("ChaosVisName_MaxDistance", "Max Distances"), LOCTEXT("ChaosVisName_MaxDistance_ToolTip", "Draws the current max distances for the sim particles as a line along its normal"), /*bDisablesSimulation =*/true),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawMaxDistanceValues, TEXT("p.ChaosClothAssetEditor.DebugDrawMaxDistanceValues"), LOCTEXT("ChaosVisName_MaxDistanceValue", "Max Distances As Numbers"), LOCTEXT("ChaosVisName_MaxDistanceValue_ToolTip", "Draws the current max distances as numbers")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawAnimDrive, TEXT("p.ChaosClothAssetEditor.DebugDrawAnimDrive"), LOCTEXT("ChaosVisName_AnimDrive", "Anim Drive"), LOCTEXT("ChaosVisName_AnimDrive_Tooltip", "Draws the current skinned reference mesh for the simulation which anim drive will attempt to reach if enabled")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawEdgeConstraint, TEXT("p.ChaosClothAssetEditor.DebugDrawEdgeConstraint"), LOCTEXT("ChaosVisName_EdgeConstraint", "Edge Constraint"), LOCTEXT("ChaosVisName_EdgeConstraint_Tooltip", "Draws the edge spring constraints")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawBendingConstraint, TEXT("p.ChaosClothAssetEditor.DebugDrawBendingConstraint"), LOCTEXT("ChaosVisName_BendingConstraint", "Bending Constraint"), LOCTEXT("ChaosVisName_BendingConstraint_Tooltip", "Draws the bending spring constraints")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawLongRangeConstraint, TEXT("p.ChaosClothAssetEditor.DebugDrawLongRangeConstraint"), LOCTEXT("ChaosVisName_LongRangeConstraint" , "Long Range Constraint"), LOCTEXT("ChaosVisName_LongRangeConstraint_Tooltip", "Draws the long range attachment constraint distances")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawWindAndPressureForces, TEXT("p.ChaosClothAssetEditor.DebugDrawWindAndPressureForces"), LOCTEXT("ChaosVisName_WindAndPressureForces", "Wind Aerodynamic And Pressure Forces"), LOCTEXT("ChaosVisName_WindAndPressure_Tooltip", "Draws the Wind drag and lift and pressure forces")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawLocalSpace, TEXT("p.ChaosClothAssetEditor.DebugDrawLocalSpace"), LOCTEXT("ChaosVisName_LocalSpace", "Local Space Reference Bone"), LOCTEXT("ChaosVisName_LocalSpace_Tooltip", "Draws the local space reference bone")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawGravity, TEXT("p.ChaosClothAssetEditor.DebugDrawGravity"), LOCTEXT("ChaosVisName_Gravity", "Gravity"), LOCTEXT("ChaosVisName_Gravity_Tooltip", "Draws gravity")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawFictitiousAngularForces, TEXT("p.ChaosClothAssetEditor.DebugDrawFictitiousAngularForces"), LOCTEXT("ChaosVisName_FictitiousAngularForces", "Fictitious Angular Forces"), LOCTEXT("ChaosVisName_Gravity_FictitiousAngularForces", "Draws fictitious angular forces (force based solver only)")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawSelfCollision, TEXT("p.ChaosClothAssetEditor.DebugDrawSelfCollision"), LOCTEXT("ChaosVisName_SelfCollision"       , "Self Collision"), LOCTEXT("ChaosVisName_SelfCollision_Tooltip", "Draws the self collision thickness/debugging information")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawSelfIntersection, TEXT("p.ChaosClothAssetEditor.DebugDrawSelfIntersection"), LOCTEXT("ChaosVisName_SelfIntersection", "Self Intersection"), LOCTEXT("ChaosVisName_SelfIntersection_Tooltip", "Draws the self intersection contour/region information")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawSelfCollisionLayers, TEXT("p.ChaosClothAssetEditor.DebugDrawSelfCollisionLayers"), LOCTEXT("ChaosVisName_SelfCollisionLayers", "Self Collision Layers"), LOCTEXT("ChaosVisName_SelfCollisionLayers_Tooltip", "Draws the self collision layers"), /*bDisablesSimulation =*/false, /*bHidesClothSections=*/true),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawSelfCollisionThickness, TEXT("p.ChaosClothAssetEditor.DebugDrawSelfCollisionThickness"), LOCTEXT("ChaosVisName_SelfCollisionThickness", "Self Collision Thickness"), LOCTEXT("ChaosVisName_SelfCollisionThickness_Tooltip", "Draws the self collision Thickness")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawKinematicColliderShaded, TEXT("p.ChaosClothAssetEditor.DebugDrawKinematicColliderShaded"), LOCTEXT("ChaosVisName_DrawKinematicColliderShaded", "Draw Kinematic Colliders (Shaded)"), LOCTEXT("ChaosVisName_DrawKinematicColliderShaded_Tooltip", "Draw kinematic cloth colliders with flat shading.")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawKinematicColliderWired, TEXT("p.ChaosClothAssetEditor.DebugDrawKinematicColliderWired"), LOCTEXT("ChaosVisName_DrawKinematicColliderWired", "Draw Kinematic Colliders (Wireframe)"), LOCTEXT("ChaosVisName_DrawKinematicColliderWired_Tooltip", "Draw kinematic cloth colliders in wireframe.")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawMultiResConstraint, TEXT("p.ChaosClothAssetEditor.DebugDrawMultiResConstraint"), LOCTEXT("ChaosVisName_DrawMultiResConstraint", "Draw Multi Res Constraint"), LOCTEXT("ChaosVisName_DrawMultiResConstraint_Tooltip", "Draw multi res constraint coarse mesh and targets.")),
	FVisualizationOption(&GetSimulationStatisticsString, TEXT("p.ChaosClothAssetEditor.DebugDrawSimulationStatistics"), LOCTEXT("ChaosVisName_SimulationStatistics"    , "Simulation Statistics"), LOCTEXT("ChaosVisName_SimulationStatistics_Tooltip", "Displays simulation statistics")),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawWeightMap, TEXT("p.ChaosClothAssetEditor.DrawWeightMap"), LOCTEXT("ChaosVisName_DrawWeightMap", "Weight Map"), LOCTEXT("ChaosVisName_DrawWeightMap_ToolTip", "Draw the weight map for the simulation mesh. You can control the name of the map to be visualized by setting the p.ChaosClothVisualization.WeightMapName console variable."), /*bDisablesSimulation =*/false, /*bHidesClothSections=*/true),
	FVisualizationOption(&::Chaos::FClothVisualization::DrawInpaintWeightsMatched, TEXT("p.ChaosClothAssetEditor.DrawInpaintWeightsMatched"), LOCTEXT("ChaosVisName_DrawInpaintWeightsMatched"        , "Transfer Skin Weights Node: Matched Vertices"), LOCTEXT("ChaosVisName_DrawInpaintWeightsMatched_ToolTip", "When transferring weights using the InpaintWeights method, will highlight the vertices for which we copied the weights directly from the source mesh. For all other vertices, the weights were computed automatically."), /*bDisablesSimulation =*/false, /*bHidesClothSections=*/true),
};
const uint32 FVisualizationOption::Count = sizeof(OptionData) / sizeof(FVisualizationOption);



} // namespace Private

FClothEditorSimulationVisualization::FClothEditorSimulationVisualization()
	: Flags(false, Private::FVisualizationOption::Count)
{
}

void FClothEditorSimulationVisualization::ExtendViewportShowMenu(FMenuBuilder& MenuBuilder, TSharedRef<FChaosClothAssetEditor3DViewportClient> ViewportClient)
{
	MenuBuilder.BeginSection(TEXT("ChaosSimulation_Visualizations"), LOCTEXT("VisualizationSection", "Visualizations"));
	{
		for (uint32 OptionIndex = 0; OptionIndex < Private::FVisualizationOption::Count; ++OptionIndex)
		{
			// Handler for visualization entry being clicked
			const FExecuteAction ExecuteAction = FExecuteAction::CreateLambda([this, OptionIndex, ViewportClient]()
			{
				Flags[OptionIndex] = !Flags[OptionIndex];

				if (UChaosClothComponent* const ClothComponent = ViewportClient->GetPreviewClothComponent())
				{
					// If we need to toggle the disabled or visibility states, handle it
					// Disable simulation
					const bool bShouldDisableSimulation = ShouldDisableSimulation();
					if (ClothComponent->IsSimulationEnabled() == bShouldDisableSimulation)
					{
						ClothComponent->SetEnableSimulation(!bShouldDisableSimulation);
					}
					// Hide cloth section
					if (Private::FVisualizationOption::OptionData[OptionIndex].bHidesClothSections)
					{
						const bool bIsClothSectionsVisible = !Flags[OptionIndex];
						ShowClothSections(ClothComponent, bIsClothSectionsVisible);
					}
				}
			});

			// Checkstate function for visualization entries
			const FIsActionChecked IsActionChecked = FIsActionChecked::CreateLambda([this, OptionIndex]()
			{
				return Flags[OptionIndex];
			});

			const FUIAction Action(ExecuteAction, FCanExecuteAction(), IsActionChecked);

			MenuBuilder.AddMenuEntry(Private::FVisualizationOption::OptionData[OptionIndex].DisplayName, Private::FVisualizationOption::OptionData[OptionIndex].ToolTip, FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::ToggleButton);
		}
	}
	MenuBuilder.EndSection();
}

void FClothEditorSimulationVisualization::DebugDrawSimulation(const UChaosClothComponent* ClothComponent, FPrimitiveDrawInterface* PDI)
{
	const ::Chaos::FClothVisualization* const Visualization = ClothComponent && ClothComponent->GetClothSimulationProxy() ? ClothComponent->GetClothSimulationProxy()->GetVisualization() : nullptr;
	if (!Visualization)
	{
		return;
	}

	for (int32 OptionIndex = 0; OptionIndex < Private::FVisualizationOption::Count; ++OptionIndex)
	{
		if (Private::FVisualizationOption::OptionData[OptionIndex].ClothVisualizationDebugDrawFunction &&
			(Private::FVisualizationOption::OptionData[OptionIndex].IsConsoleVariableEnabled() || Flags[OptionIndex]))
		{
			(Visualization->*(Private::FVisualizationOption::OptionData[OptionIndex].ClothVisualizationDebugDrawFunction))(PDI);
		}
	}
}

void FClothEditorSimulationVisualization::DebugDrawSimulationTexts(const UChaosClothComponent* ClothComponent, FCanvas* Canvas, const FSceneView* SceneView)
{
	const ::Chaos::FClothVisualization* const Visualization = ClothComponent && ClothComponent->GetClothSimulationProxy() ? ClothComponent->GetClothSimulationProxy()->GetVisualization() : nullptr;
	if (!Visualization)
	{
		return;
	}
	for (int32 OptionIndex = 0; OptionIndex < Private::FVisualizationOption::Count; ++OptionIndex)
	{
		if (Private::FVisualizationOption::OptionData[OptionIndex].ClothVisualizationDebugDrawTextsFunction &&
			(Private::FVisualizationOption::OptionData[OptionIndex].IsConsoleVariableEnabled() || Flags[OptionIndex]))
		{
			(Visualization->*(Private::FVisualizationOption::OptionData[OptionIndex].ClothVisualizationDebugDrawTextsFunction))(Canvas, SceneView);
		}
	}
}

FText FClothEditorSimulationVisualization::GetDisplayString(const UChaosClothComponent* ClothComponent) const
{
	const FClothSimulationProxy* const SimProxy = ClothComponent ? ClothComponent->GetClothSimulationProxy() : nullptr;
	if (!SimProxy)
	{
		return FText();
	}

	FText DisplayString;
	for (int32 OptionIndex = 0; OptionIndex < Private::FVisualizationOption::Count; ++OptionIndex)
	{
		if (Private::FVisualizationOption::OptionData[OptionIndex].LocalDebugDisplayStringFunction &&
			(Private::FVisualizationOption::OptionData[OptionIndex].IsConsoleVariableEnabled() || Flags[OptionIndex]))
		{
			DisplayString = Private::ConcatenateLine(DisplayString, (Private::FVisualizationOption::OptionData[OptionIndex].LocalDebugDisplayStringFunction)(SimProxy));
		}
	}
	return DisplayString;
}

bool FClothEditorSimulationVisualization::ShouldDisableSimulation() const
{
	for (uint32 OptionIndex = 0; OptionIndex < Private::FVisualizationOption::Count; ++OptionIndex)
	{
		if (Flags[OptionIndex])
		{
			const Private::FVisualizationOption& Data = Private::FVisualizationOption::OptionData[OptionIndex];

			if (Data.bDisablesSimulation)
			{
				return true;
			}
		}
	}
	return false;
}

void FClothEditorSimulationVisualization::ShowClothSections(UChaosClothComponent* ClothComponent, bool bIsClothSectionsVisible) const
{
	if (FSkeletalMeshRenderData* const SkeletalMeshRenderData = ClothComponent->GetSkeletalMeshRenderData())
	{
		for (int32 LODIndex = 0; LODIndex < SkeletalMeshRenderData->LODRenderData.Num(); ++LODIndex)
		{
			FSkeletalMeshLODRenderData& SkeletalMeshLODRenderData = SkeletalMeshRenderData->LODRenderData[LODIndex];

			for (int32 SectionIndex = 0; SectionIndex < SkeletalMeshLODRenderData.RenderSections.Num(); ++SectionIndex)
			{
				FSkelMeshRenderSection& SkelMeshRenderSection = SkeletalMeshLODRenderData.RenderSections[SectionIndex];

				if (SkelMeshRenderSection.HasClothingData())
				{
					ClothComponent->ShowMaterialSection(SkelMeshRenderSection.MaterialIndex, SectionIndex, bIsClothSectionsVisible, LODIndex);
				}
			}
		}
	}
}
} // namespace UE::Chaos::ClothAsset

#undef LOCTEXT_NAMESPACE
