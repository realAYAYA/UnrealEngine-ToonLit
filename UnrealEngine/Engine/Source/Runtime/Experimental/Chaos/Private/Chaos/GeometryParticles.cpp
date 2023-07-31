// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/GeometryParticles.h"

#include "Chaos/CastingUtilities.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/Framework/PhysicsSolverBase.h"

namespace Chaos
{
	void UpdateShapesArrayFromGeometry(FShapesArray& ShapesArray, TSerializablePtr<FImplicitObject> Geometry, const FRigidTransform3& ActorTM, IPhysicsProxyBase* Proxy)
	{
		if(Geometry)
		{
			const int32 OldShapeNum = ShapesArray.Num();
			if(const auto* Union = Geometry->template GetObject<FImplicitObjectUnion>())
			{
				ShapesArray.SetNum(Union->GetObjects().Num());

				for (int32 ShapeIndex = 0; ShapeIndex < ShapesArray.Num(); ++ShapeIndex)
				{
					TSerializablePtr<FImplicitObject> ShapeGeometry = MakeSerializable(Union->GetObjects()[ShapeIndex]);

					if (ShapeIndex >= OldShapeNum)
					{
						// If newly allocated shape, initialize it.
						ShapesArray[ShapeIndex] = FPerShapeData::CreatePerShapeData(ShapeIndex, ShapeGeometry);
					}
					else if (ShapeGeometry != ShapesArray[ShapeIndex]->GetGeometry())
					{
						// Update geometry pointer if it changed
						FPerShapeData::UpdateGeometry(ShapesArray[ShapeIndex], ShapeGeometry);
					}
				}
			}
			else
			{
				ShapesArray.SetNum(1);
				if (OldShapeNum == 0)
				{
					ShapesArray[0] = FPerShapeData::CreatePerShapeData(0, Geometry);
				}
				else
				{
					FPerShapeData::UpdateGeometry(ShapesArray[0], Geometry);
				}
			}

			if (Geometry->HasBoundingBox())
			{
				for (auto& Shape : ShapesArray)
				{
					Shape->UpdateShapeBounds(ActorTM, FVec3(0));
				}
			}
		}
		else
		{
			ShapesArray.Reset();
		}

		if(Proxy)
		{
			if(FPhysicsSolverBase* PhysicsSolverBase = Proxy->GetSolver<FPhysicsSolverBase>())
			{
				PhysicsSolverBase->SetNumDirtyShapes(Proxy, ShapesArray.Num());
			}
		}
	}

	// Unwrap transformed shapes
	// @todo(chaos): also unwrap Instanced and Scaled but that requires a lot of knock work because Convexes are usually Instanced or
	// Scaled so the Scale and Margin is stored on the wrapper (the convex itself is shared).
	// - support for Margin as per-shape data passed through the collision functions
	// - support for Scale as per-shape data passed through the collision functions (or ideally in the Transforms)
	const FImplicitObject* GetInnerGeometryInstanceData(const FImplicitObject* Implicit, FRigidTransformRealSingle3& OutTransform, FReal& OutMargin)
	{
		if (Implicit != nullptr)
		{
			const EImplicitObjectType ImplicitOuterType = Implicit->GetType();
			if (ImplicitOuterType == TImplicitObjectTransformed<FReal, 3>::StaticType())
			{
				// Transformed Implicit
				const TImplicitObjectTransformed<FReal, 3>* TransformedImplicit = Implicit->template GetObject<const TImplicitObjectTransformed<FReal, 3>>();
				OutTransform = (FRigidTransformRealSingle3)TransformedImplicit->GetTransform() * OutTransform;
				//OutMargin += TransformedImplicit->GetMargin();
				return GetInnerGeometryInstanceData(TransformedImplicit->GetTransformedObject(), OutTransform, OutMargin);
			}
			else if ((uint32)ImplicitOuterType & ImplicitObjectType::IsInstanced)
			{
				// Instanced Implicit
				// Currently we only unwrap instanced TriMesh and Heightfields. They don't have a margin, so they don't need the instance wrapper in collision detection.
				// The only other type we wrap in instances right now are Convex, so we just check for that here...
				const FImplicitObjectInstanced* Instanced = static_cast<const FImplicitObjectInstanced*>(Implicit);
				EImplicitObjectType InnerType = Instanced->GetInnerObject()->GetType();
				if (InnerType != FImplicitConvex3::StaticType())
				{
					OutMargin += Instanced->GetMargin();
					return GetInnerGeometryInstanceData(Instanced->GetInnerObject().Get(), OutTransform, OutMargin);
				}
			}
			else if ((uint32)ImplicitOuterType & ImplicitObjectType::IsScaled)
			{
				// Scaled Implicit
				//const FImplicitObjectScaled* Scaled = static_cast<const FImplicitObjectScaled*>(Implicit);
				//OutTransform.Scale *= Scaled->GetScale();
				//OutMargin += Scaled->GetMargin();
				//return GetInnerGeometryInstanceData(Scaled->GetInnerObject().Get(), OutTransform, OutMargin);
			}
		}
		return Implicit;
	}


