// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "Animation/AnimSequence.h"
#include "Misc/FrameRate.h"
#include "InterchangeAnimSequenceFactoryNode.generated.h"

namespace UE::Interchange::Animation
{
	INTERCHANGEFACTORYNODES_API FFrameRate ConvertSampleRatetoFrameRate(double SampleRate);
}

namespace UE::Interchange
{
	struct INTERCHANGEFACTORYNODES_API FAnimSequenceNodeStaticData : public FBaseNodeStaticData
	{
		static const FAttributeKey& GetAnimatedMorphTargetDependenciesKey();
		static const FAttributeKey& GetAnimatedAttributeCurveNamesKey();
		static const FAttributeKey& GetAnimatedAttributeStepCurveNamesKey();
		static const FAttributeKey& GetAnimatedMaterialCurveSuffixesKey();
		static const FAttributeKey& GetSceneNodeAnimationPayloadKeyMapKey();
		static const FAttributeKey& GetMorphTargetNodePayloadKeyMapKey();
	};
}//ns UE::Interchange

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeAnimSequenceFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	UInterchangeAnimSequenceFactoryNode();

	/**
	 * Initialize node data
	 * @param UniqueID - The uniqueId for this node
	 * @param DisplayLabel - The name of the node
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	void InitializeAnimSequenceNode(const FString& UniqueID, const FString& DisplayLabel);

	/**
	 * Override serialize to restore SlotMaterialDependencies on load.
	 */
	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		if (Ar.IsLoading() && bIsInitialized)
		{
			SceneNodeAnimationPayloadKeyMap.RebuildCache();
			MorphTargetNodePayloadKeyMap.RebuildCache();
		}
	}

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override;

	virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

	/** Get the class this node want to create */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	virtual class UClass* GetObjectClass() const override;

