// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakeRecorderSource.h"
#include "TakeRecorderSourceProperty.h"
#include "UObject/SoftObjectPtr.h"
#include "TrackRecorders/IMovieSceneTrackRecorderHost.h"
#include "Serializers/MovieSceneActorSerialization.h"
#include "Serializers/MovieSceneManifestSerialization.h"
#include "MovieSceneSequenceID.h"
#include "TakeRecorderActorSource.generated.h"

class UActorComponent;
class UMovieSceneFolder;
class UMovieScene;
class UMovieSceneTrackRecorderSettings;

DECLARE_LOG_CATEGORY_EXTERN(ActorSerialization, Verbose, All);

UENUM(BlueprintType)
enum class ETakeRecorderActorRecordType : uint8
{
	Possessable,
	Spawnable,
	ProjectDefault
};

/**
* This Take Recorder Source can record an actor from the World's properties.
* Records the properties of the actor and the components on the actor and safely
* handles new components being spawned at runtime and the actor being destroyed.
*/
UCLASS(Category="Actors", meta = (TakeRecorderDisplayName="Any Actor"))
class UTakeRecorderActorSource : public UTakeRecorderSource, public IMovieSceneTrackRecorderHost
{
public:
	GENERATED_BODY()

	/** Reference to the actor in the world that should have it's properties recorded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName="Source Actor", Category="Actor Source")
	TSoftObjectPtr<AActor> Target;

	/**
	 * Should this actor be recorded as a Possessable in Sequencer? If so the resulting Object Binding	
	 * will not create a Spawnable copy of this object and instead will possess this object in the level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Source")
	ETakeRecorderActorRecordType RecordType;

	/** 
	 * Whether to ensure that the parent hierarchy is also recorded. If recording to possessable and the parent is not recorded, 
	 * the recorded transforms will be in local space since the child will still be attached to the parent in the level after 
	 * recording.  If recording to spawnable and the parent is not recorded, the recorded transforms will be in global space 
	 * since the child will not be attached to the parent in the level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Source")
	bool bRecordParentHierarchy;

	/** Whether to perform key-reduction algorithms as part of the recording */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Source")
	bool bReduceKeys;

	/**
	 * Lists the properties and components on the current actor and whether or not each property will be
	 * recorded into a track in the resulting Level Sequence. 
	 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, meta=(ShowInnerProperties), Category = "Actor Source")
	TObjectPtr<UActorRecorderPropertyMap> RecordedProperties;

	/** Include only the animation bones/curves that match this list */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Source")
	TArray<FString> IncludeAnimationNames;

	/** Exclude all animation bones/curves that match this list */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Source")
	TArray<FString> ExcludeAnimationNames;

	/** The level sequence that this source is being recorded into. Set during PreRecording, null after PostRecording. */
	UPROPERTY()
	TObjectPtr<ULevelSequence> TargetLevelSequence;

	/** The master or uppermost level sequence that this source is being recorded into. Set during PreRecording, null after PostRecording. */
	UPROPERTY()
	TObjectPtr<ULevelSequence> MasterLevelSequence;

	/**
	* Dynamically created list of settings objects for the different factories that are recording something 
	* on this actor. If a Factory has no properties it can record the settings objects will not get created.
	* Only one instance of this object exists for a factory and the factory recorder will be passed the shared 
	* instance.
	*/
	UPROPERTY()
	TArray<TObjectPtr<UObject>> FactorySettings;
	
	/**
	* An array of section recorders created during the recording process that are capturing data about the actor/components.
	* Will be an empty list when a recording is not in progress.
	*/
	UPROPERTY()
	TArray<TObjectPtr<class UMovieSceneTrackRecorder>> TrackRecorders;

	/** The parent actor source that generated this actor source (ie. through parenting or as an attached component). Null after PostRecording */
	UPROPERTY()
	TObjectPtr<UTakeRecorderActorSource> ParentSource;

	/** Show Dialog during the (possibly) slow parts of the take recording */
	UPROPERTY()
	bool bShowProgressDialog;

public:

	/*
	 * Add a take recorder source for the given actor. 
	 *
	 * @param InActor The actor to add a source for
	 * @param InSources The sources to add the actor to
	 * @return The added source or the source already present with the same actor
	 */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	static UTakeRecorderSource* AddSourceForActor(AActor* InActor, UTakeRecorderSources* InSources);

	/*
	 * Remove the given actor from TakeRecorderSources.
	 *
	 * @param InActor The actor to remove from the sources
	 * @param InSources The sources from where to remove the actor
	 */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	static void RemoveActorFromSources(AActor* InActor, UTakeRecorderSources* InSources);


public:
	UTakeRecorderActorSource(const FObjectInitializer& ObjInit);
	
