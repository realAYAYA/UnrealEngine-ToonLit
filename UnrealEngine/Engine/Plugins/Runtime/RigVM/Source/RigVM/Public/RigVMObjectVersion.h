// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

// Custom serialization version for changes made in Dev-Anim stream
struct RIGVM_API FRigVMObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded,
		
		// ControlRig & RigVMHost compute and checks VM Hash
		AddedVMHashChecks,

		// Predicates added to execute operations
		PredicatesAddedToExecuteOps,

		// Storing paths to user defined structs map
		VMStoringUserDefinedStructMap,

		// Storing paths to user defined enums map
		VMStoringUserDefinedEnumMap,

		// Storing paths to user defined enums map
		HostStoringUserDefinedData,

		// VM Memory Storage Struct serialized
		VMMemoryStorageStructSerialized,

		// VM Memory Storage Defaults generated at VM
		VMMemoryStorageDefaultsGeneratedAtVM,

		// VM Bytecode Stores the Public Context Path
		VMBytecodeStorePublicContextPath,

		// Removing unused tooltip property from frunction header
		VMRemoveTooltipFromFunctionHeader,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FRigVMObjectVersion() {}
};
