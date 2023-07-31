// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConstraintsManager.h"
#include "Sections/MovieSceneParameterSection.h"
#include "Sections/MovieSceneConstrainedSection.h"
#include "UObject/ObjectMacros.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Sections/MovieSceneSubSection.h"
#include "ControlRig.h"
#include "MovieSceneSequencePlayer.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "MovieSceneObjectBindingID.h"
#include "Compilation/MovieSceneTemplateInterrogation.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "Channels/MovieSceneByteChannel.h"
#include "Channels/MovieSceneBoolChannel.h"
#include "Sequencer/MovieSceneControlRigSpaceChannel.h"
#include "ConstraintChannel.h"

#include "MovieSceneControlRigParameterSection.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

struct CONTROLRIG_API FControlRigBindingHelper
{
	static void BindToSequencerInstance(UControlRig* ControlRig);
	static void UnBindFromSequencerInstance(UControlRig* ControlRig);
};

struct FEnumParameterNameAndValue //uses uint8
{
	FEnumParameterNameAndValue(FName InParameterName, uint8 InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	FName ParameterName;

	uint8 Value;
};

struct FIntegerParameterNameAndValue
{
	FIntegerParameterNameAndValue(FName InParameterName, int32 InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	FName ParameterName;

	int32 Value;
};

USTRUCT()
struct CONTROLRIG_API FEnumParameterNameAndCurve
{
	GENERATED_USTRUCT_BODY()

	FEnumParameterNameAndCurve()
	{}

	FEnumParameterNameAndCurve(FName InParameterName);

	UPROPERTY()
	FName ParameterName;

	UPROPERTY()
	FMovieSceneByteChannel ParameterCurve;
};


USTRUCT()
struct CONTROLRIG_API FIntegerParameterNameAndCurve
{
	GENERATED_USTRUCT_BODY()

	FIntegerParameterNameAndCurve()
	{}
	FIntegerParameterNameAndCurve(FName InParameterName);

	UPROPERTY()
	FName ParameterName;

	UPROPERTY()
	FMovieSceneIntegerChannel ParameterCurve;
};

USTRUCT()
struct CONTROLRIG_API FSpaceControlNameAndChannel
{
	GENERATED_USTRUCT_BODY()

	FSpaceControlNameAndChannel(){}
	FSpaceControlNameAndChannel(FName InControlName) : ControlName(InControlName) {};

	UPROPERTY()
	FName ControlName;

	UPROPERTY()
	FMovieSceneControlRigSpaceChannel SpaceCurve;
};


/**
*  Data that's queried during an interrogation
*/
struct FFloatInterrogationData
{
	float Val;
	FName ParameterName;
};

struct FVector2DInterrogationData
{
	FVector2D Val;
	FName ParameterName;
};

struct FVectorInterrogationData
{
	FVector Val;
	FName ParameterName;
};

struct FEulerTransformInterrogationData
{
	FEulerTransform Val;
	FName ParameterName;
};

USTRUCT()
struct CONTROLRIG_API FChannelMapInfo
{
	GENERATED_USTRUCT_BODY()

	FChannelMapInfo() = default;

	FChannelMapInfo(int32 InControlIndex, int32 InTotalChannelIndex, int32 InChannelIndex, int32 InParentControlIndex = INDEX_NONE, FName InChannelTypeName = NAME_None,
		int32 InMaskIndex = INDEX_NONE, int32 InCategoryIndex = INDEX_NONE) :
		ControlIndex(InControlIndex),TotalChannelIndex(InTotalChannelIndex), ChannelIndex(InChannelIndex), ParentControlIndex(InParentControlIndex), ChannelTypeName(InChannelTypeName),MaskIndex(InMaskIndex),CategoryIndex(InCategoryIndex) {};
	UPROPERTY()
	int32 ControlIndex = 0; 
	UPROPERTY()
	int32 TotalChannelIndex = 0;
	UPROPERTY()
	int32 ChannelIndex = 0; //channel index for it's type.. (e.g  float, int, bool).
	UPROPERTY()
	int32 ParentControlIndex = 0;
	UPROPERTY()
	FName ChannelTypeName; 
	UPROPERTY()
	bool bDoesHaveSpace = false;
	UPROPERTY()
	int32 SpaceChannelIndex = -1; //if it has space what's the space channel index
	UPROPERTY()
	int32 MaskIndex = -1; //index for the mask
	UPROPERTY()
	int32 CategoryIndex = -1; //index for the Sequencer category node

	int32 GeneratedKeyIndex = -1; //temp index set by the ControlRigParameterTrack, not saved

