// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/UnrealString.h"
#include "Containers/Array.h"
#include "GeometryCollection/ManagedArrayCollection.h"

namespace GeometryCollection::Facades
{

	/**
	* FSelectionFacade
	* 
	* Defines common API for storing a sib indexing of a separate group. For example, to store
	* a subset of vertices defined in the group "VerticesGroup", use the function :
	*    Key = this->AddSelection({1,2,3,4});
	* The key can be saved and later used to query the values:
	*	TArray<int32> SavedVertices;
	*   this->GetSelection(Key, SavedVertices);
	*
	* The following groups are created on the collection based on which API is called. 
	* 
	*	<Group> = FSelectionFacade::UnboundGroup_<DependencyGroup>
	*	- FindAttribute<TArray<int32>>(FVertexSetInterface::IndexAttribute, <Group>);
	* 
	*	<Group> = FSelectionFacade::WeightedUnboundGroup_<DependencyGroup>
	*	- FindAttribute<TArray<int32>>(FVertexSetInterface::IndexAttribute, <Group>);
	*	- FindAttribute<TArray<float>>(FVertexSetInterface::WeightAttribute, <Group>);
	* 
	*	<Group> = FSelectionFacade::BoundGroup_<DependencyGroup>
	*	- FindAttribute<TArray<int32>>(FVertexSetInterface::IndexAttribute, <Group>);
	*	- FindAttribute<int32>(FVertexSetInterface::BoneIndexAttribute,<Group>);
	*
	*	<Group> = FSelectionFacade::WeightedBoundGroup_<DependencyGroup>
	*	- FindAttribute<TArray<int32>>(FVertexSetInterface::IndexAttribute, <Group>);
	* 	- FindAttribute<TArray<float>>(FVertexSetInterface::WeightAttribute, <Group>);
	*	- FindAttribute<int32>(FVertexSetInterface::BoneIndexAttribute, <Group>);
	* 
	*/
	class CHAOS_API FSelectionFacade
 	{
		FManagedArrayCollection* Self;

		static void InitUnboundedGroup(FManagedArrayCollection* Self, FName GroupName, FName DependencyGroup);
		static void InitWeightedUnboundedGroup(FManagedArrayCollection* Self, FName GroupName, FName DependencyGroup);
		static void InitBoundedGroup(FManagedArrayCollection* Self, FName GroupName, FName DependencyGroup, FName BoneDependencyGroup);
		static void InitWeightedBoundedGroup(FManagedArrayCollection* Self, FName GroupName, FName DependencyGroup, FName BoneDependencyGroup);

	public:

		// groups
		static const FName UnboundGroup;
		static const FName WeightedUnboundGroup;
		static const FName BoundGroup;
		static const FName WeightedBoundGroup;

		// Attributes
		static const FName IndexAttribute;
		static const FName WeightAttribute;
		static const FName BoneIndexAttribute;

		struct FSelectionKey {
			FSelectionKey(int32 InIndex = INDEX_NONE, FName InGroupName = "") :
				Index(InIndex), GroupName(InGroupName) {}
			int32 Index;
			FName GroupName;
		};

		/**
		* FSelectionFacade Constuctor
		* @param VertixDependencyGroup : GroupName the index attribute is dependent on. 
		*/
		FSelectionFacade(FManagedArrayCollection* InSelf);

		/**
		* Add the indices to the FVertexSetInterface::UnboundGroup 
		* @param Indices : Array of indices to add. 
		*/
		static FSelectionKey AddSelection(FManagedArrayCollection* Self, const TArray<int32>& Indices, FName DependencyGroup);

		/**
		* Add the indices to the FVertexSetInterface::WeightedUnboundGroup
		* @param Indices : Array of indices to add.
		* @param Indices : Array of indices to add.
		*/
		static FSelectionKey AddSelection(FManagedArrayCollection* Self, const TArray<int32>& Indices, const TArray<float>& Weights, FName DependencyGroup);

		/**
		* Add the indices to the FVertexSetInterface::BoundGroup
		* @param Indices : Array of indices to add.
		* @param Indices : Array of indices to add.
		*/
		static FSelectionKey AddSelection(FManagedArrayCollection* Self, const int32 BoneIndex, const TArray<int32>& Indices, FName DependencyGroup, FName BoneDependencyGroup = FName(""));

		/**
		* Add the indices to the FVertexSetInterface::BoundGroup
		* @param Indices : Array of indices to add.
		* @param Indices : Array of indices to add.
		*/
		static FSelectionKey AddSelection(FManagedArrayCollection* Self, const int32 BoneIndex, const TArray<int32>& Indices, const TArray<float>& Weights, FName DependencyGroup, FName BoneDependencyGroup = FName(""));

		/**
		* Return the vertex list from the given key 
		* @param Key : <GroupName and Index>
		* @param Indices : Return indices, empty if not found. 
		*/
		static void GetSelection(const FManagedArrayCollection* Self, const FSelectionKey& Key, TArray<int32>& Indices);

		/**
		* Return the vertex list, bone index, and weights from the given key
		* @param Key : <GroupName and Index>
		* @param BoneIndex : Return BoneIndex, INDEX_NONE if not found.
		* @param Indices : Return indices, empty if not found.
		* @param Weights : Return vertex weights, empty if not found.
		*/
		static void GetSelection(const FManagedArrayCollection* Self, const FSelectionKey& Key, TArray<int32>& Indices, TArray<float>& Weights);

		/**
		* Return the vertex list and bone index from the given key
		* @param Key : <GroupName and Index>
		* @param BoneIndex : Return BoneIndex, INDEX_NONE if not found.
		* @param Indices : Return indices, empty if not found.
		*/
		static void GetSelection(const FManagedArrayCollection* Self, const FSelectionKey& Key, int32& BoneIndex, TArray<int32>& Indices);

		/**
		* Return the vertex list, bone index, and weights from the given key
		* @param Key : <GroupName and Index>
		* @param BoneIndex : Return BoneIndex, INDEX_NONE if not found.
		* @param Indices : Return indices, empty if not found.
		* @param Weights : Return vertex weights, empty if not found.
		*/
		static void GetSelection(const FManagedArrayCollection* Self, const FSelectionKey& Key, int32& BoneIndex, TArray<int32>& Indices, TArray<float>& Weights);
	};

}