// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "GeometryBase.h"
#include "InteractiveToolChange.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MeshReplacementChange.generated.h"

PREDECLARE_USE_GEOMETRY_CLASS(FDynamicMesh3);
class UObject;


/**
 * FMeshReplacementChange represents an undoable *complete* change to a FDynamicMesh3.
 * Currently only valid to call Apply/Revert when the Object is a UDynamicMeshComponent
 */
class GEOMETRYFRAMEWORK_API FMeshReplacementChange : public FToolCommandChange
{
	TSharedPtr<const FDynamicMesh3, ESPMode::ThreadSafe> Before, After;

public:
	FMeshReplacementChange();
	FMeshReplacementChange(TSharedPtr<const FDynamicMesh3, ESPMode::ThreadSafe> Before, TSharedPtr<const FDynamicMesh3, ESPMode::ThreadSafe> After);

	const TSharedPtr<const FDynamicMesh3, ESPMode::ThreadSafe>& GetMesh(bool bRevert) const
	{
		return bRevert ? Before : After;
	}

	/** This function is called on Apply and Revert (last argument is true on Apply)*/
	TFunction<void(FMeshReplacementChange*, UObject*, bool)> OnChangeAppliedFunc;

	/** Makes the change to the object */
	virtual void Apply(UObject* Object) override;

	/** Reverts change to the object */
	virtual void Revert(UObject* Object) override;

	/** Describes this change (for debugging) */
	virtual FString ToString() const override;
};





UINTERFACE()
class GEOMETRYFRAMEWORK_API UMeshReplacementCommandChangeTarget : public UInterface
{
	GENERATED_BODY()
};
/**
 * IMeshReplacementCommandChangeTarget is an interface which is used to apply a mesh replacement change
 */
class GEOMETRYFRAMEWORK_API IMeshReplacementCommandChangeTarget
{
	GENERATED_BODY()
public:
	virtual void ApplyChange(const FMeshReplacementChange* Change, bool bRevert) = 0;
};


