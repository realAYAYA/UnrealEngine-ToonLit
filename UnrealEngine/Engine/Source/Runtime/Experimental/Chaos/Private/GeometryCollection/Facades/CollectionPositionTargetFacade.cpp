// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FleshCollection.cpp: FFleshCollection methods.
=============================================================================*/

#include "GeometryCollection/Facades/CollectionPositionTargetFacade.h"
#include "GeometryCollection/GeometryCollection.h"

namespace GeometryCollection::Facades
{
	// Attributes
	const FName FPositionTargetFacade::GroupName("PositionTargets");
	const FName FPositionTargetFacade::TargetIndex("TargetIndex");
	const FName FPositionTargetFacade::SourceIndex("SourceIndex");
	const FName FPositionTargetFacade::Stiffness("Stiffness");
	const FName FPositionTargetFacade::Damping("Damping");
	const FName FPositionTargetFacade::SourceName("SourceName");
	const FName FPositionTargetFacade::TargetName("TargetName");
	const FName FPositionTargetFacade::TargetWeights("TargetWeights");
	const FName FPositionTargetFacade::SourceWeights("SourceWeights");

	FPositionTargetFacade::FPositionTargetFacade(FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(&InCollection)
		, TargetIndexAttribute(InCollection, TargetIndex, GroupName)
		, SourceIndexAttribute(InCollection, SourceIndex, GroupName)
		, TargetNameAttribute(InCollection, TargetName, GroupName)
		, SourceNameAttribute(InCollection, SourceName, GroupName)
		, StiffnessAttribute(InCollection, Stiffness, GroupName)
		, DampingAttribute(InCollection, Damping, GroupName)
		, TargetWeightsAttribute(InCollection, TargetWeights, GroupName)
		, SourceWeightsAttribute(InCollection, SourceWeights, GroupName)
	{
		DefineSchema();
	}

	FPositionTargetFacade::FPositionTargetFacade(const FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(nullptr)
		, TargetIndexAttribute(InCollection, TargetIndex, GroupName)
		, SourceIndexAttribute(InCollection, SourceIndex, GroupName)
		, TargetNameAttribute(InCollection, TargetName, GroupName)
		, SourceNameAttribute(InCollection, SourceName, GroupName)
		, StiffnessAttribute(InCollection, Stiffness, GroupName)
		, DampingAttribute(InCollection, Damping, GroupName)
		, TargetWeightsAttribute(InCollection, TargetWeights, GroupName)
		, SourceWeightsAttribute(InCollection, SourceWeights, GroupName)
	{
		//DefineSchema();
	}

	bool FPositionTargetFacade::IsValid() const
	{
		return TargetIndexAttribute.IsValid() && SourceIndexAttribute.IsValid()
			&& TargetNameAttribute.IsValid() && SourceNameAttribute.IsValid() && StiffnessAttribute.IsValid()
			&& DampingAttribute.IsValid() && TargetWeightsAttribute.IsValid() && SourceWeightsAttribute.IsValid();
	}

	void FPositionTargetFacade::DefineSchema()
	{
		check(!IsConst());
		TargetIndexAttribute.Add(ManageArrayAccessor::EPersistencePolicy::MakePersistent, FGeometryCollection::VerticesGroup);
		SourceIndexAttribute.Add(ManageArrayAccessor::EPersistencePolicy::MakePersistent, FGeometryCollection::VerticesGroup);
		TargetNameAttribute.Add();
		SourceNameAttribute.Add();
		StiffnessAttribute.Add();
		DampingAttribute.Add();
		TargetWeightsAttribute.Add();
		SourceWeightsAttribute.Add();
	}


	int32 FPositionTargetFacade::AddPositionTarget(const FPositionTargetsData& InputData)
	{
		check(!IsConst());
		if (IsValid())
		{
			int32 NewIndex = TargetIndexAttribute.AddElements(1);
			//TManagedArray<TArray<int32>>& TargetIndex = TargetIndexAttribute.Modify();
			TargetIndexAttribute.Modify()[NewIndex] = InputData.TargetIndex;
			SourceIndexAttribute.Modify()[NewIndex] = InputData.SourceIndex;
			TargetNameAttribute.Modify()[NewIndex] = InputData.TargetName;
			SourceNameAttribute.Modify()[NewIndex] = InputData.SourceName;
			StiffnessAttribute.Modify()[NewIndex] = InputData.Stiffness;
			DampingAttribute.Modify()[NewIndex] = InputData.Damping;
			TargetWeightsAttribute.Modify()[NewIndex] = InputData.TargetWeights;
			SourceWeightsAttribute.Modify()[NewIndex] = InputData.SourceWeights;
			// 
			// 
			//TManagedArray<FString>& PositionTargetToGroupLocal = PositionTargetToGroupAttribute.Modify();

			//PositionTarget[NewIndex] = Key.Index;
			//PositionTargetToGroupLocal[NewIndex] = Key.GroupName.ToString();

			//TManagedArrayAccessor<TArray<int32>> TargetIndexAttribute;
			//TManagedArrayAccessor<TArray<int32>> SourceIndexAttribute;
			//TManagedArrayAccessor<FString> TargetNameAttribute;
			//TManagedArrayAccessor<FString> SourceNameAttribute;
			//TManagedArrayAccessor<float> StiffnessAttribute;
			//TManagedArrayAccessor<float> DampingAttribute;

			return NewIndex;
		}
		return INDEX_NONE;
	}

	FPositionTargetsData FPositionTargetFacade::GetPositionTarget(const int32 DataIndex) const
	{
		FPositionTargetsData ReturnData;
		if (IsValid())
		{
			if (StiffnessAttribute.Num() > DataIndex && DataIndex > -1) 
			{
				ReturnData.Stiffness = StiffnessAttribute.Get()[DataIndex];
			}

			if (DampingAttribute.Num() > DataIndex && DataIndex > -1)
			{
				ReturnData.Damping = DampingAttribute.Get()[DataIndex];
			}

			if (SourceNameAttribute.Num() > DataIndex && DataIndex > -1)
			{
				ReturnData.SourceName = SourceNameAttribute.Get()[DataIndex];
			}

			if (TargetNameAttribute.Num() > DataIndex && DataIndex > -1)
			{
				ReturnData.TargetName = TargetNameAttribute.Get()[DataIndex];
			}

			if (SourceIndexAttribute.Num() > DataIndex && DataIndex > -1)
			{
				ReturnData.SourceIndex = SourceIndexAttribute.Get()[DataIndex];
			}

			if (TargetIndexAttribute.Num() > DataIndex && DataIndex > -1)
			{
				ReturnData.TargetIndex = TargetIndexAttribute.Get()[DataIndex];
			}

			if (SourceWeightsAttribute.Num() > DataIndex && DataIndex > -1)
			{
				ReturnData.SourceWeights = SourceWeightsAttribute.Get()[DataIndex];
			}

			if (TargetWeightsAttribute.Num() > DataIndex && DataIndex > -1)
			{
				ReturnData.TargetWeights = TargetWeightsAttribute.Get()[DataIndex];
			}

		}
		return ReturnData;
	
	}



}
