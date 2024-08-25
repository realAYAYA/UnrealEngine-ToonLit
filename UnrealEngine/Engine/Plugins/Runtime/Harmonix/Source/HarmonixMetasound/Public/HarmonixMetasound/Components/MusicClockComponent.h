// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Components/ActorComponent.h"
#include "Harmonix/MusicalTimebase.h"
#include "HarmonixMidi/MidiSongPos.h"
#include "Delegates/DelegateCombinations.h"
#include "HarmonixMidi/SmoothedMidiPlayCursor.h"
#include "Templates/UniquePtr.h"

#include "MusicClockComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMusicClock, Log, All)

namespace Metasound { class FMetasoundGenerator; }
struct FMusicalTimeSpan;
class UAudioComponent;
struct FMusicClockDriverBase;

UENUM(BlueprintType)
enum class EMusicClockState : uint8
{
	Stopped,
	Paused,
	Running,
};

UENUM(BlueprintType)
enum class EMusicClockDriveMethod : uint8
{
	WallClock,
	MetaSound,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeatEvent, int, BeatNumber, int, BeatInBar);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBarEvent, int, BarNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSectionEvent, const FString&, SectionName, float, SectionStartMs, float, SectionLengthMs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayStateEvent, EMusicClockState, State);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMusicClockConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMusicClockDisconnected);

