// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "TypedElementMementoRowTypes.generated.h"

/**
 * @file MementoRowTypes.h
 * Column/Tags used internally for the memento system
 */

/**
 * MementoTag denotes that the row is a memento
 */
USTRUCT()
struct FTypedElementMementoTag : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

/**
 * A column added to a memento row which will trigger the process
 * of converting memento columns to the matching mementoizable columns in
 * the target row
 */
USTRUCT()
struct FTypedElementMementoReinstanceTarget : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	TypedElementRowHandle Target;
};

/**
 * Tag added to memento row which will cause the memento to be deleted.  It is used
 * in the case where mementoization was aborted due to a missing target object
 * from the reinstancing callback
 */
USTRUCT()
struct FTypedElementMementoReinstanceAborted : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};