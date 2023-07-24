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

	class CHAOSCLOTH_API FClothVisualization
#if WITH_EDITOR
		: public FGCObject  // Add garbage collection for cloth material
#endif  // #if WITH_EDITOR
	{
	public:
		explicit FClothVisualization(const ::Chaos::FClothingSimulationSolver* InSolver = nullptr);
		virtual ~FClothVisualization();

#if CHAOS_DEBUG_DRAW
		// Editor & runtime functions
		void SetSolver(const ::Chaos::FClothingSimulationSolver* InSolver);

		void DrawPhysMeshWired(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawAnimMeshWired(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawAnimNormals(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawOpenEdges(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawPointNormals(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawPointVelocities(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawCollision(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawBackstops(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawBackstopDistances(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawMaxDistances(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawAnimDrive(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawEdgeConstraint(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawBendingConstraint(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawLongRangeConstraint(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawWindAndPressureForces(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawLocalSpace(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawSelfCollision(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawSelfIntersection(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawBounds(FPrimitiveDrawInterface* PDI = nullptr) const;
		void DrawGravity(FPrimitiveDrawInterface* PDI = nullptr) const;

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
		void DrawBounds(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}
		void DrawGravity(FPrimitiveDrawInterface* /*PDI*/ = nullptr) const {}

#endif  // #if CHAOS_DEBUG_DRAW

#if WITH_EDITOR && CHAOS_DEBUG_DRAW
		// Editor only functions
		void DrawPhysMeshShaded(FPrimitiveDrawInterface* PDI) const;
		void DrawParticleIndices(FCanvas* Canvas, const FSceneView* SceneView) const;
		void DrawElementIndices(FCanvas* Canvas, const FSceneView* SceneView) const;
		void DrawMaxDistanceValues(FCanvas* Canvas, const FSceneView* SceneView) const;

	protected:
		// FGCObject interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("UE::Chaos::Cloth::FVisualization"); }
		// End of FGCObject interface

#else  // #if WITH_EDITOR && CHAOS_DEBUG_DRAW
		void DrawPhysMeshShaded(FPrimitiveDrawInterface* /*PDI*/) const {}
		void DrawParticleIndices(FCanvas* /*Canvas*/, const FSceneView* /*SceneView*/) const {}
		void DrawElementIndices(FCanvas* /*Canvas*/, const FSceneView* /*SceneView*/) const {}
		void DrawMaxDistanceValues(FCanvas* /*Canvas*/, const FSceneView* /*SceneView*/) const {}
#endif  // #if WITH_EDITOR && CHAOS_DEBUG_DRAW

#if CHAOS_DEBUG_DRAW
	private:
		// Simulation objects
		const ::Chaos::FClothingSimulationSolver* Solver;
#if WITH_EDITOR
		// Visualization material
		TObjectPtr<const UMaterial> ClothMaterial = nullptr;
		TObjectPtr<const UMaterial> ClothMaterialVertex = nullptr;
		TObjectPtr<const UMaterial> CollisionMaterial = nullptr;
#endif  // #if WITH_EDITOR
#endif  // #if CHAOS_DEBUG_DRAW
	};
} // End namespace UE::Chaos::Cloth
