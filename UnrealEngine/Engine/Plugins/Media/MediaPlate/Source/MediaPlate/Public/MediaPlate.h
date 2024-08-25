// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/AssetUserData.h"
#include "MediaPlate.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMediaPlateComponent;

namespace UE::MediaPlate::Private
{
	MEDIAPLATE_API void ApplyTranslucencyScreenPercentageCVar(int32 InBasis);
}

/**
 * MediaPlate is an actor that can play and show media in the world.
 */
UCLASS()
class MEDIAPLATE_API AMediaPlate : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin AActor Interface
	virtual void PostActorCreated();
	virtual void PostRegisterAllComponents() override;
	virtual void BeginDestroy() override;
	//~ End AActor Interface

	UPROPERTY(Category = MediaPlate, VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UMediaPlateComponent> MediaPlateComponent;

	/** Holds the mesh. */
	UPROPERTY(Category = MediaPlate, VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;

#if WITH_EDITOR

	/*
	 * Call this to change the static mesh to use the default media plate material and reset the overlay material.
	 */
	void UseDefaultMaterial();

	/**
	 * Call this after changing the current material to set it up for media plate.
	 */
	void ApplyCurrentMaterial();
	
	/**
	 * Setup the material for media plate use. Automatically called by ApplyCurrentMaterial.
	 */
	void ApplyMaterial(UMaterialInterface* InMaterial);

	/**
	 * Setup the overlay material for media plate use. Automatically called by ApplyCurrentMaterial.
	 */
	void ApplyOverlayMaterial(UMaterialInterface* InOverlayMaterial);

	/**
	 * Sets up parameters (like the texture) that we use in the material.
	 */
	void SetMIDParameters(UMaterialInstanceDynamic* InMaterial);
	
	/** Get the last material assigned to the static mesh, at index 0. */
	UMaterialInterface* GetLastMaterial() const { return LastMaterial; }
#endif // WITH_EDITOR

	/** Get the current static mesh material, at index 0. */
	UMaterialInterface* GetCurrentMaterial() const;

	/** Get the current static mesh overlay material, nullptr otherwise. */
	UMaterialInterface* GetCurrentOverlayMaterial() const;

private:
	/** Name for our media plate component. */
	static FLazyName MediaPlateComponentName;
	/** Name for the media texture parameter in the material. */
	static FLazyName MediaTextureName;

#if WITH_EDITOR
	UMaterialInterface* LastMaterial = nullptr;
	UMaterialInterface* LastOverlayMaterial = nullptr;

	/**
	 * Called before a level saves
	 */
	void OnPreSaveWorld(UWorld* InWorld, FObjectPreSaveContext ObjectSaveContext);

	/**
	 * Called after a level has saved.
	 */
	void OnPostSaveWorld(UWorld* InWorld, FObjectPostSaveContext ObjectSaveContext);

	/**
	 * Adds our asset user data to the static mesh component.
	 */
	void AddAssetUserData();

	/**
	 * Removes our asset user data from the static mesh component.
	 */
	void RemoveAssetUserData();

	/**
	 * Convenience function to apply create a material instance constant for media plate use.
	 */
	UMaterialInterface* CreateMaterialInstanceConstant(UMaterialInterface* InMaterial);

#endif // WITH_EDITOR
};
