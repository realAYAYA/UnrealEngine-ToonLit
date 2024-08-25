// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreFwd.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"

/**
 * Helper class that can build Editor FSoftObjectPath(s) based on EditorPathOwners in an object's outer chain.
 */
class FEditorPathHelper
{
#if WITH_EDITOR
public:
	static COREUOBJECT_API FSoftObjectPath GetEditorPath(const UObject* InObject);
	static COREUOBJECT_API FSoftObjectPath GetEditorPathFromReferencer(const UObject* InObject, const UObject* InReferencer);
	static COREUOBJECT_API FSoftObjectPath GetEditorPathFromEditorPathOwner(const UObject* InObject, const UObject* InEditorPathOwner);
	static COREUOBJECT_API UObject* GetEditorPathOwner(const UObject* InObject);
	static COREUOBJECT_API bool IsInEditorPath(const UObject* InEditorPathOwner, const UObject* InObject);
	static COREUOBJECT_API bool IsEnabled();
#endif
};