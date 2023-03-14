// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Engine/EngineBaseTypes.h"
#include "MovieSceneObjectBindingID.h"
#include "TransformableHandle.generated.h"

struct FTickFunction;
class USceneComponent;
struct FMovieSceneFloatChannel;
struct FMovieSceneDoubleChannel;
class UMovieSceneSection;
struct FFRameNumber;
struct FFrameRate;
enum class EMovieSceneTransformChannel : uint32;
class IMovieScenePlayer;
namespace UE { namespace MovieScene {struct FFixedObjectBindingID;} }
struct FMovieSceneSequenceHierarchy;
struct FMovieSceneSequenceID;

UENUM()
enum class EHandleEvent : uint8
{
	LocalTransformUpdated,
	GlobalTransformUpdated,
	ComponentUpdated,

	/** MAX - invalid */
	Max UMETA(Hidden),
};

/**
 * UTransformableHandle
 */

UCLASS(Abstract, Blueprintable)
class CONSTRAINTS_API UTransformableHandle : public UObject 
{
	GENERATED_BODY()
	
public:

	DECLARE_EVENT_TwoParams(UTransformableHandle, FHandleModifiedEvent, UTransformableHandle*, EHandleEvent);
	
	virtual ~UTransformableHandle();

	virtual void PostLoad() override;
	
	/** Sanity check to ensure the handle is safe to use. */
	virtual bool IsValid() const PURE_VIRTUAL(IsValid, return false;);
	
	/** Sets the global transform of the underlying transformable object. */
	virtual void SetGlobalTransform(const FTransform& InGlobal) const PURE_VIRTUAL(SetGlobalTransform, );
	/** Sets the local transform of the underlying transformable object in it's parent space. */
	virtual void SetLocalTransform(const FTransform& InLocal) const PURE_VIRTUAL(SetLocalTransform, );
	/** Gets the global transform of the underlying transformable object. */
	virtual FTransform GetGlobalTransform() const PURE_VIRTUAL(GetGlobalTransform, return FTransform::Identity;);
	/** Gets the local transform of the underlying transformable object in it's parent space. */
	virtual FTransform GetLocalTransform() const PURE_VIRTUAL(GetLocalTransform, return FTransform::Identity;);

	/** If true it contains objects bound to an external system, like sequencer so we don't do certain things, like remove constraints when they don't resolve*/
	virtual bool HasBoundObjects() const;

	/** Resolve the bound objects so that any object it references are resovled and correctly set up*/
	virtual void ResolveBoundObjects(FMovieSceneSequenceID LocalSequenceID, IMovieScenePlayer& Player, UObject* SubObject = nullptr) PURE_VIRTUAL(ResolveBoundObjects);

	/** Make a duplicate of myself with this outer*/
	virtual UTransformableHandle* Duplicate(UObject* NewOuter) const PURE_VIRTUAL(Duplicate, return nullptr;);


	/** Fix up Binding in case it has changed*/
	void OnBindingIDsUpdated(const TMap<UE::MovieScene::FFixedObjectBindingID, UE::MovieScene::FFixedObjectBindingID>& OldFixedToNewFixedMap, FMovieSceneSequenceID LocalSequenceID, const FMovieSceneSequenceHierarchy* Hierarchy, IMovieScenePlayer& Player);
	
	/** Perform any special ticking needed for this handle, by default it does nothing, todo need to see if we need to tick control rig also*/
	virtual void TickForBaking() {};
	/** Get the array of float channels for the specified section*/
	virtual TArrayView<FMovieSceneFloatChannel*>  GetFloatChannels(const UMovieSceneSection* InSection) const PURE_VIRTUAL(GetFloatChannels, return TArrayView<FMovieSceneFloatChannel*>(); );
	/** Get the array of double channels for the specified section*/
	virtual TArrayView<FMovieSceneDoubleChannel*>  GetDoubleChannels(const UMovieSceneSection * InSection) const PURE_VIRTUAL(GetDoubleChannels, return TArrayView<FMovieSceneDoubleChannel*>(); );
	/** Add Transform Keys at the specified times*/
	virtual bool AddTransformKeys(const TArray<FFrameNumber>& InFrames,
		const TArray<FTransform>& InTransforms,
		const EMovieSceneTransformChannel& InChannels,
		const FFrameRate& InTickResolution,
		UMovieSceneSection* InSection,
		const bool bLocal = true) const PURE_VIRTUAL(AddTransformKeys, return false;);

	/**
	 * Returns the target object containing the tick function (returned in GetTickFunction).
	 * See FTickFunction::AddPrerequisite for details.
	 **/
	virtual UObject* GetPrerequisiteObject() const PURE_VIRTUAL(GetPrerequisiteObject, return nullptr;);
	/**
	 * Returns the tick function of the underlying transformable object.
	 * This is used to set dependencies with the constraint.
	**/
	virtual FTickFunction* GetTickFunction() const PURE_VIRTUAL(GetTickFunction, return nullptr;);

