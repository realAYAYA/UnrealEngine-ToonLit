// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGrid/RenderGrid.h"
#include "RenderGrid/RenderGridManager.h"
#include "RenderGridUtils.h"
#include "IRenderGridModule.h"
#include "LevelSequence.h"
#include "Misc/Base64.h"
#include "MoviePipelineMasterConfig.h"
#include "MoviePipelineOutputSetting.h"
#include "MovieScene.h"
#include "UObject/ObjectSaveContext.h"


URenderGridJob::URenderGridJob()
	: Guid(FGuid::NewGuid())
	, WaitFramesBeforeRendering(0)
	, Sequence(nullptr)
	, bOverrideStartFrame(false)
	, CustomStartFrame(0)
	, bOverrideEndFrame(false)
	, CustomEndFrame(0)
	, bOverrideResolution(false)
	, CustomResolution(FIntPoint(3840, 2160))
	, bIsEnabled(true)
	, RenderPreset(nullptr)
{}

TOptional<int32> URenderGridJob::GetSequenceStartFrame() const
{
	if (!IsValid(Sequence))
	{
		return TOptional<int32>();
	}

	const UMovieScene* MovieScene = Sequence->MovieScene;
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	const UMoviePipelineOutputSetting* Settings = GetRenderPresetOutputSettings();

	if (bOverrideStartFrame || (IsValid(Settings) && Settings->bUseCustomPlaybackRange))
	{
		int32 Frame = (bOverrideStartFrame ? CustomStartFrame : Settings->CustomStartFrame);
		if (IsValid(Settings) && Settings->bUseCustomFrameRate)
		{
			return FMath::FloorToInt(Frame / (Settings->OutputFrameRate / DisplayRate).AsDecimal());
		}
		return Frame;
	}

	const int32 StartFrameNumber = MovieScene->GetPlaybackRange().GetLowerBoundValue().Value;
	return FMath::FloorToInt(StartFrameNumber / (TickResolution / DisplayRate).AsDecimal());
}

TOptional<int32> URenderGridJob::GetSequenceEndFrame() const
{
	if (!IsValid(Sequence))
	{
		return TOptional<int32>();
	}

	const UMovieScene* MovieScene = Sequence->MovieScene;
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	const UMoviePipelineOutputSetting* Settings = GetRenderPresetOutputSettings();

	if (bOverrideEndFrame || (IsValid(Settings) && Settings->bUseCustomPlaybackRange))
	{
		int32 Frame = (bOverrideEndFrame ? CustomEndFrame : Settings->CustomEndFrame);
		if (IsValid(Settings) && Settings->bUseCustomFrameRate)
		{
			return FMath::FloorToInt(Frame / (Settings->OutputFrameRate / DisplayRate).AsDecimal());
		}
		return Frame;
	}

	const int32 EndFrameNumber = MovieScene->GetPlaybackRange().GetUpperBoundValue().Value;
	return FMath::FloorToInt(EndFrameNumber / (TickResolution / DisplayRate).AsDecimal());
}

bool URenderGridJob::SetSequenceStartFrame(const int32 NewCustomStartFrame)
{
	int32 StartFrame = GetStartFrame().Get(0);
	SetIsUsingCustomStartFrame(true);
	SetCustomStartFrame(StartFrame);

	TOptional<int32> SequenceStartFrame = GetSequenceStartFrame();
	while (SequenceStartFrame.IsSet() && (SequenceStartFrame.Get(0) > NewCustomStartFrame))
	{
		StartFrame--;
		if (StartFrame <= INT32_MIN)
		{
			return false;
		}
		SetCustomStartFrame(StartFrame);
		SequenceStartFrame = GetSequenceStartFrame();
	}
	while (SequenceStartFrame.IsSet() && (SequenceStartFrame.Get(0) < NewCustomStartFrame))
	{
		StartFrame++;
		if (StartFrame >= INT32_MAX)
		{
			return false;
		}
		SetCustomStartFrame(StartFrame);
		SequenceStartFrame = GetSequenceStartFrame();
	}
	return (SequenceStartFrame.IsSet() && (SequenceStartFrame.Get(0) == NewCustomStartFrame));
}

