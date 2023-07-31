// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "UObject/Object.h"
#include "ILevelSequenceMetaData.h"
#include "Misc/FrameRate.h"
#include "VirtualCameraClipsMetaData.generated.h"


/**
 * Clips meta-data that is stored on ULevelSequence assets that are recorded through the virtual camera. 
 * Meta-data is retrieved through ULevelSequence::FindMetaData<UVirtualCameraClipsMetaData>()
 */

UCLASS(BlueprintType)
class UVirtualCameraClipsMetaData : public UObject, public ILevelSequenceMetaData
{
public: 
	GENERATED_BODY()
	UVirtualCameraClipsMetaData(const FObjectInitializer& ObjInit);

public: 

	/** The asset registry tag that contains the focal length for this meta-data */
	static const FName AssetRegistryTag_FocalLength;

	/** The asset registry tag that contains if the selected state for this meta-data */
	static const FName AssetRegistryTag_bIsSelected;

	/** The asset registry tag that contains the recorded level name for this meta-data */
	static const FName AssetRegistryTag_RecordedLevelName;

	/** The asset registry tag that contains the FrameCountStart in for this meta-data */
	static const FName AssetRegistryTag_FrameCountStart;

	/** The asset registry tag that contains the FrameCountEnd out for this meta-data */
	static const FName AssetRegistryTag_FrameCountEnd;

	/** The asset registry tag that contains the LengthInFrames out for this meta-data */
	static const FName AssetRegistryTag_LengthInFrames; 

	/** The asset registry tag that contains the FrameCountEnd out for this meta-data */
	static const FName AssetRegistryTag_DisplayRate;

	/** The asset registry tag that contains whether the clip was recorded with a CineCamera for this meta-data */
	static const FName AssetRegistryTag_bIsACineCameraRecording;


public:

	/**
	* Extend the default ULevelSequence asset registry tags
	*/
	virtual void ExtendAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#if WITH_EDITOR
	virtual void ExtendAssetRegistryTagMetaData(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const override;
#endif

public:
	/**
	 * @return The focal length for this clip
	 */
	UFUNCTION(BlueprintCallable, Category = "Clips")
	float GetFocalLength() const;

	/**
	* @return Whether or not the clip is selected.  
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	bool GetSelected() const;

	/**
	* @return The name of the clip's recorded level. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	FString GetRecordedLevelName() const;

	/**
	* @return The initial frame of the clip
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	int GetFrameCountStart() const;

	/**
	* @return The final frame of the clip
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	int GetFrameCountEnd() const;

	/**
	* @return The length in frames of the clip. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	int GetLengthInFrames();

	/**
	* @return The display rate of the clip. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	FFrameRate GetDisplayRate();

	/**
	* @return Whether the clip was recorded by a CineCameraActor
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	bool GetIsACineCameraRecording() const;

public:
	/**
	* Set the focal length associated with this clip. 
	* @note: Used for tracking. Does not update the StreamedCameraComponent. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	void SetFocalLength(float InFocalLength);
	
	/**
	* Set if this clip is 'selected'
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	void SetSelected(bool bInSelected);

	/**
	* Set the name of the level that the clip was recorded in. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	void SetRecordedLevelName(FString InLevelName);

	/**
	* Set the initial frame of the clip used for calculating duration.
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	void SetFrameCountStart(int InFrame); 

	/**
	* Set the final frame of the clip used for calculating duration.
	*/
	UFUNCTION(BlueprintCallable, Category = "Clips")
	void SetFrameCountEnd(int InFrame);

	/**
	 * Set the length in frames of the clip used for AssetData calculations. 
	 */
	UFUNCTION(BlueprintCallable, Category = "Clips")
	void SetLengthInFrames(int InLength);

	/**
	 * Set the DisplayRate of the clip used for AssetData calculations.
	 */
	UFUNCTION(BlueprintCallable, Category = "Clips")
	void SetDisplayRate(FFrameRate InDisplayRate);

	/**
	 * Set if the clip was recorded by a CineCameraActor
	 */
	UFUNCTION(BlueprintCallable, Category = "Clips")
	void SetIsACineCameraRecording(bool bInIsACineCameraRecording);


private:
	/** The focal length of the streamed camera used to record the take */
	UPROPERTY()
	float FocalLength;

	/** Whether or not the take was marked as 'selected' */
	UPROPERTY()
	bool bIsSelected; 

	/** The name of the level that the clip was recorded in */
	UPROPERTY()
	FString RecordedLevelName; 

	/** The initial frame of the clip used for calculating duration. */
	UPROPERTY()
	int FrameCountStart;

	/** The last frame of the clip used for calculating duration. */
	UPROPERTY()
	int FrameCountEnd; 

	/** The level sequence length in frames calculated from VirtualCameraSubsystem used for AssetData calculations */
	UPROPERTY()
	int LengthInFrames; 

	/** The display rate of the level sequence used for AssetData calculations. */
	UPROPERTY()
	FFrameRate DisplayRate;

	/** If the LevelSequence was recorded with a CineCameraActor, rather than a VirtualCameraActor */
	UPROPERTY()
	bool bIsACineCameraRecording;

};