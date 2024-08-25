// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosCloth/ChaosClothVisualization.h"
#if CHAOS_DEBUG_DRAW
#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "ChaosCloth/ChaosClothingSimulationMesh.h"
#include "ChaosCloth/ChaosClothingSimulationCloth.h"
#include "ChaosCloth/ChaosClothingSimulationCollider.h"
#include "ChaosCloth/ChaosWeightMapTarget.h"
#include "Chaos/DebugDrawQueue.h"
#include "Chaos/Capsule.h"
#include "Chaos/Convex.h"
#include "Chaos/Levelset.h"
#include "Chaos/Sphere.h"
#include "Chaos/TaperedCapsule.h"
#include "Chaos/TaperedCylinder.h"
#include "Chaos/Triangle.h"
#include "Chaos/VelocityField.h"
#include "Chaos/PBDAnimDriveConstraint.h"
#include "Chaos/PBDBendingConstraints.h"
#include "Chaos/PBDCollisionSpringConstraints.h"
#include "Chaos/PBDLongRangeConstraints.h"
#include "Chaos/PBDSelfCollisionSphereConstraints.h"
#include "Chaos/PBDSphericalConstraint.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/PBDTriangleMeshCollisions.h"
#include "Chaos/SoftsExternalForces.h"
#include "Chaos/SoftsMultiResConstraints.h"
#include "Chaos/XPBDBendingConstraints.h"
#include "Chaos/XPBDSpringConstraints.h"
#include "Chaos/XPBDAnisotropicBendingConstraints.h"
#include "Chaos/XPBDAnisotropicSpringConstraints.h"
#include "Chaos/XPBDStretchBiasElementConstraints.h"
#include "Chaos/WeightedLatticeImplicitObject.h"
#include "DynamicMeshBuilder.h"
#include "Engine/EngineTypes.h"
#include "SceneManagement.h"
#include "SceneView.h"
#if WITH_EDITOR
#include "Materials/Material.h"
#include "Engine/Canvas.h"  // For draw text
#include "CanvasItem.h"     //
#include "Engine/Engine.h"  //
#include "UObject/ICookInfo.h"
#endif  // #if WITH_EDITOR

namespace Chaos
{

namespace Private
{
static int DrawSkinnedLattice = 0;
static FAutoConsoleVariableRef CVarClothVizDrawSkinnedLattice(TEXT("p.ChaosClothVisualization.DrawSkinnedLattice"), DrawSkinnedLattice, TEXT("Draw skinned lattice, 0 = none, 1 = filled, 2 = empty, 3 = both"));

// TODO: move these options to be somewhere the new cloth editor visualization can use.
enum class EBendingDrawMode : int
{
	BuckleStatus = 0,
	ParallelGraphColor = 1,
	Anisotropy = 2,
	RestAngle = 3
};
static int32 BendingDrawMode = (int32)EBendingDrawMode::BuckleStatus;
static FAutoConsoleVariableRef CVarClothVizBendDrawMode(TEXT("p.ChaosClothVisualization.BendingDrawMode"), BendingDrawMode, TEXT("Bending draw mode, 0 = BuckleStatus, 1 = Parallel graph color, 2 = Anisotropy, 3 = RestAngle"));

enum class EStretchBiasDrawMode : int
{
	ParallelGraphColor = 0,
	WarpStretch = 1,
	WeftStretch = 2,
	BiasStretch = 3
};
static int32 StretchBiasDrawMode = (int32)EStretchBiasDrawMode::ParallelGraphColor;
static FAutoConsoleVariableRef CVarClothVizStretchBiasDrawMode(TEXT("p.ChaosClothVisualization.StretchBiasDrawMode"), StretchBiasDrawMode, TEXT("Stretch draw mode, 0 = Parallel graph color, 1 = Warp Stretch, 2 = Weft Stretch, 3 = BiasStretch"));
static float StretchBiasDrawRangeMin = -1.f;
static float StretchBiasDrawRangeMax = 1.f;
static FAutoConsoleVariableRef CVarClothVizStretchBiasDrawRangeMin(TEXT("p.ChaosClothVisualization.StretchBiasDrawRangeMin"), StretchBiasDrawRangeMin, TEXT("Min stretch in draw color range. Negative = compressed, 0 = undeformed, positive = stretched. (When drawing warp/weft stretch)"));
static FAutoConsoleVariableRef CVarClothVizStretchBiasDrawRangeMax(TEXT("p.ChaosClothVisualization.StretchBiasDrawRangeMax"), StretchBiasDrawRangeMax, TEXT("Max stretch in draw color range. Negative = compressed, 0 = undeformed, positive = stretched. (When drawing warp/weft stretch)"));
static bool bStretchBiasDrawOutOfRange = true;
static FAutoConsoleVariableRef CVarClothVizStretchBiasDrawOutOfRange(TEXT("p.ChaosClothVisualization.StretchBiasDrawOutOfRange"), bStretchBiasDrawOutOfRange, TEXT("Draw out of range elements (When drawing warp/weft stretch)"));

enum class EAnisoSpringDrawMode : int
{
	ParallelGraphColor = 0,
	Anisotropy = 1,
};
static int32 AnisoSpringDrawMode = (int32)EAnisoSpringDrawMode::ParallelGraphColor;
static FAutoConsoleVariableRef CVarClothVizAnisoSpringDrawMode(TEXT("p.ChaosClothVisualization.AnisoSpringDrawMode"), AnisoSpringDrawMode, TEXT("Stretch draw mode, 0 = Parallel graph color, 1 = Anisotropy"));

static FString WeightMapName = "";
static FAutoConsoleVariableRef CVarClothVizWeightMapName(TEXT("p.ChaosClothVisualization.WeightMapName"), WeightMapName, TEXT("Weight map name to be visualized"));

// copied from ClothEditorMode
FLinearColor PseudoRandomColor(int32 NumColorRotations)
{
	constexpr uint8 Spread = 157;  // Prime number that gives a good spread of colors without getting too similar as a rand might do.
	uint8 Seed = Spread;
	NumColorRotations = FMath::Abs(NumColorRotations);
	for (int32 Rotation = 0; Rotation < NumColorRotations; ++Rotation)
	{
		Seed += Spread;
	}
	return FLinearColor::MakeFromHSV8(Seed, 180, 140);
}
}// namespace Private

	FClothVisualization::FClothVisualization(const ::Chaos::FClothingSimulationSolver* InSolver)
		: Solver(InSolver)
	{
#if WITH_EDITOR
		FCookLoadScope CookLoadScope(ECookLoadType::EditorOnly);
		ClothMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EditorMaterials/Cloth/CameraLitDoubleSided.CameraLitDoubleSided"), nullptr, LOAD_None, nullptr);  // LOAD_EditorOnly
		ClothMaterialColor = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EditorMaterials/Cloth/CameraLitVertexColor.CameraLitVertexColor"), nullptr, LOAD_None, nullptr);  // LOAD_EditorOnly
		ClothMaterialVertex = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EditorMaterials/WidgetVertexColorMaterial"), nullptr, LOAD_None, nullptr);  // LOAD_EditorOnly
		CollisionMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EditorMaterials/PhAT_UnselectedMaterial"), nullptr, LOAD_None, nullptr);
#endif  // #if WITH_EDITOR
	}

	FClothVisualization::~FClothVisualization() = default;

	void FClothVisualization::SetSolver(const ::Chaos::FClothingSimulationSolver* InSolver)
	{
		Solver = InSolver;
	}

#if WITH_EDITOR
	void FClothVisualization::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(ClothMaterial);
		Collector.AddReferencedObject(ClothMaterialColor);
		Collector.AddReferencedObject(ClothMaterialVertex);
		Collector.AddReferencedObject(CollisionMaterial);
	}

	void FClothVisualization::DrawPhysMeshShaded(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver || !ClothMaterial)
		{
			return;
		}

		FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
		int32 VertexIndex = 0;

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			// Elements are local indexed for new solver
			const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;

			const TConstArrayView<TVec3<int32>> Elements = Cloth->GetTriangleMesh(Solver).GetElements();
			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
			const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver);
			check(InvMasses.Num() == Positions.Num());

			for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex, VertexIndex += 3)
			{
				const TVec3<int32>& Element = Elements[ElementIndex];

				const FVector3f Pos0(Positions[Element.X - Offset]); // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
				const FVector3f Pos1(Positions[Element.Y - Offset]);
				const FVector3f Pos2(Positions[Element.Z - Offset]);

				const FVector3f Normal = FVector3f::CrossProduct(Pos2 - Pos0, Pos1 - Pos0).GetSafeNormal();
				const FVector3f Tangent = ((Pos1 + Pos2) * 0.5f - Pos0).GetSafeNormal();

				const bool bIsKinematic0 = (InvMasses[Element.X - Offset] == (Softs::FSolverReal)0.);
				const bool bIsKinematic1 = (InvMasses[Element.Y - Offset] == (Softs::FSolverReal)0.);
				const bool bIsKinematic2 = (InvMasses[Element.Z - Offset] == (Softs::FSolverReal)0.);

				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos0, Tangent, Normal, FVector2f(0.f, 0.f), bIsKinematic0 ? FColor::Purple : FColor::White));
				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos1, Tangent, Normal, FVector2f(0.f, 1.f), bIsKinematic1 ? FColor::Purple : FColor::White));
				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos2, Tangent, Normal, FVector2f(1.f, 1.f), bIsKinematic2 ? FColor::Purple : FColor::White));
				MeshBuilder.AddTriangle(VertexIndex, VertexIndex + 1, VertexIndex + 2);
			}
		}

		FMatrix LocalSimSpaceToWorld(FMatrix::Identity);
		LocalSimSpaceToWorld.SetOrigin(Solver->GetLocalSpaceLocation());
		MeshBuilder.Draw(PDI, LocalSimSpaceToWorld, ClothMaterial->GetRenderProxy(), SDPG_World, false, false);
	}

	static void DrawText(FCanvas* Canvas, const FSceneView* SceneView, const FVector& Pos, const FText& Text, const FLinearColor& Color)
	{
		FVector2D PixelLocation;
		if (SceneView->WorldToPixel(Pos, PixelLocation))
		{
			FCanvasTextItem TextItem(PixelLocation, Text, GEngine->GetSmallFont(), Color);
			TextItem.Scale = FVector2D::UnitVector;
			TextItem.EnableShadow(FLinearColor::Black);
			TextItem.Draw(Canvas);
		}
	}

	void FClothVisualization::DrawParticleIndices(FCanvas* Canvas, const FSceneView* SceneView) const
	{
		if (!Solver)
		{
			return;
		}

		static const FLinearColor DynamicColor = FColor::White;
		static const FLinearColor KinematicColor = FColor::Purple;

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			const int32 Offset = Solver->GetGlobalParticleOffset(ParticleRangeId);

			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
			const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver);
			check(InvMasses.Num() == Positions.Num());

			for (int32 Index = 0; Index < Positions.Num(); ++Index)
			{
				const FVector Position = LocalSpaceLocation + FVector(Positions[Index]);

				const FText Text = FText::AsNumber(Offset + Index);
				DrawText(Canvas, SceneView, Position, Text, InvMasses[Index] == (Softs::FSolverReal)0. ? KinematicColor : DynamicColor);
			}
		}
	}

	void FClothVisualization::DrawElementIndices(FCanvas* Canvas, const FSceneView* SceneView) const
	{
		if (!Solver)
		{
			return;
		}

		static const FLinearColor DynamicColor = FColor::White;
		static const FLinearColor KinematicColor = FColor::Purple;

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			// Elements are local indexed for new solver
			const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;

			const TArray<TVec3<int32>>& Elements = Cloth->GetTriangleMesh(Solver).GetElements();
			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
			const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver);
			check(InvMasses.Num() == Positions.Num());

			for (int32 Index = 0; Index < Elements.Num(); ++Index)
			{
				const TVec3<int32>& Element = Elements[Index];
				const FVector Position = LocalSpaceLocation + (
					FVector(Positions[Element[0] - Offset]) +
					FVector(Positions[Element[1] - Offset]) +
					FVector(Positions[Element[2] - Offset])) / (FReal)3.;

				const bool bIsKinematic0 = (InvMasses[Element.X - Offset] == (Softs::FSolverReal)0.);
				const bool bIsKinematic1 = (InvMasses[Element.Y - Offset] == (Softs::FSolverReal)0.);
				const bool bIsKinematic2 = (InvMasses[Element.Z - Offset] == (Softs::FSolverReal)0.);
				const FLinearColor& Color = (bIsKinematic0 && bIsKinematic1 && bIsKinematic2) ? KinematicColor : DynamicColor;
				const FText Text = FText::AsNumber(Index);
				DrawText(Canvas, SceneView, Position, Text, Color);
			}
		}
	}

	void FClothVisualization::DrawMaxDistanceValues(FCanvas* Canvas, const FSceneView* SceneView) const
	{
		if (!Solver)
		{
			return;
		}

		static const FLinearColor DynamicColor = FColor::White;
		static const FLinearColor KinematicColor = FColor::Purple;

		FNumberFormattingOptions NumberFormattingOptions;
		NumberFormattingOptions.AlwaysSign = false;
		NumberFormattingOptions.UseGrouping = false;
		NumberFormattingOptions.RoundingMode = ERoundingMode::HalfFromZero;
		NumberFormattingOptions.MinimumIntegralDigits = 1;
		NumberFormattingOptions.MaximumIntegralDigits = 6;
		NumberFormattingOptions.MinimumFractionalDigits = 2;
		NumberFormattingOptions.MaximumFractionalDigits = 2;

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			if (Cloth->GetParticleRangeId(Solver) == INDEX_NONE)
			{
				continue;
			}

			const TConstArrayView<FRealSingle>& MaxDistances = Cloth->GetWeightMapByProperty(Solver, TEXT("MaxDistance"));
			if (!MaxDistances.Num())
			{
				continue;
			}

			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetAnimationPositions(Solver);
			const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver);
			check(MaxDistances.Num() == Positions.Num());
			check(MaxDistances.Num() == InvMasses.Num());

			for (int32 Index = 0; Index < MaxDistances.Num(); ++Index)
			{
				const FReal MaxDistance = MaxDistances[Index];
				const FVector Position = LocalSpaceLocation + FVector(Positions[Index]);

				const FText Text = FText::AsNumber(MaxDistance, &NumberFormattingOptions);
				DrawText(Canvas, SceneView, Position, Text, InvMasses[Index] == (Softs::FSolverReal)0. ? KinematicColor : DynamicColor);
			}
		}
	}

	void FClothVisualization::DrawWeightMapWithName(FPrimitiveDrawInterface* PDI, const FString& Name) const
	{
		if (!Solver || !ClothMaterialColor)
		{
			return;
		}

		FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
		int32 VertexIndex = 0;

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			// Elements are local indexed for new solver
			const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;
			const TConstArrayView<TVec3<int32>> Elements = Cloth->GetTriangleMesh(Solver).GetElements();
			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
			const TConstArrayView<FRealSingle>& WeightMap = Cloth->GetWeightMapByName(Solver, Name);

			for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex, VertexIndex += 3)
			{
				const TVec3<int32>& Element = Elements[ElementIndex];

				const FVector3f Pos0(Positions[Element.X - Offset]);
				const FVector3f Pos1(Positions[Element.Y - Offset]);
				const FVector3f Pos2(Positions[Element.Z - Offset]);

				const FVector3f Normal = FVector3f::CrossProduct(Pos2 - Pos0, Pos1 - Pos0).GetSafeNormal();
				const FVector3f Tangent = ((Pos1 + Pos2) * 0.5f - Pos0).GetSafeNormal();

				FLinearColor VertexColor1 = FLinearColor::Black;
				FLinearColor VertexColor2 = FLinearColor::Black;
				FLinearColor VertexColor3 = FLinearColor::Black;

				if (!WeightMap.IsEmpty() && WeightMap.Num() == Positions.Num()) // if map with that name exists and not empty
				{
					const FRealSingle Value0(WeightMap[Element.X - Offset]);
					const FRealSingle Value1(WeightMap[Element.Y - Offset]);
					const FRealSingle Value2(WeightMap[Element.Z - Offset]);

					VertexColor1 = FLinearColor::LerpUsingHSV(FLinearColor::Black, FLinearColor::White, (float)Value0);
					VertexColor2 = FLinearColor::LerpUsingHSV(FLinearColor::Black, FLinearColor::White, (float)Value1);
					VertexColor3 = FLinearColor::LerpUsingHSV(FLinearColor::Black, FLinearColor::White, (float)Value2);
				}
	
				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos0, Tangent, Normal, FVector2f(0.f, 0.f), VertexColor1.ToFColor(true)));
				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos1, Tangent, Normal, FVector2f(0.f, 1.f), VertexColor2.ToFColor(true)));
				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos2, Tangent, Normal, FVector2f(1.f, 1.f), VertexColor3.ToFColor(true)));
				MeshBuilder.AddTriangle(VertexIndex, VertexIndex + 1, VertexIndex + 2);
			}
		}
		
		FMatrix LocalSimSpaceToWorld(FMatrix::Identity);
		LocalSimSpaceToWorld.SetOrigin(Solver->GetLocalSpaceLocation());
		MeshBuilder.Draw(PDI, LocalSimSpaceToWorld, ClothMaterialColor->GetRenderProxy(), SDPG_World, false, false);
	}

	void FClothVisualization::DrawWeightMap(FPrimitiveDrawInterface* PDI) const
	{
		DrawWeightMapWithName(PDI, Private::WeightMapName);
	}

	void FClothVisualization::DrawInpaintWeightsMatched(FPrimitiveDrawInterface* PDI) const
	{
		DrawWeightMapWithName(PDI, TEXT("_InpaintWeightMask"));
	}

	void FClothVisualization::DrawSelfCollisionLayers(FPrimitiveDrawInterface* PDI) const
	{

		if (!Solver || !ClothMaterialColor)
		{
			return;
		}

		FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
		int32 VertexIndex = 0;

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			// Elements are local indexed for new solver
			const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;
			const TConstArrayView<TVec3<int32>> Elements = Cloth->GetTriangleMesh(Solver).GetElements();
			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
			const TConstArrayView<int32>& WeightMap = Cloth->GetFaceIntMapByProperty(Solver, TEXT("SelfCollisionLayers"));

			for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex, VertexIndex += 3)
			{

				const TVec3<int32>& Element = Elements[ElementIndex];

				const FVector3f Pos0(Positions[Element.X - Offset]);
				const FVector3f Pos1(Positions[Element.Y - Offset]);
				const FVector3f Pos2(Positions[Element.Z - Offset]);

				const FVector3f Normal = FVector3f::CrossProduct(Pos2 - Pos0, Pos1 - Pos0).GetSafeNormal();
				const FVector3f Tangent = ((Pos1 + Pos2) * 0.5f - Pos0).GetSafeNormal();

				FLinearColor VertexColor1 = FLinearColor::Gray;
				FLinearColor VertexColor2 = FLinearColor::Gray;
				FLinearColor VertexColor3 = FLinearColor::Gray;

				if (!WeightMap.IsEmpty() && WeightMap.Num() == Elements.Num() && WeightMap[ElementIndex]!=INDEX_NONE) // if map with that name exists and not empty
				{
					VertexColor1 = VertexColor2 = VertexColor3 = Chaos::Private::PseudoRandomColor(WeightMap[ElementIndex]);
				}

				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos0, Tangent, Normal, FVector2f(0.f, 0.f), VertexColor1.ToFColor(true)));
				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos1, Tangent, Normal, FVector2f(0.f, 1.f), VertexColor2.ToFColor(true)));
				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos2, Tangent, Normal, FVector2f(1.f, 1.f), VertexColor3.ToFColor(true)));
				MeshBuilder.AddTriangle(VertexIndex, VertexIndex + 1, VertexIndex + 2);
			}
		}

		FMatrix LocalSimSpaceToWorld(FMatrix::Identity);
		LocalSimSpaceToWorld.SetOrigin(Solver->GetLocalSpaceLocation());
		MeshBuilder.Draw(PDI, LocalSimSpaceToWorld, ClothMaterialColor->GetRenderProxy(), SDPG_World, false, false);
	}