bool URenderGridJob::SetSequenceEndFrame(const int32 NewCustomStartFrame)
{
	int32 EndFrame = GetEndFrame().Get(0);
	SetIsUsingCustomEndFrame(true);
	SetCustomEndFrame(EndFrame);

	TOptional<int32> SequenceEndFrame = GetSequenceEndFrame();
	while (SequenceEndFrame.IsSet() && (SequenceEndFrame.Get(0) > NewCustomStartFrame))
	{
		EndFrame--;
		if (EndFrame <= INT32_MIN)
		{
			return false;
		}
		SetCustomEndFrame(EndFrame);
		SequenceEndFrame = GetSequenceEndFrame();
	}
	while (SequenceEndFrame.IsSet() && (SequenceEndFrame.Get(0) < NewCustomStartFrame))
	{
		EndFrame++;
		if (EndFrame >= INT32_MAX)
		{
			return false;
		}
		SetCustomEndFrame(EndFrame);
		SequenceEndFrame = GetSequenceEndFrame();
	}
	return (SequenceEndFrame.IsSet() && (SequenceEndFrame.Get(0) == NewCustomStartFrame));
}

TOptional<int32> URenderGridJob::GetStartFrame() const
{
	if (bOverrideStartFrame)
	{
		return CustomStartFrame;
	}

	const UMoviePipelineOutputSetting* Settings = GetRenderPresetOutputSettings();
	if (IsValid(Settings) && Settings->bUseCustomPlaybackRange)
	{
		return Settings->CustomStartFrame;
	}

	if (!IsValid(Sequence))
	{
		return TOptional<int32>();
	}

	const UMovieScene* MovieScene = Sequence->MovieScene;
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	if (IsValid(Settings) && Settings->bUseCustomFrameRate)
	{
		DisplayRate = Settings->OutputFrameRate;
	}
	const int32 StartFrameNumber = MovieScene->GetPlaybackRange().GetLowerBoundValue().Value;
	return FMath::FloorToInt(StartFrameNumber / (TickResolution / DisplayRate).AsDecimal());
}

TOptional<int32> URenderGridJob::GetEndFrame() const
{
	if (bOverrideEndFrame)
	{
		return CustomEndFrame;
	}

	const UMoviePipelineOutputSetting* Settings = GetRenderPresetOutputSettings();
	if (IsValid(Settings) && Settings->bUseCustomPlaybackRange)
	{
		return Settings->CustomEndFrame;
	}

	if (!IsValid(Sequence))
	{
		return TOptional<int32>();
	}

	const UMovieScene* MovieScene = Sequence->MovieScene;
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	if (IsValid(Settings) && Settings->bUseCustomFrameRate)
	{
		DisplayRate = Settings->OutputFrameRate;
	}
	const int32 EndFrameNumber = MovieScene->GetPlaybackRange().GetUpperBoundValue().Value;
	return FMath::FloorToInt(EndFrameNumber / (TickResolution / DisplayRate).AsDecimal());
}

TOptional<double> URenderGridJob::GetStartTime() const
{
	if (!IsValid(Sequence))
	{
		return TOptional<double>();
	}

	const TOptional<int32> StartFrame = GetStartFrame();
	if (!StartFrame.IsSet())
	{
		return TOptional<double>();
	}

	const UMoviePipelineOutputSetting* Settings = GetRenderPresetOutputSettings();

	const UMovieScene* MovieScene = Sequence->MovieScene;
	FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	if (IsValid(Settings) && Settings->bUseCustomFrameRate)
	{
		DisplayRate = Settings->OutputFrameRate;
	}
	return StartFrame.Get(0) / DisplayRate.AsDecimal();
}

TOptional<double> URenderGridJob::GetEndTime() const
{
	if (!IsValid(Sequence))
	{
		return TOptional<double>();
	}

	const TOptional<int32> EndFrame = GetEndFrame();
	if (!EndFrame.IsSet())
	{
		return TOptional<double>();
	}

	const UMoviePipelineOutputSetting* Settings = GetRenderPresetOutputSettings();

	const UMovieScene* MovieScene = Sequence->MovieScene;
	FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	if (IsValid(Settings) && Settings->bUseCustomFrameRate)
	{
		DisplayRate = Settings->OutputFrameRate;
	}
	return EndFrame.Get(0) / DisplayRate.AsDecimal();
}

