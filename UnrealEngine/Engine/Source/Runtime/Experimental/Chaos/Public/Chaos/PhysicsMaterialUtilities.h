// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/Framework/Handles.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/Serializable.h"
#include "Chaos/ShapeInstanceFwd.h"
#include "Templates/UniquePtr.h"

namespace Chaos
{
	class FChaosPhysicsMaterial;

	namespace Private
	{
		// Get the physical material for the specified Shape on the particle.
		// NOTE: each shape may have its own material, but the particle may also have an override.
		extern CHAOS_API const FChaosPhysicsMaterial* GetPhysicsMaterial(
			const FGeometryParticleHandle* Particle,
			const FShapeInstance* Shape,
			const int32 FaceIndex,
			const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>* PhysicsMaterials,
			const TArrayCollectionArray<TUniquePtr<FChaosPhysicsMaterial>>* PerParticlePhysicsMaterials,
			const THandleArray<FChaosPhysicsMaterial>* const SimMaterials);

		// Get the first physical material on the particle (a multi-shape particle may have many)
		extern CHAOS_API const FChaosPhysicsMaterial* GetFirstPhysicsMaterial(
			const FGeometryParticleHandle* Particle,
			const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>* PhysicsMaterials,
			const TArrayCollectionArray<TUniquePtr<FChaosPhysicsMaterial>>* PerParticlePhysicsMaterials,
			const THandleArray<FChaosPhysicsMaterial>* const SimMaterials);
	}
}
