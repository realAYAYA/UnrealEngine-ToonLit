// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Misc/Crc.h"

/**
* TransformCollection (ManagedArrayCollection)
*
* Stores the TArray<T> groups necessary to process transform hierarchies.
*
* @see FTransformCollectionComponent
*/
class CHAOS_API FTransformCollection : public FManagedArrayCollection
{
public:
	typedef FManagedArrayCollection Super;

	FTransformCollection();
	FTransformCollection(FTransformCollection &) = delete;
	FTransformCollection& operator=(const FTransformCollection&) = delete;
	FTransformCollection(FTransformCollection&&) = default;
	FTransformCollection& operator=(FTransformCollection&&) = default;


	/***
	*  Attribute Groups
	*
	*   These attribute groups are predefined data member of the FTransformCollection.
	*
	*   TransformGroup ("Transform")
	*		Default Attributes :
	*
	*          FTransformArray Transform =  GetAttribute<FTransform>("Transform", TransformGroup)
	*		   FInt32Array Level = GetAttribute<int32>("Level", TransformGroup) FIX
	*		   FInt32Array Parent = GetAttribute<int32>("Parent", TransformGroup) FIX
	*		   FInt32Array Children = GetAttribute<TSet<int32>>("Children", TransformGroup) FIX
	*
	*       The TransformGroup defines transform information for each Vertex. All positional
	*       information stored within Vertices and Geometry groups should be relative to its
	*       TransformGroup Transform.
	*       Parent defines the parent index of one transform node relative to another (Invalid is no parent exists, i.e. is root)
	*       Children defines the child indices of the transform node in the transform hierarchy (leaf nodes will have no children)
	*       Level is the distance from the root node at level 0. Leaf nodes will have the highest level number.
	*/
	static const FName TransformGroup;
	static const FName TransformAttribute;
	static const FName ParentAttribute;
	static const FName ChildrenAttribute;
	static const FName ParticlesAttribute;

	/** Serialize */
	virtual void Serialize(Chaos::FChaosArchive& Ar) override;

	/*
	* SingleTransform:
	*   Create a single transform.
	*/
	static FTransformCollection SingleTransform(const FTransform& TransformRoot = FTransform::Identity);

	/*
	* AppendTransform:
	*   Append a transform at the end of the collection without
	*   parenting. 
	*/
	int32 AppendTransform(const FTransformCollection & GeometryCollection, const FTransform& TransformRoot = FTransform::Identity);

	/*
	* ParentTransforms
	*   Parent Transforms under the specified node using local parent
	*   hierarchy compensation. .
	*/
	void ParentTransforms(const int32 TransformIndex, const int32 ChildIndex);
	void ParentTransforms(const int32 TransformIndex, const TArray<int32>& SelectedBones);
	void UnparentTransform(const int32 ChildIndex);

	/*
	* RelativeTransformation
	*   Modify the specified index by the local matrix offset. 
	*/
	void RelativeTransformation(const int32& Index, const FTransform& LocalOffset); 

	/**
	* RemoveElements
	*   Remove elements from the transform collection. Transform children are re-parented
	*   under the deleted elements parent using local parent compensation [relative local matrices].
	* 
	*/
	virtual void RemoveElements(const FName & Group, const TArray<int32> & SortedDeletionList, FProcessingParameters Params = FProcessingParameters()) override;

	// Transform Group
	TManagedArray<FTransform>   Transform;
	TManagedArray<FString>      BoneName;
	TManagedArray<FLinearColor> BoneColor;
	TManagedArray<int32>        Parent;
	TManagedArray<TSet<int32>>  Children;


protected:

	/** Construct */
	void Construct();
};