	// UTakeRecorderSource Interface
	virtual bool IsValid()const override;
	virtual TArray<UTakeRecorderSource*> PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer) override;
	virtual void StartRecording(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame, class ULevelSequence* InSequence) override;
	virtual void TickRecording(const FQualifiedFrameTime& CurrentSequenceTime) override;
	virtual void StopRecording(class ULevelSequence* InSequence) override;
	virtual TArray<UTakeRecorderSource*> PostRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, const bool bCancelled) override;
	virtual void FinalizeRecording() override;
	virtual TArray<UObject*> GetAdditionalSettingsObjects() const { return TArray<UObject*>(FactorySettings); }
	virtual FString GetSubsceneTrackName(ULevelSequence* InSequence) const override;
	virtual FString GetSubsceneAssetName(ULevelSequence* InSequence) const override;
	virtual void AddContentsToFolder(class UMovieSceneFolder* InFolder) override;
	virtual FMovieSceneSequenceID GetSequenceID() const override { return TargetSequenceID; }

	// ~UTakeRecorderSource Interface

	/** Set the Target actor that we are going to record. Will reset the Recorded Property Map to defaults. */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder Actor Source")
	void SetSourceActor(TSoftObjectPtr<AActor> InTarget);

	UFUNCTION(BlueprintPure, Category = "Take Recorder Actor Source")
	TSoftObjectPtr<AActor> GetSourceActor() const { return Target; }

	/** Get the Guid of the Object Binding that this Actor Source created in the resulting Level Sequence. */
	FGuid GetObjectBindingGuid() const
	{
		return CachedObjectBindingGuid;
	}

	/** Get the record type. If set to project default, gets the type from the project settings */
	bool GetRecordToPossessable() const;

protected:

	/**
	* Request that we re-build the map of which properties to record. This should only be called when the target Actor is changed as it will
	* wipe out the users preference for which properties are going to get recorded and default them back to the Project Preferences for that
	* particular Actor/Component.
	*/
	void RebuildRecordedPropertyMap();
	void RebuildRecordedPropertyMapRecursive(const FFieldVariant& InObject, UActorRecorderPropertyMap* PropertyMap, const FString& OuterStructPath = FString());
	
	/**
	* Looks at the given component and determines what the parent of this component is. For the root component and Actor Components the
	* parent will be the root Property Map. For all other components, will attempt to find the Property Map which has a child Property Map
	* that references the given component.
	*
	* Used to find which property map a new property map should be added to while respecting the component hierarchy.
	*/
	UActorRecorderPropertyMap* GetParentPropertyMapForComponent(UActorComponent* InComponent);
	
	/** Looks through the given Property Map recursively to find a property map which references the given component. */
	UActorRecorderPropertyMap* GetPropertyMapForComponentRecursive(UActorComponent* InComponent, UActorRecorderPropertyMap* CurrentPropertyMap);

	/**
	* This is called when recording starts to generate the Section Recorders for the actor and all components that it currently has,
	* as well as again during runtime for any newly added components.
	*/
	void CreateSectionRecordersRecursive(UObject* ObjectToRecord, UActorRecorderPropertyMap* PropertyMap, TArray<UObject*>& TraversedObjects);

	/** Update our cached properties for what will be recorded. Done here so the UI doesn't have to iterate through map every frame. */
	void UpdateCachedNumberOfRecordedProperties();

	/** Returns the Guid of the Possessable in the specified sequence that represents the given actor, or an invalid Guid if the actor has no object binding in the sequence. */
	FGuid ResolveActorFromSequence(AActor* InActor, ULevelSequence* CurrentSequence) const;

	/** Remove the Possessable data for the given Guid from the sequence. Calls CleanExistingDataFromSequenceImpl afterwards for any other cleanup you may wish to do. */
	void CleanExistingDataFromSequence(const FGuid& ForGuid, ULevelSequence& InSequence);

	/** Called as part of PostRecording before Track Recorders are finalized. Calls PostProcessTrackRecordersImpl afterwards for any other post processing you wish to do before Track recorders are finalized. */
	void PostProcessTrackRecorders(ULevelSequence* InSequence);

	/** 
	* Ensure that the Object Template this recording is recording into has the specified component. Used to initialize dynamically added components that don't exist in the CDO.
	* @param InComponent - The component on the object that we want to check to see if it's a new component.
	* @param OutComponent - The newly duplicated component for the CDO (if any)
	* @return True if the given component was duplicated and OutComponent is valid.
	*/
	bool EnsureObjectTemplateHasComponent(UActorComponent* InComponent, UActorComponent*& OutComponent);

	/** 
	* Gets all components (both Scene and Actor) on the recorded Actor.
	* @param OutArray - Set of components to add found components to.
	* @param bUpdateReferencedActorList - If true will add any external Actors referenced to the NewReferencedActors list, and possibly print warnings.
	*/
	void GetAllComponents(TSet<UActorComponent*>& OutArray, bool bUpdateReferencedActorList = false);

	/**
	* Gets all Scene components that are a child of the specified component. Use the Root Component if you want all
	* child components on an actor.
	* @param OnSceneComponent - The component who's children we look at.
	* @param OutArray - Set of components to add found components to.
	* @param bUpdateReferencedActorList - If true will add any external Actors referenced to the NewReferencedActors list, and possibly print warnings.
	*/
	void GetSceneComponents(USceneComponent* OnSceneComponent, TSet<UActorComponent*>& OutArray, bool bUpdateReferencedActorList = false);

	/**
	* Returns the direct children of the specified scene component. Filters for ownership before returning.
	*/
	void GetChildSceneComponents(USceneComponent* OnSceneComponent, TSet<UActorComponent*>& OutArray, bool bUpdateReferencedActorList = false);

	/**
	* Gets all Actor components on the recorded actor, not including Scene components.
	*/
	void GetActorComponents(AActor* OnActor, TSet<UActorComponent*>& OutArray) const;

	/**
	* Iterates through the NewReferencedActors set and creates a new UTakeRecorderActorSource for each one. Only creates
	* a new source if that actor is not already being recorded by another Actor Source, will not create an Actor source 
	* for the actor we currently target.
	*/
	void CreateNewActorSourceForReferencedActors();
	
	/**
	* Walks the hierarchy of the given Target actor to ensure all parents have been added to the NewReferencedActors set.
	* This allows us to ensure that we can record transforms in local space as all parents will be recorded and we can rebuild
	* the hierarchy through Attach Tracks in Sequencer.
	*/
	void EnsureParentHierarchyIsReferenced();

	// IMovieSceneTrackRecorderHost Interface
	/** Returns true if the other actor is also being recorded by the owning UTakeRecorderSources. Useful for checking if we're recording something we own is attached to. */
	bool IsOtherActorBeingRecorded(AActor* OtherActor) const override;
	/** Returns a valid guid if the other actor is also being recorded by the owning UTakeRecorderSources. Useful for knowing the guid of that Actor without knowing if it's a Possessable or a Spawnable. */
	FGuid GetRecordedActorGuid(class AActor* OtherActor) const override;
	/** Returns id of the active level sequence for the given Actor*/
	FMovieSceneSequenceID GetLevelSequenceID(class AActor* OtherActor) override;
	/** Returns generic track recorder settings */
	FTrackRecorderSettings GetTrackRecorderSettings() const override;
	/** Returns offset that may get set when recording a skeletal mesh animation. Needed to correctly transform attached children*/
	FTransform GetRecordedActorAnimationInitialRootTransform(class AActor* OtherActor) const override;
	/** Returns the master level sequence being recorded into */
	ULevelSequence* GetMasterLevelSequence() const override { return MasterLevelSequence; };
	// ~IMovieSceneTrackRecorderHost Interface
	
	/** Initializes an instance of the specified class if we don't already have it in our Settings array. */
	void InitializeFactorySettingsObject(TSubclassOf<UMovieSceneTrackRecorderSettings> InClass);
	/** Gets the already initialized instance of the specified class from the Settings array. */
	UMovieSceneTrackRecorderSettings* GetSettingsObjectForFactory(TSubclassOf<UMovieSceneTrackRecorderSettings> InClass) const;

	/** This is called after a Spawnable object template is created. Use this to modify any settings on the template object that need to be changed (ie: disabling auto-possession of pawns). */
	virtual void PostProcessCreatedObjectTemplateImpl(AActor* ObjectTemplate);
	virtual void CleanExistingDataFromSequenceImpl(const FGuid& ForGuid, ULevelSequence& InSequence) {}
	virtual void PostProcessTrackRecordersImpl() {}
	
	
