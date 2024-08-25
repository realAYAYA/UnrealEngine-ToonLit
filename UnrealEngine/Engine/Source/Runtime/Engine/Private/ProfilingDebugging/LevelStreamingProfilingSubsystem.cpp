// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/LevelStreamingProfilingSubsystem.h"

#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Streaming/LevelStreamingDelegates.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
#include "Stats/Stats.h"

#if !WITH_EDITOR
#include "Engine/World.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelStreamingProfilingSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogLevelStreamingProfiling, Log, All);
CSV_DEFINE_CATEGORY_MODULE(ENGINE_API, LevelStreaming, true);

static bool GLevelStreamingProfilingEnabled = !UE_BUILD_SHIPPING && !UE_SERVER;
static FAutoConsoleVariableRef CVarLevelStreamingProfilingEnabled(
#if UE_BUILD_SHIPPING
	TEXT("LevelStreaming.Profiling.Enabled.Shipping"),
#else
	TEXT("LevelStreaming.Profiling.Enabled"),
#endif
	GLevelStreamingProfilingEnabled,
	TEXT("Whether to enable the LevelStreamingProfilingSubsystem automatically."),
	ECVF_Default
);

static bool GStartProfilingAutomatically = false;
static FAutoConsoleVariableRef CVarStartProfilingAutomatically(
	TEXT("LevelStreaming.Profiling.StartAutomatically"),
	GStartProfilingAutomatically,
	TEXT("Whether to start recording level streaminge events as soon as the subsystem is created."),
	ECVF_Default
);

static float GLateStreamingDistanceSquared = 0.0f;
static FAutoConsoleVariableRef CVarLateStreamingDistanceSquared(
	TEXT("LevelStreaming.Profiling.LateStreamingDistanceSquared"),
	GLateStreamingDistanceSquared,
	TEXT("The squared distance (e.g. from world partition cell bounds) below which a level is considered to have streamed in late."),
	ECVF_Default
);

ULevelStreamingProfilingSubsystem::ULevelStreamingProfilingSubsystem(const FObjectInitializer& ObjectInitializer)
{
}

ULevelStreamingProfilingSubsystem::~ULevelStreamingProfilingSubsystem()
{
}

double ULevelStreamingProfilingSubsystem::GetLateStreamingDistanceSquared()
{
	return GLateStreamingDistanceSquared;
}

bool ULevelStreamingProfilingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

bool ULevelStreamingProfilingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// If there are any loaded child classes of this subsystem and this is not the CDO of that child class, skip creation of this subsystem.
	TArray<UClass*> DerivedClasses; 
	GetDerivedClasses(ULevelStreamingProfilingSubsystem::StaticClass(), DerivedClasses, true);
	if (DerivedClasses.Num() > 0)
	{
		if (!Algo::AnyOf(DerivedClasses, [Class=GetClass()](UClass* DerivedClass) { return Class == DerivedClass; }))
		{
			return false;
		}
	}
	return GLevelStreamingProfilingEnabled && Super::ShouldCreateSubsystem(Outer);
}

void ULevelStreamingProfilingSubsystem::PostInitialize()
{
	Super::PostInitialize();

	Handle_OnLevelStreamingTargetStateChanged = FLevelStreamingDelegates::OnLevelStreamingTargetStateChanged.AddUObject(this, &ULevelStreamingProfilingSubsystem::OnLevelStreamingTargetStateChanged);
	Handle_OnLevelStreamingStateChanged = FLevelStreamingDelegates::OnLevelStreamingStateChanged.AddUObject(this, &ULevelStreamingProfilingSubsystem::OnLevelStreamingStateChanged);
	Handle_OnLevelBeginAddToWorld = FLevelStreamingDelegates::OnLevelBeginMakingVisible.AddUObject(this, &ULevelStreamingProfilingSubsystem::OnLevelStartedAddToWorld);
	Handle_OnLevelBeginRemoveFromWorld = FLevelStreamingDelegates::OnLevelBeginMakingInvisible.AddUObject(this, &ULevelStreamingProfilingSubsystem::OnLevelStartedRemoveFromWorld);
	
	if (GStartProfilingAutomatically)
	{
		StartTracking();
	}
}

