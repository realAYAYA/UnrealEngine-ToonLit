// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class ADMXMVRSceneActor;
class UDMXEntityFixturePatch;
class UDMXEntityFixtureType;

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
	/** Called when a Fixture Patch changed */
	void OnFixturePatchChanged(const UDMXEntityFixturePatch* FixturePatch);

	/** Called when a Fixture Type changed */
	void OnFixtureTypeChanged(const UDMXEntityFixtureType* FixtureType);

	/** Called before an Actor Class changed in the GDTFToActorClasses member of the Actor */
	void OnPreEditChangeActorClassInGDTFToActorClasses();

	/** Called after an Actor Class changed in the GDTFToActorClasses member of the Actor */
	void OnPostEditChangeActorClassInGDTFToActorClasses();

	/** Forces this Details Customization to refresh */
	void ForceRefresh();

	/** Handle to the Actor Class Property in the GDTFToDefaultActorClasses struct */
	TSharedPtr<IPropertyHandle> DefaultActorClassHandle;

	/** The Actors being customized in this Detais Customization */
	TArray<TWeakObjectPtr<ADMXMVRSceneActor>> OuterSceneActors;

	/** Property Utilities for this Details Customization */
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
