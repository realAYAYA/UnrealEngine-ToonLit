// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableSkeletalMeshActor.generated.h"

class UObject;

UCLASS(ClassGroup = CustomizableObject, Blueprintable, ComponentWrapperClass, ConversionRoot, meta = (ChildCanTick))
class CUSTOMIZABLEOBJECT_API ACustomizableSkeletalMeshActor : public ASkeletalMeshActor
{
	GENERATED_UCLASS_BODY()
public:
	
	class UCustomizableSkeletalComponent* GetCustomizableSkeletalComponent(int32 Index = 0) const { return CustomizableSkeletalComponents[Index]; }
	
	class USkeletalMeshComponent* GetSkeletalMeshComponentAt(int32 Index = 0) const { return SkeletalMeshComponents[Index]; }

	void AttachNewComponent();

	int32 GetNumComponents() { return CustomizableSkeletalComponents.Num(); }

private:
	
	UPROPERTY(Category = CustomizableSkeletalMesh, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray< TObjectPtr<class UCustomizableSkeletalComponent> > CustomizableSkeletalComponents;

	UPROPERTY(Category = CustomizableSkeletalMesh, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray< TObjectPtr<class USkeletalMeshComponent>> SkeletalMeshComponents;

	// TODO: This is a temporary fix to not break the demos, we should update the demos to support the arrays of components
	UPROPERTY(Category = CustomizableSkeletalMesh, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCustomizableSkeletalComponent> CustomizableSkeletalComponent;
};