TOptional<double> URenderGridJob::GetDurationInSeconds() const
{
	if (!IsValid(Sequence))
	{
		return TOptional<double>();
	}

	const TOptional<int32> StartFrame = GetStartFrame();
	const TOptional<int32> EndFrame = GetEndFrame();
	if (!StartFrame.IsSet() || !EndFrame.IsSet() || (StartFrame.Get(0) > EndFrame.Get(0)))
	{
		return TOptional<double>();
	}

	const UMoviePipelineOutputSetting* Settings = GetRenderPresetOutputSettings();

	const UMovieScene* MovieScene = Sequence->MovieScene;
	FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	if (IsValid(Settings) && Settings->bUseCustomFrameRate)
	{
		DisplayRate = Settings->OutputFrameRate;
	}
	return (EndFrame.Get(0) - StartFrame.Get(0)) / DisplayRate.AsDecimal();
}

FIntPoint URenderGridJob::GetOutputResolution() const
{
	if (bOverrideResolution)
	{
		return CustomResolution;
	}
	if (UMoviePipelineOutputSetting* Settings = GetRenderPresetOutputSettings())
	{
		return Settings->OutputResolution;
	}
	return GetDefault<UMoviePipelineOutputSetting>()->OutputResolution;
}

double URenderGridJob::GetOutputAspectRatio() const
{
	const UMoviePipelineOutputSetting* Settings = GetRenderPresetOutputSettings();
	if (IsValid(Settings))
	{
		return static_cast<double>(Settings->OutputResolution.X) / static_cast<double>(Settings->OutputResolution.Y);
	}

	const UMoviePipelineOutputSetting* DefaultSettings = GetDefault<UMoviePipelineOutputSetting>();
	return static_cast<double>(DefaultSettings->OutputResolution.X) / static_cast<double>(DefaultSettings->OutputResolution.Y);
}

bool URenderGridJob::MatchesSearchTerm(const FString& SearchTerm) const
{
	if (SearchTerm.TrimStartAndEnd().Len() <= 0)
	{
		return true;
	}
	TArray<FString> Parts;
	SearchTerm.ParseIntoArray(Parts, TEXT(" "), true);
	for (const FString& Part : Parts)
	{
		FString TrimmedPart = Part.TrimStartAndEnd();
		if (TrimmedPart.Len() <= 0)
		{
			continue;
		}
		if ((JobId.Find(TrimmedPart) != INDEX_NONE) || (JobName.Find(TrimmedPart) != INDEX_NONE) || (OutputDirectory.Find(TrimmedPart) != INDEX_NONE))
		{
			continue;
		}
		if (IsValid(RenderPreset) && (RenderPreset.GetPath().Find(TrimmedPart) != INDEX_NONE))
		{
			continue;
		}
		return false;
	}
	return true;
}

FString URenderGridJob::PurgeJobIdOrReturnEmptyString(const FString& NewJobId)
{
	static FString ValidCharacters = TEXT("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");

	FString Result;
	for (TCHAR NewJobIdChar : NewJobId)
	{
		int32 Index;
		if (ValidCharacters.FindChar(NewJobIdChar, Index))
		{
			Result += NewJobIdChar;
		}
	}
	return Result;
}

FString URenderGridJob::PurgeJobId(const FString& NewJobId)
{
	FString Result = PurgeJobIdOrReturnEmptyString(NewJobId);
	if (Result.IsEmpty())
	{
		return TEXT("0");
	}
	return Result;
}

FString URenderGridJob::PurgeJobIdOrGenerateUniqueId(URenderGrid* Grid, const FString& NewJobId)
{
	FString Result = PurgeJobIdOrReturnEmptyString(NewJobId);
	if (Result.IsEmpty())
	{
		return Grid->GenerateNextJobId();
	}
	return Result;
}

FString URenderGridJob::PurgeJobName(const FString& NewJobName)
{
	return NewJobName.TrimStartAndEnd();
}