public:
	// UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditUndo() override;
	// ~UObject Interface
private:

	virtual const FSlateBrush* GetDisplayIconImpl() const override;
	virtual FText GetCategoryTextImpl() const;
	virtual FText GetDisplayTextImpl() const override;
	virtual FText GetDescriptionTextImpl() const override;

	void ProcessRecordedTimes(ULevelSequence* InSequence);

private:
	/** Object Binding guid that is created in the Level Sequence when recording starts.*/
	FGuid CachedObjectBindingGuid;

	/** Array of actors that have some sort of referenced link to our actor (such as an object attached to our hierarchy) that need Actor Source recorders initialized for them. List is emptied after sources are created. */
	TSet<class AActor*> NewReferencedActors;
	/** Array of Actor Sources that we ended up creating that we need to clean up when we stop recording. */
	TArray<class UTakeRecorderSource*> AddedActorSources;

	/** 
	* A pointer to the Object Template instance that was created inside the Target Level Sequence at the start of recording.
	* Can be null (if recording to a possessable) or when we are not recording. 
	*/
	TWeakObjectPtr<class AActor> CachedObjectTemplate;
	/**
	* A set of components that we're currently recording. We compare the components from one frame to the next to see if any
	* components have been added or removed so we can appropriately update their Spawn tracked. Empty unless a recording is
	* in progress (but may still be empty if no components)
	*/
	TSet<class UActorComponent*> CachedComponentList;

	/**
	* ID of the TargetLevelSequence.
	*/
	FMovieSceneSequenceID TargetSequenceID;

	/**
	*  Serializer
	*/
	FActorSerializer ActorSerializer;

};
