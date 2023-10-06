// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolChange.h"
#include "SelectionSet.h"




/**
 * FMeshSelectionChange represents an reversible change to a UMeshSelectionSet
 */
class MODELINGCOMPONENTS_API FMeshSelectionChange : public FToolCommandChange
{
public:
	EMeshSelectionElementType ElementType = EMeshSelectionElementType::Vertex;
	TArray<int32> Indices;
	bool bAdded;

	/** Makes the change to the object */
	virtual void Apply(UObject* Object) override;

	/** Reverts change to the object */
	virtual void Revert(UObject* Object) override;

	/** Describes this change (for debugging) */
	virtual FString ToString() const override;
};




/**
 * FMeshSelectionChangeBuilder can be used to construct a FMeshSelectionChange.
 */
class MODELINGCOMPONENTS_API FMeshSelectionChangeBuilder
{
public:
	TUniquePtr<FMeshSelectionChange> Change;

	/**
	 * Initialize of a selection change of given type. 
	 * @param ElementType type of element (face/edge/vtx) being added/removed
	 * @param bAdding if true, output change has bAdded=true, otherwise bAdded=false
	 */
	FMeshSelectionChangeBuilder(EMeshSelectionElementType ElementType, bool bAdding);

	/**
	 * Add ElementID to list of changed elements  (which may then be added or removed, depending on bAdding parameter to constructor)
	 */
	void Add(int32 ElementID);

	/**
	 * Add Array of elements
	 */
	void Add(const TArray<int32>& Elements);

	/**
	 * Add Set of elements
	 */
	void Add(const TSet<int32>& Elements);
};
