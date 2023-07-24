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
		void InitUnboundedGroup(FName GroupName, FName DependencyGroup);
		void InitWeightedUnboundedGroup(FName GroupName, FName DependencyGroup);
		void InitBoundedGroup(FName GroupName, FName DependencyGroup, FName BoneDependencyGroup);
		void InitWeightedBoundedGroup(FName GroupName, FName DependencyGroup, FName BoneDependencyGroup);

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
		*/
		FSelectionFacade(FManagedArrayCollection& InSelf);
		FSelectionFacade(const FManagedArrayCollection& InSelf);

		/** Is the facade defined constant. */
		bool IsConst() const { return Collection==nullptr; }

		/**
		* Add the indices to the FVertexSetInterface::UnboundGroup 
		*/
		FSelectionKey AddSelection(const TArray<int32>& Indices, FName DependencyGroup);

		/**
		* Add the indices to the FVertexSetInterface::WeightedUnboundGroup
		*/
		FSelectionKey AddSelection(const TArray<int32>& Indices, const TArray<float>& Weights, FName DependencyGroup);

		/**
		* Add the indices to the FVertexSetInterface::BoundGroup
		*/
		FSelectionKey AddSelection(const int32 BoneIndex, const TArray<int32>& Indices, FName DependencyGroup, FName BoneDependencyGroup = FName(""));

		/**
		* Add the indices to the FVertexSetInterface::BoundGroup
		*/
		FSelectionKey AddSelection(const int32 BoneIndex, const TArray<int32>& Indices, const TArray<float>& Weights, FName DependencyGroup, FName BoneDependencyGroup = FName(""));

		/**
		* Return the vertex list from the given key 
		* @param Key : <GroupName and Index>
		* @param Indices : Return indices, empty if not found. 
		*/
		void GetSelection(const FSelectionKey& Key, TArray<int32>& Indices) const;

		/**
		* Return the vertex list, bone index, and weights from the given key
		* @param Key : <GroupName and Index>
		* @param BoneIndex : Return BoneIndex, INDEX_NONE if not found.
		* @param Indices : Return indices, empty if not found.
		* @param Weights : Return vertex weights, empty if not found.
		*/
		void GetSelection(const FSelectionKey& Key, TArray<int32>& Indices, TArray<float>& Weights) const;

		/**
		* Return the vertex list and bone index from the given key
		* @param Key : <GroupName and Index>
		* @param BoneIndex : Return BoneIndex, INDEX_NONE if not found.
		* @param Indices : Return indices, empty if not found.
		*/
		void GetSelection(const FSelectionKey& Key, int32& BoneIndex, TArray<int32>& Indices) const;

		/**
		* Return the vertex list, bone index, and weights from the given key
		* @param Key : <GroupName and Index>
		* @param BoneIndex : Return BoneIndex, INDEX_NONE if not found.
		* @param Indices : Return indices, empty if not found.
		* @param Weights : Return vertex weights, empty if not found.
		*/
		void GetSelection(const FSelectionKey& Key, int32& BoneIndex, TArray<int32>& Indices, TArray<float>& Weights) const;

	private:

		// const collection will be a null pointer, 
		// while non-const will be valid.
		const FManagedArrayCollection& ConstCollection;
		FManagedArrayCollection* Collection = nullptr;
	};

}