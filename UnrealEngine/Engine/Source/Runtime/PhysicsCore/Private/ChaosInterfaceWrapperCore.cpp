// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosInterfaceWrapperCore.h"

#include "Chaos/Capsule.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ImplicitObjectTransformed.h"
#include "Chaos/ParticleHandle.h"
#include "PhysXPublicCore.h"

namespace ChaosInterface
{
	FORCEINLINE ECollisionShapeType ImplicitTypeToCollisionType(int32 ImplicitObjectType)
	{
		switch (ImplicitObjectType)
		{
		case Chaos::ImplicitObjectType::Sphere: return ECollisionShapeType::Sphere;
		case Chaos::ImplicitObjectType::Box: return ECollisionShapeType::Box;
		case Chaos::ImplicitObjectType::Capsule: return ECollisionShapeType::Capsule;
		case Chaos::ImplicitObjectType::Convex: return ECollisionShapeType::Convex;
		case Chaos::ImplicitObjectType::TriangleMesh: return ECollisionShapeType::Trimesh;
		case Chaos::ImplicitObjectType::HeightField: return ECollisionShapeType::Heightfield;
		default: break;
		}

		return ECollisionShapeType::None;
	}


	ECollisionShapeType GetImplicitType(const Chaos::FImplicitObject& InGeometry)
	{
		using namespace Chaos;
		int32 ResultObjectType = GetInnerType(InGeometry.GetType());
		const Chaos::FImplicitObject* CurrentGeometry = &InGeometry;

		while (CurrentGeometry && ResultObjectType == ImplicitObjectType::Transformed)
		{
			CurrentGeometry = static_cast<const TImplicitObjectTransformed<FReal, 3>*>(CurrentGeometry)->GetGeometry();
			ResultObjectType = GetInnerType(CurrentGeometry->GetType());
		}

		return ImplicitTypeToCollisionType(ResultObjectType);
	}

	Chaos::FReal GetRadius(const Chaos::FCapsule& InCapsule)
	{
		return InCapsule.GetRadius();
	}

	Chaos::FReal GetHalfHeight(const Chaos::FCapsule& InCapsule)
	{
		return InCapsule.GetHeight() / 2.;
	}

	FCollisionFilterData GetQueryFilterData(const Chaos::FPerShapeData& Shape)
	{
		return Shape.GetQueryData();
	}

	FCollisionFilterData GetSimulationFilterData(const Chaos::FPerShapeData& Shape)
	{
		return Shape.GetSimData();
	}


}
