// Copyright Epic Games, Inc. All Rights Reserved.

// UObjectGlobalsInternal.h - Global UObject functions for use by other engine modules but not by game modules.

#pragma once

#include "CoreMinimal.h"

/**
 * Global CoreUObject delegates for use by other engine modules
 */
struct FCoreUObjectInternalDelegates
{
	/** Called before GC verification code rename a package containing a World to try and prevent such errors blocking testing and development. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FPackageRename, UPackage*);
	static COREUOBJECT_API FPackageRename& GetOnLeakedPackageRenameDelegate();
};

    