#endif  // #if WITH_EDITOR

	static void DrawPoint(FPrimitiveDrawInterface* PDI, const FVector& Pos, const FLinearColor& Color, const UMaterial* ClothMaterialVertex, const float Thickness = 1.f)  // Use color or material
	{
		if (!PDI)
		{
			FDebugDrawQueue::GetInstance().DrawDebugPoint(Pos, Color.ToFColor(true), false, KINDA_SMALL_NUMBER, SDPG_Foreground, Thickness);
			return;
		}
#if WITH_EDITOR
		if (ClothMaterialVertex)
		{
			const FMatrix& ViewMatrix = PDI->View->ViewMatrices.GetViewMatrix();
			const FVector XAxis = ViewMatrix.GetColumn(0); // Just using transpose here (orthogonal transform assumed)
			const FVector YAxis = ViewMatrix.GetColumn(1);
			DrawDisc(PDI, Pos, XAxis, YAxis, Color.ToFColor(true), 0.5f, 10, ClothMaterialVertex->GetRenderProxy(), SDPG_World);
		}
		else
		{
			PDI->DrawPoint(Pos, Color, Thickness, SDPG_World);
		}
#endif
	}

	static void DrawLine(FPrimitiveDrawInterface* PDI, const FVector& Pos0, const FVector& Pos1, const FLinearColor& Color)
	{
		if (!PDI)
		{
			FDebugDrawQueue::GetInstance().DrawDebugLine(Pos0, Pos1, Color.ToFColor(true), false, KINDA_SMALL_NUMBER, SDPG_Foreground, 0.f);
			return;
		}
#if WITH_EDITOR
		PDI->DrawLine(Pos0, Pos1, Color, SDPG_World, 0.0f, 0.001f);
#endif
	}

	static void DrawArc(FPrimitiveDrawInterface* PDI, const FVector& Base, const FVector& X, const FVector& Y, ::Chaos::FReal MinAngle, float MaxAngle, ::Chaos::FReal Radius, const FLinearColor& Color)
	{
		static const int32 Sections = 10;
		const FReal AngleStep = FMath::DegreesToRadians((MaxAngle - MinAngle) / (FReal)Sections);
		FReal CurrentAngle = FMath::DegreesToRadians(MinAngle);
		FVector LastVertex = Base + Radius * (FMath::Cos(CurrentAngle) * X + FMath::Sin(CurrentAngle) * Y);

		for(int32 i = 0; i < Sections; i++)
		{
			CurrentAngle += AngleStep;
			const FVector ThisVertex = Base + Radius * (FMath::Cos(CurrentAngle) * X + FMath::Sin(CurrentAngle) * Y);
			DrawLine(PDI, LastVertex, ThisVertex, Color);
			LastVertex = ThisVertex;
		}
	}

	static void DrawSphere(FPrimitiveDrawInterface* PDI, const ::Chaos::TSphere<::Chaos::FReal, 3>& Sphere, const FQuat& Rotation, const FVector& Position, const FLinearColor& Color)
	{
		const FReal Radius = Sphere.GetRadius();
		const FVec3 Center = Position + Rotation.RotateVector(Sphere.GetCenter());
		if (!PDI)
		{
			FDebugDrawQueue::GetInstance().DrawDebugSphere(Center, Radius, 12, Color.ToFColor(true), false, KINDA_SMALL_NUMBER, SDPG_Foreground, 0.f);
			return;
		}
#if WITH_EDITOR
		const FTransform Transform(Rotation, Center);
		DrawWireSphere(PDI, Transform, Color, Radius, 12, SDPG_World, 0.0f, 0.001f, false);
#endif
	}

	static void DrawBox(FPrimitiveDrawInterface* PDI, const ::Chaos::FAABB3& Box, const FQuat& Rotation, const FVector& Position, const FLinearColor& Color)
	{
		if (!PDI)
		{
			const FVec3 Center = Position + Rotation.RotateVector(Box.GetCenter());
			FDebugDrawQueue::GetInstance().DrawDebugBox(Center, Box.Extents() * 0.5f, Rotation, Color.ToFColor(true), false, KINDA_SMALL_NUMBER, SDPG_Foreground, 0.f);
			return;
		}
#if WITH_EDITOR
		const FMatrix BoxToWorld = FTransform(Rotation, Position).ToMatrixNoScale();
		DrawWireBox(PDI, BoxToWorld, FBox(Box.Min(), Box.Max()), Color, SDPG_World, 0.0f, 0.001f, false);
#endif
	}

	static void DrawCapsule(FPrimitiveDrawInterface* PDI, const ::Chaos::FCapsule& Capsule, const FQuat& Rotation, const FVector& Position, const FLinearColor& Color)
	{
		const FReal Radius = Capsule.GetRadius();
		const FReal HalfHeight = Capsule.GetHeight() * 0.5f + Radius;
		const FVec3 Center = Position + Rotation.RotateVector(Capsule.GetCenter());
		if (!PDI)
		{
			const FQuat Orientation = FQuat::FindBetweenNormals(FVec3::UpVector, Capsule.GetAxis());
			FDebugDrawQueue::GetInstance().DrawDebugCapsule(Center, HalfHeight, Radius, Rotation * Orientation, Color.ToFColor(true), false, KINDA_SMALL_NUMBER, SDPG_Foreground, 0.f);
			return;
		}
#if WITH_EDITOR
		const FVec3 Up = Capsule.GetAxis();
		FVec3 Forward, Right;
		Up.FindBestAxisVectors(Forward, Right);
		const FVector X = Rotation.RotateVector(Forward);
		const FVector Y = Rotation.RotateVector(Right);
		const FVector Z = Rotation.RotateVector(Up);
		DrawWireCapsule(PDI, Center, X, Y, Z, Color, Radius, HalfHeight, 12, SDPG_World, 0.0f, 0.001f, false);
#endif
	}

#if WITH_EDITOR
	static void AppendTaperedCylinderTriangles(FDynamicMeshBuilder& MeshBuilder, const FVector3f& Position1, const FVector3f& Position2, const FRealSingle Radius1, const FRealSingle Radius2, const int32 NumSides, const FLinearColor& Color)
	{
		const FQuat4f Q = (Position2 - Position1).ToOrientationQuat();
		const FVector3f I = Q.GetRightVector();
		const FVector3f J = Q.GetUpVector();
		const FVector3f K = Q.GetForwardVector();

		const FRealSingle AngleDelta = (FRealSingle)2. * (FRealSingle)PI / NumSides;
		int32 LastVertex1 = MeshBuilder.AddVertex(FDynamicMeshVertex(Position1 + I * Radius1, -K, I, FVector2f(0.f, 0.f), Color.ToFColor(true)));
		int32 LastVertex2 = MeshBuilder.AddVertex(FDynamicMeshVertex(Position2 + I * Radius2, -K, I, FVector2f(1.f, 0.f), Color.ToFColor(true))); 
		for (int32 SideIndex = 1; SideIndex <= NumSides; ++SideIndex)
		{
			const FRealSingle Angle = AngleDelta * FRealSingle(SideIndex);
			const FVector3f ArcPos = I * FMath::Cos(Angle) + J * FMath::Sin(Angle);
			
			const FVector3f Pos1 = Position1 + ArcPos * Radius1;
			const FVector3f Pos2 = Position2 + ArcPos * Radius2;
			const FVector3f Normal = (Pos1 - Position1).GetSafeNormal();

			const int32 Vertex1 = MeshBuilder.AddVertex(FDynamicMeshVertex(Pos1, -K, Normal, FVector2f(0.f, 0.f), Color.ToFColor(true)));
			const int32 Vertex2 = MeshBuilder.AddVertex(FDynamicMeshVertex(Pos2, -K, Normal, FVector2f(1.f, 0.f), Color.ToFColor(true)));
			MeshBuilder.AddTriangle(LastVertex1, LastVertex2, Vertex1);
			MeshBuilder.AddTriangle(LastVertex2, Vertex2, Vertex1);

			LastVertex1 = Vertex1;
			LastVertex2 = Vertex2;
		}
	}