	UPROPERTY()
	TArray<uint32> ConstraintsIndex; //constraints data
};


struct FMovieSceneControlRigSpaceChannel;

/**
 * Movie scene section that controls animation controller animation
 */
UCLASS()
class CONTROLRIG_API UMovieSceneControlRigParameterSection : public UMovieSceneParameterSection, public IMovieSceneConstrainedSection
{
	GENERATED_BODY()

public:

	/** Bindable events for when we add space or constraint channels. */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FSpaceChannelAddedEvent, UMovieSceneControlRigParameterSection*, const FName&, FMovieSceneControlRigSpaceChannel*);

	void AddEnumParameterKey(FName InParameterName, FFrameNumber InTime, uint8 InValue);
	void AddIntegerParameterKey(FName InParameterName, FFrameNumber InTime, int32 InValue);

	bool RemoveEnumParameter(FName InParameterName);
	bool RemoveIntegerParameter(FName InParameterName);

	TArray<FEnumParameterNameAndCurve>& GetEnumParameterNamesAndCurves();
	const TArray<FEnumParameterNameAndCurve>& GetEnumParameterNamesAndCurves() const;

	TArray<FIntegerParameterNameAndCurve>& GetIntegerParameterNamesAndCurves();
	const TArray<FIntegerParameterNameAndCurve>& GetIntegerParameterNamesAndCurves() const;

	void FixRotationWinding(FName ControlName, FFrameNumber StartFrame, FFrameNumber EndFrame);

	TArray<FSpaceControlNameAndChannel>& GetSpaceChannels();
	const TArray< FSpaceControlNameAndChannel>& GetSpaceChannels() const;
	FName FindControlNameFromSpaceChannel(const FMovieSceneControlRigSpaceChannel* SpaceChannel) const;
	
	FSpaceChannelAddedEvent& SpaceChannelAdded() { return OnSpaceChannelAdded; }

	const FName& FindControlNameFromConstraintChannel(const FMovieSceneConstraintChannel* InConstraintChannel) const;
		
	bool RenameParameterName(const FName& OldParameterName, const FName& NewParameterName);
private:

	FSpaceChannelAddedEvent OnSpaceChannelAdded;
	/** Control Rig that controls us*/
	UPROPERTY()
	TObjectPtr<UControlRig> ControlRig;

public:

	/** The class of control rig to instantiate */
	UPROPERTY(EditAnywhere, Category = "Animation")
	TSubclassOf<UControlRig> ControlRigClass;

	/** Mask for controls themselves*/
	UPROPERTY()
	TArray<bool> ControlsMask;

	/** Mask for Transform Mask*/
	UPROPERTY()
	FMovieSceneTransformMask TransformMask;

	/** The weight curve for this animation controller section */
	UPROPERTY()
	FMovieSceneFloatChannel Weight;

	/** Map from the control name to where it starts as a channel*/
	UPROPERTY()
	TMap<FName, FChannelMapInfo> ControlChannelMap;

protected:
	/** Enum Curves*/
	UPROPERTY()
	TArray<FEnumParameterNameAndCurve> EnumParameterNamesAndCurves;

	/*Integer Curves*/
	UPROPERTY()
	TArray<FIntegerParameterNameAndCurve> IntegerParameterNamesAndCurves;

	/** Space Channels*/
	UPROPERTY()
	TArray<FSpaceControlNameAndChannel>  SpaceChannels;

	/** Space Channels*/
	UPROPERTY()
	TArray<FConstraintAndActiveChannel> ConstraintsChannels;

public:

	UMovieSceneControlRigParameterSection();

	//UMovieSceneSection virtuals
	virtual void SetBlendType(EMovieSceneBlendType InBlendType) override;
	virtual UObject* GetImplicitObjectOwner() override;
	// IMovieSceneConstrainedSection overrides
	/*
	* Whether it has that channel
	*/
	virtual bool HasConstraintChannel(const FName& InConstraintName) const override;

	/*
	* Get constraint with that name
	*/
	virtual FConstraintAndActiveChannel* GetConstraintChannel(const FName& InConstraintName) override;

	/*
	*  Add Constraint channel
	*/
	virtual void AddConstraintChannel(UTickableConstraint* InConstraint) override;

	/*
	*  Remove Constraint channel
	*/
	virtual void RemoveConstraintChannel(const FName& InConstraintName) override;

	/*
	*  Get The channels
	*/
	virtual TArray<FConstraintAndActiveChannel>& GetConstraintsChannels()  override;

	/*
	*  Replace the constraint with the specified name with the new one
	*/
	virtual void ReplaceConstraint(const FName InName, UTickableConstraint* InConstraint)  override;

	/*
	*  What to do if the constraint object has been changed, for example by an undo or redo.
	*/
	virtual void OnConstraintsChanged() override;

