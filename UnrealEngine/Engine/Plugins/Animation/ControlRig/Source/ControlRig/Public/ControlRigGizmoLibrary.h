// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"

#include "ControlRigGizmoLibrary.generated.h"

class UControlRigShapeLibrary;

USTRUCT(BlueprintType, meta = (DisplayName = "Shape"))
struct CONTROLRIG_API FControlRigShapeDefinition
{
	GENERATED_USTRUCT_BODY()

	FControlRigShapeDefinition()
	{
		ShapeName = TEXT("Default");
		StaticMesh = nullptr;
		Transform = FTransform::Identity;
	}

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shape")
	FName ShapeName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shape")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shape")
	FTransform Transform;

	mutable TWeakObjectPtr<UControlRigShapeLibrary> Library;
};

UCLASS(BlueprintType, meta = (DisplayName = "Control Rig Shape Library"))
class CONTROLRIG_API UControlRigShapeLibrary : public UObject
{
	GENERATED_BODY()

public:

	UControlRigShapeLibrary();

	UPROPERTY(EditAnywhere, Category = "ShapeLibrary")
	FControlRigShapeDefinition DefaultShape;

	UPROPERTY(EditAnywhere, Category = "ShapeLibrary", meta = (DisplayName = "Override Material"))
	TSoftObjectPtr<UMaterial> DefaultMaterial;

	UPROPERTY(EditAnywhere, Category = "ShapeLibrary")
	TSoftObjectPtr<UMaterial> XRayMaterial;

	UPROPERTY(EditAnywhere, Category = "ShapeLibrary")
	FName MaterialColorParameter;

	UPROPERTY(EditAnywhere, Category = "ShapeLibrary")
	TArray<FControlRigShapeDefinition> Shapes;

	const FControlRigShapeDefinition* GetShapeByName(const FName& InName, bool bUseDefaultIfNotFound = false) const;
	static const FControlRigShapeDefinition* GetShapeByName(const FName& InName, const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>& InShapeLibraries, const TMap<FString, FString>& InLibraryNameMap, bool bUseDefaultIfNotFound = true);
	static const FString GetShapeName(const UControlRigShapeLibrary* InShapeLibrary, bool bUseNameSpace, const TMap<FString, FString>& InLibraryNameMap, const FControlRigShapeDefinition& InShape);

#if WITH_EDITOR

	// UObject interface
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;

#endif

private:

	const TArray<FName> GetUpdatedNameList(bool bReset = false);

	TArray<FName> NameList;
};