#endif

	static void DrawTaperedCylinder(FPrimitiveDrawInterface* PDI, const FVector& Position1, const FVector& Position2, const FReal Radius1, const FReal Radius2, const int32 NumSides, const FLinearColor& Color)
	{
		const FQuat Q = (Position2 - Position1).ToOrientationQuat();
		const FVector I = Q.GetRightVector();
		const FVector J = Q.GetUpVector();

		const FReal	AngleDelta = (FReal)2. * (FReal)PI / NumSides;
		FVector LastVertex1 = Position1 + I * Radius1;
		FVector LastVertex2 = Position2 + I * Radius2;

		for (int32 SideIndex = 1; SideIndex <= NumSides; ++SideIndex)
		{
			const FReal Angle = AngleDelta * FReal(SideIndex);
			const FVector ArcPos = I * FMath::Cos(Angle) + J * FMath::Sin(Angle);
			const FVector Vertex1 = Position1 + ArcPos * Radius1;
			const FVector Vertex2 = Position2 + ArcPos * Radius2;

			DrawLine(PDI, LastVertex1, Vertex1, Color);
			DrawLine(PDI, LastVertex2, Vertex2, Color);
			DrawLine(PDI, LastVertex1, LastVertex2, Color);

			LastVertex1 = Vertex1;
			LastVertex2 = Vertex2;
		}
	}

	static void DrawTaperedCylinder(FPrimitiveDrawInterface* PDI, const ::Chaos::FTaperedCylinder& TaperedCylinder, const FQuat& Rotation, const FVector& Position, const FLinearColor& Color)
	{
		const FReal Radius1 = TaperedCylinder.GetRadius1();
		const FReal Radius2 = TaperedCylinder.GetRadius2();
		const FVector Position1 = Position + Rotation.RotateVector(TaperedCylinder.GetX1());
		const FVector Position2 = Position + Rotation.RotateVector(TaperedCylinder.GetX2());
		DrawTaperedCylinder(PDI, Position1, Position2, Radius1, Radius2, 12, Color);
	}

	static void DrawConvex(FPrimitiveDrawInterface* PDI, const ::Chaos::FConvex& Convex, const FQuat& Rotation, const FVector& Position, const FLinearColor& Color)
	{
		const TArray<FConvex::FPlaneType>& Planes = Convex.GetFaces();
		for (int32 PlaneIndex1 = 0; PlaneIndex1 < Planes.Num(); ++PlaneIndex1)
		{
			const FConvex::FPlaneType& Plane1 = Planes[PlaneIndex1];

			for (int32 PlaneIndex2 = PlaneIndex1 + 1; PlaneIndex2 < Planes.Num(); ++PlaneIndex2)
			{
				const FConvex::FPlaneType& Plane2 = Planes[PlaneIndex2];

				// Find the two surface points that belong to both Plane1 and Plane2
				uint32 ParticleIndex1 = INDEX_NONE;

				const TArray<FConvex::FVec3Type>& Vertices = Convex.GetVertices();
				for (int32 ParticleIndex = 0; ParticleIndex < Vertices.Num(); ++ParticleIndex)
				{
					const FConvex::FVec3Type& X = Vertices[ParticleIndex];

					if (FMath::Square(Plane1.SignedDistance(X)) < KINDA_SMALL_NUMBER && 
						FMath::Square(Plane2.SignedDistance(X)) < KINDA_SMALL_NUMBER)
					{
						if (ParticleIndex1 != INDEX_NONE)
						{
							const FVector X1(Vertices[ParticleIndex1]);
							const FVector X2(X);
							const FVector Position1 = Position + Rotation.RotateVector(X1);
							const FVector Position2 = Position + Rotation.RotateVector(X2);
							DrawLine(PDI, Position1, Position2, Color);
							break;
						}
						ParticleIndex1 = ParticleIndex;
					}
				}
			}
		}
	}

	static void DrawCoordinateSystem(FPrimitiveDrawInterface* PDI, const FQuat& Rotation, const FVector& Position)
	{
		const FVector X = Rotation.RotateVector(FVector::ForwardVector) * 10.f;
		const FVector Y = Rotation.RotateVector(FVector::RightVector) * 10.f;
		const FVector Z = Rotation.RotateVector(FVector::UpVector) * 10.f;

		DrawLine(PDI, Position, Position + X, FLinearColor::Red);
		DrawLine(PDI, Position, Position + Y, FLinearColor::Green);
		DrawLine(PDI, Position, Position + Z, FLinearColor::Blue);
	}

	static void DrawLevelSet(FPrimitiveDrawInterface* PDI, const FTransform& Transform, const FMaterialRenderProxy* MaterialRenderProxy, const FLevelSet& LevelSet)
	{
#if WITH_EDITOR
		if (PDI && MaterialRenderProxy)
		{
			TArray<FVector3f> Vertices;
			TArray<FIntVector> Tris;
			LevelSet.GetZeroIsosurfaceGridCellFaces(Vertices, Tris);

			FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
			for (const FVector3f& V : Vertices)
			{
				MeshBuilder.AddVertex(FDynamicMeshVertex(V));
			}
			for (const FIntVector& T : Tris)
			{
				MeshBuilder.AddTriangle(T[0], T[1], T[2]);
			}

			MeshBuilder.Draw(PDI, Transform.ToMatrixWithScale(), MaterialRenderProxy, SDPG_World, false, false);
		}
		else
#endif
		{
			DrawCoordinateSystem(PDI, Transform.GetRotation(), Transform.GetTranslation());
		}
	}

	static void DrawSkinnedLevelSet(FPrimitiveDrawInterface* PDI, const TWeightedLatticeImplicitObject<FLevelSet>& SkinnedLevelSet, const FQuat& Rotation, const FVector& Position, const FMaterialRenderProxy* MaterialRenderProxy)
	{
#if WITH_EDITOR
		if (PDI && MaterialRenderProxy)
		{
			TArray<FVector3f> Vertices;
			TArray<FIntVector> Tris;
			const FLevelSet* const LevelSet = SkinnedLevelSet.GetEmbeddedObject();
			LevelSet->GetZeroIsosurfaceGridCellFaces(Vertices, Tris);

			FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
			for (const FVector3f& V : Vertices)
			{
				MeshBuilder.AddVertex(FDynamicMeshVertex(FVector3f(SkinnedLevelSet.GetDeformedPoint(FVec3(V)))));
			}
			for (const FIntVector& T : Tris)
			{
				MeshBuilder.AddTriangle(T[0], T[1], T[2]);
			}

			MeshBuilder.Draw(PDI, FTransform(Rotation, Position).ToMatrixWithScale(), MaterialRenderProxy, SDPG_World, false, false);

			if (Private::DrawSkinnedLattice)
			{
				const Chaos::TUniformGrid<double, 3>& LatticeGrid = SkinnedLevelSet.GetGrid();
				const Chaos::TArrayND<Chaos::FVec3, 3>& DeformedPoints = SkinnedLevelSet.GetDeformedPoints();
				const Chaos::TArrayND<bool, 3>& EmptyCells = SkinnedLevelSet.GetEmptyCells();
				const FColor LatticeColor = FColor::Cyan;
				const FColor EmptyLatticeColor = FColor::White;
				const Chaos::TVec3<int32> CellCounts = LatticeGrid.Counts();

				FTransform LocalToWorld(Rotation, Position);
				for (int32 I = 0; I < CellCounts.X; ++I)
				{
					for (int32 J = 0; J < CellCounts.Y; ++J)
					{
						for (int32 K = 0; K < CellCounts.Z; ++K)
						{
							const bool bIsEmpty = EmptyCells(I, J, K);
							const int32 EmptyDrawMask = bIsEmpty ? 2 : 1;
							if (EmptyDrawMask & Private::DrawSkinnedLattice)
							{
								const FVector P000 = LocalToWorld.TransformPosition(FVector(DeformedPoints(I, J, K)));
								const FVector P001 = LocalToWorld.TransformPosition(FVector(DeformedPoints(I, J, K + 1)));
								const FVector P010 = LocalToWorld.TransformPosition(FVector(DeformedPoints(I, J + 1, K)));
								const FVector P011 = LocalToWorld.TransformPosition(FVector(DeformedPoints(I, J + 1, K + 1)));
								const FVector P100 = LocalToWorld.TransformPosition(FVector(DeformedPoints(I + 1, J, K)));
								const FVector P101 = LocalToWorld.TransformPosition(FVector(DeformedPoints(I + 1, J, K + 1)));
								const FVector P110 = LocalToWorld.TransformPosition(FVector(DeformedPoints(I + 1, J + 1, K)));
								const FVector P111 = LocalToWorld.TransformPosition(FVector(DeformedPoints(I + 1, J + 1, K + 1)));

								PDI->AddReserveLines(SDPG_World, 12);
								const FColor& Color = bIsEmpty ? EmptyLatticeColor : LatticeColor;
								PDI->DrawLine(P000, P001, Color, SDPG_World);
								PDI->DrawLine(P000, P010, Color, SDPG_World);
								PDI->DrawLine(P000, P100, Color, SDPG_World);
								PDI->DrawLine(P001, P011, Color, SDPG_World);
								PDI->DrawLine(P001, P101, Color, SDPG_World);
								PDI->DrawLine(P010, P011, Color, SDPG_World);
								PDI->DrawLine(P010, P110, Color, SDPG_World);
								PDI->DrawLine(P011, P111, Color, SDPG_World);
								PDI->DrawLine(P100, P101, Color, SDPG_World);
								PDI->DrawLine(P100, P110, Color, SDPG_World);
								PDI->DrawLine(P101, P111, Color, SDPG_World);
								PDI->DrawLine(P110, P111, Color, SDPG_World);

							}
						}
					}
				}
			}
		}
		else
#endif
		{
			DrawCoordinateSystem(PDI, Rotation, Position);
		}
	}

	void FClothVisualization::DrawBounds(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		// Calculate World space bounds
		const FBoxSphereBounds Bounds = Solver->CalculateBounds();

		// Draw bounds
		DrawBox(PDI, FAABB3(-Bounds.BoxExtent, Bounds.BoxExtent), FQuat::Identity, Bounds.Origin, FLinearColor(FColor::Purple));
		DrawSphere(PDI, TSphere<FReal, 3>(FVector::ZeroVector, Bounds.SphereRadius), FQuat::Identity, Bounds.Origin, FLinearColor(FColor::Orange));

		// Draw individual cloth bounds
		static const FLinearColor Color = FLinearColor(FColor::Purple).Desaturate(0.5);
		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const FAABB3 BoundingBox = Cloth->CalculateBoundingBox(Solver);
			DrawBox(PDI, BoundingBox, FQuat::Identity, FVector::ZeroVector, Color);  // TODO: Express bounds in local coordinates for LWC
		}
	}

	void FClothVisualization::DrawGravity(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		// Draw gravity
		constexpr FReal GravityVectorLengthMultiplier = 0.01; // Make the vector smaller
		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);
			if (const Softs::FExternalForces* const ExternalForces = ClothConstraints.GetExternalForces().Get())
			{
				check(!Solver->IsLegacySolver());
				if (ExternalForces->HasPerParticleGravity())
				{
					const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();
					const TConstArrayView<Softs::FSolverVec3> Positions = Solver->GetParticleXsView(ParticleRangeId);
					for (int32 ParticleIndex = 0; ParticleIndex < Positions.Num(); ++ParticleIndex)
					{
						const FVector Pos0 = LocalSpaceLocation + FVector(Positions[ParticleIndex]);
						const FVector Pos1 = Pos0 + GravityVectorLengthMultiplier * FVector(ExternalForces->GetScaledGravity(ParticleIndex));
						DrawLine(PDI, Pos0, Pos1, FLinearColor::Red);
					}
				}
				else
				{
					const FAABB3 Bounds = Cloth->CalculateBoundingBox(Solver);

					const FVector Pos0 = Bounds.Center();
					const FVector Pos1 = Pos0 + GravityVectorLengthMultiplier * FVector(ExternalForces->GetScaledGravity(0));
					DrawLine(PDI, Pos0, Pos1, FLinearColor::Red);
				}
			}
			else
			{
				const FAABB3 Bounds = Cloth->CalculateBoundingBox(Solver);

				const FVector Pos0 = Bounds.Center();
				const FVector Pos1 = Pos0 + GravityVectorLengthMultiplier * FVector(Cloth->GetGravity(Solver));
				DrawLine(PDI, Pos0, Pos1, FLinearColor::Red);
			}
		}
	}


	void FClothVisualization::DrawFictitiousAngularForces(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);
			if (const Softs::FExternalForces* const ExternalForces = ClothConstraints.GetExternalForces().Get())
			{
				check(!Solver->IsLegacySolver());
				{
					const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();
					const TConstArrayView<Softs::FSolverVec3> Positions = Solver->GetParticleXsView(ParticleRangeId);
					const Softs::FSolverVec3& FictitousAngularVelocity = ExternalForces->GetFictitiousAngularVelocity();
					const Softs::FSolverVec3& ReferenceSpaceLocation = ExternalForces->GetReferenceSpaceLocation();

					for (int32 ParticleIndex = 0; ParticleIndex < Positions.Num(); ++ParticleIndex)
					{
						const FVector Pos0 = LocalSpaceLocation + FVector(Positions[ParticleIndex]);
						const Softs::FSolverVec3 CentrifugalAccel = -Softs::FSolverVec3::CrossProduct(FictitousAngularVelocity, Softs::FSolverVec3::CrossProduct(FictitousAngularVelocity, Positions[ParticleIndex] - ReferenceSpaceLocation));

						const FVector Pos1 = Pos0 + FVector(CentrifugalAccel);
						DrawLine(PDI, Pos0, Pos1, FLinearColor::Red);
					}
				}
			}
		}
	}

	void FClothVisualization::DrawPhysMeshWired(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		static const FLinearColor DynamicColor = FColor::White;
		static const FLinearColor KinematicColor = FColor::Purple;

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			// Elements are local indexed for new solver
			const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;
			const TConstArrayView<TVec3<int32>> Elements = Cloth->GetTriangleMesh(Solver).GetElements();
			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
			const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver);
			check(InvMasses.Num() == Positions.Num());

			for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
			{
				const TVec3<int32>& Element = Elements[ElementIndex];

				const FVector Pos0 = LocalSpaceLocation + FVector(Positions[Element.X - Offset]); // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
				const FVector Pos1 = LocalSpaceLocation + FVector(Positions[Element.Y - Offset]);
				const FVector Pos2 = LocalSpaceLocation + FVector(Positions[Element.Z - Offset]);

				const bool bIsKinematic0 = (InvMasses[Element.X - Offset] == (Softs::FSolverReal)0.);
				const bool bIsKinematic1 = (InvMasses[Element.Y - Offset] == (Softs::FSolverReal)0.);
				const bool bIsKinematic2 = (InvMasses[Element.Z - Offset] == (Softs::FSolverReal)0.);

				DrawLine(PDI, Pos0, Pos1, bIsKinematic0 && bIsKinematic1 ? KinematicColor : DynamicColor);
				DrawLine(PDI, Pos1, Pos2, bIsKinematic1 && bIsKinematic2 ? KinematicColor : DynamicColor);
				DrawLine(PDI, Pos2, Pos0, bIsKinematic2 && bIsKinematic0 ? KinematicColor : DynamicColor);
			}
		}
	}

	void FClothVisualization::DrawAnimMeshWired(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		static const FLinearColor KinematicColor = FColor::Purple;

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			// Elements are local indexed for new solver
			const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;

			const TConstArrayView<TVec3<int32>> Elements = Cloth->GetTriangleMesh(Solver).GetElements();
			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetAnimationPositions(Solver);

			for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
			{
				const TVec3<int32>& Element = Elements[ElementIndex];

				const FVector Pos0 = LocalSpaceLocation + FVector(Positions[Element.X - Offset]); // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
				const FVector Pos1 = LocalSpaceLocation + FVector(Positions[Element.Y - Offset]);
				const FVector Pos2 = LocalSpaceLocation + FVector(Positions[Element.Z - Offset]);

				DrawLine(PDI, Pos0, Pos1, KinematicColor);
				DrawLine(PDI, Pos1, Pos2, KinematicColor);
				DrawLine(PDI, Pos2, Pos0, KinematicColor);
			}
		}
	}

	void FClothVisualization::DrawOpenEdges(FPrimitiveDrawInterface* PDI) const
	{
		auto MakeSortedUintVector2 = [](uint32 Index0, uint32 Index1) -> FUintVector2
			{
				return Index0 < Index1 ? FUintVector2(Index0, Index1) : FUintVector2(Index1, Index0);
			};

		auto BuildEdgeMap = [&MakeSortedUintVector2](const TConstArrayView<TVec3<int32>>& Elements, TMap<FUintVector2, TArray<uint32>>& OutEdgeToTrianglesMap)
			{
			OutEdgeToTrianglesMap.Empty(Elements.Num() * 2);  // Rough estimate for the number of edges

				for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
				{
					const TVec3<int32>& Element = Elements[ElementIndex];
					const uint32 Index0 = Element[0];
					const uint32 Index1 = Element[1];
					const uint32 Index2 = Element[2];

					const FUintVector2 Edge0 = MakeSortedUintVector2(Index0, Index1);
					const FUintVector2 Edge1 = MakeSortedUintVector2(Index1, Index2);
					const FUintVector2 Edge2 = MakeSortedUintVector2(Index2, Index0);

					OutEdgeToTrianglesMap.FindOrAdd(Edge0).Add(ElementIndex);
					OutEdgeToTrianglesMap.FindOrAdd(Edge1).Add(ElementIndex);
					OutEdgeToTrianglesMap.FindOrAdd(Edge2).Add(ElementIndex);
				}
			};

		if (!Solver)
		{
			return;
		}

		static const FLinearColor OpenedEdgeColor = FColor::Emerald;
		static const FLinearColor ClosedEdgeColor = FColor::White;

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			// Elements are local indexed for new solver
			const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;

			const TConstArrayView<TVec3<int32>> Elements = Cloth->GetTriangleMesh(Solver).GetElements();
			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetAnimationPositions(Solver);

			TMap<FUintVector2, TArray<uint32>> EdgeToTrianglesMap;
			BuildEdgeMap(Elements, EdgeToTrianglesMap);

			for (const TPair<FUintVector2, TArray<uint32>>& EdgeToTriangles : EdgeToTrianglesMap)
			{
				const FUintVector2& Edge = EdgeToTriangles.Key;
				const TArray<uint32>& Triangles = EdgeToTriangles.Value;

				const FVector Pos0 = LocalSpaceLocation + FVector(Positions[Edge[0] - Offset]); // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
				const FVector Pos1 = LocalSpaceLocation + FVector(Positions[Edge[1] - Offset]);
				const FLinearColor& Color = (Triangles.Num() > 1) ? ClosedEdgeColor : OpenedEdgeColor;

				DrawLine(PDI, Pos0, Pos1, Color);
			}
		}
	}

	void FClothVisualization::DrawMultiResConstraint(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();
		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);
			if (const Softs::FMultiResConstraints* const MultiResConstraints = ClothConstraints.GetMultiResConstraints().Get())
			{

				const int32 CoarseParticleRangeId = MultiResConstraints->GetCoarseSoftBodyId();
				const FTriangleMesh& CoarseMesh = MultiResConstraints->GetCoarseMesh();
				TConstArrayView<Softs::FSolverVec3> CoarsePositions = Solver->GetParticleXsView(CoarseParticleRangeId);
				TConstArrayView<Softs::FSolverReal> CoarseInvMasses = Solver->GetParticleInvMassesView(CoarseParticleRangeId);

				// Draw wired coarse mesh
				static const FLinearColor DynamicColor = FColor::White;
				static const FLinearColor KinematicColor = FColor::Purple;
				for (const TVec3<int32>& Element : CoarseMesh.GetElements())
				{
					const FVector Pos0 = LocalSpaceLocation + FVector(CoarsePositions[Element.X]);
					const FVector Pos1 = LocalSpaceLocation + FVector(CoarsePositions[Element.Y]);
					const FVector Pos2 = LocalSpaceLocation + FVector(CoarsePositions[Element.Z]);
					const bool bIsKinematic0 = (CoarseInvMasses[Element.X] == (Softs::FSolverReal)0.);
					const bool bIsKinematic1 = (CoarseInvMasses[Element.Y] == (Softs::FSolverReal)0.);
					const bool bIsKinematic2 = (CoarseInvMasses[Element.Z] == (Softs::FSolverReal)0.);
					if (bIsKinematic0 && bIsKinematic1 && bIsKinematic2)
					{
						continue;
					}

					DrawLine(PDI, Pos0, Pos1, bIsKinematic0 && bIsKinematic1 ? KinematicColor : DynamicColor);
					DrawLine(PDI, Pos1, Pos2, bIsKinematic1 && bIsKinematic2 ? KinematicColor : DynamicColor);
					DrawLine(PDI, Pos2, Pos0, bIsKinematic2 && bIsKinematic0 ? KinematicColor : DynamicColor);
				}

				// Draw springs to targets
				static const FLinearColor Red(0.3f, 0.f, 0.f);
				static const FLinearColor Brown(0.1f, 0.05f, 0.f);
				const TConstArrayView<Softs::FSolverVec3> Positions = Solver->GetParticleXsView(ParticleRangeId);
				const TConstArrayView<Softs::FSolverReal> InvMasses = Solver->GetParticleInvMassesView(ParticleRangeId);
				const TArray<Softs::FSolverVec3>& TargetPositions = MultiResConstraints->GetFineTargetPositions();
				for (int32 Index = 0; Index < TargetPositions.Num(); ++Index)
				{
					if (InvMasses[Index] != (FSolverReal)0.f && MultiResConstraints->IsConstraintActive(Index))
					{
						const FVector P1 = LocalSpaceLocation + FVector(Positions[Index]);
						const FVector P2 = LocalSpaceLocation + FVector(TargetPositions[Index]);

						DrawPoint(PDI, P2, Red, nullptr, 2.f);
						DrawLine(PDI, P1, P2, Brown);
					}
				}
			}
		}
	}

	void FClothVisualization::DrawAnimNormals(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		static const FLinearColor KinematicColor = FColor::Magenta;
		constexpr FReal NormalLength = (FReal)20.;

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			if (Cloth->GetParticleRangeId(Solver) == INDEX_NONE)
			{
				continue;
			}

			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetAnimationPositions(Solver);
			const TConstArrayView<Softs::FSolverVec3> Normals = Cloth->GetAnimationNormals(Solver);
			check(Normals.Num() == Positions.Num());

			for (int32 Index = 0; Index < Positions.Num(); ++Index)
			{
				const FVector Pos0 = LocalSpaceLocation + FVector(Positions[Index]);
				const FVector Pos1 = Pos0 + FVector(Normals[Index]) * NormalLength;

				DrawLine(PDI, Pos0, Pos1, KinematicColor);
			}
		}
	}

	void FClothVisualization::DrawPointNormals(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		static const FLinearColor DynamicColor = FColor::White;
		static const FLinearColor KinematicColor = FColor::Purple;
		constexpr FReal NormalLength = (FReal)20.;

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			if (Cloth->GetParticleRangeId(Solver) == INDEX_NONE)
			{
				continue;
			}

			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
			const TConstArrayView<Softs::FSolverVec3> Normals = Cloth->GetParticleNormals(Solver);
			const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver);
			check(Normals.Num() == Positions.Num());
			check(InvMasses.Num() == Positions.Num());

			for (int32 Index = 0; Index < Positions.Num(); ++Index)
			{
				const bool bIsKinematic = (InvMasses[Index] == (Softs::FSolverReal)0.);
				const FVector Pos0 = LocalSpaceLocation + FVector(Positions[Index]);
				const FVector Pos1 = Pos0 + FVector(Normals[Index]) * NormalLength;

				DrawLine(PDI, Pos0, Pos1, bIsKinematic ? KinematicColor : DynamicColor);
			}
		}
	}

	void FClothVisualization::DrawPointVelocities(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			if (Cloth->GetParticleRangeId(Solver) == INDEX_NONE)
			{
				continue;
			}

			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
			const TConstArrayView<Softs::FSolverVec3> Velocities = Cloth->GetParticleVelocities(Solver);
			check(Velocities.Num() == Positions.Num());

			const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver);
			check(InvMasses.Num() == Positions.Num());

			for (int32 Index = 0; Index < Positions.Num(); ++Index)
			{
				constexpr FReal DefaultFPS = (FReal)60.;   // TODO: A CVAR would be nice for this
				const bool bIsKinematic = (InvMasses[Index] == 0.f);

				const FVec3 Pos0 = LocalSpaceLocation + Positions[Index];
				const FVec3 Pos1 = Pos0 + FVec3(Velocities[Index]) / DefaultFPS;  // Velocity per frame if running at DefaultFPS

				DrawLine(PDI, Pos0, Pos1, bIsKinematic ? FLinearColor::Black : FLinearColor::Yellow);
			}
		}
	}

	void FClothVisualization::DrawCollision(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		auto DrawCollision =
			[this, PDI](const FClothingSimulationCollider* Collider, const FClothingSimulationCloth* Cloth, FClothingSimulationCollider::ECollisionDataType CollisionDataType)
			{
				static const FLinearColor GlobalColor(FColor::Cyan);
				static const FLinearColor DynamicColor(FColor::Orange);
				static const FLinearColor LODsColor(FColor::Silver);
				static const FLinearColor CollidedColor(FColor::Red);

				const FLinearColor TypeColor =
					(CollisionDataType == FClothingSimulationCollider::ECollisionDataType::LODless) ? GlobalColor :
					(CollisionDataType == FClothingSimulationCollider::ECollisionDataType::External) ? DynamicColor : LODsColor;

				const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

				const TConstArrayView<FImplicitObjectPtr> CollisionGeometries = Collider->GetCollisionGeometry(Solver, Cloth, CollisionDataType);
				const TConstArrayView<Softs::FSolverVec3> Translations = Collider->GetCollisionTranslations(Solver, Cloth, CollisionDataType);
				const TConstArrayView<Softs::FSolverRotation3> Rotations = Collider->GetCollisionRotations(Solver, Cloth, CollisionDataType);
				const TConstArrayView<bool> CollisionStatus = Collider->GetCollisionStatus(Solver, Cloth, CollisionDataType);
				check(CollisionGeometries.Num() == Translations.Num());
				check(CollisionGeometries.Num() == Rotations.Num());

				for (int32 Index = 0; Index < CollisionGeometries.Num(); ++Index)
				{
					if (const FImplicitObject* const Object = CollisionGeometries[Index].GetReference())
					{
						const FLinearColor Color = CollisionStatus[Index] ? CollidedColor : TypeColor;
						const FVec3 Position = LocalSpaceLocation + FVec3(Translations[Index]);
						const FRotation3 Rotation(Rotations[Index]);

						switch (Object->GetType())
						{
						case ImplicitObjectType::Sphere:
							DrawSphere(PDI, Object->GetObjectChecked<TSphere<FReal, 3>>(), Rotation, Position, Color);
							break;

						case ImplicitObjectType::Box:
							DrawBox(PDI, Object->GetObjectChecked<TBox<FReal, 3>>().BoundingBox(), Rotation, Position, Color);
							break;

						case ImplicitObjectType::Capsule:
							DrawCapsule(PDI, Object->GetObjectChecked<FCapsule>(), Rotation, Position, Color);
							break;

						case ImplicitObjectType::Union:  // Union only used as old style tapered capsules
							for (const FImplicitObjectPtr& SubObjectPtr : Object->GetObjectChecked<FImplicitObjectUnion>().GetObjects())
							{
								if (const FImplicitObject* const SubObject = SubObjectPtr.GetReference())
								{
									switch (SubObject->GetType())
									{
									case ImplicitObjectType::Sphere:
										DrawSphere(PDI, SubObject->GetObjectChecked<TSphere<FReal, 3>>(), Rotation, Position, Color);
										break;

									case ImplicitObjectType::TaperedCylinder:
										DrawTaperedCylinder(PDI, SubObject->GetObjectChecked<FTaperedCylinder>(), Rotation, Position, Color);
										break;

									default:
										break;
									}
								}
							}
							break;

						case ImplicitObjectType::TaperedCapsule:  // New collision tapered capsules implicit type that replaces the union
							{
								const FTaperedCapsule& TaperedCapsule = Object->GetObjectChecked<FTaperedCapsule>();
								const FVec3 X1 = TaperedCapsule.GetX1();
								const FVec3 X2 = TaperedCapsule.GetX2();
								const FReal Radius1 = TaperedCapsule.GetRadius1();
								const FReal Radius2 = TaperedCapsule.GetRadius2();
								DrawSphere(PDI, TSphere<FReal, 3>(X1, Radius1), Rotation, Position, Color);
								DrawSphere(PDI, TSphere<FReal, 3>(X2, Radius2), Rotation, Position, Color);
								DrawTaperedCylinder(PDI, FTaperedCylinder(X1, X2, Radius1, Radius2), Rotation, Position, Color);
							}
							break;

						case ImplicitObjectType::Convex:
							DrawConvex(PDI, Object->GetObjectChecked<FConvex>(), Rotation, Position, Color);
							break;

						case ImplicitObjectType::Transformed: // Transformed only used for levelsets
							if (Object->GetObjectChecked<TImplicitObjectTransformed<FReal, 3>>().GetGeometry()->GetType() == ImplicitObjectType::LevelSet)
							{
								const TRigidTransform<FReal, 3>& Transform = Object->GetObjectChecked<TImplicitObjectTransformed<FReal, 3>>().GetTransform();
								const FTransform CombinedTransform = Transform * FTransform(Rotation, Position);
								const FLevelSet& LevelSet = Object->GetObjectChecked<TImplicitObjectTransformed<FReal, 3>>().GetGeometry()->GetObjectChecked<FLevelSet>();
								const FMaterialRenderProxy* MaterialRenderProxy =
#if WITH_EDITOR
									CollisionMaterial->GetRenderProxy();
#else
									nullptr;
#endif
								DrawLevelSet(PDI, CombinedTransform, MaterialRenderProxy, LevelSet);
							}
							break;
						case (ImplicitObjectType::LevelSet | ImplicitObjectType::IsWeightedLattice):
							{
								const TWeightedLatticeImplicitObject<FLevelSet>& WeightedLevelset = Object->GetObjectChecked< TWeightedLatticeImplicitObject<FLevelSet> >();
								const FMaterialRenderProxy* MaterialRenderProxy =
#if WITH_EDITOR
									CollisionMaterial->GetRenderProxy();
#else
									nullptr;
#endif
								DrawSkinnedLevelSet(PDI, WeightedLevelset, Rotation, Position, MaterialRenderProxy);

							}
							break;
						default:
							DrawCoordinateSystem(PDI, Rotation, Position);  // Draw everything else as a coordinate for now
							break;
						}
					}
				}
			};

		// Draw collisions
		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			for (const FClothingSimulationCollider* const Collider : Cloth->GetColliders())
			{
				DrawCollision(Collider, Cloth, FClothingSimulationCollider::ECollisionDataType::LODless);
				DrawCollision(Collider, Cloth, FClothingSimulationCollider::ECollisionDataType::External);
				DrawCollision(Collider, Cloth, FClothingSimulationCollider::ECollisionDataType::LODs);
			}
		}

		// Draw contacts
		check(Solver->GetCollisionContacts().Num() == Solver->GetCollisionNormals().Num());
		const bool bDrawPhis = Solver->GetCollisionContacts().Num() == Solver->GetCollisionPhis().Num();
		constexpr FReal NormalLength = (FReal)10.;

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();
		for (int32 i = 0; i < Solver->GetCollisionContacts().Num(); ++i)
		{
			const FVec3 Pos0 = LocalSpaceLocation + FVec3(Solver->GetCollisionContacts()[i]);
			const FVec3 Normal = FVec3(Solver->GetCollisionNormals()[i]);

			// Draw contact
			FVec3 TangentU, TangentV;
			Normal.FindBestAxisVectors(TangentU, TangentV);

			DrawLine(PDI, Pos0 + TangentU, Pos0 + TangentV, FLinearColor::Black);
			DrawLine(PDI, Pos0 + TangentU, Pos0 - TangentV, FLinearColor::Black);
			DrawLine(PDI, Pos0 - TangentU, Pos0 - TangentV, FLinearColor::Black);
			DrawLine(PDI, Pos0 - TangentU, Pos0 + TangentV, FLinearColor::Black);

			// Draw normal
			static const FLinearColor Brown(0.1f, 0.05f, 0.f);
			const FVec3 Pos1 = Pos0 + NormalLength * Normal;
			DrawLine(PDI, Pos0, Pos1, Brown);

			if (bDrawPhis)
			{
				const FVec3 PhiLocation = Pos0 - Solver->GetCollisionPhis()[i] * Normal;
				DrawLine(PDI, Pos0, PhiLocation, Brown);
				DrawPoint(PDI, PhiLocation, FLinearColor::Red, nullptr, 5);
			}
		}
	}

	void FClothVisualization::DrawBackstops(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		auto DrawBackstop = [PDI](const FVector& Position, const FVector& Normal, FReal Radius, const FVector& Axis, const FLinearColor& Color)
			{
				static const FReal MaxCosAngle = (FReal)0.99;
				if (FMath::Abs(FVector::DotProduct(Normal, Axis)) < MaxCosAngle)
				{
					static const FReal ArcLength = (FReal)5.; // Arch length in cm
					const FReal ArcAngle = (FReal)360. * ArcLength / FMath::Max((Radius * (FReal)2. * (FReal)PI), ArcLength);
					DrawArc(PDI, Position, Normal, FVector::CrossProduct(Axis, Normal).GetSafeNormal(), -ArcAngle / (FReal)2., ArcAngle / (FReal)2., Radius, Color);
				}
			};

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		uint8 ColorSeed = 0;

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);
			if (const Softs::FPBDSphericalBackstopConstraint* const BackstopConstraint = ClothConstraints.GetBackstopConstraints().Get())
			{
				const bool bUseLegacyBackstop = BackstopConstraint->UseLegacyBackstop();

				const TConstArrayView<Softs::FSolverVec3> AnimationPositions = Cloth->GetAnimationPositions(Solver);
				const TConstArrayView<Softs::FSolverVec3> AnimationNormals = Cloth->GetAnimationNormals(Solver);
				const TConstArrayView<Softs::FSolverVec3> ParticlePositions = Cloth->GetParticlePositions(Solver);

				for (int32 Index = 0; Index < AnimationPositions.Num(); ++Index)
				{
					ColorSeed += 157;  // Prime number that gives a good spread of colors without getting too similar as a rand might do.
					const FLinearColor ColorLight = FLinearColor::MakeFromHSV8(ColorSeed, 160, 128);
					const FLinearColor ColorDark = FLinearColor::MakeFromHSV8(ColorSeed, 160, 64);

					const FReal BackstopRadius = BackstopConstraint->GetBackstopRadius(Index) * BackstopConstraint->GetScale();
					const FReal BackstopDistance = BackstopConstraint->GetBackstopDistance(Index) * BackstopConstraint->GetScale();

					const FVector AnimationNormal(AnimationNormals[Index]);

					// Draw a line to show the current distance to the sphere
					const FVector Pos0 = LocalSpaceLocation + FVector(AnimationPositions[Index]);
					const FVector Pos1 = Pos0 - (bUseLegacyBackstop ? BackstopDistance - BackstopRadius : BackstopDistance) * AnimationNormal;
					const FVector Pos2 = LocalSpaceLocation + FVector(ParticlePositions[Index]);
					DrawLine(PDI, Pos1, Pos2, ColorLight);

					// Draw the sphere
					if (BackstopRadius > 0.f)
					{
						const FVector Center = Pos0 - (bUseLegacyBackstop ? BackstopDistance : BackstopRadius + BackstopDistance) * AnimationNormal;
						DrawBackstop(Center, AnimationNormal, BackstopRadius, FVector::ForwardVector, ColorDark);
						DrawBackstop(Center, AnimationNormal, BackstopRadius, FVector::UpVector, ColorDark);
						DrawBackstop(Center, AnimationNormal, BackstopRadius, FVector::RightVector, ColorDark);
					}
				}
			}
		}
	}

	void FClothVisualization::DrawBackstopDistances(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		uint8 ColorSeed = 0;

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);
			if (const Softs::FPBDSphericalBackstopConstraint* const BackstopConstraint = ClothConstraints.GetBackstopConstraints().Get())
			{
				const bool bUseLegacyBackstop = BackstopConstraint->UseLegacyBackstop();
				const TConstArrayView<Softs::FSolverVec3> AnimationPositions = Cloth->GetAnimationPositions(Solver);
				const TConstArrayView<Softs::FSolverVec3> AnimationNormals = Cloth->GetAnimationNormals(Solver);

				for (int32 Index = 0; Index < AnimationPositions.Num(); ++Index)
				{
					ColorSeed += 157;  // Prime number that gives a good spread of colors without getting too similar as a rand might do.
					const FLinearColor ColorLight = FLinearColor::MakeFromHSV8(ColorSeed, 160, 128);
					const FLinearColor ColorDark = FLinearColor::MakeFromHSV8(ColorSeed, 160, 64);

					const FReal BackstopRadius = BackstopConstraint->GetBackstopRadius(Index) * BackstopConstraint->GetScale();
					const FReal BackstopDistance = BackstopConstraint->GetBackstopDistance(Index) * BackstopConstraint->GetScale();

					const FVector AnimationNormal(AnimationNormals[Index]);

					// Draw a line to the sphere boundary
					const FVector Pos0 = LocalSpaceLocation + FVector(AnimationPositions[Index]);
					const FVector Pos1 = Pos0 - (bUseLegacyBackstop ? BackstopDistance - BackstopRadius : BackstopDistance) * AnimationNormal;
					DrawLine(PDI, Pos0, Pos1, ColorDark);
				}
			}
		}
	}

	void FClothVisualization::DrawMaxDistances(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		// Draw max distances
		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const TConstArrayView<FRealSingle>& MaxDistances = Cloth->GetWeightMapByProperty(Solver, TEXT("MaxDistance"));
			if (!MaxDistances.Num())
			{
				continue;
			}

			const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver);
			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetAnimationPositions(Solver);
			const TConstArrayView<Softs::FSolverVec3> Normals = Cloth->GetAnimationNormals(Solver);
			check(Normals.Num() == Positions.Num());
			check(MaxDistances.Num() == Positions.Num());
			check(InvMasses.Num() == Positions.Num());

			for (int32 Index = 0; Index < MaxDistances.Num(); ++Index)
			{
				const FReal MaxDistance = (FReal)MaxDistances[Index];
				const FVector Position = LocalSpaceLocation + FVector(Positions[Index]);
				if (InvMasses[Index] == (Softs::FSolverReal)0.)
				{
#if WITH_EDITOR
					DrawPoint(PDI, Position, FLinearColor::Red, ClothMaterialVertex);
#else
					DrawPoint(nullptr, Position, FLinearColor::Red, nullptr);
#endif
				}
				else
				{
					DrawLine(PDI, Position, Position + FVector(Normals[Index]) * MaxDistance, FLinearColor::White);
				}
			}
		}
	}

	void FClothVisualization::DrawAnimDrive(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);
			if (const Softs::FPBDAnimDriveConstraint* const AnimDriveConstraint = ClothConstraints.GetAnimDriveConstraints().Get())
			{
				const TConstArrayView<FRealSingle>& AnimDriveStiffnessMultipliers = Cloth->GetWeightMapByProperty(Solver, TEXT("AnimDriveStiffness"));
				const TConstArrayView<Softs::FSolverVec3> AnimationPositions = Cloth->GetAnimationPositions(Solver);
				const TConstArrayView<Softs::FSolverVec3> ParticlePositions = Cloth->GetParticlePositions(Solver);
				check(ParticlePositions.Num() == AnimationPositions.Num());

				const FVec2 AnimDriveStiffness = AnimDriveConstraint->GetStiffness();
				const FRealSingle StiffnessOffset = AnimDriveStiffness[0];
				const FRealSingle StiffnessRange = AnimDriveStiffness[1] - AnimDriveStiffness[0];

				for (int32 Index = 0; Index < ParticlePositions.Num(); ++Index)
				{
					const FRealSingle Stiffness = AnimDriveStiffnessMultipliers.IsValidIndex(Index) ?
						StiffnessOffset + AnimDriveStiffnessMultipliers[Index] * StiffnessRange :
						StiffnessOffset;

					const FVector AnimationPosition = LocalSpaceLocation + FVector(AnimationPositions[Index]);
					const FVector ParticlePosition = LocalSpaceLocation + FVector(ParticlePositions[Index]);
					DrawLine(PDI, AnimationPosition, ParticlePosition, FLinearColor(FColor::Cyan) * Stiffness);
				}
			}
		}
	}
	template<typename SpringConstraintType, typename GetEndPointsFuncType>
	static void DrawSpringConstraintColors(FPrimitiveDrawInterface* PDI, const ::Chaos::FVec3& LocalSpaceLocation, const SpringConstraintType* const SpringConstraints, GetEndPointsFuncType GetEndPoints)
	{
		check(SpringConstraints);

		const TArray<int32>& ConstraintsPerColorStartIndex = SpringConstraints->GetConstraintsPerColorStartIndex();
		if (ConstraintsPerColorStartIndex.Num() > 1)
		{
			const int32 ConstraintColorNum = ConstraintsPerColorStartIndex.Num() - 1;
			const uint8 HueOffset = 196 / ConstraintColorNum;
			auto ConstraintColor =
				[HueOffset](int32 ColorIndex)->FLinearColor
				{
					return FLinearColor::MakeFromHSV8(ColorIndex * HueOffset, 255, 255);
				};

			for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
			{
				const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
				const int32 ColorEnd = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1];
				const FLinearColor DrawColor = ConstraintColor(ConstraintColorIndex);
				for (int32 ConstraintIndex = ColorStart; ConstraintIndex < ColorEnd; ++ConstraintIndex)
				{
					// Draw line
					Softs::FSolverVec3 P1, P2;
					GetEndPoints(ConstraintIndex, P1, P2);

					const FVec3 Pos0 = FVec3(P1) + LocalSpaceLocation;
					const FVec3 Pos1 = FVec3(P2) + LocalSpaceLocation;
					DrawLine(PDI, Pos0, Pos1, DrawColor);
				}
			}
		}
		else
		{
			for (int32 ConstraintIndex = 0; ConstraintIndex < SpringConstraints->GetConstraints().Num(); ++ConstraintIndex)
			{
				// Draw line
				Softs::FSolverVec3 P1, P2;
				GetEndPoints(ConstraintIndex, P1, P2);

				const FVec3 Pos0 = FVec3(P1) + LocalSpaceLocation;
				const FVec3 Pos1 = FVec3(P2) + LocalSpaceLocation;

				DrawLine(PDI, Pos0, Pos1, FLinearColor::Black);
			}
		}
	}

	template<typename SpringConstraintType>
	static void DrawSpringConstraintColors(FPrimitiveDrawInterface* PDI, const TConstArrayView<::Chaos::Softs::FSolverVec3>& Positions, const ::Chaos::FVec3& LocalSpaceLocation, const SpringConstraintType* const SpringConstraints)
	{
		DrawSpringConstraintColors(PDI, LocalSpaceLocation, SpringConstraints, [&SpringConstraints, &Positions](const int32 ConstraintIndex, Softs::FSolverVec3& P1, Softs::FSolverVec3& P2)
		{
			P1 = Positions[SpringConstraints->GetConstraints()[ConstraintIndex][0]];
			P2 = Positions[SpringConstraints->GetConstraints()[ConstraintIndex][1]];
		});
	}

	static void DrawStretchBiasConstraints_ParallelGraphColor(FPrimitiveDrawInterface* PDI, const TConstArrayView<::Chaos::Softs::FSolverVec3>& Positions, const ::Chaos::FVec3& LocalSpaceLocation, const FMaterialRenderProxy* MaterialRenderProxy, const Softs::FXPBDStretchBiasElementConstraints* const SpringConstraints)
	{
#if WITH_EDITOR
		if (PDI && MaterialRenderProxy)
		{
			const TArray<TVec3<int32>>& Constraints = SpringConstraints->GetConstraints();
			const TArray<int32>& ConstraintsPerColorStartIndex = SpringConstraints->GetConstraintsPerColorStartIndex();

			const int32 ConstraintColorNum = ConstraintsPerColorStartIndex.Num() - 1;
			const uint8 HueOffset = 196 / ConstraintColorNum;

			auto ConstraintColor =
				[HueOffset](int32 ColorIndex)->FColor
			{
				return FLinearColor::MakeFromHSV8(ColorIndex * HueOffset, 255, 255).ToFColor(true);
			};

			FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
			int32 VertexIndex = 0;
			for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
			{
				const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
				const int32 ColorEnd = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1];
				const FColor DrawColor = ConstraintColor(ConstraintColorIndex);
				for (int32 ConstraintIndex = ColorStart; ConstraintIndex < ColorEnd; ++ConstraintIndex, VertexIndex += 3)
				{
					const TVec3<int32>& Constraint = Constraints[ConstraintIndex];
					const FVector3f Pos0 = FVector3f(Positions[Constraint[0]]);
					const FVector3f Pos1 = FVector3f(Positions[Constraint[1]]);
					const FVector3f Pos2 = FVector3f(Positions[Constraint[2]]);
					const FVector3f Normal = FVector3f::CrossProduct(Pos2 - Pos0, Pos1 - Pos0).GetSafeNormal();
					const FVector3f Tangent = ((Pos1 + Pos2) * 0.5f - Pos0).GetSafeNormal();

					MeshBuilder.AddVertex(FDynamicMeshVertex(Pos0, Tangent, Normal, FVector2f(0.f, 0.f), DrawColor));
					MeshBuilder.AddVertex(FDynamicMeshVertex(Pos1, Tangent, Normal, FVector2f(0.f, 1.f), DrawColor));
					MeshBuilder.AddVertex(FDynamicMeshVertex(Pos2, Tangent, Normal, FVector2f(1.f, 1.f), DrawColor));
					MeshBuilder.AddTriangle(VertexIndex, VertexIndex + 1, VertexIndex + 2);
				}
			}

			FMatrix LocalSimSpaceToWorld(FMatrix::Identity);
			LocalSimSpaceToWorld.SetOrigin(LocalSpaceLocation);
			MeshBuilder.Draw(PDI, LocalSimSpaceToWorld, MaterialRenderProxy, SDPG_World, false, false);
		}
