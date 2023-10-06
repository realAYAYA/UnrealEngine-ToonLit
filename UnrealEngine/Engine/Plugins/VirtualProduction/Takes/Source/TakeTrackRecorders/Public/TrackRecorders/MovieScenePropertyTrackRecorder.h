// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/LazyObjectPtr.h"
#include "IMovieSceneTrackRecorderFactory.h"
#include "MovieSceneTrackRecorder.h"
#include "MovieSceneTrackPropertyRecorder.h"
#include "Serializers/MovieScenePropertySerialization.h"
#include "MovieScenePropertyTrackRecorder.generated.h"

// Forward Declare
class UObject;

class TAKETRACKRECORDERS_API FMovieScenePropertyTrackRecorderFactory : public IMovieSceneTrackRecorderFactory
{
public:
	virtual ~FMovieScenePropertyTrackRecorderFactory() {}

	// Property Track only records individual UProperties on an object.
	virtual bool CanRecordObject(UObject* InObjectToRecord) const override { return false; }
	virtual UMovieSceneTrackRecorder* CreateTrackRecorderForObject() const override { return nullptr; }

	virtual bool CanRecordProperty(UObject* InObjectToRecord, class FProperty* InPropertyToRecord) const override;
	virtual class UMovieSceneTrackRecorder* CreateTrackRecorderForProperty(UObject* InObjectToRecord, const FName& InPropertyToRecord) const override;
	

	virtual FText GetDisplayName() const override { return NSLOCTEXT("MovieScenePropertyTrackTrackRecorderFactory", "DisplayName", "Property Track"); }

	virtual bool IsSerializable() const override { return true; }
	virtual FName GetSerializedType() const override { return FName("Property"); }

	UMovieSceneTrackRecorder* CreateTrackRecorderForPropertyEnum(ESerializedPropertyType ePropertyType, const FName& InPropertyToRecord) const;

};

UCLASS(BlueprintType)
class TAKETRACKRECORDERS_API UMovieScenePropertyTrackRecorder : public UMovieSceneTrackRecorder
{
	GENERATED_BODY()
protected:
	// UMovieSceneTrackRecorder Interface
	virtual void CreateTrackImpl() override;
	virtual void SetSectionStartTimecodeImpl(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame) override;
	virtual void FinalizeTrackImpl() override;
	virtual void RecordSampleImpl(const FQualifiedFrameTime& CurrentTime) override;
	// ~UMovieSceneTrackRecorder Interface

	virtual void SetSavedRecordingDirectory(const FString& InDirectory) override
	{
		Directory = InDirectory;
	}
	virtual bool LoadRecordedFile(const FString& InFileName, UMovieScene *InMovieScene, TMap<FGuid, AActor*>& ActorGuidToActorMap, TFunction<void()> InCompletionCallback) override;


public:
	/** Name of the specific property that we want to record. */
	FName PropertyToRecord;

	/** The property recorder for the specific property that we are recording. */
	TSharedPtr<class IMovieSceneTrackPropertyRecorder> PropertyRecorder;

	/** Cached Directory Name for serialization, used later when we create the PropertyRecorder*/
	FString Directory;
};
