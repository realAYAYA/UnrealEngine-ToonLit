// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

class UPackage;
struct FConcertPackageInfo;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConcertClientLocalPackageEvent, const FConcertPackageInfo&, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnConcertClientLocalPackageDiscarded, UPackage*);

/**
 * Bridge between the editor package events and Concert.
 * Deals with converting package update events to Concert package data.
 */
class IConcertClientPackageBridge
{
public:
	/** Scoped struct to ignore a local save */
	struct FScopedIgnoreLocalSave : private TGuardValue<bool>
	{
		FScopedIgnoreLocalSave(IConcertClientPackageBridge& InPackageBridge)
			: TGuardValue(InPackageBridge.GetIgnoreLocalSaveRef(), true)
		{
		}
	};

	/** Scoped struct to ignore a local discard */
	struct FScopedIgnoreLocalDiscard : private TGuardValue<bool>
	{
		FScopedIgnoreLocalDiscard(IConcertClientPackageBridge& InPackageBridge)
			: TGuardValue(InPackageBridge.GetIgnoreLocalDiscardRef(), true)
		{
		}
	};

	virtual ~IConcertClientPackageBridge() = default;

	/**
	 * Called when a local package event happens.
	 */
	virtual FOnConcertClientLocalPackageEvent& OnLocalPackageEvent() = 0;

	/**
	 * Called when a local package is discarded.
	 */
	virtual FOnConcertClientLocalPackageDiscarded& OnLocalPackageDiscarded() = 0;

protected:
	/**
	 * Function to access the internal bool controlling whether local saves are currently being tracked.
	 * @note Exists to implement FScopedIgnoreLocalSave.
	 */
	virtual bool& GetIgnoreLocalSaveRef() = 0;

	/**
	 * Function to access the internal bool controlling whether local discards are currently being tracked.
	 * @note Exists to implement FScopedIgnoreLocalDiscard.
	 */
	virtual bool& GetIgnoreLocalDiscardRef() = 0;
};