void ULevelStreamingProfilingSubsystem::Deinitialize() 
{
	if (IsTracking())
	{
		StopTrackingAndReport();
	}

	if (!ReportWritingTask.IsCompleted())
	{
		UE_LOG(LogLevelStreamingProfiling, Log, TEXT("Waiting for report writing task to complete during Deinitialize for safety"));
		ReportWritingTask.Wait();
	}

	FLevelStreamingDelegates::OnLevelStreamingTargetStateChanged.Remove(Handle_OnLevelStreamingTargetStateChanged);
	FLevelStreamingDelegates::OnLevelStreamingStateChanged.Remove(Handle_OnLevelStreamingStateChanged);
	FLevelStreamingDelegates::OnLevelBeginMakingVisible.Remove(Handle_OnLevelBeginAddToWorld);
	FLevelStreamingDelegates::OnLevelBeginMakingInvisible.Remove(Handle_OnLevelBeginRemoveFromWorld);
	
	Handle_OnLevelStreamingTargetStateChanged.Reset();
	Handle_OnLevelStreamingStateChanged.Reset();
	Handle_OnLevelBeginAddToWorld.Reset();
	Handle_OnLevelBeginRemoveFromWorld.Reset();

	Super::Deinitialize();
}

const TCHAR* ULevelStreamingProfilingSubsystem::EnumToString(ULevelStreamingProfilingSubsystem::ELevelState State)
{
	using ELevelState = ULevelStreamingProfilingSubsystem::ELevelState;
	switch (State)
	{
	case ELevelState::None: return TEXT("None");
	case ELevelState::QueuedForLoading: return TEXT("QueuedForLoading");
	case ELevelState::Loading: return TEXT("Loading");
	case ELevelState::Loaded: return TEXT("Loaded");
	case ELevelState::QueuedForAddToWorld: return TEXT("QueuedForAddToWorld");
	case ELevelState::AddingToWorld: return TEXT("AddingToWorld");
	case ELevelState::AddedToWorld: return TEXT("AddedToWorld");
	case ELevelState::QueuedForRemoveFromWorld: return TEXT("QueuedForRemoveFromWorld");
	case ELevelState::RemovingFromWorld: return TEXT("RemovingFromWorld");
	case ELevelState::RemovedFromWorld: return TEXT("RemovedFromWorld");
	default:
		check(false);
		return TEXT("Unknown");
	}
}
	
TConstArrayView<ULevelStreamingProfilingSubsystem::FLevelStats> ULevelStreamingProfilingSubsystem::GetLevelStats() const
{
	return LevelStats;
}

TUniquePtr<ULevelStreamingProfilingSubsystem::FActiveLevel> ULevelStreamingProfilingSubsystem::MakeActiveLevel(const ULevelStreaming* StreamingLevel, ELevelState InitialState, ULevel* LoadedLevel)
{
	TUniquePtr<FActiveLevel> Level = MakeUnique<FActiveLevel>(LevelStats.AddDefaulted());
	Level->State = ELevelState::None;
	Level->StateStartTime = FPlatformTime::Seconds();
	if (LoadedLevel)
	{
		UPackage* Package = LoadedLevel->GetPackage();
		LevelStats[Level->StatsIndex].PackageNameInMemory = Package->GetFName();
		LevelStats[Level->StatsIndex].PackageNameOnDisk = Package->GetLoadedPath().GetPackageFName();
	}
	else
	{
		switch (InitialState)
		{
		case ELevelState::Loaded: 
		case ELevelState::QueuedForAddToWorld: 
		case ELevelState::AddingToWorld: 
		case ELevelState::AddedToWorld: 
		case ELevelState::QueuedForRemoveFromWorld: 
		case ELevelState::RemovingFromWorld: 
		case ELevelState::RemovedFromWorld: 
			UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Entered initial state %s without loaded level. %s %s"), 
				EnumToString(InitialState),
				*StreamingLevel->GetName(), 
				*StreamingLevel->GetWorldAssetPackageName()
				);
			break;
		default:
			break;
		}
	}
	if (const UWorldPartitionRuntimeCell* Cell = Cast<const UWorldPartitionRuntimeCell>(StreamingLevel->GetWorldPartitionCell()))
	{
		LevelStats[Level->StatsIndex].CellBounds = Cell->GetCellBounds();
		LevelStats[Level->StatsIndex].ContentBounds = Cell->GetContentBounds();
		LevelStats[Level->StatsIndex].bIsHLOD = Cell->GetIsHLOD();
	}
	UpdateLevelState(*Level, StreamingLevel, InitialState, Level->StateStartTime);
	return MoveTemp(Level);
}

void ULevelStreamingProfilingSubsystem::UpdateLevelState(ULevelStreamingProfilingSubsystem::FActiveLevel& Level, const ULevelStreaming* StreamingLevel, ULevelStreamingProfilingSubsystem::ELevelState NewState, double Time)
{	
	ELevelState OldState = Level.State;
	Level.State = NewState;
	Level.StateStartTime = Time;

	UpdateTrackingData(Level.StatsIndex, LevelStats[Level.StatsIndex], StreamingLevel, OldState, NewState);
}

