// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class ADMXMVRSceneActor;
class UDMXEntityFixturePatch;
class UDMXEntityFixtureType;
class UDMXImportGDTF;

enum class ECheckBoxState : uint8;
class IPropertyHandle;
class IPropertyUtilities;


/** Details customization for the 'FixtureType FunctionProperties' details view */
class FDMXMVRSceneActorDetails
	: public IDetailCustomization
{
public:
	/** Creates an instance of this details customization */
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization interface

private:
	/** Creates the section where the user can refresh the MVR scene from a changed DMX Library */
	void CreateRefreshMVRSceneSection(IDetailLayoutBuilder& DetailBuilder);

	/** Creates the section where the user can select an actor class for each GDTF in the MVR Scene */
	void CreateGDTFToActorClassSection(IDetailLayoutBuilder& DetailBuilder);

	/** Called when the Refresh Actors from DMX Library button was clicked */
	FReply OnRefreshActorsFromDMXLibraryClicked();

	/** Called when a GDTF to Actor Class Group was clicked */
	FReply OnSelectGDTFToActorClassGroupClicked(UObject* GDTFObject);

	/** Called when a Fixture Patch changed */
	void OnFixturePatchChanged(const UDMXEntityFixturePatch* FixturePatch);

	/** Called when a Fixture Type changed */
	void OnFixtureTypeChanged(const UDMXEntityFixtureType* FixtureType);

	/** Called when a sub-level is loaded */
	void OnMapChange(uint32 MapChangeFlags);

	/** Called when an actor got deleted in editor */
	void OnActorDeleted(AActor* DeletedActor);

	/** Called before an Actor Class changed in the GDTFToActorClasses member of the Actor */
	void OnPreEditChangeActorClassInGDTFToActorClasses();

	/** Called after an Actor Class changed in the GDTFToActorClasses member of the Actor */
	void OnPostEditChangeActorClassInGDTFToActorClasses();

	/** Returns true if any actor in the current level makes use of the specified GDTF */
	bool IsAnyActorUsingGDTF(const UDMXImportGDTF* GDTF) const;

	/** Requests this Details Customization to refresh */
	void RequestRefresh();

	/** Handle to the Actor Class Property in the GDTFToDefaultActorClasses struct */
	TSharedPtr<IPropertyHandle> DefaultActorClassHandle;

	/** The Actors being customized in this Detais Customization */
	TArray<TWeakObjectPtr<ADMXMVRSceneActor>> OuterSceneActors;

	/** Property Utilities for this Details Customization */
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