	/** Generates a hash value of the underlying transformable object. */
	virtual uint32 GetHash() const PURE_VIRTUAL(GetHash, return 0;);

	/** Returns the underlying targeted object. */
	virtual TWeakObjectPtr<UObject> GetTarget() const PURE_VIRTUAL(GetTarget, return nullptr;);

	/** Check for direct dependencies with InOther. */
	virtual bool HasDirectDependencyWith(const UTransformableHandle& InOther) const PURE_VIRTUAL(HasDirectDependencyWith, return false;);

	/** Look for a possible tick function that can be used as a prerequisite. */
	virtual FTickPrerequisite GetPrimaryPrerequisite() const PURE_VIRTUAL(GetPrimaryPrerequisite, return FTickPrerequisite(););
	
	FHandleModifiedEvent& HandleModified();

#if WITH_EDITOR
	virtual FString GetLabel() const PURE_VIRTUAL(GetLabel, return FString(););
	virtual FString GetFullLabel() const PURE_VIRTUAL(GetFullLabel, return FString(););
#endif

	//possible bindingID
	UPROPERTY(EditAnywhere, Category = "Binding")
	FMovieSceneObjectBindingID ConstraintBindingID;
protected:
	FHandleModifiedEvent OnHandleModified;
};

/**
 * UTransformableComponentHandle
 */

UCLASS(Blueprintable)
class CONSTRAINTS_API UTransformableComponentHandle : public UTransformableHandle 
{
	GENERATED_BODY()
	
public:
	
	virtual ~UTransformableComponentHandle();

	virtual void PostLoad() override;
	
	/** Sanity check to ensure that Component. */
	virtual bool IsValid() const override;
	
	/** Sets the global transform of Component. */
	virtual void SetGlobalTransform(const FTransform& InGlobal) const override;
	/** Sets the local transform of Component in it's attachment. */
	virtual void SetLocalTransform(const FTransform& InLocal) const override;
	/** Gets the global transform of Component. */
	virtual FTransform GetGlobalTransform() const override;
	/** Gets the local transform of Component in it's attachment. */
	virtual FTransform GetLocalTransform() const override;
	/** Tick the component*/
	virtual void TickForBaking() override;
	/** Returns the target object containing the tick function (e.i. Component). */
	virtual UObject* GetPrerequisiteObject() const override;
	/** Returns Component's tick function. */
	virtual FTickFunction* GetTickFunction() const override;

	/** Generates a hash value of Component. */
	virtual uint32 GetHash() const override;

	/** Returns the underlying targeted object. */
	virtual TWeakObjectPtr<UObject> GetTarget() const override;

	/** Check for direct dependencies (ie hierarchy) with InOther. */
	virtual bool HasDirectDependencyWith(const UTransformableHandle& InOther) const override;

	/** Look for a possible tick function that can be used as a prerequisite. */
	virtual FTickPrerequisite GetPrimaryPrerequisite() const override;

	/** Get the array of float channels for the specified section*/
	virtual TArrayView<FMovieSceneFloatChannel*>  GetFloatChannels(const UMovieSceneSection* InSection) const override;
	/** Get the array of double channels for the specified section*/
	virtual TArrayView<FMovieSceneDoubleChannel*>  GetDoubleChannels(const UMovieSceneSection* InSection) const override;
	/** Add Transform Keys at the specified times*/
	virtual bool AddTransformKeys(const TArray<FFrameNumber>& InFrames,
		const TArray<FTransform>& InTransforms,
		const EMovieSceneTransformChannel& InChannels,
		const FFrameRate& InTickResolution,
		UMovieSceneSection* InSection,
		const bool bLocal = true) const override;

	/** Resolve the bound objects so that any object it references are resovled and correctly set up*/
	virtual void ResolveBoundObjects(FMovieSceneSequenceID LocalSequenceID, IMovieScenePlayer& Player, UObject* SubObject = nullptr) override;

	/** Make a duplicate of myself with this outer*/
	virtual UTransformableHandle* Duplicate(UObject* NewOuter) const override;

#if WITH_EDITOR
	/** Returns labels used for UI. */
	virtual FString GetLabel() const override;
	virtual FString GetFullLabel() const override;
#endif
	
	/** The Component that this handle is pointing at. */
	UPROPERTY(BlueprintReadOnly, Category = "Object")
	TWeakObjectPtr<USceneComponent> Component;

	/** Optional socket name on Component. */
	UPROPERTY(BlueprintReadOnly, Category = "Object")
	FName SocketName = NAME_None;

	/** Registers/Unregisters useful delegates to track changes in the Component's transform. */
	void UnregisterDelegates() const;
	void RegisterDelegates();
	
	/** @todo document */
	void OnActorMoving(AActor* InActor);
	void OnPostPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent);
};