void ULevelStreamingProfilingSubsystem::StartTracking()
{
	if (!ReportWritingTask.IsCompleted())
	{
		UE_LOG(LogLevelStreamingProfiling, Warning, TEXT("StartTracking called while writing previous report - waiting for completion."));
		ReportWritingTask.Wait();
	}
	
	if (bIsTracking)
	{
		UE_LOG(LogLevelStreamingProfiling, Warning, TEXT("StartTracking called while already tracking"));
	}
	else
	{
		bIsTracking = true;

		UE_LOG(LogLevelStreamingProfiling, Log, TEXT("Starting to track level streaming events"));

		LevelStats.Reset();
		ActiveLevels.Reset();
	}
}

void ULevelStreamingProfilingSubsystem::StopTrackingAndReport()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_LevelStreamingProfilingSubsystem_StopTrackingAndReport);
	if (!bIsTracking)
	{
		UE_LOG(LogLevelStreamingProfiling, Warning, TEXT("StopTrackingAndReport called without StartTracking"));
		return;
	}

	bIsTracking = false;
	UE_LOG(LogLevelStreamingProfiling, Log, TEXT("Reporting level streaming stats"));

	ReportWritingTask = UE::Tasks::Launch(UE_SOURCE_LOCATION,
	[this]() 
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_LevelStreamingProfilingSubsystem_ReportWritingTask);
		FString OutDir = FPaths::ProfilingDir() / TEXT("LevelStreaming");
		IFileManager::Get().MakeDirectory(*OutDir, true);

		FString Filename = OutDir / FString::Printf(TEXT("LevelStreaming_(%s).tsv"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*Filename));
		if (Ar.IsValid())
		{
			TUtf8StringBuilder<1024> Builder;
			FUtf8StringView EOL = UTF8TEXTVIEW("\n");
			auto Write = [&Ar, &Builder, EOL]() {
				Ar->Serialize(Builder.GetData(), Builder.Len());
				Ar->Serialize((void*)EOL.GetData(), EOL.Len());
				Builder.Reset();
			};
			auto AddDouble = [&Builder](TOptional<double> Field, double UnitsMultiplier) {
				Builder << '\t';
				if (Field)
				{
					Builder.Appendf("%.1f", Field.GetValue() * UnitsMultiplier);
				}
			};

			auto AddVector = [&Builder](TOptional<FVector> Field) {
				Builder << '\t';
				if (Field)
				{
					Builder.Appendf("%.0f,%.0f,%.0f", Field->X, Field->Y, Field->Z);
				}
			};

			auto AddBounds = [&Builder](const FBox& Field) {
				Builder << '\t';
				if (Field.IsValid)
				{
					Builder.Appendf("%.0f,%.0f,%.0f,%.0f,%.0f,%.0f", Field.Min.X, Field.Min.Y, Field.Min.Z, Field.Max.X, Field.Max.Y, Field.Max.Z);
				}
			};

			Builder << "NameOnDisk\tNameInMemory\tTimeQueuedForLoadingMS\tTimeLoadingMS\tTimeQueuedForAddToWorldMS\tTimeAddingToWorldMS\tTimeInWorldS\tTimeQueuedForRemoveFromWorldMS\tTimeRemovingFromWorldMS\tStreamInDistance_Cell\tStreamInDistance_Content\tStreamInSourceLocation\tCellBounds\tContentBounds\tbIsHLOD";
			AugmentReportHeader(Builder);
			Write();
			for (int32 i=0; i < LevelStats.Num(); ++i)
			{
				const FLevelStats& Stats = LevelStats[i];
				if (!Stats.bValid) { continue; }
				Builder << Stats.PackageNameOnDisk;
				Builder << '\t' << Stats.PackageNameInMemory;
				AddDouble(Stats.TimeQueuedForLoading, 1000.0);
				AddDouble(Stats.TimeLoading, 1000.0);
				AddDouble(Stats.TimeQueueudForAddToWorld, 1000.0);
				AddDouble(Stats.TimeAddingToWorld, 1000.0);
				AddDouble(Stats.TimeInWorld, 1.0);
				AddDouble(Stats.TimeQueuedForRemoveFromWorld, 1000.0);
				AddDouble(Stats.TimeRemovingFromWorld, 1000.0);
				AddDouble(Stats.FinalStreamInDistance_Cell, 1.0);
				AddDouble(Stats.FinalStreamInDistance_Content, 1.0);
				AddVector(Stats.FinalStreamInLocation);
				AddBounds(Stats.CellBounds);
				AddBounds(Stats.ContentBounds);
				Builder << '\t' << (int32)Stats.bIsHLOD;
				AugmentReportRow(Builder, i);
				Write();
			}

	#if CSV_PROFILER
			// Write a metadata row like CSV files 
			TMap<FString, FString> Metadata = FCsvProfiler::Get()->GetMetadataMapCopy();
			if (Metadata.Num() > 0)
			{
				for (const TPair<FString, FString>& Pair : Metadata)
				{ 		
					Builder << '[' << Pair.Key << "]\t" << Pair.Value << '\t';
				}
				Write();
			}
	#endif

			PostReport();
		}
	}, UE::Tasks::ETaskPriority::BackgroundLow);
}

