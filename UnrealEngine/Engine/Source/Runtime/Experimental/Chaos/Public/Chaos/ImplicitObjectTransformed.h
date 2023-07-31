// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Box.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/Transform.h"
#include "ChaosArchive.h"
#include "Templates/EnableIf.h"
#include "AABB.h"

namespace Chaos
{

inline void TImplicitObjectTransformSerializeHelper(FChaosArchive& Ar, TSerializablePtr<FImplicitObject>& Obj)
{
	Ar << Obj;
}

inline void TImplicitObjectTransformSerializeHelper(FChaosArchive& Ar, const FImplicitObject* Obj)
{
	check(false);
}

inline void TImplicitObjectTransformAccumulateSerializableHelper(TArray<Pair<TSerializablePtr<FImplicitObject>, FRigidTransform3>>& Out, TSerializablePtr<FImplicitObject> Obj, const FRigidTransform3& NewTM)
{
	Obj->AccumulateAllSerializableImplicitObjects(Out, NewTM, Obj);
}

inline void TImplicitObjectTransformAccumulateSerializableHelper(TArray<Pair<TSerializablePtr<FImplicitObject>, FRigidTransform3>>& Out, const FImplicitObject* Obj, const FRigidTransform3& NewTM)
{
	check(false);
}

/**
 * Transform the contained shape. If you pass a TUniquePtr to the constructor, ownership is transferred to the TransformedImplicit. If you pass a
 * SerializablePtr, the lifetime of the object must be handled externally (do not delete it before deleting the TransformedImplicit).
 * @template bSerializable Whether the shape can be serialized (usually true). Set to false for transient/stack-allocated objects. 
 */
template<class T, int d, bool bSerializable = true>
class TImplicitObjectTransformed final : public FImplicitObject
{
	using FStorage = TImplicitObjectPtrStorage<T, d, bSerializable>;
	using ObjectType = typename FStorage::PtrType;

public:
	using FImplicitObject::GetTypeName;

	/**
	 * Create a transform around an ImplicitObject. Lifetime of the wrapped object is managed externally.
	 */
	TImplicitObjectTransformed(ObjectType Object, const TRigidTransform<T, d>& InTransform)
	    : FImplicitObject(EImplicitObject::HasBoundingBox, ImplicitObjectType::Transformed)
	    , MObject(Object)
	    , MTransform(InTransform)
	    , MLocalBoundingBox(Object->BoundingBox().TransformedAABB(InTransform))
	{
		this->bIsConvex = Object->IsConvex();
		this->bDoCollide = MObject->GetDoCollide();
	}

	/**
	 * Create a transform around an ImplicitObject and take control of its lifetime.
	 */
	TImplicitObjectTransformed(TUniquePtr<Chaos::FImplicitObject> &&ObjectOwner, const TRigidTransform<T, d>& InTransform)
	    : FImplicitObject(EImplicitObject::HasBoundingBox, ImplicitObjectType::Transformed)
		, MObjectOwner(MoveTemp(ObjectOwner))
	    , MTransform(InTransform)
	{
		static_assert(bSerializable, "Non-serializable TImplicitObjectTransformed created with a UniquePtr");
		this->MObject = FStorage::Convert(MObjectOwner);
		this->MLocalBoundingBox = MObject->BoundingBox().TransformedAABB(InTransform);
		this->bIsConvex = MObject->IsConvex();
		this->bDoCollide = MObject->GetDoCollide();
	}

	TImplicitObjectTransformed(const TImplicitObjectTransformed<T, d, bSerializable>& Other) = delete;
	TImplicitObjectTransformed(TImplicitObjectTransformed<T, d, bSerializable>&& Other)
	    : FImplicitObject(EImplicitObject::HasBoundingBox, ImplicitObjectType::Transformed)
	    , MObject(Other.MObject)
		, MObjectOwner(MoveTemp(Other.MObjectOwner))
	    , MTransform(Other.MTransform)
	    , MLocalBoundingBox(MoveTemp(Other.MLocalBoundingBox))
	{
		this->bIsConvex = Other.MObject->IsConvex();
		this->bDoCollide = Other.MObject->GetDoCollide();
	}

