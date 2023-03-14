// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Guid.h"

struct OPTIMUSCORE_API FOptimusObjectVersion
{
	// Not instantiable.
	FOptimusObjectVersion() = delete;

	enum Type
	{
		InitialVersion,

		SwitchToMeshDeformerBase,
		ReparentResourcesAndVariables,
		SwitchToParameterBindingArrayStruct,
		AddBindingsToGraph,
		ComponentProviderSupport,
		SetPrimaryBindingName,
		DataDomainExpansion,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;
};
