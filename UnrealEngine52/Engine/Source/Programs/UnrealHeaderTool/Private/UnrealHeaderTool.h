// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

// Enable property tags for enums and structures
#define UHT_ENABLE_VALUE_PROPERTY_TAG 1

// Enable property tags for delegates
#define UHT_ENABLE_DELEGATE_PROPERTY_TAG 1

// Enable property tags for class and interfaces wrapped in porters 
// DO NOT ENABLE: This does not handle forward declarations.  Also,
// we might never need this tracking since it is always by pointer.
#define UHT_ENABLE_PTR_PROPERTY_TAG 0

struct FHeaderParserNames
{
	static const FName NAME_IsConversionRoot;
	static const FName NAME_HideCategories;
	static const FName NAME_ShowCategories;
	static const FName NAME_SparseClassDataTypes;
	static const FName NAME_BlueprintType;
	static const FName NAME_AutoCollapseCategories;
	static const FName NAME_HideFunctions;
	static const FName NAME_AutoExpandCategories;
	static const FName NAME_PrioritizeCategories;
};