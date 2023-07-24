// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TransformableHandle.h"
#include "Rigs/RigHierarchyDefines.h"
#include "ControlRig.h"

#include "ControlRigTransformableHandle.generated.h"

struct FRigBaseElement;
struct FRigControlElement;

class UControlRig;
class USkeletalMeshComponent;
class URigHierarchy;

/**
 * UTransformableControlHandle
 */

UCLASS(Blueprintable)
class CONTROLRIG_API UTransformableControlHandle : public UTransformableHandle 
{
	GENERATED_BODY()
	
public:
	
	virtual ~UTransformableControlHandle();

	virtual void PostLoad() override;
	
	/** Sanity check to ensure that ControlRig and ControlName are safe to use. */
	virtual bool IsValid() const override;

	/** Sets the global transform of the control. */
	virtual void SetGlobalTransform(const FTransform& InGlobal) const override;
	/** Sets the local transform of the control. */
	virtual void SetLocalTransform(const FTransform& InLocal) const override;
	/** Gets the global transform of the control. */
	virtual FTransform GetGlobalTransform() const  override;
	/** Sets the local transform of the control. */
	virtual FTransform GetLocalTransform() const  override;

	/** Returns the target object containing the tick function (e.i. SkeletalComponent bound to ControlRig). */
	virtual UObject* GetPrerequisiteObject() const override;
	/** Returns ths SkeletalComponent tick function. */
	virtual FTickFunction* GetTickFunction() const override;

	/** Generates a hash value based on ControlRig and ControlName. */
	static uint32 ComputeHash(const UControlRig* InControlRig, const FName& InControlName);
	virtual uint32 GetHash() const override;
	
	/** Returns the underlying targeted object. */
	virtual TWeakObjectPtr<UObject> GetTarget() const override;

	/** Get the array of float channels for the specified section*/
	virtual TArrayView<FMovieSceneFloatChannel*>  GetFloatChannels(const UMovieSceneSection* InSection) const override;
	/** Get the array of double channels for the specified section*/
	virtual TArrayView<FMovieSceneDoubleChannel*>  GetDoubleChannels(const UMovieSceneSection* InSection) const override;
	virtual bool AddTransformKeys(const TArray<FFrameNumber>& InFrames,
		const TArray<FTransform>& InTransforms,
		const EMovieSceneTransformChannel& InChannels,
		const FFrameRate& InTickResolution,
		UMovieSceneSection* InSection,
		const bool bLocal = true) const override;

	/** Resolve the bound objects so that any object it references are resolved and correctly set up*/
	virtual void ResolveBoundObjects(FMovieSceneSequenceID LocalSequenceID, IMovieScenePlayer& Player, UObject* SubObject = nullptr) override;

	/** Make a duplicate of myself with this outer*/
	virtual UTransformableHandle* Duplicate(UObject* NewOuter) const override;

	/** tick things when baking*/ 
	virtual void TickForBaking() override;

	/** Returns the skeletal mesh bound to ControlRig. */
	USkeletalMeshComponent* GetSkeletalMesh() const;

	/** Registers/Unregisters useful delegates to track changes in the control's transform. */
	void UnregisterDelegates() const;
	void RegisterDelegates();

	/** Check for direct dependencies (ie hierarchy + skeletal mesh) with InOther. */
	virtual bool HasDirectDependencyWith(const UTransformableHandle& InOther) const override;

	/** Look for a possible tick function that can be used as a prerequisite. */
	virtual FTickPrerequisite GetPrimaryPrerequisite() const override;

#if WITH_EDITOR
	/** Returns labels used for UI. */
	virtual FString GetLabel() const override;
	virtual FString GetFullLabel() const override;
#endif

	/** The ControlRig that this handle is pointing at. */
	UPROPERTY(BlueprintReadOnly, Category = "Object")
	TSoftObjectPtr<UControlRig> ControlRig;

	/** The ControlName of the control that this handle is pointing at. */
	UPROPERTY(BlueprintReadOnly, Category = "Object")
	FName ControlName;

	/** @todo document */
	void OnControlModified(
		UControlRig* InControlRig,
		FRigControlElement* InControl,
		const FRigControlModifiedContext& InContext);
	
private:

	/** @todo document */
	void OnHierarchyModified(
		ERigHierarchyNotification InNotif,
		URigHierarchy* InHierarchy,
		const FRigBaseElement* InElement);

	void OnControlRigBound(UControlRig* InControlRig);
	void OnObjectBoundToControlRig(UObject* InObject);

#if WITH_EDITOR
	/** @todo document */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& InOldToNewInstances);
#endif

	/** @todo document */
	FRigControlElement* GetControlElement() const;
};