#endif
	}

	static void DrawStretchBiasConstraints_WarpWeftStretch(FPrimitiveDrawInterface* PDI, const TConstArrayView<::Chaos::Softs::FSolverVec3>& Positions, const ::Chaos::FVec3& LocalSpaceLocation, const FMaterialRenderProxy* MaterialRenderProxy, const Softs::FXPBDStretchBiasElementConstraints* const SpringConstraints)
	{
#if WITH_EDITOR
		if (PDI && MaterialRenderProxy)
		{
			const TArray<TVec3<int32>>& Constraints = SpringConstraints->GetConstraints();
			const float StretchRangeMinClamped = Private::StretchBiasDrawRangeMin <= Private::StretchBiasDrawRangeMax ? Private::StretchBiasDrawRangeMin : 0.f;
			const float StretchRangeMaxClamped = Private::StretchBiasDrawRangeMin <= Private::StretchBiasDrawRangeMax ? Private::StretchBiasDrawRangeMax : 0.f;
			const float StretchRange = StretchRangeMaxClamped - StretchRangeMinClamped;
			const float StretchRangeInv = StretchRange > UE_KINDA_SMALL_NUMBER ? 1.f / StretchRange : 0.f;
			const bool bIsWeftStretch = (Private::EStretchBiasDrawMode)Private::StretchBiasDrawMode == Private::EStretchBiasDrawMode::WeftStretch;
			auto ConstraintColor =
				[bIsWeftStretch, StretchRangeMinClamped, StretchRangeMaxClamped, StretchRangeInv, &Constraints, &Positions, &SpringConstraints](int32 ConstraintIndex, bool& bOutOfRange)->FColor
			{
				bOutOfRange = false;

				// TODO: make these configurable
				constexpr float OutOfRangeMinHue = 240.f; // blue
				constexpr float MinHue = 180.f; // cyan
				constexpr float MaxHue = 60.f; // yellow
				constexpr float OutOfRangeMaxHue = 0.f; // red
				constexpr float StretchedValue = 1.f;
				constexpr float CompressedValue = 0.5;

				const TVec3<int32>& Constraint = Constraints[ConstraintIndex];
				const Softs::FSolverVec3& P0 = Positions[Constraint[0]];
				const Softs::FSolverVec3& P1 = Positions[Constraint[1]];
				const Softs::FSolverVec3& P2 = Positions[Constraint[2]];
				Softs::FSolverVec3 dX_dU, dX_dV;
				SpringConstraints->CalculateUVStretch(ConstraintIndex, P0, P1, P2, dX_dU, dX_dV);
				const Softs::FSolverReal Stretch = bIsWeftStretch ? dX_dV.Length() : dX_dU.Length();

				const int32 WarpWeftIndex = bIsWeftStretch ? 1 : 0;

				const Softs::FSolverVec2 StretchScale = SpringConstraints->GetWarpWeftScale(ConstraintIndex);
				const Softs::FSolverReal RestStretch = SpringConstraints->GetRestStretchLengths()[ConstraintIndex][WarpWeftIndex] * StretchScale[WarpWeftIndex];
				const Softs::FSolverReal RestStretchInv = (Softs::FSolverReal)1. / FMath::Max(RestStretch, UE_KINDA_SMALL_NUMBER);

				const float StretchRatio = (Stretch - RestStretch) * RestStretchInv;
				if (StretchRatio < StretchRangeMinClamped)
				{
					bOutOfRange = true;
					return FLinearColor(OutOfRangeMinHue, 1.f, StretchRatio < 0 ? CompressedValue : StretchedValue).HSVToLinearRGB().ToFColor(true);
				}
				if (StretchRatio > StretchRangeMaxClamped)
				{
					bOutOfRange = true;
					return FLinearColor(OutOfRangeMaxHue, 1.f, StretchRatio < 0 ? CompressedValue : StretchedValue).HSVToLinearRGB().ToFColor(true);
				}

				// Convert from [StretchRangeMinClamped, StretchRangeMaxClamped] --> [MinHue, MaxHue]
				const float Hue = (MinHue + (MaxHue - MinHue) * (StretchRatio - StretchRangeMinClamped) * StretchRangeInv);
				return FLinearColor(Hue, 1.f, StretchRatio < 0 ? CompressedValue : StretchedValue).HSVToLinearRGB().ToFColor(true);
			};

			FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
			int32 VertexIndex = 0;
			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
			{

				bool bIsOutOfRange;
				const FColor DrawColor = ConstraintColor(ConstraintIndex, bIsOutOfRange);
				if (bIsOutOfRange && !Private::bStretchBiasDrawOutOfRange)
				{
					continue;
				}

				const TVec3<int32>& Constraint = Constraints[ConstraintIndex];
				const FVector3f Pos0 = FVector3f(Positions[Constraint[0]]);
				const FVector3f Pos1 = FVector3f(Positions[Constraint[1]]);
				const FVector3f Pos2 = FVector3f(Positions[Constraint[2]]);
				const FVector3f Normal = FVector3f::CrossProduct(Pos2 - Pos0, Pos1 - Pos0).GetSafeNormal();
				const FVector3f Tangent = ((Pos1 + Pos2) * 0.5f - Pos0).GetSafeNormal();

				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos0, Tangent, Normal, FVector2f(0.f, 0.f), DrawColor));
				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos1, Tangent, Normal, FVector2f(0.f, 1.f), DrawColor));
				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos2, Tangent, Normal, FVector2f(1.f, 1.f), DrawColor));
				MeshBuilder.AddTriangle(VertexIndex, VertexIndex + 1, VertexIndex + 2);
				VertexIndex += 3;
			}

			FMatrix LocalSimSpaceToWorld(FMatrix::Identity);
			LocalSimSpaceToWorld.SetOrigin(LocalSpaceLocation);
			MeshBuilder.Draw(PDI, LocalSimSpaceToWorld, MaterialRenderProxy, SDPG_World, false, false);
		}