FString URenderGridJob::PurgeOutputDirectory(const FString& NewOutputDirectory)
{
	return UE::RenderGrid::Private::FRenderGridUtils::NormalizeOutputDirectory(FPaths::ConvertRelativePathToFull(NewOutputDirectory))
		.Replace(*UE::RenderGrid::Private::FRenderGridUtils::NormalizeOutputDirectory(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())), TEXT("{project_dir}/"));
}

FString URenderGridJob::GetOutputDirectory() const
{
	return UE::RenderGrid::Private::FRenderGridUtils::NormalizeOutputDirectory(FPaths::ConvertRelativePathToFull(OutputDirectory.Replace(TEXT("{project_dir}"), *FPaths::ProjectDir())));
}

UMoviePipelineOutputSetting* URenderGridJob::GetRenderPresetOutputSettings() const
{
	if (!IsValid(RenderPreset))
	{
		return nullptr;
	}
	for (UMoviePipelineSetting* Settings : RenderPreset->FindSettingsByClass(UMoviePipelineOutputSetting::StaticClass(), false))
	{
		if (!IsValid(Settings))
		{
			continue;
		}
		if (UMoviePipelineOutputSetting* OutputSettings = Cast<UMoviePipelineOutputSetting>(Settings))
		{
			if (!OutputSettings->IsEnabled())
			{
				continue;
			}
			return OutputSettings;
		}
	}
	return nullptr;
}

bool URenderGridJob::HasRemoteControlValue(const TSharedPtr<FRemoteControlEntity>& RemoteControlEntity) const
{
	if (!RemoteControlEntity.IsValid())
	{
		return false;
	}

	const FString Key = RemoteControlEntity->GetId().ToString();
	return !!RemoteControlValues.Find(Key);
}

bool URenderGridJob::ConstGetRemoteControlValue(const TSharedPtr<FRemoteControlEntity>& RemoteControlEntity, TArray<uint8>& OutBinaryArray) const
{
	if (!RemoteControlEntity.IsValid())
	{
		return false;
	}

	const FString Key = RemoteControlEntity->GetId().ToString();
	if (const FRenderGridRemoteControlPropertyData* DataPtr = RemoteControlValues.Find(Key))
	{
		OutBinaryArray.Append((*DataPtr).Bytes);
		return true;
	}
	return false;
}

bool URenderGridJob::GetRemoteControlValue(const TSharedPtr<FRemoteControlEntity>& RemoteControlEntity, TArray<uint8>& OutBinaryArray)
{
	if (!RemoteControlEntity.IsValid())
	{
		return false;
	}

	const FString Key = RemoteControlEntity->GetId().ToString();
	if (FRenderGridRemoteControlPropertyData* DataPtr = RemoteControlValues.Find(Key))
	{
		OutBinaryArray.Append((*DataPtr).Bytes);
		return true;
	}

	if (!URenderGridPropRemoteControl::GetValueOfEntity(RemoteControlEntity, OutBinaryArray))
	{
		return false;
	}
	RemoteControlValues.Add(Key, FRenderGridRemoteControlPropertyData(TArray(OutBinaryArray)));
	return true;
}

bool URenderGridJob::SetRemoteControlValue(const TSharedPtr<FRemoteControlEntity>& RemoteControlEntity, const TArray<uint8>& BinaryArray)
{
	if (!RemoteControlEntity.IsValid())
	{
		return false;
	}
	const FString Key = RemoteControlEntity->GetId().ToString();
	RemoteControlValues.Add(Key, FRenderGridRemoteControlPropertyData(TArray(BinaryArray)));
	return true;
}


URenderGrid::URenderGrid()
	: Guid(FGuid::NewGuid())
	, PropsSourceType(ERenderGridPropsSourceType::RemoteControl)
	, PropsSourceOrigin_RemoteControl(nullptr)
	, bExecutingBlueprintEvent(false)
	, CachedPropsSource(nullptr)
	, CachedPropsSourceType(ERenderGridPropsSourceType::Local)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		LoadValuesFromCDO();
		OnPreSaveCDO().AddUObject(this, &URenderGrid::SaveValuesToCDO);
	}
}

