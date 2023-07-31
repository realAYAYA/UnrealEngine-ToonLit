// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/Set.h"
#include "GameFramework/Actor.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "CustomizableInstanceLODManagement.generated.h"


// This is an abstract base class, override it to create a new Instance LOD management system and register with UCustomizableObjectSystem::SetInstanceLODManagement
UCLASS(Blueprintable, BlueprintType)
class CUSTOMIZABLEOBJECT_API UCustomizableInstanceLODManagementBase : public UObject
{
	GENERATED_BODY()
public:
	UCustomizableInstanceLODManagementBase() : UObject() {};
	virtual ~UCustomizableInstanceLODManagementBase() {};

	virtual void UpdateInstanceDistsAndLODs() { check(0 && "You must override this"); };

	virtual int32 GetNumGeneratedInstancesLimitFullLODs() { check(0 && "You must override this"); return 0; };
	virtual int32 GetNumGeneratedInstancesLimitLOD1() { check(0 && "You must override this"); return 0; };
	virtual int32 GetNumGeneratedInstancesLimitLOD2() { check(0 && "You must override this"); return 0; };

	virtual bool IsOnlyUpdateCloseCustomizableObjectsEnabled() const { check(0 && "You must override this"); return false; };
	virtual float GetOnlyUpdateCloseCustomizableObjectsDist() const { check(0 && "You must override this"); return 0.f; };
};


UCLASS(Blueprintable, BlueprintType)
class CUSTOMIZABLEOBJECT_API UCustomizableInstanceLODManagement : public UCustomizableInstanceLODManagementBase
{
	GENERATED_BODY()
public:
	UCustomizableInstanceLODManagement();
	virtual ~UCustomizableInstanceLODManagement();

	virtual void UpdateInstanceDistsAndLODs() override;

	virtual int32 GetNumGeneratedInstancesLimitFullLODs() override;
	virtual int32 GetNumGeneratedInstancesLimitLOD1() override;
	virtual int32 GetNumGeneratedInstancesLimitLOD2() override;

	// Used to define the actors that will be considered the centers of LOD distance calculations. If left empty it will revert to the default, the first player controller actor
	void AddViewCenter(const AActor* const InCentralActor) { ViewCenters.Add(TWeakObjectPtr<const AActor>(InCentralActor)); };
	void RemoveViewCenter(const AActor* const InCentralActor) { ViewCenters.Remove(TWeakObjectPtr<const AActor>(InCentralActor)); };
	void ClearViewCenters() { ViewCenters.Empty(); }

	// Sets how many of the nearest instances to the player will have updates with priority over LOD changes, only works if LODs are enabled with b.NumGeneratedInstancesLimitLODs
	void SetNumberOfPriorityUpdateInstances(int32 NumPriorityUpdateInstances);
	int32 GetNumberOfPriorityUpdateInstances() const;

	virtual bool IsOnlyUpdateCloseCustomizableObjectsEnabled() const override;
	virtual float GetOnlyUpdateCloseCustomizableObjectsDist() const override;
	void EnableOnlyUpdateCloseCustomizableObjects(float CloseDist);
	void DisableOnlyUpdateCloseCustomizableObjects();

private:
	// The list of actors that define a view radius (CloseDist) around them used for LOD management
	TSet<TWeakObjectPtr<const AActor>> ViewCenters;
	
	// Sets how many of the nearest instances to the player will have updates with priority over LOD changes
	int32 NumPriorityUpdateInstances = 1;

	bool bOnlyUpdateCloseCustomizableObjects;
	float CloseCustomizableObjectsDist;	
};