void ULevelStreamingProfilingSubsystem::OnLevelStreamingTargetStateChanged(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LevelIfLoaded, ELevelStreamingState CurrentState, ELevelStreamingTargetState PrevTarget, ELevelStreamingTargetState NewTarget)
{
	if (World != GetWorld())
	{
		return;
	}

	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("OnLevelStreamingTargetStateChanged: %s %s. %s: %s -> %s (LoadedLevel: %s)"),
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName(),
		::EnumToString(CurrentState),
		::EnumToString(PrevTarget),
		::EnumToString(NewTarget),
		LevelIfLoaded ? TEXT("NOT NULL") : TEXT("NULL")
	);

	switch (NewTarget)
	{
	case ELevelStreamingTargetState::LoadedNotVisible:
		switch (CurrentState)
		{
		case ELevelStreamingState::LoadedNotVisible:
			return OnLevelUnqueuedForRemoveFromWorld(World, StreamingLevel, LevelIfLoaded);
		case ELevelStreamingState::LoadedVisible:
			return OnLevelQueuedForRemoveFromWorld(World, StreamingLevel, LevelIfLoaded);
		case ELevelStreamingState::Unloaded:
			return OnLevelQueuedForLoading(World, StreamingLevel);
		case ELevelStreamingState::MakingVisible:
			return;
		default:
			UE_LOG(LogLevelStreamingProfiling, Warning, TEXT("Unexpected desired state LoadedNotVisible in current state %s"), ::EnumToString(CurrentState));
			return;
		}
	case ELevelStreamingTargetState::LoadedVisible:
		switch (CurrentState)
		{
		case ELevelStreamingState::LoadedVisible:
			return OnLevelUnqueuedForAddToWorld(World, StreamingLevel, LevelIfLoaded);
		case ELevelStreamingState::LoadedNotVisible:
			return OnLevelQueuedForAddToWorld(World, StreamingLevel, LevelIfLoaded);
		default:
			UE_LOG(LogLevelStreamingProfiling, Warning, TEXT("Unexpected desired state LoadedVisible in current state %s"), ::EnumToString(CurrentState));
			return;
		}
	case ELevelStreamingTargetState::Unloaded:
		return;
	case ELevelStreamingTargetState::UnloadedAndRemoved:
		return;
	}
}

void ULevelStreamingProfilingSubsystem::OnLevelStreamingStateChanged(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LevelIfLoaded, ELevelStreamingState PrevState, ELevelStreamingState NewState)
{
	if (World != GetWorld())
	{
		return;
	}

	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("OnLevelStreamingStateChanged: %s %s. %s -> %s (LoadedLevel: %s)"),
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName(),
		::EnumToString(PrevState),
		::EnumToString(NewState),
		LevelIfLoaded ? TEXT("NOT NULL") : TEXT("NULL")
	);
	switch (NewState)
	{
	case ELevelStreamingState::Removed: 
		OnStreamingLevelRemoved(World, StreamingLevel);
		break;
	case ELevelStreamingState::Unloaded: 
		if (PrevState == ELevelStreamingState::Loading)
		{
			OnLevelFinishedAsyncLoading(World, StreamingLevel, nullptr);
		}
		else
		{
			UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Ignoring transition to Unloaded from previous state %s"), ::EnumToString(PrevState));
		}
		break;
	case ELevelStreamingState::FailedToLoad: 
		OnLevelFinishedAsyncLoading(World, StreamingLevel, LevelIfLoaded);
		break;
	case ELevelStreamingState::Loading: 
		OnLevelStartedAsyncLoading(World, StreamingLevel);
		break;
	case ELevelStreamingState::LoadedNotVisible: 
		switch (PrevState)
		{
		case ELevelStreamingState::LoadedNotVisible:
		case ELevelStreamingState::Loading:
		case ELevelStreamingState::Unloaded:
			if (LevelIfLoaded) // Temp hack, filter out early calls before loadedlevel is set. 
			{
				OnLevelFinishedAsyncLoading(World, StreamingLevel, LevelIfLoaded);
			}
			else
			{
				UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Ignoring transition to LoadedNotVisible from previous state %s because LoadedLevel is not yet available"),
					::EnumToString(PrevState));
			}
			break;
		default:
			UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Ignoring transition to LoadedNotVisible from previous state %s"), ::EnumToString(PrevState));
			break;
		case ELevelStreamingState::MakingInvisible:
			OnLevelFinishedRemoveFromWorld(World, StreamingLevel, LevelIfLoaded);
		}
		break;
	case ELevelStreamingState::LoadedVisible: 
		if (PrevState == ELevelStreamingState::MakingVisible)
		{
			OnLevelFinishedAddToWorld(World, StreamingLevel, LevelIfLoaded);
		}
		else
		{
			UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Ignoring transition to LoadedVisible from previous state %s"), ::EnumToString(PrevState));
		}
		break;
	case ELevelStreamingState::MakingVisible: 
	case ELevelStreamingState::MakingInvisible: 
		break;
	}
}