	FPerShapeData::FPerShapeData(int32 InShapeIdx)
		: bHasCachedLeafInfo(false)
		, Proxy(nullptr)
		, ShapeIdx(InShapeIdx)
		, Geometry()
		, WorldSpaceInflatedShapeBounds(FAABB3(FVec3(0), FVec3(0)))
	{
	}

	FPerShapeData::FPerShapeData(int32 InShapeIdx, TSerializablePtr<FImplicitObject> InGeometry, bool bInHasCachedLeafInfo)
		: bHasCachedLeafInfo(bInHasCachedLeafInfo)
		, Proxy(nullptr)
		, ShapeIdx(InShapeIdx)
		, Geometry(InGeometry)
		, WorldSpaceInflatedShapeBounds(FAABB3(FVec3(0), FVec3(0)))
	{
	}

	FPerShapeData::FPerShapeData(FPerShapeData&& Other)
		: bHasCachedLeafInfo(Other.bHasCachedLeafInfo)
		, Proxy(MoveTemp(Other.Proxy))
		, DirtyFlags(MoveTemp(Other.DirtyFlags))
		, ShapeIdx(MoveTemp(Other.ShapeIdx))
		, CollisionData(MoveTemp(Other.CollisionData))
		, Materials(MoveTemp(Other.Materials))
		, Geometry(MoveTemp(Other.Geometry))
		, WorldSpaceInflatedShapeBounds(MoveTemp(Other.WorldSpaceInflatedShapeBounds))
	{
	}

	FPerShapeData::~FPerShapeData()
	{
	}

	TUniquePtr<FPerShapeData> FPerShapeData::CreatePerShapeData(int32 ShapeIdx, TSerializablePtr<FImplicitObject> InGeometry)
	{
		FReal LeafMargin = 0.0f;
		FRigidTransformRealSingle3 LeafRelativeTransform = FRigidTransformRealSingle3::Identity;
		const FImplicitObject* LeafGeometry = GetInnerGeometryInstanceData(InGeometry.Get(), LeafRelativeTransform, LeafMargin);

		if (RequiresCachedLeafInfo(LeafRelativeTransform, LeafGeometry, InGeometry.Get()))
		{
			return TUniquePtr<FPerShapeData>(new FPerShapeDataCachedLeafInfo(ShapeIdx, InGeometry, LeafGeometry, LeafRelativeTransform));
		}
		else
		{
			return TUniquePtr<FPerShapeData>(new FPerShapeData(ShapeIdx, InGeometry));
		}
	}

	void FPerShapeData::UpdateGeometry(TUniquePtr<FPerShapeData>& ShapePtr, TSerializablePtr<FImplicitObject> InGeometry)
	{
		ShapePtr->Geometry = InGeometry;

		FReal LeafMargin = 0.0f;
		FRigidTransformRealSingle3 LeafRelativeTransform = FRigidTransformRealSingle3::Identity;
		const FImplicitObject* LeafGeometry = GetInnerGeometryInstanceData(InGeometry.Get(), LeafRelativeTransform, LeafMargin);

		if (ShapePtr->bHasCachedLeafInfo)
		{
			// set relative info
			ShapePtr->SetLeafRelativeTransform(LeafRelativeTransform);
			ShapePtr->SetLeafGeometry(LeafGeometry);
			return;
		}

		if (RequiresCachedLeafInfo(LeafRelativeTransform, LeafGeometry, InGeometry.Get()))
		{
			// We need to move to FPerShapeDataCachedLeafInfo to cache this info.
			ShapePtr = TUniquePtr<FPerShapeData>(new FPerShapeDataCachedLeafInfo(MoveTemp(*ShapePtr.Get()), LeafGeometry, LeafRelativeTransform));
		}
	}

	bool FPerShapeData::RequiresCachedLeafInfo(const FRigidTransformRealSingle3& RelativeTransform, const FImplicitObject* LeafGeometry, const FImplicitObject* Geometry)
	{
		return (LeafGeometry != Geometry || !RelativeTransform.Equals(FRigidTransformRealSingle3::Identity));
	}

	void FPerShapeData::UpdateShapeBounds(const FRigidTransform3& WorldTM, const FVec3& BoundsExpansion)
	{
		if (Geometry && Geometry->HasBoundingBox())
		{
			WorldSpaceInflatedShapeBounds = Geometry->CalculateTransformedBounds(WorldTM).ThickenSymmetrically(BoundsExpansion);
		}
	}