	virtual FImplicitObject* Duplicate() const override
	{
		if(MObjectOwner)
		{
			check(bSerializable);
			TUniquePtr<FImplicitObject> DupObj(MObjectOwner->Duplicate());
			return new TImplicitObjectTransformed<T,d,true>(MoveTemp(DupObj), MTransform);
		}
		else
		{
			check(false);	//duplicate only supported for owned geometry
			return nullptr;
		}
	}

	virtual TUniquePtr<FImplicitObject> Copy() const
	{
		if (MObject)
		{
			TUniquePtr<FImplicitObject> ObjectCopy(MObject->Copy());
			return MakeUnique<TImplicitObjectTransformed<T,d>>(MoveTemp(ObjectCopy), MTransform);
		}
		else
		{
			check(false);
			return nullptr;
		}
	}
	
	virtual TUniquePtr<FImplicitObject> CopyWithScale(const FVec3& Scale) const override
	{
		if(MObject)
		{
			//return MakeCopyWithScaleTransformed(MObjectOwner, MTransform, Scale);
			// since we cannot have a { Scaled -- Transformed -- Shape } ( scaled can only directly reference concrete shapes )
			// we need to scale the transform translation and set the Scaled on the shape itself like { (Adjusted)Transformed -- Scaled -- Shape }  
			FRigidTransform3 AdjustedTransform{ MTransform };
			AdjustedTransform.ScaleTranslation(Scale);

			TUniquePtr<FImplicitObject> ScaledObject(MObject->CopyWithScale(Scale));
			return MakeUnique<TImplicitObjectTransformed<FReal,3>>(MoveTemp(ScaledObject), AdjustedTransform);
		}
		else
		{
			check(false);
			return nullptr;
		}
	}

	virtual TUniquePtr<FImplicitObject> DeepCopy() const
	{
		if(MObject)
		{
			TUniquePtr<FImplicitObject> ObjectCopy(MObject->DeepCopy());
			return MakeUnique<TImplicitObjectTransformed<T,d>>(MoveTemp(ObjectCopy), MTransform);
		}
		else
		{
			check(false);
			return nullptr;
		}
	}

	virtual TUniquePtr<FImplicitObject> DeepCopyWithScale(const FVec3& Scale) const override
	{
		if(MObject)
		{
			//return MakeCopyWithScaleTransformed(MObjectOwner, MTransform, Scale);
			// since we cannot have a { Scaled -- Transformed -- Shape } ( scaled can only directly reference concrete shapes )
			// we need to scale the transform translation and set the Scaled on the shape itself like { (Adjusted)Transformed -- Scaled -- Shape }  
			FRigidTransform3 AdjustedTransform{ MTransform };
			AdjustedTransform.ScaleTranslation(Scale);

			TUniquePtr<FImplicitObject> ScaledObject(MObject->DeepCopyWithScale(Scale));
			return MakeUnique<TImplicitObjectTransformed<FReal,3>>(MoveTemp(ScaledObject), AdjustedTransform);
		}
		else
		{
			check(false);
			return nullptr;
		}
	}
	
	~TImplicitObjectTransformed() {}

	static constexpr EImplicitObjectType StaticType()
	{
		return ImplicitObjectType::Transformed;
	}

	const FImplicitObject* GetTransformedObject() const
	{
		return MObject.Get();
	}

	FReal GetMargin() const
	{
		// If the inner shape is quadratic, we have no margin
		return (MObject->GetRadius() > 0.0f) ? 0.0f : Margin;
	}

	FReal GetRadius() const
	{
		// If the inner shape is quadratic, so are we
		return (MObject->GetRadius() > 0.0f) ? Margin : 0.0f;
	}

	bool GetDoCollide() const
	{
		return MObject->GetDoCollide();
	}

	virtual T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const override
	{
		auto TransformedX = MTransform.InverseTransformPosition(x);
		auto Phi = MObject->PhiWithNormal(TransformedX, Normal);
		Normal = MTransform.TransformVector(Normal);
		return Phi;
	}