void ULevelStreamingProfilingSubsystem::OnLevelQueuedForLoading(UWorld* World, const ULevelStreaming* StreamingLevel)
{
	if (!bIsTracking)
	{
		return;
	}
	TUniquePtr<FActiveLevel>& Level = ActiveLevels.FindOrAdd(StreamingLevel);
	if (Level.IsValid())
	{
		// TODO: If level is in state 'None', could consider keeping old load time if level is found in memory?
		UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Streaming level %s queued for loading while already active."), 
			*StreamingLevel->GetWorldAssetPackageName());
	}
	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s queued for loading"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	Level = MakeActiveLevel(StreamingLevel, ELevelState::QueuedForLoading);
}

void ULevelStreamingProfilingSubsystem::OnLevelUnqueuedForLoading(UWorld* World, const ULevelStreaming* StreamingLevel)
{
	if (!bIsTracking)
	{
		return;
	}
	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s unqueued for loading"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	int32 NumRemoved = ActiveLevels.Remove(StreamingLevel);
	if (NumRemoved == 0)
	{	
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s unqueued for loading when it wasn't tracked"), 
			*StreamingLevel->GetName(), 
			*StreamingLevel->GetWorldAssetPackageName());
	}
}

void ULevelStreamingProfilingSubsystem::OnStreamingLevelRemoved(UWorld* World, const ULevelStreaming* StreamingLevel)
{
	if (!bIsTracking)
	{
		return;
	}
	int32 NumRemoved = ActiveLevels.Remove(StreamingLevel);
	if (NumRemoved != 0)
	{
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s removed entirely"), 
			*StreamingLevel->GetName(), 
			*StreamingLevel->GetWorldAssetPackageName());
	}
}

void ULevelStreamingProfilingSubsystem::OnLevelStartedAsyncLoading(UWorld* World, const ULevelStreaming* StreamingLevel)
{
	if (!bIsTracking)
	{
		return;
	}

	TUniquePtr<FActiveLevel>& Level = ActiveLevels.FindOrAdd(StreamingLevel);
	if (!Level.IsValid())
	{
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s started loading while untracked."), 
			*StreamingLevel->GetWorldAssetPackageName());
		Level = MakeActiveLevel(StreamingLevel, ELevelState::QueuedForLoading);
	}

	if (Level->State != ELevelState::QueuedForLoading)
	{
		UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Streaming level %s started loading in unexpected state %s"),
			*StreamingLevel->GetWorldAssetPackageName(), EnumToString(Level->State));
	}
	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s started async loading"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	double Time = FPlatformTime::Seconds();
	LevelStats[Level->StatsIndex].bValid = true;
	LevelStats[Level->StatsIndex].TimeQueuedForLoading = Time - Level->StateStartTime;
	UpdateLevelState(*Level, StreamingLevel, ELevelState::Loading, FPlatformTime::Seconds());
}

