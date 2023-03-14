// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimDataNotifications.h"
#include "Animation/AttributeCurve.h"

#include "AnimDataModel.generated.h"

/**
 * Structure encapsulating a single bone animation track.
 */
USTRUCT(BlueprintType)
struct ENGINE_API FBoneAnimationTrack
{
	GENERATED_BODY()

	/** Internally stored data representing the animation bone data */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Model")
	FRawAnimSequenceTrack InternalTrackData;

	/** Index corresponding to the bone this track corresponds to within the target USkeleton */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Model")
	int32 BoneTreeIndex = INDEX_NONE;

	/** Name of the bone this track corresponds to */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Model")
	FName Name;
};

/**
 * Structure encapsulating animated curve data. Currently only contains Float and Transform curves.
 */
USTRUCT(BlueprintType)
struct ENGINE_API FAnimationCurveData
{
	GENERATED_BODY()

	/** Float-based animation curves */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Model")
	TArray<FFloatCurve>	FloatCurves;

	/** FTransform-based animation curves, used for animation layer editing */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Model")
	TArray<FTransformCurve>	TransformCurves;
};

/**
 * Structure encapsulating animated (bone) attribute data.
 */
USTRUCT(BlueprintType)
struct ENGINE_API FAnimatedBoneAttribute
{
	GENERATED_BODY()

	/** Identifier to reference this attribute by */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Model")
	FAnimationAttributeIdentifier Identifier;	

	/** Curve containing the (animated) attribute data */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Model")
	FAttributeCurve Curve;
};

struct FTrackToSkeletonMap;
struct FAnimationCurveIdentifier;

/**
 * The Model represents the source data for animations. It contains both bone animation data as well as animated curves.
 * They are currently only a sub-object of a AnimSequenceBase instance. The instance derives all runtime data from the source data. 
 */
UCLASS(BlueprintType, meta=(DebugTreeLeaf))
class ENGINE_API UAnimDataModel : public UObject
{
	GENERATED_BODY()
public:

	/** Begin UAnimDataModel overrides */
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual bool IsEditorOnly() const override { return true; }
	/** End UAnimDataModel overrides */

	/**
	* @return	Total length of play-able animation data 
	*/
	UFUNCTION(BlueprintPure, Category=AnimationDataModel)
	float GetPlayLength() const;
	
	/**
	* @return	Total number of frames of animation data stored 
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	int32 GetNumberOfFrames() const;

	/**
	* @return	Total number of animation data keys stored 
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	int32 GetNumberOfKeys() const;

	/**
	* @return	Frame rate at which the animation data is key-ed 
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	const FFrameRate& GetFrameRate() const;
	
	/**
	* @return	Array containg all bone animation tracks 
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const;
	
	/**
	* @return	Bone animation track for the provided index
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	const FBoneAnimationTrack& GetBoneTrackByIndex(int32 TrackIndex) const;

	/**
	* @return	Bone animation track for the provided (bone) name
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	const FBoneAnimationTrack& GetBoneTrackByName(FName TrackName) const;
	
	/**
	* @return	Bone animation track for the provided (bone) name if found, otherwise returns a nullptr 
	*/
	const FBoneAnimationTrack* FindBoneTrackByName(FName Name) const;

	/**
	* @return	Bone animation track for the provided index if valid, otherwise returns a nullptr 
	*/
	const FBoneAnimationTrack* FindBoneTrackByIndex(int32 BoneIndex) const;

	/**
	* @return	Internal track index for the provided bone animation track if found, otherwise returns INDEX_NONE 
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	int32 GetBoneTrackIndex(const FBoneAnimationTrack& Track) const;

	/**
	* @return	Internal track index for the provided (bone) name if found, otherwise returns INDEX_NONE 
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	int32 GetBoneTrackIndexByName(FName TrackName) const;

	/**
	* @return	Whether or not the provided track index is valid 
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	bool IsValidBoneTrackIndex(int32 TrackIndex) const;

	/**
	* @return	Total number of bone animation tracks
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	const int32 GetNumBoneTracks() const;

	/**
	* Populates the provided array with all contained (bone) track names
	*
	* @param	OutNames	[out] Array containing all bone track names
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	void GetBoneTrackNames(TArray<FName>& OutNames) const;

	/** Returns all contained curve animation data */
	const FAnimationCurveData& GetCurveData() const;

