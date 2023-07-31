// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/ControlRigSequence.h"
#include "UObject/Package.h"
#include "MovieScene.h"
#include "ControlRig.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigSequence)

#define LOCTEXT_NAMESPACE "ControlRigSequence"

static TAutoConsoleVariable<int32> CVarControlRigDefaultEvaluationType(
	TEXT("ControlRigSequence.DefaultEvaluationType"),
	0,
	TEXT("0: Playback locked to playback frames\n1: Unlocked playback with sub frame interpolation"),
	ECVF_Default);

static TAutoConsoleVariable<FString> CVarControlRigDefaultTickResolution(
	TEXT("ControlRigSequence.DefaultTickResolution"),
	TEXT("24000fps"),
	TEXT("Specifies default a tick resolution for newly created control rig sequences. Examples: 30 fps, 120/1 (120 fps), 30000/1001 (29.97), 0.01s (10ms)."),
	ECVF_Default);

static TAutoConsoleVariable<FString> CVarControlRigDefaultDisplayRate(
	TEXT("ControlRigSequence.DefaultDisplayRate"),
	TEXT("30fps"),
	TEXT("Specifies default a display frame rate for newly created control rig sequences; also defines frame locked frame rate where sequences are set to be frame locked. Examples: 30 fps, 120/1 (120 fps), 30000/1001 (29.97), 0.01s (10ms)."),
	ECVF_Default); 

UControlRigSequence::UControlRigSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LastExportedFrameRate(30.0f)
{
	bParentContextsAreSignificant = false;
}

void UControlRigSequence::Initialize()
{
	MovieScene = NewObject<UMovieScene>(this, NAME_None, RF_Transactional);
	MovieScene->SetFlags(RF_Transactional);

	const bool bFrameLocked = CVarControlRigDefaultEvaluationType.GetValueOnGameThread() != 0;

	MovieScene->SetEvaluationType(bFrameLocked ? EMovieSceneEvaluationType::FrameLocked : EMovieSceneEvaluationType::WithSubFrames);

	FFrameRate TickResolution(60000, 1);
	TryParseString(TickResolution, *CVarControlRigDefaultTickResolution.GetValueOnGameThread());
	MovieScene->SetTickResolutionDirectly(TickResolution);

	FFrameRate DisplayRate(30, 1);
	TryParseString(DisplayRate, *CVarControlRigDefaultDisplayRate.GetValueOnGameThread());
	MovieScene->SetDisplayRate(DisplayRate);
}

void UControlRigSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
}

bool UControlRigSequence::CanPossessObject(UObject& Object, UObject* InPlaybackContext) const
{
	return false; // we only support spawnables
}

UObject* UControlRigSequence::GetParentObject(UObject* Object) const
{
	return nullptr;
}

void UControlRigSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
}

UObject* UControlRigSequence::MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName)
{
	UObject* NewInstance = NewObject<UObject>(MovieScene, InSourceObject.GetClass(), ObjectName);

	UEngine::CopyPropertiesForUnrelatedObjects(&InSourceObject, NewInstance);

	return NewInstance;
}

bool UControlRigSequence::CanAnimateObject(UObject& InObject) const
{
	return InObject.IsA<UControlRig>();
}

#undef LOCTEXT_NAMESPACE

