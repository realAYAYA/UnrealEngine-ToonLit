// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class FProperty;
class UClass;
class UObject;
class UStruct;

namespace UE { class FPropertyBag; }
namespace UE { class FPropertyPathName; }

namespace UE
{

/**
 * Query if InstanceDataObject support is enabled for a specific object.
 *
 * Pass nullptr to query if the system is enabled.
 */
bool IsInstanceDataObjectSupportEnabled(UObject* Object = nullptr);

/** Generate a UClass that contains the union of the properties of PropertyBag and OwnerClass. */
UClass* CreateInstanceDataObjectClass(const FPropertyBag* PropertyBag, UClass* OwnerClass, UObject* Outer);

/** Mark a property within the object as having been set during deserialization. */
void MarkPropertySetBySerialization(UObject* Object, const FPropertyPathName& Path);
	
/** Query whether a property within the object was set when the object was deserialized. */
bool WasPropertySetBySerialization(UObject* Object, const FPropertyPathName& Path);
/** Query whether a property in the struct was set when the struct was deserialized. */
bool WasPropertySetBySerialization(const UStruct* Struct, const void* StructData, const FProperty* Property, int32 ArrayIndex = 0);

} // UE
