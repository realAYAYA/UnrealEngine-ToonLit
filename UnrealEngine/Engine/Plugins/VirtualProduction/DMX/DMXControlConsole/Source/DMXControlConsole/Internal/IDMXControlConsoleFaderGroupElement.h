// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsoleFaderGroup.h"
#include "UObject/Interface.h"
#include "UObject/Object.h"

#include "IDMXControlConsoleFaderGroupElement.generated.h"

class UDMXControlConsoleControllerBase;
class UDMXControlConsoleFaderBase;


/** 
 * Interface used by elmeents that can be placed inside a Fixture Group.
 * Note, an Element has to be a set of consecutive channels.
 */
UINTERFACE()
class DMXCONTROLCONSOLE_API UDMXControlConsoleFaderGroupElement
	: public UInterface
{
	GENERATED_BODY()
};

class DMXCONTROLCONSOLE_API IDMXControlConsoleFaderGroupElement
{
	GENERATED_BODY()

public:
	/** Returns the Fader Group this Element resides in */
	virtual UDMXControlConsoleFaderGroup& GetOwnerFaderGroupChecked() const = 0;

	/** Returns the Element Controller of this Element */
	virtual UDMXControlConsoleControllerBase* GetElementController() const = 0;

	/** Sets the Element Controller of this Element */
	virtual void SetElementController(UDMXControlConsoleControllerBase* NewController) = 0;

	/** Returns the index of the Element in the Fader Group */
	virtual int32 GetIndex() const = 0;

	/** Returns the Faders of this Element */
	virtual const TArray<UDMXControlConsoleFaderBase*>& GetFaders() const = 0;

	/** Returns the index of the Element in the Fader Group */
	virtual int32 GetUniverseID() const = 0;

	/** Returns the first Channel of this Element */
	virtual int32 GetStartingAddress() const = 0;

	/** Returns the last Channel of this Element */
	virtual int32 GetEndingAddress() const = 0;

#if WITH_EDITOR
	/** True if the Element matches the Control Console filtering system */
	virtual bool IsMatchingFilter() const { return bIsMatchingFilter; }

	/** Sets wheter the Element matches the Control Console filtering system */
	virtual void SetIsMatchingFilter(bool bMatches) { bIsMatchingFilter = bMatches; }
#endif // WITH_EDITOR

	/** Destroys the Element */
	virtual void Destroy() = 0;

protected:
#if WITH_EDITOR
	/** True if the Element matches Control Console filtering system */
	bool bIsMatchingFilter = true;
#endif // WITH_EDITOR
};
