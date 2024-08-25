// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "DMObjectMaterialProperty.generated.h"

class FProperty;
class UDynamicMaterialInstance;
class UDynamicMaterialModel;
class UObject;
class UPrimitiveComponent;

/**
 * Defines a material property slot that can be a Material Designer Instance.
 */
USTRUCT(BlueprintType)
struct DYNAMICMATERIALEDITOR_API FDMObjectMaterialProperty
{
	GENERATED_BODY()

	FDMObjectMaterialProperty();

	// UPrimitiveComponent Material Index
	FDMObjectMaterialProperty(UPrimitiveComponent* InOuter, int32 InIndex);

	// Class Property (including potential array index)
	FDMObjectMaterialProperty(UObject* InOuter, FProperty* InProperty, int32 InIndex = INDEX_NONE);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material")
	TWeakObjectPtr<UObject> OuterWeak;

	/** C++ version of property */
	FProperty* Property;

	/** Blueprint version of property */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material")
	FName PropertyName;

	/** Component or array property index. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material")
	int32 Index;

	UDynamicMaterialModel* GetMaterialModel() const;

	UDynamicMaterialInstance* GetMaterial() const;
	void SetMaterial(UDynamicMaterialInstance* DynamicMaterial);

	bool IsValid() const;

	FText GetPropertyName(bool bInIgnoreNewStatus) const;

	void Reset();
};
