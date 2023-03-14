// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/Blueprint.h"
#include "Factories/Factory.h"
#include "ControlRig.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintFactory.generated.h"

UCLASS(HideCategories=Object)
class CONTROLRIGEDITOR_API UControlRigBlueprintFactory : public UFactory
{
	GENERATED_BODY()

public:
	UControlRigBlueprintFactory();

	// The parent class of the created blueprint
	UPROPERTY(EditAnywhere, Category="Control Rig Factory", meta=(AllowAbstract = ""))
	TSubclassOf<UControlRig> ParentClass;

	// UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	/**
	 * Create a new control rig asset within the contents space of the project.
	 * @param InDesiredPackagePath The package path to use for the control rig asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Control Rig")
	static UControlRigBlueprint* CreateNewControlRigAsset(const FString& InDesiredPackagePath);

	/**
	 * Create a new control rig asset within the contents space of the project
	 * based on a skeletal mesh or skeleton object.
	 * @param The SkeletalMesh / Skeleton object to base the control rig asset on
	 */
	UFUNCTION(BlueprintCallable, Category = "Control Rig")
	static UControlRigBlueprint* CreateControlRigFromSkeletalMeshOrSkeleton(UObject* InSelectedObject);

	/**
	 * Creates the riggraph if necessary
	 * @param InBlueprint The blueprint to setup up
	 */
	static void CreateRigGraphIfRequired(UControlRigBlueprint* InBlueprint);
};