	/**
	* @return	Total number of stored FTransform curves
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	int32 GetNumberOfTransformCurves() const;

	/**
	* @return	Total number of stored float curves
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	int32 GetNumberOfFloatCurves() const;

	/**
	* @return	Array containing all stored float curves 
	*/
	const TArray<struct FFloatCurve>& GetFloatCurves() const;

	/**
	* @return	Array containing all stored FTransform curves 
	*/
	const TArray<struct FTransformCurve>& GetTransformCurves() const;

	/**
	* @return	Curve ptr for the provided identifier if valid, otherwise returns a nullptr 
	*/
	const FAnimCurveBase* FindCurve(const FAnimationCurveIdentifier& CurveIdentifier) const;

	/**
	* @return	Float Curve ptr for the provided identifier if valid, otherwise returns a nullptr
	*/
	const FFloatCurve* FindFloatCurve(const FAnimationCurveIdentifier& CurveIdentifier) const;

	/**
	* @return	Transform Curve ptr for the provided identifier if valid, otherwise returns a nullptr
	*/
	const FTransformCurve* FindTransformCurve(const FAnimationCurveIdentifier& CurveIdentifier) const;

	/**
	* @return	Rich curve ptr for the provided identifier if valid, otherwise returns a nullptr
	*/
	const FRichCurve* FindRichCurve(const FAnimationCurveIdentifier& CurveIdentifier) const;

	/**
	* @return	Curve object for the provided identifier if valid
	*/
	const FAnimCurveBase& GetCurve(const FAnimationCurveIdentifier& CurveIdentifier) const;

	/**
	* @return	Float Curve object for the provided identifier if valid
	*/
	const FFloatCurve& GetFloatCurve(const FAnimationCurveIdentifier& CurveIdentifier) const;

	/**
	* @return	Transform Curve object for the provided identifier if valid
	*/
	const FTransformCurve& GetTransformCurve(const FAnimationCurveIdentifier& CurveIdentifier) const;

	/**
	* @return	Rich Curve object for the provided identifier if valid
	*/
	const FRichCurve& GetRichCurve(const FAnimationCurveIdentifier& CurveIdentifier) const;
		
	/**
	* @return	Animated (bone) attributes stored
	*/
	TArrayView<const FAnimatedBoneAttribute> GetAttributes() const;

	/**
	* @return	Number of animated (bone) attributes stored
	*/
	int32 GetNumberOfAttributes() const;

	/**
	* @return	Number of animated (bone) attributes stored for the specified bone index
	*/
	int32 GetNumberOfAttributesForBoneIndex(const int32 BoneIndex) const;

	/**
	* @return	All animated (bone) attributes stored for the specified bone name
	*/
	void GetAttributesForBone(const FName& BoneName, TArray<const FAnimatedBoneAttribute*>& OutBoneAttributes) const;

	/**
	* @return	Animated (bone) attribute object for the provided identifier if valid
	*/
	const FAnimatedBoneAttribute& GetAttribute(const FAnimationAttributeIdentifier& AttributeIdentifier) const;

	/**
	* @return	Animated (bone) attribute ptr for the provided identifier if valid, otherwise returns a nullptr 
	*/
	const FAnimatedBoneAttribute* FindAttribute(const FAnimationAttributeIdentifier& AttributeIdentifier) const;
		
	/**
	* @return	The outer UAnimSequence object if found, otherwise returns a nullptr 
	*/
	UFUNCTION(BlueprintPure, Category = AnimationDataModel)
	UAnimSequence* GetAnimationSequence() const;

	/**
	* @return	Multicast delegate which is broadcasted to propagated changes to any internal data, see FAnimDataModelModifiedEvent and EAnimDataModelNotifyType
	*/
	FAnimDataModelModifiedEvent& GetModifiedEvent() { return ModifiedEvent; }