UWorld* URenderGrid::GetWorld() const
{
	if (HasAllFlags(RF_ClassDefaultObject))
	{
		// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
		return nullptr;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			return Context.World();
		}
	}

	if (IsValid(GWorld))
	{
		return GWorld;
	}

	if (UWorld* CachedWorld = CachedWorldWeakPtr.Get(); IsValid(CachedWorld))
	{
		return CachedWorld;
	}
	UObject* Outer = GetOuter();// Could be a GameInstance, could be World, could also be a WidgetTree, so we're just going to follow the outer chain to find the world we're in.
	while (Outer)
	{
		UWorld* World = Outer->GetWorld();
		if (IsValid(World))
		{
			CachedWorldWeakPtr = World;
			return World;
		}
		Outer = Outer->GetOuter();
	}
	return nullptr;
}

void URenderGrid::PreSave(FObjectPreSaveContext SaveContext)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		OnPreSaveCDO().Broadcast();
	}
	Super::PreSave(SaveContext);
	SaveValuesToCDO();
}

void URenderGrid::PostLoad()
{
	Super::PostLoad();
	LoadValuesFromCDO();

	if (PropsSourceType == ERenderGridPropsSourceType::Local)
	{
		PropsSourceType = ERenderGridPropsSourceType::RemoteControl;
	}
}

void URenderGrid::BeginEditor()
{
	bExecutingBlueprintEvent = true;
	ReceiveBeginEditor();
	bExecutingBlueprintEvent = false;
}

void URenderGrid::EndEditor()
{
	bExecutingBlueprintEvent = true;
	ReceiveEndEditor();
	bExecutingBlueprintEvent = false;
}

void URenderGrid::BeginBatchRender(URenderGridQueue* Queue)
{
	bExecutingBlueprintEvent = true;
	ReceiveBeginBatchRender(Queue);
	bExecutingBlueprintEvent = false;
}

void URenderGrid::EndBatchRender(URenderGridQueue* Queue)
{
	bExecutingBlueprintEvent = true;
	ReceiveEndBatchRender(Queue);
	bExecutingBlueprintEvent = false;
}

void URenderGrid::BeginJobRender(URenderGridQueue* Queue, URenderGridJob* Job)
{
	bExecutingBlueprintEvent = true;
	ReceiveBeginJobRender(Queue, Job);
	bExecutingBlueprintEvent = false;
}

void URenderGrid::EndJobRender(URenderGridQueue* Queue, URenderGridJob* Job)
{
	bExecutingBlueprintEvent = true;
	ReceiveEndJobRender(Queue, Job);
	bExecutingBlueprintEvent = false;
}

void URenderGrid::BeginViewportRender(URenderGridJob* Job)
{
	bExecutingBlueprintEvent = true;
	ReceiveBeginViewportRender(Job);
	bExecutingBlueprintEvent = false;
}

void URenderGrid::EndViewportRender(URenderGridJob* Job)
{
	bExecutingBlueprintEvent = true;
	ReceiveEndViewportRender(Job);
	bExecutingBlueprintEvent = false;
}

void URenderGrid::CopyValuesToOrFromCDO(const bool bToCDO)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	URenderGrid* CDO = GetCDO();
	if (!IsValid(CDO))
	{
		return;
	}

	for (FProperty* Property = GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))// if not [Transient]
		{
			void* Data = Property->ContainerPtrToValuePtr<void>(this);
			void* DataCDO = Property->ContainerPtrToValuePtr<void>(CDO);
			if (bToCDO)
			{
				Property->CopyCompleteValue(DataCDO, Data);
			}
			else
			{
				Property->CopyCompleteValue(Data, DataCDO);
			}
		}
	}

	if (bToCDO)
	{
		CDO->RenderGridJobs = TArray<TObjectPtr<URenderGridJob>>();
		for (URenderGridJob* Job : RenderGridJobs)
		{
			if (!IsValid(Job))
			{
				continue;
			}
			if (URenderGridJob* JobCopy = DuplicateObject(Job, CDO); IsValid(JobCopy))
			{
				CDO->RenderGridJobs.Add(JobCopy);
			}
		}
	}
}

