// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SequencerCurveEditorObject.generated.h"


class ISequencer;
class ULevelSequence;
class FCurveEditor;
struct FMovieSceneChannel;
class UMovieSceneSection;
struct FCurveModelID;


USTRUCT(BlueprintType)
struct SEQUENCERSCRIPTINGEDITOR_API FSequencerChannelProxy
{
	GENERATED_BODY()

	FSequencerChannelProxy()
		: Section(nullptr)
	{}

	FSequencerChannelProxy(const FName& InChannelName, UMovieSceneSection* InSection)
		: ChannelName(InChannelName)
		, Section(InSection)
	{}

	UPROPERTY(BlueprintReadWrite, Category = Channel)
	FName ChannelName;

	UPROPERTY(BlueprintReadWrite, Category = Channel)
	TObjectPtr<UMovieSceneSection> Section;
};


/*
* Class to hold sequencer curve editor functions
*/
UCLASS()
class SEQUENCERSCRIPTINGEDITOR_API USequencerCurveEditorObject : public UObject
{
public:

	GENERATED_BODY()

public:
	/** Open curve editor*/
	UFUNCTION(BlueprintCallable, Category = "Sequencer Curve Editor")
	void OpenCurveEditor();

	UFUNCTION(BlueprintPure, Category = "Sequencer Curve Editor")
	/** Is curve editor open*/
	bool IsCurveEditorOpen();

	UFUNCTION(BlueprintCallable, Category = "Sequencer Curve Editor")
	/** Close curve editor*/
	void CloseCurveEditor();

public:

	/** Gets the channel with selected keys */
	UFUNCTION(BlueprintPure, Category = "Sequencer Curve Editor")
	TArray<FSequencerChannelProxy> GetChannelsWithSelectedKeys();

	/** Gets the selected keys with this channel */
	UFUNCTION(BlueprintPure, Category = "Sequencer Curve Editor")
	TArray<int32> GetSelectedKeys(const FSequencerChannelProxy& ChannelProxy);

	/** Select keys */
	UFUNCTION(BlueprintCallable, Category = "Sequencer Curve Editor")
	void SelectKeys(const FSequencerChannelProxy& Channel, const TArray<int32>& Indices);

	/** Empties the current selection. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer Curve Editor")
	void EmptySelection();

public:

	/** Get if a custom color for specified channel idendified by it's class and identifier exists */
	UFUNCTION(BlueprintCallable, Category = "Sequencer Curve Editor")
	bool HasCustomColorForChannel(UClass* Class, const FString& Identifier);

	/** Get custom color for specified channel idendified by it's class and identifier,if none exists will return white*/
	UFUNCTION(BlueprintCallable, Category = "Sequencer Curve Editor")
	FLinearColor GetCustomColorForChannel(UClass* Class, const FString& Identifier);

	/** Set Custom Color for specified channel idendified by it's class and identifier. This will be stored in editor user preferences.*/
	UFUNCTION(BlueprintCallable, Category = "Sequencer Curve Editor")
	void SetCustomColorForChannel(UClass* Class, const FString& Identifier, const FLinearColor& NewColor);

	/** Set Custom Color for specified channels idendified by it's class and identifiers. This will be stored in editor user preferences.*/
	UFUNCTION(BlueprintCallable, Category = "Sequencer Curve Editor")
	void SetCustomColorForChannels(UClass* Class, const TArray<FString>& Identifiers, const TArray<FLinearColor>& NewColors);

	/** Set Random Colors for specified channels idendified by it's class and identifiers. This will be stored in editor user preferences.*/
	UFUNCTION(BlueprintCallable, Category = "Sequencer Curve Editor")
	void SetRandomColorForChannels(UClass* Class, const TArray<FString>& Identifiers);

	/** Delete for specified channel idendified by it's class and identifier.*/
	UFUNCTION(BlueprintCallable, Category = "Sequencer Curve Editor")
	void DeleteColorForChannels(UClass* Class, FString& Identifier);

public:

	/**
	 * Function to assign a sequencer singleton.
	 * NOTE: Only to be called by ULevelSequenceBlueprintLibrary::SetSequencer.
	 */
	void SetSequencer(TSharedPtr<ISequencer>& InSequencer);

public:

	/**
	 * Utility function to get curve from a section and a name
	 */
	FCurveModelID GetCurve(UMovieSceneSection* InSection, const FName& InName);

	/**
	Utility function to get curve editor
	*/
	TSharedPtr<FCurveEditor> GetCurveEditor();

private:
	//internal sequencer
	TWeakPtr<ISequencer> CurrentSequencer;
};