	/**
	* @return	GUID representing the contained data and state 
	*/
	FGuid GenerateGuid() const;
private:
	/** Helper functionality used by UAnimDataController to retrieve mutable data */ 
	FBoneAnimationTrack* FindMutableBoneTrackByName(FName Name);
	FBoneAnimationTrack& GetMutableBoneTrackByName(FName Name);
	FTransformCurve* FindMutableTransformCurveById(const FAnimationCurveIdentifier& CurveIdentifier);
	FFloatCurve* FindMutableFloatCurveById(const FAnimationCurveIdentifier& CurveIdentifier);
	FAnimCurveBase* FindMutableCurveById(const FAnimationCurveIdentifier& CurveIdentifier);	   
	FRichCurve* GetMutableRichCurve(const FAnimationCurveIdentifier& CurveIdentifier);

	/**
	* Broadcasts a new EAnimDataModelNotifyType with the provided payload data alongside it.
	*
	* @param	NotifyType			Type of notify to broadcast
	* @param	PayloadData			Typed payload data
	*/
	template<typename T>
	void Notify(EAnimDataModelNotifyType NotifyType, const T& PayloadData)
	{
		UScriptStruct* TypeScriptStruct = T::StaticStruct();

		const FAnimDataModelNotifPayload Payload((int8*)&PayloadData, TypeScriptStruct);
		ModifiedEvent.Broadcast(NotifyType, this, Payload);

		if (ModifiedEventDynamic.IsBound())
		{
			ModifiedEventDynamic.Broadcast(NotifyType, this, Payload);
		}

		// Only regenerate transient data when not in a bracket, or at the end of one
		{
			if (NotifyType == EAnimDataModelNotifyType::BracketOpened)
			{
				++BracketCounter;
			}
			if (NotifyType == EAnimDataModelNotifyType::BracketClosed)
			{
				--BracketCounter;
			}

			check(BracketCounter >= 0);
		}
	}

	/**
	* Broadcasts a new EAnimDataModelNotifyType alongside of an empty payload.
	*
	* @param	NotifyType			Type of notify to broadcast
	*/
	void Notify(EAnimDataModelNotifyType NotifyType)
	{
		FEmptyPayload EmptyPayload;
		const FAnimDataModelNotifPayload Payload((int8*)&EmptyPayload, FEmptyPayload::StaticStruct());

		ModifiedEvent.Broadcast(NotifyType, this, Payload);

		if (ModifiedEventDynamic.IsBound())
		{
			ModifiedEventDynamic.Broadcast(NotifyType, this, Payload);
		}

		// Only regenerate transient data when not in a bracket, or at the end of one
		{
			if (NotifyType == EAnimDataModelNotifyType::BracketOpened)
			{
				++BracketCounter;
			}
			if (NotifyType == EAnimDataModelNotifyType::BracketClosed)
			{
				--BracketCounter;
			}

			check(BracketCounter >= 0);
		}
	}

private:

	UPROPERTY(Transient)
	int32 BracketCounter = 0;
private:
	/** Dynamic delegate event allows scripting to register to any broadcasted notify. */
	UPROPERTY(BlueprintAssignable, Transient, Category = AnimationDataModel, meta = (ScriptName = "ModifiedEvent", AllowPrivateAccess = "true"))
	FAnimDataModelModifiedDynamicEvent ModifiedEventDynamic;
	
	/** Native delegate event allows for registerings to any broadcasted notify. */
	FAnimDataModelModifiedEvent ModifiedEvent;

	/** All individual bone animation tracks */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Animation Data Model", meta = (AllowPrivateAccess = "true"))
	TArray<FBoneAnimationTrack> BoneAnimationTracks;

	/** Total playable length of the contained animation data */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Data Model", meta = (AllowPrivateAccess = "true"))
	float PlayLength;
	
	/** Rate at which the animated data is sampled */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Data Model", meta = (AllowPrivateAccess = "true"))
	FFrameRate FrameRate;

	/** Total number of sampled animated frames */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Data Model", meta = (AllowPrivateAccess = "true"))
	int32 NumberOfFrames;

	/** Total number of sampled animated keys */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Data Model", meta = (AllowPrivateAccess = "true"))
	int32 NumberOfKeys;
	
	/** Container with all animated curve data */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Data Model", meta = (AllowPrivateAccess = "true"))
	FAnimationCurveData CurveData;
	
	/** Container with all animated (bone) attribute data */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Data Model", meta = (AllowPrivateAccess = "true"))
	TArray<FAnimatedBoneAttribute> AnimatedBoneAttributes;	

	friend class UAnimDataController;
	friend class FAnimDataControllerTestBase;
};

