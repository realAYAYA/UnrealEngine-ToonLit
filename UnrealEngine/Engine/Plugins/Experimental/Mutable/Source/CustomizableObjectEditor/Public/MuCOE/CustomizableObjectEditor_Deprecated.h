// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/ObjectPtr.h"
#include "EdGraph/EdGraphPin.h"

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

// Deprecated, do not use!
constexpr int32 UV_LAYOUT_DEFAULT = -2;

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

// UCustomizableObjectNodeSkeletalMesh
// Deprecated, do not use!
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeSkeletalMeshMaterial
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	TObjectPtr<UEdGraphPin_Deprecated> MeshPin_DEPRECATED;

	UPROPERTY()
	TArray<TObjectPtr<UEdGraphPin_Deprecated>> LayoutPins_DEPRECATED;

	UPROPERTY()
	TArray<TObjectPtr<UEdGraphPin_Deprecated>> ImagePins_DEPRECATED;

	UPROPERTY()
	FEdGraphPinReference MeshPinRef;

	UPROPERTY()
	TArray<FEdGraphPinReference> LayoutPinsRef;

	UPROPERTY()
	TArray<FEdGraphPinReference> ImagePinsRef;
};

// Deprecated, do not use!
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeSkeletalMeshLOD
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FCustomizableObjectNodeSkeletalMeshMaterial> Materials;
};

// UCustomizableObjectNodeColorVariation
// Deprecated, do not use!
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectColorVariation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FString Tag;
};

// UCustomizableObjectFloatVariation
// Deprecated, do not use!
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectFloatVariation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FString Tag;
};

// UCustomizableObjectMaterialVariation
// Deprecated, do not use!
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectMaterialVariation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FString Tag;
};

// UCustomizableObjectMeshVariation
// Deprecated, do not use!
USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectMeshVariation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FString Tag;
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/Texture2D.h"
#endif