	virtual bool Raycast(const TVector<T, d>& StartPoint, const TVector<T, d>& Dir, const T Length, const T Thickness, T& OutTime, TVector<T, d>& OutPosition, TVector<T, d>& OutNormal, int32& OutFaceIndex) const override
	{
		const TVector<T, d> LocalStart = MTransform.InverseTransformPosition(StartPoint);
		const TVector<T, d> LocalDir = MTransform.InverseTransformVector(Dir);
		TVector<T, d> LocalPosition;
		TVector<T, d> LocalNormal;

		if (MObject->Raycast(LocalStart, LocalDir, Length, Thickness, OutTime, LocalPosition, LocalNormal, OutFaceIndex))
		{
			if (OutTime != 0.0f)
			{
				OutPosition = MTransform.TransformPosition(LocalPosition);
				OutNormal = MTransform.TransformVector(LocalNormal);
			}
			return true;
		}
		
		return false;
	}

	virtual int32 FindMostOpposingFace(const TVector<T, 3>& Position, const TVector<T, 3>& UnitDir, int32 HintFaceIndex, T SearchDistance) const override
	{
		const TVector<T, d> LocalPosition = MTransform.InverseTransformPositionNoScale(Position);
		const TVector<T, d> LocalDir = MTransform.InverseTransformVectorNoScale(UnitDir);
		return MObject->FindMostOpposingFace(LocalPosition, LocalDir, HintFaceIndex, SearchDistance);
	}

	virtual TVector<T, 3> FindGeometryOpposingNormal(const TVector<T, d>& DenormDir, int32 FaceIndex, const TVector<T, d>& OriginalNormal) const override
	{
		const TVector<T, d> LocalDenormDir = MTransform.InverseTransformVectorNoScale(DenormDir);
		const TVector<T, d> LocalOriginalNormal = MTransform.InverseTransformVectorNoScale(OriginalNormal);
		const TVector<T, d> LocalNormal = MObject->FindGeometryOpposingNormal(LocalDenormDir, FaceIndex, LocalOriginalNormal);
		return MTransform.TransformVectorNoScale(LocalNormal);
	}

	virtual bool Overlap(const TVector<T, d>& Point, const T Thickness) const override
	{
		const TVector<T, d> LocalPoint = MTransform.InverseTransformPosition(Point);
		return MObject->Overlap(LocalPoint, Thickness);
	}