void ULevelStreamingProfilingSubsystem::OnLevelFinishedAsyncLoading(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LoadedLevel)
{
	if (!bIsTracking)
	{
		return;
	}
	TUniquePtr<FActiveLevel>& Level = ActiveLevels.FindOrAdd(StreamingLevel);
	if (!Level.IsValid())
	{
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s finished loading while untracked."), 
			*StreamingLevel->GetWorldAssetPackageName());
		Level = MakeActiveLevel(StreamingLevel, ELevelState::Loading, LoadedLevel);
	}

	double Time = FPlatformTime::Seconds();
	if (Level->State == ELevelState::QueuedForLoading)
	{
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s finished async loading while queued - must have been found in memory."), 
			*StreamingLevel->GetName(), 
			*StreamingLevel->GetWorldAssetPackageName());
		// Fill in stats so we calculate the time til now as queued time, and loading time as 0
		LevelStats[Level->StatsIndex].bValid = true;
		LevelStats[Level->StatsIndex].TimeQueuedForLoading = Time - Level->StateStartTime;
		UpdateLevelState(*Level, StreamingLevel, ELevelState::Loading, Time);
	}
	else if (Level->State != ELevelState::Loading)
	{
		UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Streaming level %s finished loading in unexpected state %s"),
			*StreamingLevel->GetWorldAssetPackageName(), EnumToString(Level->State));
	}

	if (!LoadedLevel)
	{
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s cancelled or failed async loading"), 
			*StreamingLevel->GetName(), 
			*StreamingLevel->GetWorldAssetPackageName());
		LevelStats[Level->StatsIndex].TimeLoading = Time - Level->StateStartTime;
		LevelStats[Level->StatsIndex].bValid = true; // Record this as a cancelled load
		UpdateLevelState(*Level, StreamingLevel, ELevelState::None, Time);
		return;
	}

	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s finished async loading"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	LevelStats[Level->StatsIndex].PackageNameOnDisk = LoadedLevel->GetOutermost()->GetLoadedPath().GetPackageFName();
	LevelStats[Level->StatsIndex].PackageNameInMemory = LoadedLevel->GetOutermost()->GetFName();
	LevelStats[Level->StatsIndex].bValid = true;
	LevelStats[Level->StatsIndex].TimeLoading = Time - Level->StateStartTime;
	UpdateLevelState(*Level, StreamingLevel, ELevelState::Loaded, Time);
}

void ULevelStreamingProfilingSubsystem::OnLevelQueuedForAddToWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LoadedLevel)
{
	if (!bIsTracking)
	{
		return;
	}

	TUniquePtr<FActiveLevel>& Level = ActiveLevels.FindOrAdd(StreamingLevel);
	if (!Level.IsValid())
	{
		// Level loaded before we started profiling 
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s queued for adding to world while untracked."), 
			*StreamingLevel->GetWorldAssetPackageName());
		Level = MakeActiveLevel(StreamingLevel, ELevelState::Loaded, LoadedLevel);
	}

	if (Level->State == ELevelState::RemovedFromWorld)
	{
		// Streaming back in, make a new entry 
		int32 OldIndex = Level->StatsIndex;
		Level->StatsIndex = LevelStats.AddDefaulted();
		LevelStats[Level->StatsIndex] = LevelStats[OldIndex];
		LevelStats[Level->StatsIndex].TimeQueueudForAddToWorld = 0.0;
		LevelStats[Level->StatsIndex].TimeAddingToWorld = 0.0;
		LevelStats[Level->StatsIndex].TimeInWorld = 0.0;
		LevelStats[Level->StatsIndex].TimeQueuedForRemoveFromWorld = 0.0;
		LevelStats[Level->StatsIndex].TimeRemovingFromWorld = 0.0;
	}
	else if (Level->State != ELevelState::Loaded)
	{
		UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Streaming level %s queued for adding to world in unexpected state %s"),
			*StreamingLevel->GetWorldAssetPackageName(), EnumToString(Level->State));
	}

	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s queued for adding to world"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	double Time = FPlatformTime::Seconds();
	LevelStats[Level->StatsIndex].bValid = true;
	UpdateLevelState(*Level, StreamingLevel, ELevelState::QueuedForAddToWorld, Time);
}

void ULevelStreamingProfilingSubsystem::OnLevelUnqueuedForAddToWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LoadedLevel)
{
	if (!bIsTracking)
	{
		return;
	}

	TUniquePtr<FActiveLevel>& Level = ActiveLevels.FindOrAdd(StreamingLevel);
	if (!Level.IsValid())
	{
		// Level loaded before we started profiling 
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s unqueued for adding to world while untracked."), 
			*StreamingLevel->GetWorldAssetPackageName());
		Level = MakeActiveLevel(StreamingLevel, ELevelState::Loaded, LoadedLevel);
	}

	if (Level->State != ELevelState::QueuedForAddToWorld)
	{
		UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Streaming level %s unqueud for adding to world in unexpected state %s"),
			*StreamingLevel->GetWorldAssetPackageName(), EnumToString(Level->State));
	}

	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s unqueued for adding to world"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	double Time = FPlatformTime::Seconds();
	LevelStats[Level->StatsIndex].bValid = true;
	UpdateLevelState(*Level, StreamingLevel, ELevelState::Loaded, Time);
}