	void FPerShapeData::UpdateWorldSpaceState(const FRigidTransform3& WorldTransform, const FVec3& BoundsExpansion)
	{
		FRigidTransform3 LeafWorldTransform = WorldTransform;
		if (bHasCachedLeafInfo)
		{
			FPerShapeDataCachedLeafInfo& LeafInfo = *static_cast<FPerShapeDataCachedLeafInfo*>(this);
			LeafWorldTransform = FRigidTransform3::MultiplyNoScale(FRigidTransform3(GetLeafRelativeTransform()), WorldTransform);
			LeafInfo.LeafWorldTransform = LeafWorldTransform;
		}

		const FImplicitObject* LeafGeometry = GetLeafGeometry();
		if ((LeafGeometry != nullptr) && LeafGeometry->HasBoundingBox())
		{
			WorldSpaceInflatedShapeBounds = LeafGeometry->CalculateTransformedBounds(LeafWorldTransform).ThickenSymmetrically(BoundsExpansion);;
		}
	}

	const FImplicitObject* FPerShapeData::GetLeafGeometry() const
	{
		if (bHasCachedLeafInfo)
		{
			const FPerShapeDataCachedLeafInfo& LeafInfo = *static_cast<const FPerShapeDataCachedLeafInfo*>(this);
			return LeafInfo.LeafGeometry;
		}

		return Geometry.Get();
	}

	void FPerShapeData::SetLeafGeometry(const FImplicitObject* LeafGeometry)
	{
		if (ensure(bHasCachedLeafInfo))
		{
			FPerShapeDataCachedLeafInfo& LeafInfo = *static_cast<FPerShapeDataCachedLeafInfo*>(this);
			LeafInfo.LeafGeometry = LeafGeometry;
		}
	}

	FRigidTransformRealSingle3 FPerShapeData::GetLeafRelativeTransform() const
	{
		if (bHasCachedLeafInfo)
		{
			const FPerShapeDataCachedLeafInfo& LeafInfo = *static_cast<const FPerShapeDataCachedLeafInfo*>(this);
			return LeafInfo.LeafRelativeTransform;
		}

		return FRigidTransformRealSingle3::Identity;
	}

	FRigidTransform3 FPerShapeData::GetLeafWorldTransform(const FGeometryParticleHandle* Particle) const
	{
		if (bHasCachedLeafInfo)
		{
			const FPerShapeDataCachedLeafInfo& LeafInfo = *static_cast<const FPerShapeDataCachedLeafInfo*>(this);
			return LeafInfo.LeafWorldTransform;
		}

		return FParticleUtilities::GetActorWorldTransform(FConstGenericParticleHandle(Particle));
	}
	
	void FPerShapeData::UpdateLeafWorldTransform(FGeometryParticleHandle* Particle)
	{
		if (bHasCachedLeafInfo)
		{
			FPerShapeDataCachedLeafInfo& LeafInfo = *static_cast<FPerShapeDataCachedLeafInfo*>(this);
			FRigidTransform3 ParticleTransform = FParticleUtilities::GetActorWorldTransform(FConstGenericParticleHandle(Particle));
			LeafInfo.LeafWorldTransform = FRigidTransform3::MultiplyNoScale(FRigidTransform3(GetLeafRelativeTransform()), ParticleTransform);
		}
	}

	void FPerShapeData::SetLeafRelativeTransform(const FRigidTransformRealSingle3& RelativeTransform)
	{
		if (ensure(bHasCachedLeafInfo))
		{
			FPerShapeDataCachedLeafInfo& LeafInfo = *static_cast<FPerShapeDataCachedLeafInfo*>(this);
			LeafInfo.LeafRelativeTransform = RelativeTransform;
		}
	}

	FPerShapeData* FPerShapeData::SerializationFactory(FChaosArchive& Ar, FPerShapeData*)
	{
		//todo: need to rework serialization for shapes, for now just give them all shape idx 0
		return Ar.IsLoading() ? new FPerShapeData(0) : nullptr;
	}

	void FPerShapeData::Serialize(FChaosArchive& Ar)
	{
		Ar.UsingCustomVersion(FExternalPhysicsCustomObjectVersion::GUID);
		Ar.UsingCustomVersion(FExternalPhysicsMaterialCustomObjectVersion::GUID);

		Ar << Geometry;
		Ar << CollisionData;
		Ar << Materials;

		if (Ar.CustomVer(FExternalPhysicsCustomObjectVersion::GUID) >= FExternalPhysicsCustomObjectVersion::SerializeShapeWorldSpaceBounds)
		{
			TBox<FReal,3>::SerializeAsAABB(Ar,WorldSpaceInflatedShapeBounds);
		}
		else
		{
			// This should be set by particle serializing this FPerShapeData.
			WorldSpaceInflatedShapeBounds = FAABB3(FVec3(0.0f, 0.0f, 0.0f), FVec3(0.0f, 0.0f, 0.0f));
		}

	}

