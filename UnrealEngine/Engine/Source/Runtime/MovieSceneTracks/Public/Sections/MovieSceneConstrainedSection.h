// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/NameTypes.h"
#include "UObject/Interface.h"
#include "MovieSceneSection.h"
#include "ConstraintsManager.h"
#include "ConstraintChannel.h"
#include "MovieSceneConstrainedSection.generated.h"



/**
 * Functionality to add to sections that contain constraints
 */

UINTERFACE()
class MOVIESCENETRACKS_API UMovieSceneConstrainedSection : public UInterface
{
public:
	GENERATED_BODY()
};

/**
 * Interface to be added to UMovieSceneSection types when they contain entity data
 */
class MOVIESCENETRACKS_API IMovieSceneConstrainedSection
{
public:

	GENERATED_BODY()
	IMovieSceneConstrainedSection();

	DECLARE_MULTICAST_DELEGATE_TwoParams(FConstraintChannelAddedEvent, IMovieSceneConstrainedSection*, FMovieSceneConstraintChannel*);


	/*
	* If it has that constraint with that Name
	*/
	virtual bool HasConstraintChannel(const FName& InConstraintName) const = 0;

	/*
	* Get constraint with that name
	*/
	virtual FConstraintAndActiveChannel* GetConstraintChannel(const FName& InConstraintName) = 0;

	/*
	*  Add Constraint channel
	*/
	virtual void AddConstraintChannel(UTickableConstraint* InConstraint) = 0;
	
	/*
	*  Remove Constraint channel
	*/
	virtual void RemoveConstraintChannel(const FName& InConstraintName) = 0;

	/*
	*  Get The channels
	*/
	virtual TArray<FConstraintAndActiveChannel>& GetConstraintsChannels() = 0;

	/*
	*  Replace the constraint with the specified name with the new one
	*/
	virtual void ReplaceConstraint(const FName InName, UTickableConstraint* InConstraint) = 0;

	/*
	* Added Delegate
	*/
	 FConstraintChannelAddedEvent& ConstraintChannelAdded() { return OnConstraintChannelAdded; }

	 /*
	 *  What to do if the constraint object has been changed, for example by an undo or redo. By default nothing to be overriden if needed.
	 */
	 virtual void OnConstraintsChanged() {};

	/*
	*  Removed delegate that get's added by the track editor
	*/
	FDelegateHandle OnConstraintRemovedHandle;


protected:

	FConstraintChannelAddedEvent OnConstraintChannelAdded;


};