void URenderGrid::SetPropsSource(ERenderGridPropsSourceType InPropsSourceType, UObject* InPropsSourceOrigin)
{
	if (InPropsSourceType == ERenderGridPropsSourceType::RemoteControl)
	{
		PropsSourceType = InPropsSourceType;
		PropsSourceOrigin_RemoteControl = Cast<URemoteControlPreset>(InPropsSourceOrigin);
		return;
	}
	PropsSourceType = ERenderGridPropsSourceType::RemoteControl;
}

URenderGridPropsSourceBase* URenderGrid::GetPropsSource() const
{
	UObject* PropsSourceOrigin = GetPropsSourceOrigin();
	if (!IsValid(CachedPropsSource) || (CachedPropsSourceType != PropsSourceType) || (CachedPropsSourceOriginWeakPtr.Get() != PropsSourceOrigin))
	{
		CachedPropsSourceType = PropsSourceType;
		CachedPropsSourceOriginWeakPtr = PropsSourceOrigin;
		CachedPropsSource = UE::RenderGrid::IRenderGridModule::Get().CreatePropsSource(const_cast<URenderGrid*>(this), PropsSourceType, PropsSourceOrigin);
	}
	return CachedPropsSource;
}

UObject* URenderGrid::GetPropsSourceOrigin() const
{
	if (PropsSourceType == ERenderGridPropsSourceType::RemoteControl)
	{
		return PropsSourceOrigin_RemoteControl;
	}
	return nullptr;
}

void URenderGrid::AddRenderGridJob(URenderGridJob* Job)
{
	if (IsValid(Job))
	{
		RenderGridJobs.Add(Job);
	}
}

void URenderGrid::RemoveRenderGridJob(URenderGridJob* Job)
{
	RenderGridJobs.Remove(Job);
}

void URenderGrid::InsertRenderGridJob(URenderGridJob* Job, int32 Index)
{
	if (IsValid(Job))
	{
		RenderGridJobs.Insert(Job, Index);
	}
}

bool URenderGrid::HasRenderGridJob(URenderGridJob* Job) const
{
	return (RenderGridJobs.Find(Job) != INDEX_NONE);
}

int32 URenderGrid::GetIndexOfRenderGridJob(URenderGridJob* Job) const
{
	return RenderGridJobs.Find(Job);
}

TArray<URenderGridJob*> URenderGrid::GetRenderGridJobs() const
{
	TArray<URenderGridJob*> Result;
	for (URenderGridJob* Job : RenderGridJobs)
	{
		if (!IsValid(Job))
		{
			continue;
		}
		Result.Add(Job);
	}
	return Result;
}

TArray<URenderGridJob*> URenderGrid::GetEnabledRenderGridJobs() const
{
	TArray<URenderGridJob*> Result;
	for (URenderGridJob* Job : RenderGridJobs)
	{
		if (!IsValid(Job))
		{
			continue;
		}
		if (Job->GetIsEnabled())
		{
			Result.Add(Job);
		}
	}
	return Result;
}

TArray<URenderGridJob*> URenderGrid::GetDisabledRenderGridJobs() const
{
	TArray<URenderGridJob*> Result;
	for (URenderGridJob* Job : RenderGridJobs)
	{
		if (!IsValid(Job))
		{
			continue;
		}
		if (!Job->GetIsEnabled())
		{
			Result.Add(Job);
		}
	}
	return Result;
}

void URenderGrid::InsertRenderGridJobBefore(URenderGridJob* Job, URenderGridJob* BeforeJob)
{
	if (IsValid(Job))
	{
		const TArray<TObjectPtr<URenderGridJob>>::SizeType Index = RenderGridJobs.Find(BeforeJob);
		if (Index == INDEX_NONE)
		{
			RenderGridJobs.Add(Job);
		}
		else
		{
			RenderGridJobs.Insert(Job, Index);
		}
	}
}

void URenderGrid::InsertRenderGridJobAfter(URenderGridJob* Job, URenderGridJob* AfterJob)
{
	if (IsValid(Job))
	{
		const TArray<TObjectPtr<URenderGridJob>>::SizeType Index = RenderGridJobs.Find(AfterJob);
		if (Index == INDEX_NONE)
		{
			RenderGridJobs.Add(Job);
		}
		else
		{
			RenderGridJobs.Insert(Job, Index + 1);
		}
	}
}