#endif
	}

	static void DrawStretchBiasConstraints_BiasStretch(FPrimitiveDrawInterface* PDI, const TConstArrayView<::Chaos::Softs::FSolverVec3>& Positions, const ::Chaos::FVec3& LocalSpaceLocation, const FMaterialRenderProxy* MaterialRenderProxy, const Softs::FXPBDStretchBiasElementConstraints* const SpringConstraints)
	{

#if WITH_EDITOR
		if (PDI && MaterialRenderProxy)
		{
			const TArray<TVec3<int32>>& Constraints = SpringConstraints->GetConstraints();
			auto ConstraintColor = [&Constraints, &Positions, &SpringConstraints](int32 ConstraintIndex)->FColor
			{
				const TVec3<int32>& Constraint = Constraints[ConstraintIndex];
				const Softs::FSolverVec3& P0 = Positions[Constraint[0]];
				const Softs::FSolverVec3& P1 = Positions[Constraint[1]];
				const Softs::FSolverVec3& P2 = Positions[Constraint[2]];
				Softs::FSolverVec3 dX_dU, dX_dV;
				SpringConstraints->CalculateUVStretch(ConstraintIndex, P0, P1, P2, dX_dU, dX_dV);
				
				const Softs::FSolverVec3 dX_dU_normalized = dX_dU.GetSafeNormal();
				const Softs::FSolverVec3 dX_dV_normalized = dX_dV.GetSafeNormal();
				const Softs::FSolverReal Shear = FMath::Abs(Softs::FSolverVec3::DotProduct(dX_dU_normalized, dX_dV_normalized));

				constexpr float UndeformedHue = 240.f; // blue
				constexpr float MaxDeformedHue = 360.f; // red

				return FLinearColor(UndeformedHue + Shear * (MaxDeformedHue - UndeformedHue), 1.f, 1.f).HSVToLinearRGB().ToFColor(true);
			};

			FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
			int32 VertexIndex = 0;
			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex, VertexIndex += 3)
			{
				const FColor DrawColor = ConstraintColor(ConstraintIndex);

				const TVec3<int32>& Constraint = Constraints[ConstraintIndex];
				const FVector3f Pos0 = FVector3f(Positions[Constraint[0]]);
				const FVector3f Pos1 = FVector3f(Positions[Constraint[1]]);
				const FVector3f Pos2 = FVector3f(Positions[Constraint[2]]);
				const FVector3f Normal = FVector3f::CrossProduct(Pos2 - Pos0, Pos1 - Pos0).GetSafeNormal();
				const FVector3f Tangent = ((Pos1 + Pos2) * 0.5f - Pos0).GetSafeNormal();

				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos0, Tangent, Normal, FVector2f(0.f, 0.f), DrawColor));
				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos1, Tangent, Normal, FVector2f(0.f, 1.f), DrawColor));
				MeshBuilder.AddVertex(FDynamicMeshVertex(Pos2, Tangent, Normal, FVector2f(1.f, 1.f), DrawColor));
				MeshBuilder.AddTriangle(VertexIndex, VertexIndex + 1, VertexIndex + 2);
			}

			FMatrix LocalSimSpaceToWorld(FMatrix::Identity);
			LocalSimSpaceToWorld.SetOrigin(LocalSpaceLocation);
			MeshBuilder.Draw(PDI, LocalSimSpaceToWorld, MaterialRenderProxy, SDPG_World, false, false);
		}