UCLASS(ClassGroup = (MetaSoundMusic), PrioritizeCategories = "MusicClock", meta = (BlueprintSpawnableComponent, DisplayName = "Music Clock", ScriptName = MusicClockComponent))
class HARMONIXMETASOUND_API UMusicClockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UMusicClockComponent();

	void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "MusicClock")
	EMusicClockDriveMethod DriveMethod = EMusicClockDriveMethod::MetaSound;

	UPROPERTY(EditDefaultsOnly, Category = "MusicClock", meta = (EditCondition = "DriveMethod == EMusicClockDriveMethod::MetaSound"))
	FName MetasoundOutputName = "MIDI Clock";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicClock", meta = (EditCondition = "DriveMethod == EMusicClockDriveMethod::MetaSound"))
	TObjectPtr<UAudioComponent> MetasoundsAudioComponent;

	UPROPERTY(EditDefaultsOnly, Category = "MusicClock", meta = (EditCondition = "DriveMethod == EMusicClockDriveMethod::WallClock"))
	TObjectPtr<UMidiFile> TempoMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicClock|Defaults")
	float Tempo = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicClock|Defaults")
	int TimeSignatureNum = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicClock|Defaults")
	int TimeSignatureDenom = 4;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "MusicClock")
	float CurrentBeatDurationSec = 0.5f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "MusicClock")
	float CurrentBarDurationSec = 2.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "MusicClock")
	float CurrentClockAdvanceRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicClock")
	bool RunPastMusicEnd = false;

	UFUNCTION(BlueprintCallable, Category = "MusicClock")
	void Start();

	UFUNCTION(BlueprintCallable, Category = "MusicClock")
	void Pause();

	UFUNCTION(BlueprintCallable, Category = "MusicClock")
	void Continue();

	UFUNCTION(BlueprintCallable, Category = "MusicClock")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "MusicClock")
	EMusicClockState GetState() const { return State; }

	// Getters for all of the fields in mCurrentSongPos...
	
	// Time from the beginning of the authored music content.
	// NOTE: INCLUDES time for count-in and pickup bars.
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetSecondsIncludingCountIn(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	// Time from Bar 1 Beat 1. The classic "start of the song".
	// NOTE: DOES NOT INCLUDE time for count-in and pickup bars.
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetSecondsFromBarOne(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	// Returns the fractional total bars from the beginning of the authored music content.
	// NOTE: INCLUDES time for count-in and pickup bars.
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetBarsIncludingCountIn(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	// Returns the fractional total beats from the beginning of the authored music content.
	// NOTE: INCLUDES time for count-in and pickup bars.
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetBeatsIncludingCountIn(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	// Returns the "classic" musical timestamp in the form Bar (int) & Beat (float). In this form...
	//    - Bar 1, Beat 1.0 is the "beginning of the song" AFTER count-in/pickups
	//    - Bar 0, Beat 1.0 would be one bar BEFORE the "beginning of the song"... eg. a bar of count-in or pickup.
	//    - While Bar can be positive or negative, Beat is always >= 1.0 and is read as "beat in the bar". Again... '1' based!
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	FMusicTimestamp GetCurrentTimestamp(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	/** Returns the name of the section that we're currently in (intro, chorus, outro) */
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	FString GetCurrentSectionName(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	/** Returns the index of the current section for the provided time base. [0, Num-1] */
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	int32 GetCurrentSectionIndex(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	UFUNCTION(BlueprintPure, Category = "MusicClock")
	const TArray<FSongSection>& GetSongSections() const;

	/** Returns the start time of the current section in milliseconds */
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetCurrentSectionStartMs(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	/** Returns the length of the current section in milliseconds */
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetCurrentSectionLengthMs(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	/** Gets a value expressed in beats between 0-1 that indicates how much progress we made in the current beat */
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetDistanceFromCurrentBeat(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	/** Gets a value expressed in beats between 0-1 that indicates how close we are to the next beat. */
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetDistanceToNextBeat(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	/** Gets a value expressed in beats between 0-1 that indicates how close we are to the closest beat (current beat or next beat). */
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetDistanceToClosestBeat(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	/** Gets a value expressed in bars between 0-1 that indicates how much progress we made towards the current bar to the next one. */
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetDistanceFromCurrentBar(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	/** Gets a value expressed in bars between 0-1 that indicates how close we are to the next bar. */
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetDistanceToNextBar(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	/** Gets value expressed in bars between 0-1 that indicates how close we are to the closest bar (current bar or next bar). */
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetDistanceToClosestBar(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetDeltaBar(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetDeltaBeat(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	const FMidiSongPos& GetSongPos(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	/** Returns the remaining time until the end of the MIDI in milliseconds based on the timestamp corresponding to the passed Timebase */
	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetSongRemainingMs(ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	UFUNCTION(BlueprintPure, Category = "Count In")
	float GetCountInSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Tick")
	float TickToMs(float Tick) const;

	UFUNCTION(BlueprintPure, Category = "Beat")
	float BeatToMs(float Beat) const;

	UFUNCTION(BlueprintPure, Category = "Beat")
	float GetMsPerBeatAtMs(float Ms) const;

	UFUNCTION(BlueprintPure, Category = "Beat")
	float GetNumBeatsInBarAtMs(float Ms) const;

	UFUNCTION(BlueprintPure, Category = "Beat")
	float GetBeatInBarAtMs(float Ms) const;
	
	UFUNCTION(BlueprintPure, Category = "Bar")
	float BarToMs(float Bar) const;

	UFUNCTION(BlueprintPure, Category = "Bar")
	float GetMsPerBarAtMs(float Ms) const;

	UFUNCTION(BlueprintPure, Category = "Section")
	FString GetSectionNameAtMs(float Ms) const;

	UFUNCTION(BlueprintPure, Category = "Section")
	float GetSectionLengthMsAtMs(float Ms) const;

	UFUNCTION(BlueprintPure, Category = "Section")
	float GetSectionStartMsAtMs(float Ms) const;

	UFUNCTION(BlueprintPure, Category = "Section")
	float GetSectionEndMsAtMs(float Ms) const;

	UFUNCTION(BlueprintPure, Category = "Section")
	int32 GetNumSections() const;

	UFUNCTION(BlueprintPure, Category = "Song Data")
	float GetSongLengthMs() const;

	UFUNCTION(BlueprintPure, Category = "Song Data")
	float GetSongLengthBeats() const;

	UFUNCTION(BlueprintPure, Category = "Song Data")
	float GetSongLengthBars() const;

	const FSongMaps& GetSongMaps() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicClock")
	ECalibratedMusicTimebase TimebaseForBarAndBeatEvents = ECalibratedMusicTimebase::VideoRenderTime;

	UPROPERTY(BlueprintAssignable, Category = "MusicClock")
	FPlayStateEvent PlayStateEvent;

	UPROPERTY(BlueprintAssignable, Category = "MusicClock")
	FBeatEvent BeatEvent;

	UPROPERTY(BlueprintAssignable, Category = "MusicClock")
	FBarEvent BarEvent;

	UPROPERTY(BlueprintAssignable, Category = "MusicClock")
	FSectionEvent SectionEvent;

	UPROPERTY(BlueprintAssignable, Category = "MusicClock")
	FMusicClockConnected MusicClockConnectedEvent;

	UPROPERTY(BlueprintAssignable, Category = "MusicClock")
	FMusicClockDisconnected MusicClockDisconnectedEvent;

private:
	// Don't let C++ access these directly! They are a blueprint convenience and only work because 
	// they specify getter functions!
	UPROPERTY(BlueprintGetter = GetCurrentSmoothedAudioRenderSongPos, Category = "MusicClock")
	FMidiSongPos CurrentSmoothedAudioRenderSongPos;
	UPROPERTY(BlueprintGetter = GetCurrentVideoRenderSongPos, Category = "MusicClock")
	FMidiSongPos CurrentVideoRenderSongPos;
	UPROPERTY(BlueprintGetter = GetCurrentPlayerExperiencedSongPos, Category = "MusicClock")
	FMidiSongPos CurrentPlayerExperiencedSongPos;

public:
	// Getter functions for the Blueprint properties exposed above...
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MusicClock")
	FMidiSongPos GetCurrentSmoothedAudioRenderSongPos() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MusicClock")
	FMidiSongPos GetCurrentVideoRenderSongPos() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MusicClock")
	FMidiSongPos GetCurrentPlayerExperiencedSongPos() const;

	// Note: Not const as it might cause the clock to update from its source.
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float MeasureSpanProgress(const FMusicalTimeSpan& Span, ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	UFUNCTION(BlueprintCallable, Category = "Audio|MusicClock", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static UMusicClockComponent* CreateMetasoundDrivenMusicClock(UObject* WorldContextObject, UAudioComponent* InAudioComponent, FName MetasoundOuputPinName = "MIDI Clock", bool Start = true);

	UFUNCTION(BlueprintCallable, Category = "Audio|MusicClock", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static UMusicClockComponent* CreateWallClockDrivenMusicClock(UObject* WorldContextObject, UMidiFile* WithTempoMap, bool Start = true);

	UFUNCTION(BlueprintCallable, Category = "MusicClock")
	bool ConnectToMetasoundOnAudioComponent(UAudioComponent* InAudioComponent);

	UFUNCTION(BlueprintCallable, Category = "MusicClock")
	void ConnectToWallClockForMidi(UMidiFile* InTempoMap);

	// Note: Not const as it might cause the clock to update from its source.
	FMidiSongPos CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase = ECalibratedMusicTimebase::VideoRenderTime) const;

	const FMidiSongPos& GetRawUnsmoothedAudioRenderPos() const { return RawUnsmoothedAudioRenderPos; }

protected:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction);

private:
	friend struct FMusicClockDriverBase;
	friend struct FMetasoundMusicClockDriver;
	friend struct FWallClockMusicClockDriver;
	friend class  UMidiClockUpdateSubsystem;

	TSharedPtr<FMusicClockDriverBase> ClockDriver;

	EMusicClockState State = EMusicClockState::Stopped;

	FSongMaps DefaultMaps;
	FMidiSongPos RawUnsmoothedAudioRenderPos;

	float AudioRenderDeltaBarF = 0.0f;
	float AudioRenderDeltaBeatF = 0.0f;
	float PlayerExperienceDeltaBarF = 0.0f;
	float PlayerExperienceDeltaBeatF = 0.0f;
	float VideoRenderDeltaBarF = 0.0f;
	float VideoRenderDeltaBeatF = 0.0f;
	int32 LastBroadcastBar = -1;
	int32 LastBroadcastBeat = -1;
	FSongSection LastBroadcastSongSection;

	FMidiSongPos PrevAudioRenderSongPos;
	FMidiSongPos PrevPlayerExperiencedSongPos;
	FMidiSongPos PrevVideoRenderSongPos;

	uint64    LastUpdateFrame = 0;

	void CreateClockDriver();
	void BroadcastSongPosChanges();
	void MakeDefaultSongMap();
	bool ConnectToMetasound();
	void ConnectToWallClock();

	// Ensures the clock will be updated once per frame.  Should only get called on the game thread.
	void EnsureClockIsValidForGameFrame() const;// TODO: Cleanup task - UE-205069 - If we find we are able to use the new 
												// MidiClock/MusicClockComponent ticking methods, this function should be
												// deleted and all of the call sites cleaned up... as only this next, 
												// non-const "Ensure" function will be required...
	void EnsureClockIsValidForGameFrameFromSubsystem();
};

struct FMusicClockDriverBase : public TSharedFromThis<FMusicClockDriverBase>
{
public:
	FMusicClockDriverBase() = delete;
	FMusicClockDriverBase(UMusicClockComponent* DrivenClock)
		: Clock(DrivenClock)
	{}
	virtual ~FMusicClockDriverBase() = default;

	void EnsureClockIsValidForGameFrame();

	virtual bool CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase, FMidiSongPos& OutResult) const = 0;

	virtual void Disconnect() = 0;
	virtual void OnStart() = 0;
	virtual void OnPause() = 0;
	virtual void OnContinue() = 0;
	virtual void OnStop() = 0;
	virtual const FSongMaps* GetCurrentSongMaps() const = 0;

protected:
	UMusicClockComponent* Clock;
private:
	virtual bool RefreshCurrentSongPos() = 0;
};

