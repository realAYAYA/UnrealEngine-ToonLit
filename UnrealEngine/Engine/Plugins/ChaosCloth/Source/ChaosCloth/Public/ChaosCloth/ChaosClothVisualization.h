// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Build.h"      // For CHAOS_DEBUG_DRAW
#include "Chaos/Declares.h"  //

#if WITH_EDITOR
#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"
#endif  // #if WITH_EDITOR

class FCanvas;
class FSceneView;
class FPrimitiveDrawInterface;
class UMaterial;

namespace Chaos
{
	class FClothingSimulationSolver;

	class FClothVisualization
#if WITH_EDITOR
		: public FGCObject  // Add garbage collection for cloth material
#endif  // #if WITH_EDITOR
	{
	public:
		CHAOSCLOTH_API explicit FClothVisualization(const ::Chaos::FClothingSimulationSolver* InSolver = nullptr);
		CHAOSCLOTH_API virtual ~FClothVisualization();

#if CHAOS_DEBUG_DRAW
		// Editor & runtime functions
		CHAOSCLOTH_API void SetSolver(const ::Chaos::FClothingSimulationSolver* InSolver);

		CHAOSCLOTH_API void DrawPhysMeshWired(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawAnimMeshWired(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawAnimNormals(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawOpenEdges(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawPointNormals(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawPointVelocities(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawCollision(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawBackstops(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawBackstopDistances(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawMaxDistances(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawAnimDrive(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawEdgeConstraint(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawBendingConstraint(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawLongRangeConstraint(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawWindAndPressureForces(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawLocalSpace(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawSelfCollision(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawSelfIntersection(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawSelfCollisionThickness(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawKinematicColliderWired(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawBounds(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawGravity(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawFictitiousAngularForces(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawMultiResConstraint(FPrimitiveDrawInterface* PDI = nullptr) const;

#else  // #if CHAOS_DEBUG_DRAW
		void SetSolver(const ::Chaos::FClothingSimulationSolver* /*InSolver*/) {}

		void DrawPhysMeshWired(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawAnimMeshWired(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawAnimNormals(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawOpenEdges(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawPointNormals(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawPointVelocities(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawCollision(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawBackstops(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawBackstopDistances(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawMaxDistances(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawAnimDrive(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawEdgeConstraint(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawBendingConstraint(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawLongRangeConstraint(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawWindAndPressureForces(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawLocalSpace(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawSelfCollision(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawSelfIntersection(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawSelfCollisionThickness(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawKinematicColliderWired(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawBounds(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawGravity(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawFictitiousAngularForces(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawMultiResConstraint(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}

#endif  // #if CHAOS_DEBUG_DRAW

#if WITH_EDITOR && CHAOS_DEBUG_DRAW
		// Editor only functions
		CHAOSCLOTH_API void DrawPhysMeshShaded(FPrimitiveDrawInterface* PDI) const;
		CHAOSCLOTH_API void DrawParticleIndices(FCanvas* Canvas, const FSceneView* SceneView) const;
		CHAOSCLOTH_API void DrawElementIndices(FCanvas* Canvas, const FSceneView* SceneView) const;
		CHAOSCLOTH_API void DrawMaxDistanceValues(FCanvas* Canvas, const FSceneView* SceneView) const;
		CHAOSCLOTH_API void DrawWeightMap(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawSelfCollisionLayers(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawInpaintWeightsMatched(FPrimitiveDrawInterface* PDI = nullptr) const;
		CHAOSCLOTH_API void DrawKinematicColliderShaded(FPrimitiveDrawInterface* PDI) const;
	protected:
		// FGCObject interface
		CHAOSCLOTH_API virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("UE::Chaos::Cloth::FVisualization"); }
		// End of FGCObject interface

#else  // #if WITH_EDITOR && CHAOS_DEBUG_DRAW
		void DrawPhysMeshShaded(FPrimitiveDrawInterface* /*PDI*/) const {}
		void DrawParticleIndices(FCanvas* /*Canvas*/, const FSceneView* /*SceneView*/) const {}
		void DrawElementIndices(FCanvas* /*Canvas*/, const FSceneView* /*SceneView*/) const {}
		void DrawMaxDistanceValues(FCanvas* /*Canvas*/, const FSceneView* /*SceneView*/) const {}
		CHAOSCLOTH_API void DrawKinematicColliderShaded(FPrimitiveDrawInterface* /*PDI*/) const {}
#endif  // #if WITH_EDITOR && CHAOS_DEBUG_DRAW

#if CHAOS_DEBUG_DRAW
	private:
		// Simulation objects
		const ::Chaos::FClothingSimulationSolver* Solver;
#if WITH_EDITOR
		// Visualization material
		TObjectPtr<const UMaterial> ClothMaterial = nullptr;
		TObjectPtr<const UMaterial> ClothMaterialColor = nullptr;
		TObjectPtr<const UMaterial> ClothMaterialVertex = nullptr;
		TObjectPtr<const UMaterial> CollisionMaterial = nullptr;
		
		void DrawWeightMapWithName(FPrimitiveDrawInterface* PDI, const FString& Name) const;
#endif  // #if WITH_EDITOR
#endif  // #if CHAOS_DEBUG_DRAW
	};
} // End namespace UE::Chaos::Cloth