#endif
	}

	template<typename ConstraintType>
	static void DrawEdgeAnisotropy(FPrimitiveDrawInterface* PDI, const TConstArrayView<::Chaos::Softs::FSolverVec3>& Positions, const ::Chaos::FVec3& LocalSpaceLocation, const ConstraintType* const BendingConstraints)
	{
		const auto& Constraints = BendingConstraints->GetConstraints(); // auto because this could be Vec2 or Vec4, but we always just care about first two indices
		const TArray<Softs::FSolverVec3>& WarpWeftBiasBaseMultipliers = BendingConstraints->GetWarpWeftBiasBaseMultipliers();
		for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
		{
			const Softs::FSolverVec3& P1 = Positions[Constraints[ConstraintIndex][0]];
			const Softs::FSolverVec3& P2 = Positions[Constraints[ConstraintIndex][1]];
			const Softs::FSolverVec3& Multiplier = WarpWeftBiasBaseMultipliers[ConstraintIndex];

			const FVec3 Pos0 = FVec3(P1) + LocalSpaceLocation;
			const FVec3 Pos1 = FVec3(P2) + LocalSpaceLocation;
			DrawLine(PDI, Pos0, Pos1, FLinearColor(Multiplier[0], Multiplier[1], Multiplier[2]));
		}
	}

	template<typename ConstraintType>
	static void DrawAxialSpringAnisotropy(FPrimitiveDrawInterface* PDI, const TConstArrayView<::Chaos::Softs::FSolverVec3>& Positions, const ::Chaos::FVec3& LocalSpaceLocation, const ConstraintType* const AxialConstraints)
	{
		const TArray<TVec3<int32>>& Constraints = AxialConstraints->GetConstraints();
		const TArray<Softs::FSolverVec3>& WarpWeftBiasBaseMultipliers = AxialConstraints->GetWarpWeftBiasBaseMultipliers();
		const TArray<Softs::FSolverReal>& Barys = AxialConstraints->GetBarys();
		for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
		{
			const Softs::FSolverVec3& P1 = Positions[Constraints[ConstraintIndex][0]];
			const Softs::FSolverVec3& P2 = Positions[Constraints[ConstraintIndex][1]];
			const Softs::FSolverVec3& P3 = Positions[Constraints[ConstraintIndex][2]];
			const Softs::FSolverVec3 P = Barys[ConstraintIndex] * P2 + ((FSolverReal)1. - Barys[ConstraintIndex]) * P3;
			const Softs::FSolverVec3& Multiplier = WarpWeftBiasBaseMultipliers[ConstraintIndex];

			const FVec3 Pos0 = FVec3(P1) + LocalSpaceLocation;
			const FVec3 Pos1 = FVec3(P) + LocalSpaceLocation;
			DrawLine(PDI, Pos0, Pos1, FLinearColor(Multiplier[0], Multiplier[1], Multiplier[2]));
		}
	}

	template<typename SpringConstraintType>
	static void DrawAxialSpringConstraintColors(FPrimitiveDrawInterface* PDI, const TConstArrayView<::Chaos::Softs::FSolverVec3>& Positions, const ::Chaos::FVec3& LocalSpaceLocation, const SpringConstraintType* const SpringConstraints)
	{
		DrawSpringConstraintColors(PDI, LocalSpaceLocation, SpringConstraints, [&SpringConstraints, &Positions](const int32 ConstraintIndex, Softs::FSolverVec3& P1, Softs::FSolverVec3& P2)
		{
			P1 = Positions[SpringConstraints->GetConstraints()[ConstraintIndex][0]];
			const Softs::FSolverReal Bary = SpringConstraints->GetBarys()[ConstraintIndex];
			
			P2 = Bary * Positions[SpringConstraints->GetConstraints()[ConstraintIndex][1]] + ((FSolverReal)1. - Bary)* Positions[SpringConstraints->GetConstraints()[ConstraintIndex][2]];
		});
	}

	void FClothVisualization::DrawEdgeConstraint(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);

			// Constraints are locally indexed for new solver
			const TConstArrayView<Softs::FSolverVec3> Positions = Solver->IsLegacySolver() ? TConstArrayView<Softs::FSolverVec3>(Solver->GetParticleXs()) : Solver->GetParticleXsView(ParticleRangeId);

			if (const Softs::FPBDEdgeSpringConstraints* const EdgeConstraints = ClothConstraints.GetEdgeSpringConstraints().Get())
			{
				DrawSpringConstraintColors(PDI, Positions, LocalSpaceLocation, EdgeConstraints);
			}

			if (const Softs::FXPBDEdgeSpringConstraints* const EdgeConstraints = ClothConstraints.GetXEdgeSpringConstraints().Get())
			{
				DrawSpringConstraintColors(PDI, Positions, LocalSpaceLocation, EdgeConstraints);
			}

			if (const Softs::FXPBDAnisotropicSpringConstraints* const AnisoSpringConstraints = ClothConstraints.GetXAnisoSpringConstraints().Get())
			{
				switch ((Private::EAnisoSpringDrawMode)Private::AnisoSpringDrawMode)
				{
				case Private::EAnisoSpringDrawMode::Anisotropy:
					DrawEdgeAnisotropy(PDI, Positions, LocalSpaceLocation, &AnisoSpringConstraints->GetEdgeConstraints());
					DrawAxialSpringAnisotropy(PDI, Positions, LocalSpaceLocation, &AnisoSpringConstraints->GetAxialConstraints());
					break;
				case Private::EAnisoSpringDrawMode::ParallelGraphColor: // fallthrough
				default:
					DrawSpringConstraintColors(PDI, Positions, LocalSpaceLocation, &AnisoSpringConstraints->GetEdgeConstraints());
					DrawAxialSpringConstraintColors(PDI, Positions, LocalSpaceLocation, &AnisoSpringConstraints->GetAxialConstraints());
					break;
				}
			}

			if (const Softs::FXPBDStretchBiasElementConstraints* const StretchConstraints = ClothConstraints.GetXStretchBiasConstraints().Get())
			{
				const FMaterialRenderProxy* MaterialRenderProxy =
#if WITH_EDITOR
					ClothMaterialColor->GetRenderProxy();
#else
					nullptr;
#endif
				switch ((Private::EStretchBiasDrawMode)Private::StretchBiasDrawMode)
				{
				case Private::EStretchBiasDrawMode::WarpStretch: // fallthrough
				case Private::EStretchBiasDrawMode::WeftStretch:
					DrawStretchBiasConstraints_WarpWeftStretch(PDI, Positions, LocalSpaceLocation, MaterialRenderProxy, StretchConstraints);
					break;
				case Private::EStretchBiasDrawMode::BiasStretch:
					DrawStretchBiasConstraints_BiasStretch(PDI, Positions, LocalSpaceLocation, MaterialRenderProxy, StretchConstraints);
					break;
				case Private::EStretchBiasDrawMode::ParallelGraphColor: // fallthrough
				default:
					DrawStretchBiasConstraints_ParallelGraphColor(PDI, Positions, LocalSpaceLocation, MaterialRenderProxy, StretchConstraints);
				}

			}
		}
	}

	static void DrawBendingElementBuckleStatus(FPrimitiveDrawInterface* PDI, const TConstArrayView<::Chaos::Softs::FSolverVec3>& Positions, const ::Chaos::FVec3& LocalSpaceLocation, const ::Chaos::Softs::FPBDBendingConstraintsBase* const BendingConstraints)
	{
		const TArray<TVec4<int32>>& Constraints = BendingConstraints->GetConstraints();
		const TArray<bool>& IsBuckled = BendingConstraints->GetIsBuckled();

		// Color constraint edge with red or blue: Red = Buckled, Blue = Not Buckled. 
		for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
		{
			const Softs::FSolverVec3& P1 = Positions[Constraints[ConstraintIndex][0]];
			const Softs::FSolverVec3& P2 = Positions[Constraints[ConstraintIndex][1]];

			const bool bIsBuckled = IsBuckled.IsValidIndex(ConstraintIndex) ? IsBuckled[ConstraintIndex] : false; // IsBuckled is empty if the simulation is paused.

			const FVec3 Pos0 = FVec3(P1) + LocalSpaceLocation;
			const FVec3 Pos1 = FVec3(P2) + LocalSpaceLocation;
			DrawLine(PDI, Pos0, Pos1, bIsBuckled ? FLinearColor::Red : FLinearColor::Blue);
		}
	}



	static void DrawBendingElementRestAngle(FPrimitiveDrawInterface* PDI, const TConstArrayView<::Chaos::Softs::FSolverVec3>& Positions, const ::Chaos::FVec3& LocalSpaceLocation, const ::Chaos::Softs::FPBDBendingConstraintsBase* const BendingConstraints)
	{
		const TArray<TVec4<int32>>& Constraints = BendingConstraints->GetConstraints();
		const TArray<Softs::FSolverReal>& RestAngles = BendingConstraints->GetRestAngles();
		for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
		{
			const Softs::FSolverVec3& P1 = Positions[Constraints[ConstraintIndex][0]];
			const Softs::FSolverVec3& P2 = Positions[Constraints[ConstraintIndex][1]];
			const Softs::FSolverReal RestAngle = RestAngles[ConstraintIndex];
			const uint8 ColorSat = (uint8)FMath::Clamp(FMath::Abs(RestAngle) / UE_PI * 256, 0, 255);

			const FVec3 Pos0 = FVec3(P1) + LocalSpaceLocation;
			const FVec3 Pos1 = FVec3(P2) + LocalSpaceLocation;
			DrawLine(PDI, Pos0, Pos1, FLinearColor::MakeFromHSV8(RestAngle > 0 ? 170 : 0, ColorSat, 255));
		}
	}

	void FClothVisualization::DrawBendingConstraint(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);


			// Constraints are locally indexed for new solver
			const TConstArrayView<Softs::FSolverVec3> Positions = Solver->IsLegacySolver() ? TConstArrayView<Softs::FSolverVec3>(Solver->GetParticleXs()) : Solver->GetParticleXsView(ParticleRangeId);

			if (const Softs::FPBDBendingSpringConstraints* const BendingConstraints = ClothConstraints.GetBendingSpringConstraints().Get())
			{
				DrawSpringConstraintColors(PDI, Positions, LocalSpaceLocation, BendingConstraints);
			}

			if (const Softs::FXPBDBendingSpringConstraints* const BendingConstraints = ClothConstraints.GetXBendingSpringConstraints().Get())
			{
				DrawSpringConstraintColors(PDI, Positions, LocalSpaceLocation, BendingConstraints);
			}

			if (const Softs::FPBDBendingConstraints* const BendingConstraints = ClothConstraints.GetBendingElementConstraints().Get())
			{
				switch ((Private::EBendingDrawMode)Private::BendingDrawMode)
				{
				case Private::EBendingDrawMode::RestAngle:
					DrawBendingElementRestAngle(PDI, Positions, LocalSpaceLocation, BendingConstraints);
					break;
				case Private::EBendingDrawMode::ParallelGraphColor:
					DrawSpringConstraintColors(PDI, Positions, LocalSpaceLocation, BendingConstraints);
					break;
				case Private::EBendingDrawMode::BuckleStatus:
				default:
					DrawBendingElementBuckleStatus(PDI, Positions, LocalSpaceLocation, BendingConstraints);
					break;
				}
			}

			if (const Softs::FXPBDBendingConstraints* const BendingConstraints = ClothConstraints.GetXBendingElementConstraints().Get())
			{
				switch ((Private::EBendingDrawMode)Private::BendingDrawMode)
				{
				case Private::EBendingDrawMode::RestAngle:
					DrawBendingElementRestAngle(PDI, Positions, LocalSpaceLocation, BendingConstraints);
					break;
				case Private::EBendingDrawMode::ParallelGraphColor:
					DrawSpringConstraintColors(PDI, Positions, LocalSpaceLocation, BendingConstraints);
					break;
				case Private::EBendingDrawMode::BuckleStatus:
				default:
					DrawBendingElementBuckleStatus(PDI, Positions, LocalSpaceLocation, BendingConstraints);
					break;
				}
			}

			if (const Softs::FXPBDAnisotropicBendingConstraints* const BendingConstraints = ClothConstraints.GetXAnisoBendingElementConstraints().Get())
			{
				switch ((Private::EBendingDrawMode)Private::BendingDrawMode)
				{
				case Private::EBendingDrawMode::RestAngle:
					DrawBendingElementRestAngle(PDI, Positions, LocalSpaceLocation, BendingConstraints);
					break;
				case Private::EBendingDrawMode::ParallelGraphColor:
					DrawSpringConstraintColors(PDI, Positions, LocalSpaceLocation, BendingConstraints);
					break;
				case Private::EBendingDrawMode::Anisotropy:
					DrawEdgeAnisotropy(PDI, Positions, LocalSpaceLocation, BendingConstraints);
					break;
				case Private::EBendingDrawMode::BuckleStatus:
				default:
					DrawBendingElementBuckleStatus(PDI, Positions, LocalSpaceLocation, BendingConstraints);
					break;
				}
			}
		}
	}

	void FClothVisualization::DrawLongRangeConstraint(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		auto PseudoRandomColor =
			[](int32 NumColorRotations) -> FLinearColor
			{
				static const uint8 Spread = 157;  // Prime number that gives a good spread of colors without getting too similar as a rand might do.
				uint8 Seed = Spread;
				for (int32 i = 0; i < NumColorRotations; ++i)
				{
					Seed += Spread;
				}
				return FLinearColor::MakeFromHSV8(Seed, 160, 128);
			};

		auto Darken =
			[](const FLinearColor& Color) -> FLinearColor
			{
				FLinearColor ColorHSV = Color.LinearRGBToHSV();
				ColorHSV.B *= .5f;
				return ColorHSV.HSVToLinearRGB();
			};

		int32 ColorOffset = 0;

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);
			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);

			if (const Softs::FPBDLongRangeConstraints* const LongRangeConstraints = ClothConstraints.GetLongRangeConstraints().Get())
			{
				const TArray<TConstArrayView<Softs::FPBDLongRangeConstraints::FTether>>& Tethers = LongRangeConstraints->GetTethers();

				for (int32 BatchIndex = 0; BatchIndex < Tethers.Num(); ++BatchIndex)
				{
					const FLinearColor Color = PseudoRandomColor(ColorOffset + BatchIndex);
					const FLinearColor DarkenedColor = Darken(Color);

					const TConstArrayView<Softs::FPBDLongRangeConstraints::FTether>& TetherBatch = Tethers[BatchIndex];

					// Draw tethers
					for (const Softs::FPBDLongRangeConstraints::FTether& Tether : TetherBatch)
					{
						const int32 KinematicIndex = LongRangeConstraints->GetStartIndex(Tether);
						const int32 DynamicIndex = LongRangeConstraints->GetEndIndex(Tether);
						const FReal TargetLength = LongRangeConstraints->GetTargetLength(Tether);

						const FVec3 Pos0 = FVec3(Positions[KinematicIndex]) + LocalSpaceLocation;
						const FVec3 Pos1 = FVec3(Positions[DynamicIndex]) + LocalSpaceLocation;

						DrawLine(PDI, Pos0, Pos1, Color);
#if WITH_EDITOR
						DrawPoint(PDI, Pos1, Color, ClothMaterialVertex);
#else
						DrawPoint(nullptr, Pos1, Color, nullptr);
#endif
						FVec3 Direction = Pos1 - Pos0;
						const float Length = Direction.SafeNormalize();
						if (Length > SMALL_NUMBER)
						{
							const FVec3 Pos2 = Pos1 + Direction * (TargetLength - Length);
							DrawLine(PDI, Pos1, Pos2, DarkenedColor);
						}
					}
				}

				// Rotate the colors for each cloth
				ColorOffset += Tethers.Num();
			}
		}
	}

	void FClothVisualization::DrawWindAndPressureForces(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		constexpr FReal ForceLength = (FReal)10.;
		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const Softs::FVelocityAndPressureField* VelocityField = nullptr;
			if (!Solver->IsLegacySolver())
			{
				const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);
				VelocityField = ClothConstraints.GetVelocityAndPressureField().Get();
				if (!VelocityField)
				{
					continue;
				}
			}
			else
			{
				VelocityField = &Solver->GetWindVelocityAndPressureField(Cloth->GetGroupId());
			}

			// Constraints are locally indexed for new solver
			const TConstArrayView<Softs::FSolverVec3> Positions = Solver->IsLegacySolver() ? TConstArrayView<Softs::FSolverVec3>(Solver->GetParticleXs()) : Solver->GetParticleXsView(ParticleRangeId);
			const TConstArrayView<Softs::FSolverReal> InvMasses = Solver->IsLegacySolver() ? TConstArrayView<Softs::FSolverReal>(Solver->GetParticleInvMasses()) : Solver->GetParticleInvMassesView(ParticleRangeId);

			const TConstArrayView<TVec3<int32>>& Elements = VelocityField->GetElements();
			const TConstArrayView<Softs::FSolverVec3> Forces = VelocityField->GetForces();
			check(InvMasses.Num() == Positions.Num());

			for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
			{
				const TVec3<int32>& Element = Elements[ElementIndex];
				const FVec3 Position = LocalSpaceLocation + (
					FVec3(Positions[Element.X]) +
					FVec3(Positions[Element.Y]) +
					FVec3(Positions[Element.Z])) / (FReal)3.;

				const bool bIsKinematic0 = !InvMasses[Element.X];
				const bool bIsKinematic1 = !InvMasses[Element.Y];
				const bool bIsKinematic2 = !InvMasses[Element.Z];
				const bool bIsKinematic = bIsKinematic0 || bIsKinematic1 || bIsKinematic2;

				const FVec3 Force = FVec3(Forces[ElementIndex]) * ForceLength;
				DrawLine(PDI, Position, Position + Force, bIsKinematic ? FColor::Cyan : FColor::Green);
			}
		}
	}

	void FClothVisualization::DrawLocalSpace(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		// Draw local space
		DrawCoordinateSystem(PDI, FQuat::Identity, Solver->GetLocalSpaceLocation());

		// Draw reference spaces
		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}
			const FRigidTransform3& ReferenceSpaceTransform = Cloth->GetReferenceSpaceTransform();
			DrawCoordinateSystem(PDI, ReferenceSpaceTransform.GetRotation(), ReferenceSpaceTransform.GetLocation());
		}
	}

	void FClothVisualization::DrawSelfCollision(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);

			if (const Softs::FPBDCollisionSpringConstraints* const SelfCollisionConstraints = ClothConstraints.GetSelfCollisionConstraints().Get())
			{
				// Constraints are locally indexed for new solver
				const TConstArrayView<Softs::FSolverVec3> Positions = Solver->IsLegacySolver() ? TConstArrayView<Softs::FSolverVec3>(Solver->GetParticleXs()) : Solver->GetParticleXsView(ParticleRangeId);
				const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;
				const TArray<TVec4<int32>>& Constraints = SelfCollisionConstraints->GetConstraints();
				const TArray<Softs::FSolverVec3>& Barys = SelfCollisionConstraints->GetBarys();
				const TArray<bool>& FlipNormals = SelfCollisionConstraints->GetFlipNormals();

				for (int32 Index = 0; Index < Constraints.Num(); ++Index)
				{
					const FReal Height = (FReal)SelfCollisionConstraints->GetConstraintThickness(Index);
					const TVec4<int32>& Constraint = Constraints[Index];
					const FVec3 Bary(Barys[Index]);

					const FVector P = LocalSpaceLocation + FVector(Positions[Constraint[0]]);
					const FVector P0 = LocalSpaceLocation + FVector(Positions[Constraint[1]]);
					const FVector P1 = LocalSpaceLocation + FVector(Positions[Constraint[2]]);
					const FVector P2 = LocalSpaceLocation + FVector(Positions[Constraint[3]]);

					const FVector Pos0 = P0 * Bary[0] + P1 * Bary[1] + P2 * Bary[2];

					static const FLinearColor Brown(0.1f, 0.05f, 0.f);
					static const FLinearColor Red(0.3f, 0.f, 0.f);
					const FTriangle Triangle(P0, P1, P2);
					const FVector Normal = FlipNormals[Index] ? -Triangle.GetNormal() : Triangle.GetNormal();

					// Draw point to surface line (=normal)
					const FVector Pos1 = Pos0 + Height * Normal;
					DrawPoint(PDI, Pos0, Brown, nullptr, 2.f);
					DrawLine(PDI, Pos0, Pos1, FlipNormals[Index] ? Red : Brown);

					// Draw pushup to point
					static const FLinearColor Orange(0.3f, 0.15f, 0.f);
					DrawPoint(PDI, P, Orange, nullptr, 2.f);
					DrawLine(PDI, Pos1, P, Orange);
				}

				const TArray<int32>& KinematicCollidingParticles = SelfCollisionConstraints->GetKinematicCollidingParticles();
				const TArray<TMap<int32, Softs::FSolverReal>>& KinematicColliderTimers = SelfCollisionConstraints->GetKinematicColliderTimers();
				const FTriangleMesh& TriangleMesh = SelfCollisionConstraints->GetTriangleMesh();
				for (int32 Index1 : KinematicCollidingParticles)
				{
					const FVector P = LocalSpaceLocation + FVector(Positions[Index1]);

					static const FLinearColor Orange(0.3f, 0.15f, 0.f);
					DrawPoint(PDI, P, Orange, nullptr, 2.f);

					const TMap<int32, Softs::FSolverReal>& Timers = KinematicColliderTimers[Index1 - Offset];
					for (const TPair<int32, Softs::FSolverReal>& ElemAndTimer : Timers)
					{
						const int32 Index2 = TriangleMesh.GetElements()[ElemAndTimer.Get<0>()][0];
						const int32 Index3 = TriangleMesh.GetElements()[ElemAndTimer.Get<0>()][1];
						const int32 Index4 = TriangleMesh.GetElements()[ElemAndTimer.Get<0>()][2];

						const Softs::FSolverVec3& P1 = Positions[Index1];
						const Softs::FSolverVec3& P2 = Positions[Index2];
						const Softs::FSolverVec3& P3 = Positions[Index3];
						const Softs::FSolverVec3& P4 = Positions[Index4];
						Softs::FSolverVec3 Bary;
						const FVector Pos1 = LocalSpaceLocation + FVector(FindClosestPointAndBaryOnTriangle(P2, P3, P4, P1, Bary));

						static const FLinearColor LtRed(0.6f, 0.f, 0.f);
						static const FLinearColor DkRed(0.3f, 0.f, 0.f);
						const FLinearColor& Color = ElemAndTimer.Get<1>() > 0.f ? LtRed : DkRed;
						DrawPoint(PDI, Pos1, Color, nullptr, 2.f);
						DrawLine(PDI, Pos1, P, Color);
					}
				}
			}

			if (const Softs::FPBDSelfCollisionSphereConstraints* const SelfCollisionSphereConstraints =
				ClothConstraints.GetSelfCollisionSphereConstraints().Get())
			{
				// Constraints are locally indexed for new solver
				const TConstArrayView<Softs::FSolverVec3> Positions = Solver->IsLegacySolver() ? TConstArrayView<Softs::FSolverVec3>(Solver->GetParticleXs()) : Solver->GetParticleXsView(ParticleRangeId);
				const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;
				const TArray<TVec2<int32>>& Constraints = SelfCollisionSphereConstraints->GetConstraints();
				for (int32 Index = 0; Index < Constraints.Num(); ++Index)
				{
					const TVec2<int32>& Constraint = Constraints[Index];
					const FVector P0 = LocalSpaceLocation + FVector(Positions[Constraint[0]]);
					const FVector P1 = LocalSpaceLocation + FVector(Positions[Constraint[1]]);
					static const FLinearColor Brown(0.1f, 0.05f, 0.f);
					DrawLine(PDI, P0, P1, Brown);
				}

				if (const TSet<int32>* const VertexSet = SelfCollisionSphereConstraints->GetVertexSet())
				{
					const FReal Radius = (FReal)SelfCollisionSphereConstraints->GetRadius();
					for (const int32 Vertex : *VertexSet)
					{
						const FVector P0 = LocalSpaceLocation + FVector(Positions[Vertex + Offset]);
						DrawSphere(PDI, TSphere<FReal, 3>(FVector::ZeroVector, Radius), FQuat::Identity,
							P0, FLinearColor(FColor::Orange));
					}
				}
			}
		}
	}

	void FClothVisualization::DrawSelfIntersection(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);

			static const FLinearColor Red(1.f, 0.f, 0.f);
			static const FLinearColor White(1.f, 1.f, 1.f);
			static const FLinearColor Black(0.f, 0.f, 0.f);
			static const FLinearColor Teal(0.f, 0.5f, 0.5f);
			static const FLinearColor Orange(1.f, .5f, 0.f);
			static const FLinearColor Green(0.f, 1.f, 0.f);
			static const FLinearColor Yellow(1.f, 1.f, 0.f);
			static const FLinearColor Blue(0.f, 0.f, 1.f);

			if (const Softs::FPBDTriangleMeshCollisions* const SelfCollisionInit = ClothConstraints.GetSelfCollisionInit().Get())
			{
				// Constraints are locally indexed for new solver
				const TConstArrayView<Softs::FSolverVec3> Positions = Solver->IsLegacySolver() ? TConstArrayView<Softs::FSolverVec3>(Solver->GetParticleXs()) : Solver->GetParticleXsView(ParticleRangeId);
				const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;

				const FTriangleMesh& TriangleMesh = Cloth->GetTriangleMesh(Solver);

				// Draw contours
				const TArray<TArray<Softs::FPBDTriangleMeshCollisions::FBarycentricPoint>>& ContourPoints = SelfCollisionInit->GetIntersectionContourPoints();
				const TArray<TArray<Softs::FPBDTriangleMeshCollisions::FBarycentricPoint>>& PostStepContourPoints = SelfCollisionInit->GetPostStepIntersectionContourPoints();
				const TArray<Softs::FPBDTriangleMeshCollisions::FContourType>& ContourTypes = SelfCollisionInit->GetIntersectionContourTypes();
				check(ContourPoints.Num() == ContourTypes.Num());

				static const FLinearColor ColorsForType[(int8)Softs::FPBDTriangleMeshCollisions::FContourType::Count] =
				{
					Teal,
					Red,
					Blue,
					Yellow,
					White,
					Black
				};

				auto DrawContour = [&LocalSpaceLocation, PDI, &Positions](const TArray<Softs::FPBDTriangleMeshCollisions::FBarycentricPoint>& Contour,
					const FLinearColor& ContourColor)
				{
					for (int32 PointIdx = 0; PointIdx < Contour.Num() - 1; ++PointIdx)
					{
						const Softs::FPBDTriangleMeshCollisions::FBarycentricPoint& Point0 = Contour[PointIdx];
						const FVector EndPoint0 = LocalSpaceLocation + (1.f - Point0.Bary[0] - Point0.Bary[1]) * Positions[Point0.Vertices[0]] + Point0.Bary[0] * Positions[Point0.Vertices[1]] + Point0.Bary[1] * Positions[Point0.Vertices[2]];
						const Softs::FPBDTriangleMeshCollisions::FBarycentricPoint& Point1 = Contour[PointIdx + 1];
						const FVector EndPoint1 = LocalSpaceLocation + (1.f - Point1.Bary[0] - Point1.Bary[1]) * Positions[Point1.Vertices[0]] + Point1.Bary[0] * Positions[Point1.Vertices[1]] + Point1.Bary[1] * Positions[Point1.Vertices[2]];
						DrawLine(PDI, EndPoint0, EndPoint1, ContourColor);
						DrawPoint(PDI, EndPoint0, ContourColor, nullptr, 1.f);
						DrawPoint(PDI, EndPoint1, ContourColor, nullptr, 1.f);
					}

				};

				for( int32 ContourIndex = 0; ContourIndex < ContourPoints.Num(); ++ContourIndex)
				{
					const TArray<Softs::FPBDTriangleMeshCollisions::FBarycentricPoint>& Contour = ContourPoints[ContourIndex];
					const FLinearColor& ContourColor = ColorsForType[(int8)ContourTypes[ContourIndex]];
					DrawContour(Contour, ContourColor);
				}
				for (int32 ContourIndex = 0; ContourIndex < PostStepContourPoints.Num(); ++ContourIndex)
				{
					const TArray<Softs::FPBDTriangleMeshCollisions::FBarycentricPoint>& Contour = PostStepContourPoints[ContourIndex];
					DrawContour(Contour, Orange);
				}

				// Draw GIA colors
				const TConstArrayView<Softs::FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors = SelfCollisionInit->GetVertexGIAColors();
				static const FLinearColor Gray(0.5f, 0.5f, 0.5f);
				if (VertexGIAColors.Num())
				{
					const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();
					for (int32 ParticleIdx = Offset; ParticleIdx < VertexGIAColors.Num(); ++ParticleIdx)
					{
						if (VertexGIAColors[ParticleIdx].ContourIndexBits)
						{
							const bool bIsLoop = VertexGIAColors[ParticleIdx].IsLoop();
							const bool bIsBoundary = VertexGIAColors[ParticleIdx].IsBoundary();
							const bool bAnyWhite = (VertexGIAColors[ParticleIdx].ContourIndexBits & ~VertexGIAColors[ParticleIdx].ColorBits);
							const bool bAnyBlack = (VertexGIAColors[ParticleIdx].ContourIndexBits & VertexGIAColors[ParticleIdx].ColorBits);
							const FLinearColor& VertColor = bIsLoop ? Red : bIsBoundary ? Blue : (bAnyWhite && bAnyBlack) ? Gray : bAnyWhite ? White : Black;
						
							DrawPoint(PDI, LocalSpaceLocation + Positions[ParticleIdx], VertColor, nullptr, 5.f);
						}
					}
				}
				const TArray<Softs::FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors = SelfCollisionInit->GetTriangleGIAColors();
				if (TriangleGIAColors.Num() == TriangleMesh.GetNumElements())
				{
					const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();
					for (int32 TriangleIdx = 0; TriangleIdx < TriangleGIAColors.Num(); ++TriangleIdx)
					{
						if (TriangleGIAColors[TriangleIdx].ContourIndexBits)
						{
							const bool bIsLoop = TriangleGIAColors[TriangleIdx].IsLoop();
							const bool bAnyWhite = (TriangleGIAColors[TriangleIdx].ContourIndexBits & ~TriangleGIAColors[TriangleIdx].ColorBits);
							const bool bAnyBlack = (TriangleGIAColors[TriangleIdx].ContourIndexBits & TriangleGIAColors[TriangleIdx].ColorBits);
							const FLinearColor& TriColor = bIsLoop ? Red : (bAnyWhite && bAnyBlack) ? Gray : bAnyWhite ? White : Black;
							DrawLine(PDI, LocalSpaceLocation + Positions[Elements[TriangleIdx][0]], LocalSpaceLocation + Positions[Elements[TriangleIdx][1]], TriColor);
							DrawLine(PDI, LocalSpaceLocation + Positions[Elements[TriangleIdx][1]], LocalSpaceLocation + Positions[Elements[TriangleIdx][2]], TriColor);
							DrawLine(PDI, LocalSpaceLocation + Positions[Elements[TriangleIdx][0]], LocalSpaceLocation + Positions[Elements[TriangleIdx][2]], TriColor);
						}
					}
				}

				// Draw contour minimization gradients
				const TArray<Softs::FPBDTriangleMeshCollisions::FContourMinimizationIntersection>& ContourMinimizationIntersections = SelfCollisionInit->GetContourMinimizationIntersections();
				constexpr FReal MaxDrawImpulse = 1.;
				constexpr FReal RegularizeEpsilonSq = 1.;
				for (const Softs::FPBDTriangleMeshCollisions::FContourMinimizationIntersection& Intersection : ContourMinimizationIntersections)
				{
					Softs::FSolverReal GradientLength;
					Softs::FSolverVec3 GradientDir;
					Intersection.GlobalGradientVector.ToDirectionAndLength(GradientDir, GradientLength);
					const FVector Delta = FVector(GradientDir) * MaxDrawImpulse * GradientLength * FMath::InvSqrt(GradientLength * GradientLength + RegularizeEpsilonSq);

					const FVector EdgeCenter = LocalSpaceLocation + .5 * (Positions[Intersection.EdgeVertices[0]] + Positions[Intersection.EdgeVertices[1]]);
					const FVector TriCenter = LocalSpaceLocation + (Positions[Intersection.FaceVertices[0]] + Positions[Intersection.FaceVertices[1]] + Positions[Intersection.FaceVertices[2]]) / 3.;

					DrawPoint(PDI, EdgeCenter, Green, nullptr, 2.f);
					DrawLine(PDI, EdgeCenter, EdgeCenter + Delta, Green);
					DrawPoint(PDI, TriCenter, Green, nullptr, 2.f);
					DrawLine(PDI, TriCenter, TriCenter - Delta, Green);
				}
			}
		}
	}

	void FClothVisualization::DrawSelfCollisionThickness(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

#if WITH_EDITOR
		if (ClothMaterialColor)
		{
			FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());

			for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
			{
				const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
				if (ParticleRangeId == INDEX_NONE)
				{
					continue;
				}

				const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);

				if (const Softs::FPBDCollisionSpringConstraints* const SelfCollisionConstraints = ClothConstraints.GetSelfCollisionConstraints().Get())
				{
					const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;
					const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
					const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver);
					const TConstArrayView<int32>& WeightMap = Cloth->GetFaceIntMapByProperty(Solver, TEXT("SelfCollisionLayers"));

					const TConstArrayView<TVec2<int32>> Edges = Cloth->GetTriangleMesh(Solver).GetSegmentMesh().GetElements();
					const TArray<TVec2<int32>>& EdgeToFaces = Cloth->GetTriangleMesh(Solver).GetEdgeToFaces();
					for(int32 EdgeIndex = 0; EdgeIndex < Edges.Num(); ++EdgeIndex)
					{
						const TVec2<int32>& Edge = Edges[EdgeIndex];
					
						const bool bIsKinematic0 = (InvMasses[Edge[0] - Offset] == (Softs::FSolverReal)0.);
						const bool bIsKinematic1 = (InvMasses[Edge[1] - Offset] == (Softs::FSolverReal)0.);
						if (bIsKinematic0 && bIsKinematic1)
						{
							continue;
						}

						const FVector3f Position1(Positions[Edge[0] - Offset]); // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
						const FVector3f Position2(Positions[Edge[1] - Offset]);

						const FRealSingle Radius1 = (FRealSingle)SelfCollisionConstraints->GetParticleThickness(Edge[0]);
						const FRealSingle Radius2 = (FRealSingle)SelfCollisionConstraints->GetParticleThickness(Edge[1]);
						const int32 Face1Layer = WeightMap.IsValidIndex(EdgeToFaces[EdgeIndex][0]) ? WeightMap[EdgeToFaces[EdgeIndex][0]] : INDEX_NONE;
						const int32 Face2Layer = WeightMap.IsValidIndex(EdgeToFaces[EdgeIndex][1]) ? WeightMap[EdgeToFaces[EdgeIndex][1]] : INDEX_NONE;
						const FLinearColor Color = Chaos::Private::PseudoRandomColor((Face1Layer == Face2Layer || Face2Layer == INDEX_NONE) ? Face1Layer : INDEX_NONE);
						
						AppendTaperedCylinderTriangles(MeshBuilder, Position1, Position2, Radius1, Radius2, 6, Color);
					}
				}
			}
			FMatrix LocalSimSpaceToWorld(FMatrix::Identity);
			LocalSimSpaceToWorld.SetOrigin(Solver->GetLocalSpaceLocation());
			MeshBuilder.Draw(PDI, LocalSimSpaceToWorld, ClothMaterialColor->GetRenderProxy(), SDPG_World, false, false);
		}
		else
