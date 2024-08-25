// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: FGeometryCollection methods.
=============================================================================*/

#include "GeometryCollection/Facades/CollectionTetrahedralBindingsFacade.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/ManagedArrayAccessor.h"

namespace GeometryCollection::Facades
{
	// Groups 
	const FName FTetrahedralBindings::MeshBindingsGroupName = "MeshBindings";

	// Attributes
	const FName FTetrahedralBindings::MeshIdAttributeName = "MeshId";

	const FName FTetrahedralBindings::ParentsAttributeName = "Parents";
	const FName FTetrahedralBindings::WeightsAttributeName = "Weights";
	const FName FTetrahedralBindings::OffsetsAttributeName = "Offsets";
	const FName FTetrahedralBindings::MaskAttributeName = "Mask";

	FTetrahedralBindings::FTetrahedralBindings(FManagedArrayCollection& InCollection)
		: MeshIdAttribute(InCollection, MeshIdAttributeName, MeshBindingsGroupName)
	{}

	FTetrahedralBindings::FTetrahedralBindings(const FManagedArrayCollection& InCollection)
		: MeshIdAttribute(InCollection, MeshIdAttributeName, MeshBindingsGroupName)
	{}

	FTetrahedralBindings::~FTetrahedralBindings()
	{}

	void FTetrahedralBindings::DefineSchema()
	{
		check(!IsConst());
		TManagedArray<FString>& MeshIdValues = 
			MeshIdAttribute.IsValid() ? MeshIdAttribute.Modify() : MeshIdAttribute.Add();
	}

	bool FTetrahedralBindings::IsValid() const
	{
		return MeshIdAttribute.IsValid() && 
			(Parents && Parents->IsValid()) && 
			(Weights && Weights->IsValid()) && 
			(Offsets && Offsets->IsValid()) &&
			(Mask && Mask->IsValid());
	}

	FName FTetrahedralBindings::GenerateMeshGroupName(
		const int32 TetMeshIdx,
		const FName& MeshId,
		const int32 LOD)
	{
		FString Str = FString::Printf(TEXT("TetrahedralBindings:TetMeshIdx:%d:%s:%d"), TetMeshIdx, *MeshId.ToString(), LOD);
		return FName(Str.Len(), *Str);
	}

	bool FTetrahedralBindings::ContainsBindingsGroup(const int32 TetMeshIdx, const FName& MeshId, const int32 LOD) const
	{
		return ContainsBindingsGroup(GenerateMeshGroupName(TetMeshIdx, MeshId, LOD));
	}

	bool FTetrahedralBindings::ContainsBindingsGroup(const FName& GroupName) const
	{
		check(MeshIdAttribute.IsValid());
		const TManagedArray<FString>* MeshIdValues =
			MeshIdAttribute.IsValid() ? MeshIdAttribute.Find() : nullptr;
		return MeshIdValues ? MeshIdValues->Contains(GroupName.ToString()) : false;
	}

	int32 FTetrahedralBindings::GetTetMeshIndex(const FName& MeshId, const int32 LOD) const
	{
		const TManagedArray<FString>* MeshIdValues =
			MeshIdAttribute.IsValid() ? MeshIdAttribute.Find() : nullptr;
		if (MeshIdValues)
		{
			FString Suffix = FString::Printf(TEXT(":%s:%d"), *MeshId.ToString(), LOD);
			for (int32 i = 0; i < MeshIdValues->Num(); i++)
			{
				const FString& Entry = (*MeshIdValues)[i];
				if (Entry.EndsWith(Suffix))
				{
					FString Str = Entry;
					Str.RemoveAt(0, FString(TEXT("TetrahedralBindings:TetMeshIdx:")).Len(), EAllowShrinking::No);
					Str.RemoveAt(Str.Len() - Suffix.Len(), Suffix.Len());
					return FCString::Atoi(*Str);
				}
			}
		}
		return INDEX_NONE;
	}

	void FTetrahedralBindings::AddBindingsGroup(const int32 TetMeshIdx, const FName& MeshId, const int32 LOD)
	{
		AddBindingsGroup(GenerateMeshGroupName(TetMeshIdx, MeshId, LOD));
	}

	void FTetrahedralBindings::AddBindingsGroup(const FName& GroupName)
	{
		if (ContainsBindingsGroup(GroupName))
		{
			ReadBindingsGroup(GroupName);
			return;
		}
		check(MeshIdAttribute.IsValid());
		check(MeshIdAttribute.IsPersistent());

		check(!IsConst());
		const int32 Idx = MeshIdAttribute.AddElements(1);
		MeshIdAttribute.Modify()[Idx] = GroupName.ToString();

		Parents.Reset();
		Weights.Reset();
		Offsets.Reset();
		Mask.Reset();
		FManagedArrayCollection& Collection = *MeshIdAttribute.GetCollection();
		Parents.Reset(new TManagedArrayAccessor<FIntVector4>(Collection, ParentsAttributeName, GroupName));
		Weights.Reset(new TManagedArrayAccessor<FVector4f>(Collection, WeightsAttributeName, GroupName));
		Offsets.Reset(new TManagedArrayAccessor<FVector3f>(Collection, OffsetsAttributeName, GroupName));
		Mask.Reset(new TManagedArrayAccessor<float>(Collection, MaskAttributeName, GroupName));
		Parents->Add(ManageArrayAccessor::EPersistencePolicy::MakePersistent, FGeometryCollection::VerticesGroup);
		Weights->Add();
		Offsets->Add();
		Mask->Add();
	}

