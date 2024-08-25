// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsInterfaceDeclaresCore.h"
#include "Subsystems/WorldSubsystem.h"
#include "PhysicsMoverManager.generated.h"

//////////////////////////////////////////////////////////////////////////

class UMoverNetworkPhysicsLiaisonComponent;

UCLASS()
class MOVER_API UPhysicsMoverManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	void PrePhysicsUpdate(FPhysScene* PhysScene, float DeltaTime);

	void RegisterPhysicsMoverComponent(TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponent> InPhysicsMoverComp);
	void UnregisterPhysicsMoverComponent(TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponent> InPhysicsMoverComp);

private:
	TArray<TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponent>> PhysicsMoverComponents;
	FDelegateHandle PhysScenePreTickCallbackHandle;
	class FPhysicsMoverManagerAsyncCallback* AsyncCallback = nullptr;
};