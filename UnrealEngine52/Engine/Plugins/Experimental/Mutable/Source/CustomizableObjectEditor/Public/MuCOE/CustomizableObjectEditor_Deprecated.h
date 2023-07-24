// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CustomizableObjectEditor_Deprecated.generated.h"

class UTexture2D;


// Place to hide all deprecated data structures. They are still needed for deserialization backwards compatibility.

// UCustomizableObjectNodeMaterial
// Deprecated, do not use!
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeMaterialImage
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	int32 UVLayout = 0;

	UPROPERTY()
	TObjectPtr<UTexture2D> ReferenceTexture = nullptr;
	
	UPROPERTY()
	int32 LayerIndex = 0;

	UPROPERTY()
	FString PinName;
};

// Deprecated, do not use!
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeMaterialVector
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	FString Name;
	
	UPROPERTY()
	int32 LayerIndex = 0;

	UPROPERTY()
	FString PinName;
};

// Deprecated, do not use!
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeMaterialScalar
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Name;
	
	UPROPERTY()
	int32 LayerIndex = 0;

	UPROPERTY()
	FString PinName;
};

// UCustomizableObjectNodeEditMaterial
// Deprecated, do not use!
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeEditMaterialImage
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Name;
};

// UCustomizableObjectNodeExtendMaterial
// Deprecated, do not use!
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeExtendMaterialImage
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Name;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/Texture2D.h"
#endif
