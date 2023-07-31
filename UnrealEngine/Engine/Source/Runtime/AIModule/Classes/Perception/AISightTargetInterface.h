// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "AISightTargetInterface.generated.h"

class AActor;

UINTERFACE()
class AIMODULE_API UAISightTargetInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class AIMODULE_API IAISightTargetInterface
{
	GENERATED_IINTERFACE_BODY()

	/**	
	 * The method needs to check whether the implementer is visible from given observer's location. 
	 * @param ObserverLocation	The location of the observer
	 * @param OutSeenLocation	The first visible target location
	 * @param OutSightStrengh	The sight strength for how well the target is seen
	 * @param IgnoreActor		The actor to ignore when doing test
	 * @param bWasVisible		If available, it is the previous visibility state
	 * @param UserData			If available, it is a data passed between visibility tests for the users to store whatever they want
	 * @return	True if visible from the observer's location
	 */
	virtual bool CanBeSeenFrom(const FVector& ObserverLocation, FVector& OutSeenLocation, int32& NumberOfLoSChecksPerformed, float& OutSightStrength, const AActor* IgnoreActor = nullptr, const bool* bWasVisible = nullptr, int32* UserData = nullptr) const
	{ 
		NumberOfLoSChecksPerformed = 0;
		OutSightStrength = 0;
		return false; 
	}

	UE_DEPRECATED(4.27, "This function is deprecated. Use the new CanBeSeenFrom method signature")
	virtual bool CanBeSeenFrom(const FVector& ObserverLocation, FVector& OutSeenLocation, int32& NumberOfLoSChecksPerformed, float& OutSightStrength, const AActor* IgnoreActor = nullptr) const final
	{
		NumberOfLoSChecksPerformed = 0;
		OutSightStrength = 0;
		return false;
	}
};