	//not override but needed
	const TArray<FConstraintAndActiveChannel>& GetConstraintsChannels() const;

#if WITH_EDITOR
	//Function to save control rig key when recording.
	void RecordControlRigKey(FFrameNumber FrameNumber, bool bSetDefault, ERichCurveInterpMode InInterpMode);

	//Function to load an Anim Sequence into this section. It will automatically resize to the section size.
	//Will return false if fails or is canceled
	virtual bool LoadAnimSequenceIntoThisSection(UAnimSequence* Sequence, UMovieScene* MovieScene, UObject* BoundObject, bool bKeyReduce, float Tolerance, FFrameNumber InStartFrame = 0);
#endif
	const TArray<bool>& GetControlsMask() const
	{
		return ControlsMask;
	}

	bool GetControlsMask(int32 Index) const
	{
		if (Index >= 0 && Index < ControlsMask.Num())
		{
			return ControlsMask[Index];
		}
		return false;
	}

	void SetControlsMask(const TArray<bool>& InMask)
	{
		ControlsMask = InMask;
		ReconstructChannelProxy();
	}

	void SetControlsMask(int32 Index, bool Val)
	{
		if (Index >= 0 && Index < ControlsMask.Num())
		{
			ControlsMask[Index] = Val;
		}
		ReconstructChannelProxy();
	}

	void FillControlsMask(bool Val)
	{
		ControlsMask.Init(Val, ControlsMask.Num());
		ReconstructChannelProxy();
	}
	/**
	* This function returns the active category index of the control, based upon what controls are active/masked or not
	* If itself is masked it returns INDEX_NONE
	*/
	int32 GetActiveCategoryIndex(FName ControlName) const;
	/**
	* Access the transform mask that defines which channels this track should animate
	*/
	FMovieSceneTransformMask GetTransformMask() const
	{
		return TransformMask;
	}

	/**
	 * Set the transform mask that defines which channels this track should animate
	 */
	void SetTransformMask(FMovieSceneTransformMask NewMask)
	{
		TransformMask = NewMask;
		ReconstructChannelProxy();
	}

public:

	/** Recreate with this Control Rig*/
	void RecreateWithThisControlRig(UControlRig* InControlRig, bool bSetDefault);

	/* Set the control rig for this section */
	void SetControlRig(UControlRig* InControlRig);
	/* Get the control rig for this section */
	UControlRig* GetControlRig() const { return ControlRig; }

	/** Whether or not to key currently, maybe evaluating so don't*/
	void  SetDoNotKey(bool bIn) const { bDoNotKey = bIn; }
	/** Get Whether to key or not*/
	bool GetDoNotKey() const { return bDoNotKey; }

	/**  Whether or not this section has scalar*/
	bool HasScalarParameter(FName InParameterName) const;

	/**  Whether or not this section has bool*/
	bool HasBoolParameter(FName InParameterName) const;

	/**  Whether or not this section has enum*/
	bool HasEnumParameter(FName InParameterName) const;

	/**  Whether or not this section has int*/
	bool HasIntegerParameter(FName InParameterName) const;

	/**  Whether or not this section has scalar*/
	bool HasVector2DParameter(FName InParameterName) const;

	/**  Whether or not this section has scalar*/
	bool HasVectorParameter(FName InParameterName) const;

	/**  Whether or not this section has scalar*/
	bool HasColorParameter(FName InParameterName) const;

	/**  Whether or not this section has scalar*/
	bool HasTransformParameter(FName InParameterName) const;

	/**  Whether or not this section has space*/
	bool HasSpaceChannel(FName InParameterName) const;

	/** Get The Space Channel for the Control*/
	FSpaceControlNameAndChannel* GetSpaceChannel(FName InParameterName);

	/** Adds specified scalar parameter. */
	void AddScalarParameter(FName InParameterName,  TOptional<float> DefaultValue, bool bReconstructChannel);

	/** Adds specified bool parameter. */
	void AddBoolParameter(FName InParameterName, TOptional<bool> DefaultValue, bool bReconstructChannel);

	/** Adds specified enum parameter. */
	void AddEnumParameter(FName InParameterName, UEnum* Enum,TOptional<uint8> DefaultValue, bool bReconstructChannel);

	/** Adds specified int parameter. */
	void AddIntegerParameter(FName InParameterName, TOptional<int32> DefaultValue, bool bReconstructChannel);

	/** Adds a a key for a specific vector parameter. */
	void AddVectorParameter(FName InParameterName, TOptional<FVector> DefaultValue, bool bReconstructChannel);

	/** Adds a a key for a specific vector2D parameter. */
	void AddVector2DParameter(FName InParameterName, TOptional<FVector2D> DefaultValue, bool bReconstructChannel);

	/** Adds a a key for a specific color parameter. */
	void AddColorParameter(FName InParameterName, TOptional<FLinearColor> DefaultValue, bool bReconstructChannel);