public:
	/** Get the skeleton factory node unique id. Return false if the attribute is not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomSkeletonFactoryNodeUid(FString& AttributeValue) const;

	/** Set the skeleton factory node unique id. Return false if the attribute cannot be set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomSkeletonFactoryNodeUid(const FString& AttributeValue);


	/**********************************************************************************************
	 * Import bone tracks API begin
	 */

	/**
	 * Get the import bone tracks state. The attribute will be true if we need to import bone tracks.
	 * False if we do not import bone tracks.
	 * 
	 * Note - Return false if the attribute is not set. Return true if the attribute exist and can be query.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomImportBoneTracks(bool& AttributeValue) const;

	/** Set the import bone tracks state. Pass true to import bone tracks, false to not import bone tracks. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomImportBoneTracks(const bool& AttributeValue);

	/** Get the import bone tracks sample rate. Return false if the attribute is not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomImportBoneTracksSampleRate(double& AttributeValue) const;

	/** Set the import bone tracks sample rate. Return false if the attribute cannot be set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomImportBoneTracksSampleRate(const double& AttributeValue);

	/** Get the import bone tracks start time in second. Return false if the attribute is not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomImportBoneTracksRangeStart(double& AttributeValue) const;

	/** Set the import bone tracks start time in second. Return false if the attribute cannot be set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomImportBoneTracksRangeStart(const double& AttributeValue);

	/** Get the import bone tracks end time in second. Return false if the attribute is not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomImportBoneTracksRangeStop(double& AttributeValue) const;

	/** Set the import bone tracks end time in second. Return false if the attribute cannot be set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomImportBoneTracksRangeStop(const double& AttributeValue);

	/*
	 * Import bone tracks API end
	 **********************************************************************************************/


	/**********************************************************************************************
	 * Curves API begin
	 */


	 /**
	  * Get the import attribute curves state. If true this mean we want to import all user custom attributes
	  * we can find on a node.
	  * 
	  * Return false if the attribute is not set.
	  */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomImportAttributeCurves(bool& AttributeValue) const;

	/** Set the import attribute curves state. Return false if the attribute cannot be set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomImportAttributeCurves(const bool& AttributeValue);

	/**
	 * Get the custom attribute DoNotImportCurveWithZero, return false if the attribute is not set.
	 * 
	 * Note - If value is true, do not import if it doesn't have any value other than zero. This is to avoid adding extra curves to evaluate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomDoNotImportCurveWithZero(bool& AttributeValue) const;

	/**
	 * Set the custom attribute DoNotImportCurveWithZero. Return false if the attribute cannot be set.
	 * 
	 * Note - If value is true, do not import if it doesn't have any value other than zero. This is to avoid adding extra curves to evaluate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomDoNotImportCurveWithZero(const bool& AttributeValue);

	/**
	 * Get the custom attribute RemoveCurveRedundantKeys, return false if the attribute is not set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomRemoveCurveRedundantKeys(bool& AttributeValue) const;

	/**
	 * Set the custom attribute RemoveCurveRedundantKeys. Return false if the attribute cannot be set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomRemoveCurveRedundantKeys(const bool& AttributeValue);

	


	/*********************************************************************************************************
	 * Morph target curve API Begin
	 *
	 * Note - Morpgh target curve payload is FRichCurve.
	 */

	/**
	 * Get the custom attribute DeleteExistingMorphTargetCurves, return false if the attribute is not set.
	 * 
	 * Note - If true, all previous moprh target curves will be deleted when doing a re-import.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomDeleteExistingMorphTargetCurves(bool& AttributeValue) const;

	/**
	 * Set the custom attribute DeleteExistingMorphTargetCurves. Return false if the attribute cannot be set.
	 * 
	 * Note - If true, all previous moprh target curves will be deleted when doing a re-import.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomDeleteExistingMorphTargetCurves(const bool& AttributeValue);

	/** return how many animated moprh target (moprh target are translated into a mesh node and can be animated with a float curve) this anim sequence depends on. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	int32 GetAnimatedMorphTargetDependeciesCount() const;

	/** Get all animated moprh target unique id (moprh target are translated into a mesh node and can be animated with a float curve) this anim sequence depends on. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	void GetAnimatedMorphTargetDependencies(TArray<FString>& OutDependencies) const;

	/** Get an animated moprh target unique id (moprh target are translated into a mesh node and can be animated with a float curve) point by the specified index. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	void GetAnimatedMorphTargetDependency(const int32 Index, FString& OutDependency) const;

	/** Add an animated moprh target unique id (moprh target are translated into a mesh node and can be animated with a float curve). */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetAnimatedMorphTargetDependencyUid(const FString& DependencyUid);

	/** Remove one animated moprh target unique id (moprh target are translated into a mesh node and can be animated with a float curve). Return false if we cannot remove the moprh target unique id. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool RemoveAnimatedMorphTargetDependencyUid(const FString& DependencyUid);

	/**
	 * Morph target curve API End
	 *********************************************************************************************************/


	
	/*********************************************************************************************************
	 * Attribute curve API Begin
	 * 
	 * Note - Attribute curve payload information can be retrieve via the UInterchangeUserDefinedAttributesAPI.
	 *        Attribute curves are import has float FRichCurve
	 */

	 /** Return how many animated attribute curve names this anim sequence drive (curve are FRichCurve of type float). */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	int32 GetAnimatedAttributeCurveNamesCount() const;

	/** Get all animated attribute curve names. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	void GetAnimatedAttributeCurveNames(TArray<FString>& OutAttributeCurveNames) const;

	/** Get an animated attribute curve name point by the specified index. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	void GetAnimatedAttributeCurveName(const int32 Index, FString& OutAttributeCurveName) const;

	/** Add an animated attribute curve name. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetAnimatedAttributeCurveName(const FString& AttributeCurveName);

	/** Remove one animated attribute curve name. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool RemoveAnimatedAttributeCurveName(const FString& AttributeCurveName);

	/**
	 * Attribute curve API End
	 *********************************************************************************************************/



	 /*********************************************************************************************************
	  * Material curve API Begin
	  *
	  * Note - Material curve are attribute curve that can animate a material parameter.
	  */

	/**
	 * Get the custom attribute MaterialDriveParameterOnCustomAttribute, return false if the attribute is not set.
	 *
	 * Note - If true, Set Material Curve Type for all custom attributes that exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomMaterialDriveParameterOnCustomAttribute(bool& AttributeValue) const;

	/**
	 * Set the custom attribute MaterialDriveParameterOnCustomAttribute. Return false if the attribute cannot be set.
	 *
	 * Note - If true, Set Material Curve Type for all custom attributes that exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomMaterialDriveParameterOnCustomAttribute(const bool& AttributeValue);

	/** Return how many animated material curve suffixes this anim sequence drive (curve are FRichCurve of type float). */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	int32 GetAnimatedMaterialCurveSuffixesCount() const;

	/** Get all animated material curve suffixes. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	void GetAnimatedMaterialCurveSuffixes(TArray<FString>& OutMaterialCurveSuffixes) const;

	/** Get an animated material curve suffixe point by the specified index. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	void GetAnimatedMaterialCurveSuffixe(const int32 Index, FString& OutMaterialCurveSuffixe) const;

	/** Add an animated material curve suffixe. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetAnimatedMaterialCurveSuffixe(const FString& MaterialCurveSuffixe);

	/** Remove one animated material curve suffixe. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool RemoveAnimatedMaterialCurveSuffixe(const FString& MaterialCurveSuffixe);

	/**
	 * Material curve API End
	 *********************************************************************************************************/



	/*********************************************************************************************************
	 * Attribute step curve API Begin
	 * 
	 * Note - Attribute step curve payload information can be retrieve via the UInterchangeUserDefinedAttributesAPI.
	 *        Attribute step curves are import has a TArray<float> for key time and a TArray<ValueType> for the value.
	 *        Supported value type are: int32, float, FString.
	 */

	 /** Return how many animated attribute step curve names this anim sequence drive. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	int32 GetAnimatedAttributeStepCurveNamesCount() const;

	/** Get all animated attribute step curve names. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	void GetAnimatedAttributeStepCurveNames(TArray<FString>& OutAttributeStepCurveNames) const;

	/** Get an animated attribute step curve name point by the specified index. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	void GetAnimatedAttributeStepCurveName(const int32 Index, FString& OutAttributeStepCurveName) const;

	/** Add an animated attribute step curve name. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetAnimatedAttributeStepCurveName(const FString& AttributeStepCurveName);

	/** Remove one animated attribute step curve name. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool RemoveAnimatedAttributeStepCurveName(const FString& AttributeStepCurveName);

	/**
	 * Attribute step curve API End
	 *********************************************************************************************************/



	/**
	 * Get the custom attribute DeleteExistingCustomAttributeCurves, return false if the attribute is not set.
	 * 
	 * Note - If true, all previous custom attribute curves will be deleted when doing a re-import.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomDeleteExistingCustomAttributeCurves(bool& AttributeValue) const;

	/**
	 * Set the custom attribute DeleteExistingCustomAttributeCurves. Return false if the attribute cannot be set.
	 * 
	 * Note - If true, all previous custom attribute curves will be deleted when doing a re-import.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomDeleteExistingCustomAttributeCurves(const bool& AttributeValue);
	
	/*
	 * Curves API end
	 **********************************************************************************************/


	/**
	 * Get the custom attribute DeleteExistingNonCurveCustomAttributes, return false if the attribute is not set.
	 * 
	 * Note - If true, all previous non-curve custom attributes will be deleted when doing a re-import.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomDeleteExistingNonCurveCustomAttributes(bool& AttributeValue) const;

	/**
	 * Set the custom attribute DeleteExistingNonCurveCustomAttributes. Return false if the attribute cannot be set.
	 * 
	 * Note - If true, all previous non-curve custom attributes will be deleted when doing a re-import.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomDeleteExistingNonCurveCustomAttributes(const bool& AttributeValue);

	/**
	 * Query the optional existing USkeleton this anim must use. The anim sequence factory will use this skeleton instead of the imported one
	 * (from GetCustomSkeletonFactoryNodeUid) if this attribute is set and the skeleton pointer is valid.
	 * Pipeline set this attribute in case the user want to specify an existing skeleton.
	 * Return false if the attribute was not set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool GetCustomSkeletonSoftObjectPath(FSoftObjectPath& AttributeValue) const;

	/**
	 * Set the optional existing USkeleton this anim must use. The AnimSequence factory will use this skeleton instead of the imported one
	 * (from GetCustomSkeletonFactoryNodeUid) if this attribute is set and the skeleton pointer is valid.
	 * Pipeline set this attribute in case the user want to specify an existing skeleton.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimSequence")
	bool SetCustomSkeletonSoftObjectPath(const FSoftObjectPath& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	void GetSceneNodeAnimationPayloadKeys(TMap<FString, FString>& OutSceneNodeAnimationPayloads) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool GetAnimationPayloadKeyFromSceneNodeUid(const FString& SceneNodeUid, FString& OutPayloadKey) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool SetAnimationPayloadKeyForSceneNodeUid(const FString& SceneNodeUid, const FString& PayloadKey);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool RemoveAnimationPayloadKeyForSceneNodeUid(const FString& SceneNodeUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	void GetMorphTargetNodeAnimationPayloadKeys(TMap<FString, FString>& OutMorphTargetNodeAnimationPayloads) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool GetAnimationPayloadKeyFromMorphTargetNodeUid(const FString& MorphTargetNodeUid, FString& OutPayloadKey) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool SetAnimationPayloadKeyForMorphTargetNodeUid(const FString& MorphTargetNodeUid, const FString& PayloadKey);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool RemoveAnimationPayloadKeyForMorphTargetNodeUid(const FString& MorphTargetNodeUid);

private:
	const UE::Interchange::FAttributeKey Macro_CustomSkeletonFactoryNodeUidKey = UE::Interchange::FAttributeKey(TEXT("SkeletonFactoryNodeUid"));

	const UE::Interchange::FAttributeKey Macro_CustomImportBoneTracksKey = UE::Interchange::FAttributeKey(TEXT("ImportBoneTracks"));
	const UE::Interchange::FAttributeKey Macro_CustomImportBoneTracksSampleRateKey = UE::Interchange::FAttributeKey(TEXT("ImportBoneTracksSampleRate"));
	const UE::Interchange::FAttributeKey Macro_CustomImportBoneTracksRangeStartKey = UE::Interchange::FAttributeKey(TEXT("ImportBoneTracksRangeStart"));
	const UE::Interchange::FAttributeKey Macro_CustomImportBoneTracksRangeStopKey = UE::Interchange::FAttributeKey(TEXT("ImportBoneTracksRangeStop"));

	const UE::Interchange::FAttributeKey Macro_CustomImportAttributeCurvesKey = UE::Interchange::FAttributeKey(TEXT("ImportAttributeCurves"));
	const UE::Interchange::FAttributeKey Macro_CustomDoNotImportCurveWithZeroKey = UE::Interchange::FAttributeKey(TEXT("DoNotImportCurveWithZero"));
	const UE::Interchange::FAttributeKey Macro_CustomRemoveCurveRedundantKeysKey = UE::Interchange::FAttributeKey(TEXT("RemoveCurveRedundantKeys"));
	const UE::Interchange::FAttributeKey Macro_CustomMaterialDriveParameterOnCustomAttributeKey = UE::Interchange::FAttributeKey(TEXT("MaterialDriveParameterOnCustomAttribute"));
	const UE::Interchange::FAttributeKey Macro_CustomDeleteExistingMorphTargetCurvesKey = UE::Interchange::FAttributeKey(TEXT("DeleteExistingMorphTargetCurves"));
	const UE::Interchange::FAttributeKey Macro_CustomDeleteExistingCustomAttributeCurvesKey = UE::Interchange::FAttributeKey(TEXT("DeleteExistingCustomAttributeCurves"));

	const UE::Interchange::FAttributeKey Macro_CustomDeleteExistingNonCurveCustomAttributesKey = UE::Interchange::FAttributeKey(TEXT("DeleteExistingNonCurveCustomAttributes"));

	const UE::Interchange::FAttributeKey Macro_CustomSkeletonSoftObjectPathKey = UE::Interchange::FAttributeKey(TEXT("SkeletonSoftObjectPath"));

	UE::Interchange::TArrayAttributeHelper<FString> AnimatedMorphTargetDependencies;
	UE::Interchange::TArrayAttributeHelper<FString> AnimatedAttributeCurveNames;
	UE::Interchange::TArrayAttributeHelper<FString> AnimatedAttributeStepCurveNames;
	UE::Interchange::TArrayAttributeHelper<FString> AnimatedMaterialCurveSuffixes;

	UE::Interchange::TMapAttributeHelper<FString, FString> SceneNodeAnimationPayloadKeyMap;
	UE::Interchange::TMapAttributeHelper<FString, FString> MorphTargetNodePayloadKeyMap;
};
