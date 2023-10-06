// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/WeakObjectPtr.h"

class FArchive;
 
// Helper struct to test if struct serializer object reference declaration tests work properly
struct FTestUndeclaredScriptStructObjectReferencesTest
{
	TObjectPtr<UObject> StrongObjectPointer;
	TSoftObjectPtr<UObject> SoftObjectPointer;
	FSoftObjectPath SoftObjectPath;
	TWeakObjectPtr<UObject> WeakObjectPointer;
	
	bool Serialize(FArchive& Ar);
};

template<> struct TBaseStructure<FTestUndeclaredScriptStructObjectReferencesTest>
{
	static UScriptStruct* Get();
};