	virtual Pair<TVector<T, d>, bool> FindClosestIntersectionImp(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const override
	{
		auto TransformedStart = MTransform.InverseTransformPosition(StartPoint);
		auto TransformedEnd = MTransform.InverseTransformPosition(EndPoint);
		auto ClosestIntersection = MObject->FindClosestIntersection(TransformedStart, TransformedEnd, Thickness);
		if (ClosestIntersection.Second)
		{
			ClosestIntersection.First = MTransform.TransformPosition(ClosestIntersection.First);
		}
		return ClosestIntersection;
	}

	virtual int32 FindClosestFaceAndVertices(const FVec3& Position, TArray<FVec3>& FaceVertices, FReal SearchDist = 0.01f) const override
	{
		const FVec3 LocalPoint = MTransform.InverseTransformPosition(Position);
		int32 FaceIndex = MObject->FindClosestFaceAndVertices(LocalPoint, FaceVertices, SearchDist);
		if (FaceIndex != INDEX_NONE)
		{
			for (FVec3& Vec : FaceVertices)
			{
				Vec = MTransform.TransformPosition(Vec);
			}
		}
		return FaceIndex;
	}

	const TRigidTransform<T, d>& GetTransform() const { return MTransform; }
	void SetTransform(const TRigidTransform<T, d>& InTransform)
	{
		MLocalBoundingBox = MObject->BoundingBox().TransformedAABB(InTransform);
		MTransform = InTransform;
	}

	virtual void AccumulateAllImplicitObjects(TArray<Pair<const FImplicitObject*, TRigidTransform<T, d>>>& Out, const TRigidTransform<T, d>& ParentTM) const override
	{
		const TRigidTransform<T, d> NewTM = MTransform * ParentTM;
		MObject->AccumulateAllImplicitObjects(Out, NewTM);
	}

	virtual void AccumulateAllSerializableImplicitObjects(TArray<Pair<TSerializablePtr<FImplicitObject>, TRigidTransform<T, d>>>& Out, const TRigidTransform<T, d>& ParentTM, TSerializablePtr<FImplicitObject> This) const override
	{
		check(bSerializable);
		const TRigidTransform<T, d> NewTM = MTransform * ParentTM;
		TImplicitObjectTransformAccumulateSerializableHelper(Out, MObject, NewTM);
	}

	virtual void FindAllIntersectingObjects(TArray < Pair<const FImplicitObject*, TRigidTransform<T, d>>>& Out, const TAABB<T, d>& LocalBounds) const override
	{
		const TAABB<T, d> SubobjectBounds = LocalBounds.TransformedAABB(MTransform.Inverse());
		int32 NumOut = Out.Num();
		MObject->FindAllIntersectingObjects(Out, SubobjectBounds);
		if (Out.Num() > NumOut)
		{
			Out[NumOut].Second = Out[NumOut].Second * MTransform;
		}
	}

	virtual const TAABB<T, d> BoundingBox() const override { return MLocalBoundingBox; }

	// Calculate the tight-fitting world-space bounding box
	virtual FAABB3 CalculateTransformedBounds(const FRigidTransform3& InTransform) const
	{
		return MObject->CalculateTransformedBounds(FRigidTransform3::MultiplyNoScale(MTransform ,InTransform));
	}

	const FReal GetVolume() const
	{
		// TODO: More precise volume!
		return BoundingBox().GetVolume();
	}

	const FMatrix33 GetInertiaTensor(const FReal Mass) const
	{
		// TODO: More precise inertia!
		return BoundingBox().GetInertiaTensor(Mass);
	}

	const FVec3 GetCenterOfMass() const
	{
		// TODO: Actually compute this!
		return BoundingBox().GetCenterOfMass();
	}


	const ObjectType Object() const { return MObject; }
	
	virtual void Serialize(FChaosArchive& Ar) override
	{
		check(bSerializable);
		FChaosArchiveScopedMemory ScopedMemory(Ar, GetTypeName(), false);
		FImplicitObject::SerializeImp(Ar);
		TImplicitObjectTransformSerializeHelper(Ar, MObject);
		Ar << MTransform;
		TBox<T, d>::SerializeAsAABB(Ar, MLocalBoundingBox);
	}

	virtual uint32 GetTypeHash() const override
	{
		// Combine the hash from the inner, non transformed object with our transform
		return HashCombine(MObject->GetTypeHash(), ::GetTypeHash(MTransform));
	}

	virtual uint16 GetMaterialIndex(uint32 HintIndex) const override
	{
		return MObject->GetMaterialIndex(HintIndex);
	}

private:
	ObjectType MObject;
	TUniquePtr<Chaos::FImplicitObject> MObjectOwner;
	TRigidTransform<T, d> MTransform;
	TAABB<T, d> MLocalBoundingBox;

	//needed for serialization
	TImplicitObjectTransformed() : FImplicitObject(EImplicitObject::HasBoundingBox, ImplicitObjectType::Transformed) {}

	friend FImplicitObject;	//needed for serialization
};

namespace Utilities
{
	inline TUniquePtr<FImplicitObject> DuplicateImplicitWithTransform(const FImplicitObject* const InObject, FTransform NewTransform)
	{
		if(!InObject)
		{
			return nullptr;
		}

		const EImplicitObjectType OuterType = InObject->GetType();

		if(GetInnerType(OuterType) == ImplicitObjectType::Transformed)
		{
			TUniquePtr<FImplicitObject> NewTransformed = InObject->Copy();
			TImplicitObjectTransformed<FReal, 3>* InnerTransformed = static_cast<TImplicitObjectTransformed<FReal, 3>*>(NewTransformed.Get());
			InnerTransformed->SetTransform(NewTransform);

			return MoveTemp(NewTransformed);
		}
		else
		{
			TUniquePtr<FImplicitObject> NewInnerObject = InObject->Copy();
			return MakeUnique<TImplicitObjectTransformed<FReal, 3>>(MoveTemp(NewInnerObject), NewTransform);
		}
	}
}

template <typename T, int d>
using TImplicitObjectTransformedNonSerializable = TImplicitObjectTransformed<T, d, false>;

}