void ULevelStreamingProfilingSubsystem::OnLevelStartedAddToWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LoadedLevel)
{
	if (!bIsTracking || World != GetWorld())
	{
		return;
	}

	TUniquePtr<FActiveLevel>& Level = ActiveLevels.FindOrAdd(StreamingLevel);
	if (!Level.IsValid())
	{
		// Level loaded before we started profiling 
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s started adding to world while untracked."), 
			*StreamingLevel->GetWorldAssetPackageName());
		Level = MakeActiveLevel(StreamingLevel, ELevelState::QueuedForAddToWorld, LoadedLevel);
	}

	if (Level->State != ELevelState::QueuedForAddToWorld)
	{
		UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Streaming level %s started adding to world in unexpected state %s"),
			*StreamingLevel->GetWorldAssetPackageName(), EnumToString(Level->State));
	}

	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s started adding to world"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	double Time = FPlatformTime::Seconds();
	LevelStats[Level->StatsIndex].bValid = true;
	LevelStats[Level->StatsIndex].TimeQueueudForAddToWorld = Time - Level->StateStartTime; 
	UpdateLevelState(*Level, StreamingLevel, ELevelState::AddingToWorld, Time);
}

void ULevelStreamingProfilingSubsystem::OnLevelFinishedAddToWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LoadedLevel)
{
	if (!bIsTracking)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FortStreamingProfiling.OnLevelFinishedAddToWorld);

	TUniquePtr<FActiveLevel>& Level = ActiveLevels.FindOrAdd(StreamingLevel);
	if (!Level.IsValid())
	{
		// Level was streaming in before we started tracking 
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s finished adding to world while untracked."), 
			*StreamingLevel->GetWorldAssetPackageName());
		Level = MakeActiveLevel(StreamingLevel, ELevelState::AddingToWorld, LoadedLevel);
	}

	if (Level->State != ELevelState::AddingToWorld)
	{
		UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Streaming level %s finished adding to world in unexpected state %s"),
			*StreamingLevel->GetWorldAssetPackageName(), EnumToString(Level->State));
	}

	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s finished adding to world"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	double Time = FPlatformTime::Seconds();
	LevelStats[Level->StatsIndex].bValid = true;
	LevelStats[Level->StatsIndex].TimeAddingToWorld = Time - Level->StateStartTime;
	Level->TimeAddedToWorld = Time;

	if (World && World->GetWorldPartition())
	{
		FBox CellBounds = LevelStats[Level->StatsIndex].CellBounds;
		if (CellBounds.IsValid)
		{
			double MinDistance = MAX_dbl;
			MinDistance = MAX_dbl;
			FVector Location;
			for (const FWorldPartitionStreamingSource& Source : World->GetWorldPartition()->GetStreamingSources())
			{
				double Distance = CellBounds.ComputeSquaredDistanceToPoint(Source.Location);
				if (Distance < MinDistance)
				{ 
					MinDistance = Distance;
					Location = Source.Location;
				}
			}
			if (MinDistance != MAX_dbl)
			{
				UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s %s streamed in at distance %.0f from cell bounds"), 
					*StreamingLevel->GetName(), *StreamingLevel->GetWorldAssetPackageName(), MinDistance);
				LevelStats[Level->StatsIndex].FinalStreamInDistance_Cell = FMath::Sqrt(MinDistance);
				LevelStats[Level->StatsIndex].FinalStreamInLocation = Location;
			}
		}

		FBox ContentBounds = LevelStats[Level->StatsIndex].ContentBounds;
		if (ContentBounds.IsValid)
		{
			double MinDistance = MAX_dbl;
			for (const FWorldPartitionStreamingSource& Source : World->GetWorldPartition()->GetStreamingSources())
			{
				double Distance = ContentBounds.ComputeSquaredDistanceToPoint(Source.Location);
				MinDistance = FMath::Min(Distance, MinDistance);
			}
			if (MinDistance != MAX_dbl)
			{
				UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s %s streamed in at distance %.0f from content bounds"), 
					*StreamingLevel->GetName(), *StreamingLevel->GetWorldAssetPackageName(), MinDistance);
				LevelStats[Level->StatsIndex].FinalStreamInDistance_Content = FMath::Sqrt(MinDistance);
			}
		}
	}
	UpdateLevelState(*Level, StreamingLevel, ELevelState::AddedToWorld, Time);
}

