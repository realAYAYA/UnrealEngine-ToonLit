// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "TakeRecorderSources.h"
#include "TakeRecorderSourceProperty.generated.h"

USTRUCT(BlueprintType)
struct TAKESCORE_API FActorRecordedProperty
{
	GENERATED_BODY()

	FActorRecordedProperty()
		: PropertyName(NAME_None)
		, bEnabled(false)
		, RecorderName()
	{
	}

	FActorRecordedProperty(const FName& InName, const bool bInEnabled, const FText& InRecorderName)
	{
		PropertyName = InName;
		bEnabled = bInEnabled;
		RecorderName = InRecorderName;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Property")
	FName PropertyName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Property")
	bool bEnabled;

	UPROPERTY(VisibleAnywhere, Category = "Property")
	FText RecorderName;
};

/**
* This represents a list of all possible properties and components on an actor
* which can be recorded by the Actor Recorder and whether or not the user wishes
* to record them. If you wish to expose a property to be recorded it needs to be marked
* as "Interp" (C++) or "Expose to Cinematics" in Blueprints.
*/
UCLASS(BlueprintType)
class TAKESCORE_API UActorRecorderPropertyMap : public UObject
{
	GENERATED_BODY()

public:
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	UPROPERTY(VisibleAnywhere, Category = "Property")
	TSoftObjectPtr<UObject> RecordedObject;

	/* Represents properties exposed to Cinematics that can possibly be recorded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	TArray<FActorRecordedProperty> Properties;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, meta=(ShowInnerProperties, EditFixedOrder), Category = "Property")
	TArray<TObjectPtr<UActorRecorderPropertyMap>> Children;

public:
	struct Cache
	{
		Cache& operator+=(const Cache& InOther)
		{
			Properties += InOther.Properties;
			Components += InOther.Components;
			return *this;
		}
		/** The number of properties (both on actor + components) that we are recording. This includes child maps.*/
		int32 Properties = 0;
		/** The number of components that belong to the target actor that we are recording. This include child maps.*/
		int32 Components = 0;
	};

	/** Return the number of properties and components participating in recording.*/
	Cache CachedPropertyComponentCount() const
	{
		return RecordingInfo;
	}

	/** Visit all properties recursively and cache the record state. */
	void UpdateCachedValues();
private:
	int32 NumberOfPropertiesRecordedOnThis() const;
	int32 NumberOfComponentsRecordedOnThis() const;

	/** This is called by the children to let the parent know that the number of recorded properties has changed*/
	void ChildChanged();

	TWeakObjectPtr<UActorRecorderPropertyMap> Parent;

	Cache RecordingInfo;
};