FString URenderGrid::GenerateUniqueRandomJobId()
{
	TArray<uint8> ByteArray;

	{// adding timestamp bytes >>
		const int64 Value = FDateTime::UtcNow().GetTicks();
		bool Add = false;
		for (int32 i = 56; i >= 0; i -= 8)
		{
			if (!Add)
			{
				if ((Value >> i) <= 0)
				{
					// don't add a 0 byte to the start
					continue;
				}
				Add = true;
			}
			ByteArray.Add(Value >> i);
		}
	}// adding timestamp bytes <<

	{// adding random bytes >>
		for (int32 i = 1; i <= 16; i++)
		{
			ByteArray.Add(FMath::Rand() & 0xff);
		}
	}// adding random bytes <<

	return FBase64::Encode(ByteArray).Replace(TEXT("="), TEXT("")).Replace(TEXT("/"), TEXT("_")).Replace(TEXT("+"), TEXT("-"));
}

FString URenderGrid::GenerateNextJobId()
{
	int32 Max = 0;
	for (URenderGridJob* Job : RenderGridJobs)
	{
		if (!IsValid(Job))
		{
			continue;
		}
		int32 Value = FCString::Atoi(*Job->GetJobId());
		if (Value > Max)
		{
			Max = Value;
		}
	}
	FString Result = FString::FromInt(Max + 1);
	while (Result.Len() < UE::RenderGrid::FRenderGridManager::GeneratedIdCharacterLength)
	{
		Result = TEXT("0") + Result;
	}
	return Result;
}

bool URenderGrid::DoesJobIdExist(const FString& JobId)
{
	const FString JobIdToLower = JobId.ToLower();
	for (URenderGridJob* Job : RenderGridJobs)
	{
		if (!IsValid(Job))
		{
			continue;
		}
		if (JobIdToLower == Job->GetJobId().ToLower())
		{
			return true;
		}
	}
	return false;
}

URenderGridJob* URenderGrid::CreateTempRenderGridJob()
{
	URenderGridJob* Job = NewObject<URenderGridJob>(this);
	Job->SetJobId(GenerateUniqueRandomJobId());
	Job->SetJobName(TEXT("New"));
	Job->SetOutputDirectory(FPaths::ProjectDir() / TEXT("Saved/MovieRenders/"));

	if (URenderGridPropsSourceRemoteControl* PropsSource = GetPropsSource<URenderGridPropsSourceRemoteControl>())
	{
		TArray<uint8> BinaryArray;
		for (URenderGridPropRemoteControl* Field : PropsSource->GetProps()->GetAllCasted())
		{
			if (Field->GetValue(BinaryArray))
			{
				Job->SetRemoteControlValue(Field->GetRemoteControlEntity(), BinaryArray);
			}
		}
	}
	return Job;
}

URenderGridJob* URenderGrid::CreateAndAddNewRenderGridJob()
{
	URenderGridJob* Job = CreateTempRenderGridJob();
	Job->SetJobId(GenerateNextJobId());
	AddRenderGridJob(Job);
	return Job;
}

URenderGridJob* URenderGrid::DuplicateAndAddRenderGridJob(URenderGridJob* Job)
{
	if (!IsValid(Job))
	{
		return nullptr;
	}

	if (URenderGridJob* DuplicateRenderGridJob = DuplicateObject(Job, this); IsValid(DuplicateRenderGridJob))
	{
		DuplicateRenderGridJob->GenerateNewGuid();
		DuplicateRenderGridJob->SetJobId(GenerateNextJobId());
		InsertRenderGridJobAfter(DuplicateRenderGridJob, Job);
		return DuplicateRenderGridJob;
	}
	return nullptr;
}

bool URenderGrid::ReorderRenderGridJob(URenderGridJob* Job, URenderGridJob* DroppedOnJob, const bool bAfter)
{
	if (!IsValid(Job) || !IsValid(DroppedOnJob) || !HasRenderGridJob(Job) || !HasRenderGridJob(DroppedOnJob))
	{
		return false;
	}

	RemoveRenderGridJob(Job);
	if (bAfter)
	{
		InsertRenderGridJobAfter(Job, DroppedOnJob);
	}
	else
	{
		InsertRenderGridJobBefore(Job, DroppedOnJob);
	}
	return true;
}
