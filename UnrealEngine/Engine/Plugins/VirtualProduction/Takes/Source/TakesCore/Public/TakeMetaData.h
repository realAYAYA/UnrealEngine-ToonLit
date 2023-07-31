// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "ILevelSequenceMetaData.h"
#include "TakePreset.h"
#include "Misc/DateTime.h"
#include "Misc/QualifiedFrameTime.h"
#include "TakeMetaData.generated.h"

class UTakePreset;

/**
 * Take meta-data that is stored on ULevelSequence assets that are recorded through the Take Recorder.
 * Meta-data is retrieved through ULevelSequence::FindMetaData<UTakeMetaData>()
 */
UCLASS(config=EditorSettings, PerObjectConfig, BlueprintType)
class TAKESCORE_API UTakeMetaData : public UObject, public ILevelSequenceMetaData
{
public:
	GENERATED_BODY()

	UTakeMetaData(const FObjectInitializer& ObjInit);

public:

	/** The asset registry tag that contains the slate for this meta-data */
	static const FName AssetRegistryTag_Slate;

	/** The asset registry tag that contains the take number for this meta-data */
	static const FName AssetRegistryTag_TakeNumber;

	/** The asset registry tag that contains the timestamp for this meta-data */
	static const FName AssetRegistryTag_Timestamp;

	/** The asset registry tag that contains the timecode in for this meta-data */
	static const FName AssetRegistryTag_TimecodeIn;

	/** The asset registry tag that contains the timecode out for this meta-data */
	static const FName AssetRegistryTag_TimecodeOut;

	/** The asset registry tag that contains the user-description for this meta-data */
	static const FName AssetRegistryTag_Description;

	/** The asset registry tag that contains the level-path for this meta-data */
	static const FName AssetRegistryTag_LevelPath;

	/**
	 * Access the global config instance that houses default settings for take meta data for a given project
	 */
	static UTakeMetaData* GetConfigInstance();

	/**
	 * Create a new meta-data object from the project defaults
	 *
	 * @param Outer    The object to allocate the new meta-data within
	 * @param Name     The name for the new object. Must not already exist
	 */
	static UTakeMetaData* CreateFromDefaults(UObject* Outer, FName Name);

public:

	/**
	 * Extend the default ULevelSequence asset registry tags
	 */
	virtual void ExtendAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	/**
	 * Extend the default ULevelSequence asset registry tag meta-data
	 */
	virtual void ExtendAssetRegistryTagMetaData(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const override;

public:

	/**
	 * Check whether this take is locked
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	bool IsLocked() const;

	/**
	 * Check if this take was recorded (as opposed
	 * to being setup for recording)
	 */ 
	UFUNCTION(BlueprintCallable, Category="Take")
	bool Recorded() const;

	/**
	 * @return The slate for this take
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	const FString& GetSlate() const;

	/**
	 * @return The take number for this take
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	int32 GetTakeNumber() const;

	/**
	 * @return The timestamp for this take
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	FDateTime GetTimestamp() const;

	/**
	 * @return The timecode in for this take
	 */
	UFUNCTION(BlueprintCallable, Category = "Take")
	FTimecode GetTimecodeIn() const;

	/**
	 * @return The timecode out for this take
	 */
	UFUNCTION(BlueprintCallable, Category = "Take")
	FTimecode GetTimecodeOut() const;

	/**
	 * @return The duration for this take
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	FFrameTime GetDuration() const;

	/**
	 * @return The frame-rate for this take
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	FFrameRate GetFrameRate();

	/**
	 * @return The user-provided description for this take
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	FString GetDescription() const;

	/**
	 * @return The preset on which the take was originally based
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	UTakePreset* GetPresetOrigin() const;

	/**
	 * @return The AssetPath of the Level used to create a Recorded Level Sequence
	 */ 	
	UFUNCTION(BlueprintCallable, Category="Take")
	FString GetLevelPath() const;

	/**
	 * @return The Map used to create this recording
	 */ 
	UFUNCTION(BlueprintCallable, Category="Take")
	ULevel* GetLevelOrigin() const;

	/**
	*  @return Get if we get frame rate from time code
	*/
	UFUNCTION(BlueprintCallable, Category = "Take")
	bool GetFrameRateFromTimecode() const;

public:

	/**
	 * Lock this take, causing it to become read-only
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	void Lock();

	/**
	 * Unlock this take if it is read-only, allowing it to be modified once again
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	void Unlock();

	/**
	 * Generate the desired asset path for this take meta-data
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	FString GenerateAssetPath(const FString& PathFormatString) const;

	/**
	 * Set the slate for this take and reset its take number to 1
	 * @param bEmitChanged Whether or not to send a slate changed event
	 * @note: Only valid for takes that have not been locked
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	void SetSlate(FString InSlate, bool bEmitChanged = true);

	/**
	 * Set this take's take number. Take numbers are always clamped to be >= 1.
	 * @param bEmitChanged Whether or not to send a take number changed event
	 * @note: Only valid for takes that have not been locked
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	void SetTakeNumber(int32 InTakeNumber, bool bEmitChanged = true);

	/**
	 * Set this take's timestamp
	 * @note: Only valid for takes that have not been locked
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	void SetTimestamp(FDateTime InTimestamp);

	/**
	 * Set this take's timecode in
	 * @note: Only valid for takes that have not been locked
	 */
	UFUNCTION(BlueprintCallable, Category = "Take")
	void SetTimecodeIn(FTimecode InTimecodeIn);

	/**
	 * Set this take's timecode out
	 * @note: Only valid for takes that have not been locked
	 */
	UFUNCTION(BlueprintCallable, Category = "Take")
	void SetTimecodeOut(FTimecode InTimecodeOut);

	/**
	 * Set this take's duration
	 * @note: Only valid for takes that have not been locked
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	void SetDuration(FFrameTime InDuration);

	/**
	 * Set this take's frame-rate
	 * @note: Only valid for takes that have not been locked
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	void SetFrameRate(FFrameRate InFrameRate);

	/**
	 * Set this take's user-provided description
	 * @note: Only valid for takes that have not been locked
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	void SetDescription(FString InDescription);

	/**
	 * Set the preset on which the take is based
	 * @note: Only valid for takes that have not been locked
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	void SetPresetOrigin(UTakePreset* InPresetOrigin);

	/**
	 *  Set the map used to create this recording
	 */
	UFUNCTION(BlueprintCallable, Category="Take")
	void SetLevelOrigin(ULevel* InLevelOrigin);

	/**
	*  Set if we get frame rate from time code
	*/
	UFUNCTION(BlueprintCallable, Category = "Take")
	void SetFrameRateFromTimecode(bool InFromTimecode);

private:

	/** Whether the take is locked */
	UPROPERTY()
	bool bIsLocked;

	/** The user-provided slate information for the take */
	UPROPERTY(config)
	FString Slate;

	/** The take number */
	UPROPERTY()
	int32 TakeNumber;

	/** The timestamp at which the take was initiated */
	UPROPERTY()
	FDateTime Timestamp;

	/** The timecode at the start of recording */
	UPROPERTY()
	FTimecode TimecodeIn;

	/** The timecode at the end of recording */
	UPROPERTY()
	FTimecode TimecodeOut;

	/** The desired duration for the take */
	UPROPERTY(config)
	FFrameTime Duration;

	/** The frame rate the take was recorded at */
	UPROPERTY()
	FFrameRate FrameRate;

	/** A user-provided description for the take */
	UPROPERTY(config)
	FString Description;

	/** The preset that the take was based off */
	UPROPERTY(config)
	TSoftObjectPtr<UTakePreset> PresetOrigin;

#if WITH_EDITORONLY_DATA
	/** The level map used to create this recording */
	UPROPERTY()
	TSoftObjectPtr<ULevel> LevelOrigin;
#endif // WITH_EDITORONLY_DATA

	/** Whether or not we get or frame rate from Timecode, default to true */
	UPROPERTY()
	bool bFrameRateFromTimecode;
};