	FShapeOrShapesArray::FShapeOrShapesArray(const FGeometryParticleHandle* Particle)
	{
		if (Particle)
		{
			const FImplicitObject* Geometry = Particle->Geometry().Get();
			if (Geometry)
			{
				if (Geometry->IsUnderlyingUnion())
				{
					ShapeArray = &Particle->ShapesArray();
					bIsSingleShape = false;
				}
				else
				{
					Shape = Particle->ShapesArray()[0].Get();
					bIsSingleShape = true;
				}

				return;
			}
		}

		Shape = nullptr;
		bIsSingleShape = true;
	}


	template <typename T, int d, EGeometryParticlesSimType SimType>
	void TGeometryParticlesImp<T, d, SimType>::SetHandle(int32 Index, FGeometryParticleHandle* Handle)
	{
		Handle->SetSOALowLevel(this);
		MGeometryParticleHandle[Index] = AsAlwaysSerializable(Handle);
	}

	template <>
	void TGeometryParticlesImp<FRealSingle, 3, EGeometryParticlesSimType::Other>::SetHandle(int32 Index, FGeometryParticleHandle* Handle)
	{
		check(false);  // TODO: Implement EGeometryParticlesSimType::Other (cloth) particle serialization
	}

	template <>
	void TGeometryParticlesImp<FRealDouble, 3, EGeometryParticlesSimType::Other>::SetHandle(int32 Index, FGeometryParticleHandle* Handle)
	{
		check(false);  // TODO: Implement EGeometryParticlesSimType::Other (cloth) particle serialization
	}

	template<class T, int d, EGeometryParticlesSimType SimType>
	CHAOS_API TGeometryParticlesImp<T, d, SimType>* TGeometryParticlesImp<T, d, SimType>::SerializationFactory(FChaosArchive& Ar, TGeometryParticlesImp<T,d,SimType>* Particles)
	{
		int8 ParticleType = Ar.IsLoading() ? 0 : (int8)Particles->ParticleType();
		Ar << ParticleType;
		switch ((EParticleType)ParticleType)
		{
		case EParticleType::Static: return Ar.IsLoading() ? new TGeometryParticlesImp<T, d, SimType>() : nullptr;
		case EParticleType::Kinematic: return Ar.IsLoading() ? new TKinematicGeometryParticlesImp<T, d, SimType>() : nullptr;
		case EParticleType::Rigid: return Ar.IsLoading() ? new TPBDRigidParticles<T, d>() : nullptr;
		case EParticleType::Clustered: return Ar.IsLoading() ? new TPBDRigidClusteredParticles<T, d>() : nullptr;
		default:
			check(false); return nullptr;
		}
	}
	
	template<>
	TGeometryParticlesImp<FRealSingle, 3, EGeometryParticlesSimType::Other>* TGeometryParticlesImp<FRealSingle, 3, EGeometryParticlesSimType::Other>::SerializationFactory(FChaosArchive& Ar, TGeometryParticlesImp<FRealSingle, 3, EGeometryParticlesSimType::Other>* Particles)
	{
		check(false);  // TODO: Implement EGeometryParticlesSimType::Other (cloth) particle serialization
		return nullptr;
	}

	template<>
	TGeometryParticlesImp<FRealDouble, 3, EGeometryParticlesSimType::Other>* TGeometryParticlesImp<FRealDouble, 3, EGeometryParticlesSimType::Other>::SerializationFactory(FChaosArchive& Ar, TGeometryParticlesImp<FRealDouble, 3, EGeometryParticlesSimType::Other>* Particles)
	{
		check(false);  // TODO: Implement EGeometryParticlesSimType::Other (cloth) particle serialization
		return nullptr;
	}

	template <typename T, int d, EGeometryParticlesSimType SimType>
	void TGeometryParticlesImp<T, d, SimType>::SerializeGeometryParticleHelper(FChaosArchive& Ar, TGeometryParticlesImp<T, d, EGeometryParticlesSimType::RigidBodySim>* GeometryParticles)
	{
		auto& SerializableGeometryParticles = AsAlwaysSerializableArray(GeometryParticles->MGeometryParticle);
		Ar << SerializableGeometryParticles;
	}

	template class TGeometryParticlesImp<FReal, 3, EGeometryParticlesSimType::RigidBodySim>;
	template class TGeometryParticlesImp<FRealSingle, 3, EGeometryParticlesSimType::Other>;
	template class TGeometryParticlesImp<FRealDouble, 3, EGeometryParticlesSimType::Other>;
}