#endif
		{
			for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
			{
				const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
				if (ParticleRangeId == INDEX_NONE)
				{
					continue;
				}

				const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);

				if (const Softs::FPBDCollisionSpringConstraints* const SelfCollisionConstraints = ClothConstraints.GetSelfCollisionConstraints().Get())
				{
					const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;
					const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
					const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver);

					const TConstArrayView<TVec2<int32>> Edges = Cloth->GetTriangleMesh(Solver).GetSegmentMesh().GetElements();
					for (const TVec2<int32>& Edge : Edges)
					{
						const bool bIsKinematic0 = (InvMasses[Edge[0] - Offset] == (Softs::FSolverReal)0.);
						const bool bIsKinematic1 = (InvMasses[Edge[1] - Offset] == (Softs::FSolverReal)0.);
						if (bIsKinematic0 && bIsKinematic1)
						{
							continue;
						}

						const FVector Position1(Positions[Edge[0] - Offset]); // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
						const FVector Position2(Positions[Edge[1] - Offset]);

						const FReal Radius1 = (FReal)SelfCollisionConstraints->GetParticleThickness(Edge[0]);
						const FReal Radius2 = (FReal)SelfCollisionConstraints->GetParticleThickness(Edge[1]);
						DrawTaperedCylinder(PDI, Position1 + LocalSpaceLocation, Position2 + LocalSpaceLocation, Radius1, Radius2, 6, FLinearColor::Gray);
					}

					for (int32 VertexIndex = 0; VertexIndex < Positions.Num(); ++VertexIndex)
					{
						const bool bIsKinematic0 = (InvMasses[VertexIndex] == (Softs::FSolverReal)0.);
						if (bIsKinematic0)
						{
							continue;
						}

						const FVector Position1(Positions[VertexIndex]);
						const FReal Radius1 = (FReal)SelfCollisionConstraints->GetParticleThickness(VertexIndex + Offset);
						const FTransform Transform(Position1 + LocalSpaceLocation);
						DrawWireSphere(PDI, Transform, FLinearColor::Gray, Radius1, 6, SDPG_World, 0.0f, 0.001f, false);
					}
				}
			}
		}
	}

	void FClothVisualization::DrawKinematicColliderWired(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver)
		{
			return;
		}

		static const FLinearColor WireframeColor = FColor::Silver;
		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);
			if (const Softs::FPBDTriangleMeshCollisions* const SelfCollisionInit = ClothConstraints.GetSelfCollisionInit().Get())
			{
				const FTriangleMesh& KinematicColliderMesh = SelfCollisionInit->GetCollidableSubMesh().GetKinematicColliderSubMesh();
				// Elements are local indexed for new solver
				const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;
				const TConstArrayView<TVec3<int32>> Elements = KinematicColliderMesh.GetElements();
				const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
				for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
				{
					const TVec3<int32>& Element = Elements[ElementIndex];

					const FVector Pos0 = LocalSpaceLocation + FVector(Positions[Element.X - Offset]); // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
					const FVector Pos1 = LocalSpaceLocation + FVector(Positions[Element.Y - Offset]);
					const FVector Pos2 = LocalSpaceLocation + FVector(Positions[Element.Z - Offset]);

					DrawLine(PDI, Pos0, Pos1, WireframeColor);
					DrawLine(PDI, Pos1, Pos2, WireframeColor);
					DrawLine(PDI, Pos2, Pos0, WireframeColor);
				}
			}
		}
	}

