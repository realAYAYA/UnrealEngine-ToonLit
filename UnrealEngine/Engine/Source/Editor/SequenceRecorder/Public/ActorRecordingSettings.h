// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Templates/Casts.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "ActorRecordingSettings.generated.h"

class UObject;

USTRUCT()
struct SEQUENCERECORDER_API FActorRecordingSettings
{
	GENERATED_BODY()

	FActorRecordingSettings();

	FActorRecordingSettings(class UObject* InOuter);

	void CreateSettingsObjectsFromFactory();

	template <typename SettingsType>
	SettingsType* GetSettingsObject() const
	{
		for (UObject* SettingsObject : Settings)
		{
			if (SettingsType* TypedSettingsObject = Cast<SettingsType>(SettingsObject))
			{
				return TypedSettingsObject;
			}
		}

		return nullptr;
	}

private:
	/** External settings objects for recorders that supply them. Displayed via a details customization  */
	UPROPERTY(EditAnywhere, Category = "Actor Recording")
	TArray<TObjectPtr<UObject>> Settings;

	/** An optional outer that settings objects should be created with. */
	TWeakObjectPtr<UObject> Outer;
};