void ULevelStreamingProfilingSubsystem::OnLevelQueuedForRemoveFromWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LoadedLevel)
{
	if (!bIsTracking)
	{
		return;
	}

	TUniquePtr<FActiveLevel>& Level = ActiveLevels.FindOrAdd(StreamingLevel);
	if (!Level.IsValid())
	{
		// Level loaded before we started profiling 
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s queued for removing from world while untracked."), 
			*StreamingLevel->GetWorldAssetPackageName());
		Level = MakeActiveLevel(StreamingLevel, ELevelState::AddedToWorld, LoadedLevel);
	}

	if (Level->State != ELevelState::AddedToWorld)
	{
		UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Streaming level %s queued for removing from world in unexpected state %s"),
			*StreamingLevel->GetWorldAssetPackageName(), EnumToString(Level->State));
	}

	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s queued for removing from world"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	double Time = FPlatformTime::Seconds();
	LevelStats[Level->StatsIndex].bValid = true;
	UpdateLevelState(*Level, StreamingLevel, ELevelState::QueuedForRemoveFromWorld, Time);
}

void ULevelStreamingProfilingSubsystem::OnLevelUnqueuedForRemoveFromWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LoadedLevel)
{
	if (!bIsTracking)
	{
		return;
	}

	TUniquePtr<FActiveLevel>& Level = ActiveLevels.FindOrAdd(StreamingLevel);
	if (!Level.IsValid())
	{
		// Level loaded before we started profiling 
		UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s unqueued for removing from world while untracked."), 
			*StreamingLevel->GetWorldAssetPackageName());
		Level = MakeActiveLevel(StreamingLevel, ELevelState::QueuedForRemoveFromWorld, LoadedLevel);
	}

	if (Level->State != ELevelState::QueuedForRemoveFromWorld)
	{
		UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Streaming level %s unqueued for removing from world in unexpected state %s"),
			*StreamingLevel->GetWorldAssetPackageName(), EnumToString(Level->State));
	}

	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s unqueued for removing from world"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	double Time = FPlatformTime::Seconds();
	LevelStats[Level->StatsIndex].bValid = true;
	UpdateLevelState(*Level, StreamingLevel, ELevelState::AddedToWorld, Time);
}

void ULevelStreamingProfilingSubsystem::OnLevelStartedRemoveFromWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel*)
{
	if (!bIsTracking || World != GetWorld())
	{
		return;
	}

	TUniquePtr<FActiveLevel>& Level = ActiveLevels.FindOrAdd(StreamingLevel);
	if (!Level.IsValid())
	{
		// Level streaming out that streamed in before we started profiling
		return;
	}

	if (Level->State != ELevelState::QueuedForRemoveFromWorld)
	{
		UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Streaming level %s started removing from world in unexpected state %s"),
			*StreamingLevel->GetWorldAssetPackageName(), EnumToString(Level->State));
	}

	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s started removing from world"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	double Time = FPlatformTime::Seconds();
	LevelStats[Level->StatsIndex].bValid = true;
	LevelStats[Level->StatsIndex].TimeQueuedForRemoveFromWorld = Time - Level->StateStartTime;
	if (Level->TimeAddedToWorld)
	{
		LevelStats[Level->StatsIndex].TimeInWorld = Time - Level->TimeAddedToWorld.GetValue(); // Not really a perf thing but may give an idea of churn 
	}
	UpdateLevelState(*Level, StreamingLevel, ELevelState::RemovingFromWorld, Time);
}

void ULevelStreamingProfilingSubsystem::OnLevelFinishedRemoveFromWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel*)
{
	if (!bIsTracking)
	{
		return;
	}

	TUniquePtr<FActiveLevel>& Level = ActiveLevels.FindOrAdd(StreamingLevel);
	if (!Level.IsValid())
	{
		// Level streaming out that streamed in before we started profiling
		return;
	}

	if (Level->State != ELevelState::RemovingFromWorld)
	{
		UE_LOG(LogLevelStreamingProfiling, Error, TEXT("Streaming level %s started removing from world in unexpected state %s"),
			*StreamingLevel->GetWorldAssetPackageName(), EnumToString(Level->State));
	}

	UE_LOG(LogLevelStreamingProfiling, Verbose, TEXT("Streaming level %s: %s finished removing from world"), 
		*StreamingLevel->GetName(), 
		*StreamingLevel->GetWorldAssetPackageName());
	double Time = FPlatformTime::Seconds();
	LevelStats[Level->StatsIndex].bValid = true;
	LevelStats[Level->StatsIndex].TimeRemovingFromWorld = Time - Level->StateStartTime; 
	UpdateLevelState(*Level, StreamingLevel, ELevelState::RemovedFromWorld, Time);
}