	/** Adds a a key for a specific transform parameter*/
	void AddTransformParameter(FName InParameterName, TOptional<FEulerTransform> DefaultValue, bool bReconstructChannel);

	/** Add Space Parameter for a specified Control, no Default since that is Parent space*/
	void AddSpaceChannel(FName InControlName, bool bReconstructChannel);


	/** Clear Everything Out*/
	void ClearAllParameters();

	/** Evaluates specified scalar parameter. Will not get set if not found */
	TOptional<float> EvaluateScalarParameter(const  FFrameTime& InTime, FName InParameterName);

	/** Evaluates specified bool parameter. Will not get set if not found */
	TOptional<bool> EvaluateBoolParameter(const  FFrameTime& InTime, FName InParameterName);

	/** Evaluates specified enum parameter. Will not get set if not found */
	TOptional<uint8> EvaluateEnumParameter(const  FFrameTime& InTime, FName InParameterName);

	/** Evaluates specified int parameter. Will not get set if not found */
	TOptional<int32> EvaluateIntegerParameter(const  FFrameTime& InTime, FName InParameterName);

	/** Evaluates a a key for a specific vector parameter. Will not get set if not found */
	TOptional<FVector> EvaluateVectorParameter(const FFrameTime& InTime, FName InParameterName);

	/** Evaluates a a key for a specific vector2D parameter. Will not get set if not found */
	TOptional<FVector2D> EvaluateVector2DParameter(const  FFrameTime& InTime, FName InParameterName);

	/** Evaluates a a key for a specific color parameter. Will not get set if not found */
	TOptional<FLinearColor> EvaluateColorParameter(const  FFrameTime& InTime, FName InParameterName);

	/** Evaluates a a key for a specific transform parameter. Will not get set if not found */
	TOptional<FEulerTransform> EvaluateTransformParameter(const  FFrameTime& InTime, FName InParameterName);
	

	/** Evaluates a a key for a specific space parameter. Will not get set if not found */
	TOptional<FMovieSceneControlRigSpaceBaseKey> EvaluateSpaceChannel(const  FFrameTime& InTime, FName InParameterName);

	/** Key Zero Values on all or just selected controls in these section at the specified time */
	void KeyZeroValue(FFrameNumber InFrame, bool bSelected);

	/** Key the Weights to the specified value */
	void KeyWeightValue(FFrameNumber InFrame, float InVal);
;
	/** Remove All Keys, but maybe not space keys if bIncludeSpaceKeys is false */
	void RemoveAllKeys(bool bIncludeSpaceKeys);

	/** Whether or not create a space channel for a particular control */
	bool CanCreateSpaceChannel(FName InControlName) const;

public:
	/**
	* Access the interrogation key for control rig data 
	*/
	 static FMovieSceneInterrogationKey GetFloatInterrogationKey();
	 static FMovieSceneInterrogationKey GetVector2DInterrogationKey();
	 static FMovieSceneInterrogationKey GetVector4InterrogationKey();
	 static FMovieSceneInterrogationKey GetVectorInterrogationKey();
	 static FMovieSceneInterrogationKey GetTransformInterrogationKey();

	virtual void ReconstructChannelProxy() override;

protected:

	//~ UMovieSceneSection interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditImport() override;
	virtual void PostLoad() override;
	virtual float GetTotalWeightValue(FFrameTime InTime) const override;
	virtual void OnBindingIDsUpdated(const TMap<UE::MovieScene::FFixedObjectBindingID, UE::MovieScene::FFixedObjectBindingID>& OldFixedToNewFixedMap, FMovieSceneSequenceID LocalSequenceID, const FMovieSceneSequenceHierarchy* Hierarchy, IMovieScenePlayer& Player) override;
	virtual void GetReferencedBindings(TArray<FGuid>& OutBindings) override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;


	// When true we do not set a key on the section, since it will be set because we changed the value
	// We need this because control rig notifications are set on every change even when just changing sequencer time
	// which forces a sequencer eval, not like the editor where changes are only set on UI changes(changing time doesn't send change delegate)
	mutable bool bDoNotKey;

public:
	// Special list of Names that we should only Modify. Needed to handle Interaction (FK/IK) since Control Rig expecting only changed value to be set
	//not all Controls
	mutable TSet<FName> ControlsToSet;


public:
	//Test Controls really are new
	bool IsDifferentThanLastControlsUsedToReconstruct(const TArray<FRigControlElement*>& NewControls) const;

private:
	void StoreLastControlsUsedToReconstruct(const TArray<FRigControlElement*>& NewControls);
	//Last set of Controls used to reconstruct the channel proxies, used to make sure controls really changed if we want to reconstruct
	//only care to check name and type
	TArray<TPair<FName, ERigControlType>> LastControlsUsedToReconstruct;
};