#if WITH_EDITOR
	void FClothVisualization::DrawKinematicColliderShaded(FPrimitiveDrawInterface* PDI) const
	{
		if (!Solver || !CollisionMaterial)
		{
			return;
		}

		FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
		int32 VertexIndex = 0;

		for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
		{
			const int32 ParticleRangeId = Cloth->GetParticleRangeId(Solver);
			if (ParticleRangeId == INDEX_NONE)
			{
				continue;
			}

			const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(ParticleRangeId);
			if (const Softs::FPBDTriangleMeshCollisions* const SelfCollisionInit = ClothConstraints.GetSelfCollisionInit().Get())
			{
				const FTriangleMesh& KinematicColliderMesh = SelfCollisionInit->GetCollidableSubMesh().GetKinematicColliderSubMesh();
				// Elements are local indexed for new solver
				const int32 Offset = Solver->IsLegacySolver() ? ParticleRangeId : 0;
				const TConstArrayView<TVec3<int32>> Elements = KinematicColliderMesh.GetElements();
				const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver);
				for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex, VertexIndex += 3)
				{
					const TVec3<int32>& Element = Elements[ElementIndex];
					const FVector3f Pos0(Positions[Element.X - Offset]); // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
					const FVector3f Pos1(Positions[Element.Y - Offset]);
					const FVector3f Pos2(Positions[Element.Z - Offset]);

					const FVector3f Normal = FVector3f::CrossProduct(Pos2 - Pos0, Pos1 - Pos0).GetSafeNormal();
					const FVector3f Tangent = ((Pos1 + Pos2) * 0.5f - Pos0).GetSafeNormal();

					MeshBuilder.AddVertex(FDynamicMeshVertex(Pos0, Tangent, Normal, FVector2f(0.f, 0.f), FColor::White));
					MeshBuilder.AddVertex(FDynamicMeshVertex(Pos1, Tangent, Normal, FVector2f(0.f, 1.f), FColor::White));
					MeshBuilder.AddVertex(FDynamicMeshVertex(Pos2, Tangent, Normal, FVector2f(1.f, 1.f), FColor::White));
					MeshBuilder.AddTriangle(VertexIndex, VertexIndex + 1, VertexIndex + 2);
				}
			}
		}

		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();
		FMatrix LocalSimSpaceToWorld(FMatrix::Identity);
		LocalSimSpaceToWorld.SetOrigin(Solver->GetLocalSpaceLocation());
		MeshBuilder.Draw(PDI, LocalSimSpaceToWorld, CollisionMaterial->GetRenderProxy(), SDPG_World, false, false);
	}
#endif // #if WITH_EDITOR
}  // End namespace Chaos
#else  // #if CHAOS_DEBUG_DRAW
namespace Chaos
{
	FClothVisualization::FClothVisualization(const ::Chaos::FClothingSimulationSolver* /*InSolver*/) {}

	FClothVisualization::~FClothVisualization() = default;
}  // End namespace UE::Chaos::Cloth
#endif  // #if CHAOS_DEBUG_DRAW