	bool FTetrahedralBindings::ReadBindingsGroup(const int32 TetMeshIdx, const FName& MeshId, const int32 LOD)
	{
		return ReadBindingsGroup(GenerateMeshGroupName(TetMeshIdx, MeshId, LOD));
	}

	bool FTetrahedralBindings::ReadBindingsGroup(const FName& GroupName)
	{
		check(MeshIdAttribute.IsValid());
		Parents.Reset();
		Weights.Reset();
		Offsets.Reset();
		Mask.Reset();
		if (!MeshIdAttribute.Find()->Contains(GroupName.ToString()))
		{
			return false;
		}
		// This is an existing group, so find the existing bindings arrays.
		if (!IsConst())
		{
			FManagedArrayCollection* Collection = MeshIdAttribute.GetCollection();
			Parents.Reset(new TManagedArrayAccessor<FIntVector4>(*Collection, ParentsAttributeName, GroupName));
			Weights.Reset(new TManagedArrayAccessor<FVector4f>(*Collection, WeightsAttributeName, GroupName));
			Offsets.Reset(new TManagedArrayAccessor<FVector3f>(*Collection, OffsetsAttributeName, GroupName));
			Mask.Reset(new TManagedArrayAccessor<float>(*Collection, MaskAttributeName, GroupName));
		}
		else
		{
			const FManagedArrayCollection& ConstCollection = MeshIdAttribute.GetConstCollection();
			Parents.Reset(new TManagedArrayAccessor<FIntVector4>(ConstCollection, ParentsAttributeName, GroupName));
			Weights.Reset(new TManagedArrayAccessor<FVector4f>(ConstCollection, WeightsAttributeName, GroupName));
			Offsets.Reset(new TManagedArrayAccessor<FVector3f>(ConstCollection, OffsetsAttributeName, GroupName));
			Mask.Reset(new TManagedArrayAccessor<float>(ConstCollection, MaskAttributeName, GroupName));
		}
		return Parents->IsValid() && Weights->IsValid() && Offsets->IsValid() && Mask->IsValid();
	}

	void FTetrahedralBindings::RemoveBindingsGroup(const int32 TetMeshIdx, const FName& MeshId, const int32 LOD)
	{
		RemoveBindingsGroup(GenerateMeshGroupName(TetMeshIdx, MeshId, LOD));
	}

	void FTetrahedralBindings::RemoveBindingsGroup(const FName& GroupName)
	{
		check(!IsConst());
		TManagedArray<FString>& MeshIdValues = MeshIdAttribute.Modify();
		int32 Idx = MeshIdValues.Find(GroupName.ToString());
		if (Idx != INDEX_NONE)
		{
			TArray<int32> Indices;
			Indices.Add(Idx);
			MeshIdValues.RemoveElements(Indices);
		}

		FManagedArrayCollection& Collection = *MeshIdAttribute.GetCollection();
		if (Parents)
		{
			Parents->Remove();
			Parents.Reset();
		}
		if (Weights)
		{
			Weights->Remove();
			Weights.Reset();
		}
		if (Offsets)
		{
			Offsets->Remove();
			Offsets.Reset();
		}
		if (Mask)
		{
			Mask->Remove();
			Mask.Reset();
		}
		// Only drop the group if it's empty at this point?
		if (Collection.NumAttributes(GroupName) == 0)
		{
			Collection.RemoveGroup(GroupName);
		}
	}

	void 
	FTetrahedralBindings::SetBindingsData(
		const TArray<FIntVector4>& ParentsIn,
		const TArray<FVector4f>& WeightsIn,
		const TArray<FVector3f>& OffsetsIn,
		const TArray<float>& MaskIn)
	{
		check(!IsConst());
		check(IsValid());
		check((ParentsIn.Num() == WeightsIn.Num()) && (ParentsIn.Num() == OffsetsIn.Num()) && (ParentsIn.Num() == MaskIn.Num()));

		const int32 Num = ParentsIn.Num();
		const int32 CurrNum = Parents->Num();//Collection.NumElements(CurrGroupName);
		Parents->AddElements(Num - CurrNum); // Resizes the group
		TManagedArray<FIntVector4>& ParentsValues = Parents->Modify();
		TManagedArray<FVector4f>& WeightsValues = Weights->Modify();
		TManagedArray<FVector3f>& OffsetsValues = Offsets->Modify();
		TManagedArray<float>& MaskValues = Mask->Modify();
		for (int32 i = 0; i < Num; i++)
		{
			ParentsValues[i] = ParentsIn[i];
			WeightsValues[i] = WeightsIn[i];
			OffsetsValues[i] = OffsetsIn[i];
			MaskValues[i] = MaskIn[i];
		}
	}


};


