// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

class FGLTFConvertBuilder;
struct FCapturedPropSegment;
class ULevelVariantSets;
class UMaterialInterface;
class UMeshComponent;
class UVariantSet;
class UVariant;
class UVariantObjectBinding;
class UPropertyValue;

struct FGLTFVariantUtility
{
	static const TArray<FCapturedPropSegment>& GetCapturedPropSegments(const UPropertyValue* Property);

	static bool IsStaticMeshProperty(const UPropertyValue* Property);
	static bool IsSkeletalMeshProperty(const UPropertyValue* Property);
	static bool IsVisibleProperty(const UPropertyValue* Property);

	template <typename T>
	static bool TryGetPropertyValue(UPropertyValue* Property, T& OutValue)
	{
		return TryGetPropertyValue(Property, &OutValue, sizeof(T));
	}

	static bool TryGetPropertyValue(UPropertyValue* Property, void* OutData, uint32 OutSize);

	static FString GetLogContext(const UPropertyValue* Property);
	static FString GetLogContext(const UVariantObjectBinding* Binding);
	static FString GetLogContext(const UVariant* Variant);
	static FString GetLogContext(const UVariantSet* VariantSet);
	static FString GetLogContext(const ULevelVariantSets* LevelVariantSets);

	static FGLTFJsonMaterial* AddUniqueMaterial(FGLTFConvertBuilder& Builder, const UMaterialInterface* Material, const UMeshComponent* MeshComponent, int32 MaterialIndex);
};
