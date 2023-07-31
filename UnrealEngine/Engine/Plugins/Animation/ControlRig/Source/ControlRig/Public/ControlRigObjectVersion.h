// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

// Custom serialization version for changes made in Dev-Anim stream
struct CONTROLRIG_API FControlRigObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded,

		// Added execution pins and removed hierarchy ref pins
		RemovalOfHierarchyRefPins,

		// Refactored operators to store FCachedPropertyPath instead of string
		OperatorsStoringPropertyPaths,

		// Introduced new RigVM as a backend
		SwitchedToRigVM,

		// Added a new transform as part of the control
		ControlOffsetTransform,

		// Using a cache data structure for key indices now
		RigElementKeyCache,

		// Full variable support
		BlueprintVariableSupport,

		// Hierarchy V2.0
		RigHierarchyV2,

		// RigHierarchy to support multi component parent constraints
		RigHierarchyMultiParentConstraints,

		// RigHierarchy now supports space favorites per control
		RigHierarchyControlSpaceFavorites,

		// RigHierarchy now stores min and max values as float storages 
		StorageMinMaxValuesAsFloatStorage,

		// RenameGizmoToShape 
		RenameGizmoToShape,

		// BoundVariableWithInjectionNode 
		BoundVariableWithInjectionNode,

		// Switch limit control over to per channel limits 
		PerChannelLimits,

		// Removed the parent cache for multi parent elements 
		RemovedMultiParentParentCache,

		// Deprecation of parameters
		RemoveParameters,

		// Added rig curve element value state flag
		CurveElementValueStateFlag,

		// Added the notion of a per control animation type
		ControlAnimationType,

		// Added preferred permutation for templates
		TemplatesPreferredPermutatation,

		// Added preferred euler angles to controls
		PreferredEulerAnglesForControls,

		// Added rig hierarchy element metadata
		HierarchyElementMetadata,

		// Converted library nodes to templates
		LibraryNodeTemplates,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FControlRigObjectVersion() {}
};
