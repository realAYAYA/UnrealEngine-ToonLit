// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakInterfacePtr.h"
#include "AvaSceneSubsystem.generated.h"

class IAvaSceneInterface;
class ULevel;

UCLASS(MinimalAPI, DisplayName = "Motion Design Scene Subsystem")
class UAvaSceneSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterSceneInterface(ULevel* InLevel, IAvaSceneInterface* InSceneInterface);

	/** Gets the Scene Interface for the World's Persistent Level */
	AVALANCHE_API IAvaSceneInterface* GetSceneInterface() const;

	/** Gets the Scene Interface for the provided Level */
	AVALANCHE_API IAvaSceneInterface* GetSceneInterface(ULevel* InLevel) const;

	/** Returns the first interface found in the given Level, if any */
	AVALANCHE_API static IAvaSceneInterface* FindSceneInterface(ULevel* InLevel);

protected:
	//~ Begin UWorldSubsystem
	AVALANCHE_API virtual void PostInitialize() override;
	AVALANCHE_API virtual bool DoesSupportWorldType(const EWorldType::Type InWorldType) const override;
	//~ End UWorldSubsystem

private:
	TMap<TWeakObjectPtr<ULevel>, TWeakInterfacePtr<IAvaSceneInterface>> SceneInterfaces;
};
