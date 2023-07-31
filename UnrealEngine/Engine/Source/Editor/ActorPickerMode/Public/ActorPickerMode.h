// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class AActor;

DECLARE_DELEGATE_OneParam( FOnGetAllowedClasses, TArray<const UClass*>& );
DECLARE_DELEGATE_OneParam( FOnActorSelected, AActor* );
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnShouldFilterActor, const AActor*);

/**
 * Actor picker mode module
 */
class ACTORPICKERMODE_API FActorPickerModeModule : public IModuleInterface
{
public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End of IModuleInterface

	/** 
	 * Enter actor picking mode (note: will cancel any current actor picking)
	 * @param	InOnGetAllowedClasses	Delegate used to only allow actors using a particular set of classes (empty to accept all actor classes; works alongside InOnShouldFilterActor)
	 * @param	InOnShouldFilterActor	Delegate used to only allow particular actors (empty to accept all actors; works alongside InOnGetAllowedClasses)
	 * @param	InOnActorSelected		Delegate to call when a valid actor is selected
	 */
	void BeginActorPickingMode(FOnGetAllowedClasses InOnGetAllowedClasses, FOnShouldFilterActor InOnShouldFilterActor, FOnActorSelected InOnActorSelected) const;

	/** Exit actor picking mode */
	void EndActorPickingMode() const;

	/** @return Whether or not actor picking mode is currently active */
	bool IsInActorPickingMode() const;
	
private:

	/** Handler for when the application is deactivated. */
	void OnApplicationDeactivated(const bool IsActive) const;

private:
	FDelegateHandle OnApplicationDeactivatedHandle;
};
