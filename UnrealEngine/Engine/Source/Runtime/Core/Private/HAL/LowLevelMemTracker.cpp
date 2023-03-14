// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/LowLevelMemTracker.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER
#include "HAL/LowLevelMemStats.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "LowLevelMemTrackerPrivate.h"
#include "MemPro/MemProProfiler.h"
#include "Misc/CString.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "Misc/VarArgs.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Templates/Atomic.h"
#include "Trace/Trace.inl"



UE_TRACE_CHANNEL(MemTagChannel, "Memory overview", true)

UE_TRACE_EVENT_BEGIN(LLM, TagsSpec, NoSync|Important)
	UE_TRACE_EVENT_FIELD(const void*, TagId)
	UE_TRACE_EVENT_FIELD(const void*, ParentId)
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, Name)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(LLM, TrackerSpec, NoSync|Important)
	UE_TRACE_EVENT_FIELD(uint8, TrackerId)
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, Name)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(LLM, TagValue)
	UE_TRACE_EVENT_FIELD(uint8, TrackerId)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(const void*[], Tags)
	UE_TRACE_EVENT_FIELD(int64[], Values)
UE_TRACE_EVENT_END()

#define LLM_CSV_PROFILER_WRITER_ENABLED CSV_PROFILER 

#if LLM_CSV_PROFILER_WRITER_ENABLED
	CSV_DEFINE_CATEGORY(LLM, true);
	CSV_DEFINE_CATEGORY(LLMPlatform, true);
#endif

TAutoConsoleVariable<int32> CVarLLMTrackPeaks(TEXT("LLM.TrackPeaks"), 0, TEXT("Track peak memory in each category since process start rather than current frame's value."));

TAutoConsoleVariable<int32> CVarLLMWriteInterval(
	TEXT("LLM.LLMWriteInterval"),
	1,
	TEXT("The number of seconds between each line in the LLM csv (zero to write every frame)")
);

TAutoConsoleVariable<int32> CVarLLMHeaderMaxSize(
	TEXT("LLM.LLMHeaderMaxSize"),
#if LLM_ALLOW_ASSETS_TAGS
	500000, // When using asset tags, you will have MANY more LLM titles since so many are auto generated.
#else
	5000,
#endif
	TEXT("The maximum total number of characters allowed for all of the LLM titles")
);

FAutoConsoleCommand LLMSnapshot(
	TEXT("LLMSnapshot"), 
	TEXT("Takes a single LLM Snapshot of one frame. This command requires the commandline -llmdisableautopublish"), 
	FConsoleCommandDelegate::CreateLambda([]() 
	{
		FLowLevelMemTracker::Get().PublishDataSingleFrame();
	}));

FAutoConsoleCommand DumpLLM(
	TEXT("DumpLLM"),
	TEXT("Logs out the current and peak sizes of all tracked LLM tags"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* InWorld, FOutputDevice& Ar)
	{
		bool bCSV = false;
		for (const FString& Arg : Args)
		{
			if (FParse::Param(*Arg, TEXT("CSV")))
			{
				bCSV = true;
			}
		}
		FLowLevelMemTracker::Get().DumpToLog(bCSV ? FLowLevelMemTracker::EDumpFormat::CSV : FLowLevelMemTracker::EDumpFormat::PlainText, &Ar);
	}));

void FLowLevelMemTracker::DumpToLog(EDumpFormat DumpFormat, FOutputDevice* OutputDevice)
{
	if (!IsEnabled())
	{
		return;
	}
	const float InvToMb = 1.0 / (1024 * 1024);
	FOutputDevice& Ar = *(OutputDevice ? OutputDevice : GLog);
	if (DumpFormat == EDumpFormat::CSV)
	{
		Ar.Logf(TEXT(",TagName,SizeMB,PeakMB,Tracker,PathName"));
	}
	else
	{
		Ar.Logf(TEXT("%40s %12s %12s  Tracker PathName"), TEXT("TagName"), TEXT("SizeMB"), TEXT("PeakMB"));
	}

	for (ELLMTracker TrackerType : {ELLMTracker::Default, ELLMTracker::Platform})
	{
		const TCHAR* TrackerName = TrackerType == ELLMTracker::Default ? TEXT("Default") : TEXT("Platform");

		struct FTagLine
		{
			FString TagName;
			FString Line;
			int64 Size;
		};
		TArray<FTagLine> TagLines;
		for (const UE::LLMPrivate::FTagData* TagData : GetTrackedTags(TrackerType))
		{
			int64 CurrentAmount = GetTagAmountForTracker(TrackerType, TagData, false /* bPeakAmount */);
			int64 PeakAmount = GetTagAmountForTracker(TrackerType, TagData, true /* bPeakAmount */);
			FString TagName = GetTagUniqueName(TagData).ToString();
			FString TagLine;
			if (DumpFormat == EDumpFormat::CSV)
			{
				TagLine = FString::Printf(TEXT(",%s,%.3f,%.3f,%s,%s"),
					*TagName,
					static_cast<float>(CurrentAmount) * InvToMb,
					static_cast<float>(PeakAmount) * InvToMb,
					TrackerName,
					*GetTagDisplayPathName(TagData));
			}
			else
			{
				TagLine = FString::Printf(TEXT("%40s %12.3f %12.3f %8s %s"),
					*TagName,
					static_cast<float>(CurrentAmount) * InvToMb,
					static_cast<float>(PeakAmount) * InvToMb,
					TrackerName,
					*GetTagDisplayPathName(TagData));
			}
			TagLines.Add(FTagLine{ MoveTemp(TagName), MoveTemp(TagLine), CurrentAmount });
		}
		TagLines.Sort([](const FTagLine& A, const FTagLine& B)
			{
				if (A.Size != B.Size)
				{
					return A.Size > B.Size;
				}
				return A.TagName < B.TagName;
			});
		for (FTagLine& TagLine : TagLines)
		{
			Ar.Logf(TEXT("%s"), *TagLine.Line);
		}
	}
}

DECLARE_LLM_MEMORY_STAT(TEXT("LLM Overhead"), STAT_LLMOverheadTotal, STATGROUP_LLMOverhead);

/*
* LLM Summary stats referenced by ELLMTagNames
*/
DEFINE_STAT(STAT_EngineSummaryLLM);
DEFINE_STAT(STAT_ProjectSummaryLLM);
DEFINE_STAT(STAT_NetworkingSummaryLLM);
DEFINE_STAT(STAT_AudioSummaryLLM);
DEFINE_STAT(STAT_TrackedTotalSummaryLLM);
DEFINE_STAT(STAT_MeshesSummaryLLM);
DEFINE_STAT(STAT_PhysicsSummaryLLM);
DEFINE_STAT(STAT_PhysXSummaryLLM);
DEFINE_STAT(STAT_ChaosSummaryLLM);
DEFINE_STAT(STAT_UObjectSummaryLLM);
DEFINE_STAT(STAT_AnimationSummaryLLM);
DEFINE_STAT(STAT_StaticMeshSummaryLLM);
DEFINE_STAT(STAT_MaterialsSummaryLLM);
DEFINE_STAT(STAT_ParticlesSummaryLLM);
DEFINE_STAT(STAT_NiagaraSummaryLLM);
DEFINE_STAT(STAT_UISummaryLLM);
DEFINE_STAT(STAT_NavigationSummaryLLM);
DEFINE_STAT(STAT_TexturesSummaryLLM);
DEFINE_STAT(STAT_MediaStreamingSummaryLLM);

/*
 * LLM stats referenced by ELLMTagNames
 */
DECLARE_LLM_MEMORY_STAT(TEXT("Total"), STAT_TotalLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Untracked"), STAT_UntrackedLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Total"), STAT_PlatformTotalLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Tracked Total"), STAT_TrackedTotalLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Untagged"), STAT_UntaggedTotalLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("WorkingSetSize"), STAT_WorkingSetSizeLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PagefileUsed"), STAT_PagefileUsedLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Tracked Total"), STAT_PlatformTrackedTotalLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Untagged"), STAT_PlatformUntaggedTotalLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Untracked"), STAT_PlatformUntrackedLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Overhead"), STAT_PlatformOverheadLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("OS Available"), STAT_PlatformOSAvailableLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("FMalloc"), STAT_FMallocLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("FMalloc Unused"), STAT_FMallocUnusedLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ThreadStack"), STAT_ThreadStackLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ThreadStackPlatform"), STAT_ThreadStackPlatformLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Program Size"), STAT_ProgramSizePlatformLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Program Size"), STAT_ProgramSizeLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("OOM Backup Pool"), STAT_OOMBackupPoolPlatformLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("OOM Backup Pool"), STAT_OOMBackupPoolLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("GenericPlatformMallocCrash"), STAT_GenericPlatformMallocCrashLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("GenericPlatformMallocCrash"), STAT_GenericPlatformMallocCrashPlatformLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Engine Misc"), STAT_EngineMiscLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("TaskGraph Misc Tasks"), STAT_TaskGraphTasksMiscLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Linear Allocator"), STAT_LinearAllocatorLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Audio"), STAT_AudioLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioMisc"), STAT_AudioMiscLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioSoundWaves"), STAT_AudioSoundWavesLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioSoundWaveProxies"), STAT_AudioSoundWaveProxiesLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioMixer"), STAT_AudioMixerLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioMixerPlugins"), STAT_AudioMixerPluginsLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioPrecache"), STAT_AudioPrecacheLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioDecompress"), STAT_AudioDecompressLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioRealtimePrecache"), STAT_AudioRealtimePrecacheLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioFullDecompress"), STAT_AudioFullDecompressLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioStreamCache"), STAT_AudioStreamCacheLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioStreamCacheCompressedData"), STAT_AudioStreamCacheCompressedDataLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AudioSynthesis"), STAT_AudioSynthesisLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("RealTimeCommunications"), STAT_RealTimeCommunicationsLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("FName"), STAT_FNameLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Networking"), STAT_NetworkingLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Meshes"), STAT_MeshesLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Stats"), STAT_StatsLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Shaders"), STAT_ShadersLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PSO"), STAT_PSOLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Textures"), STAT_TexturesLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("TextureMetaData"), STAT_TextureMetaDataLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("VirtualTextureSystem"), STAT_VirtualTextureSystemLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Render Targets"), STAT_RenderTargetsLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("SceneRender"), STAT_SceneRenderLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("RHIMisc"), STAT_RHIMiscLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysX TriMesh"), STAT_PhysXTriMeshLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysX ConvexMesh"), STAT_PhysXConvexMeshLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AsyncLoading"), STAT_AsyncLoadingLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("UObject"), STAT_UObjectLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Animation"), STAT_AnimationLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("StaticMesh"), STAT_StaticMeshLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Materials"), STAT_MaterialsLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Particles"), STAT_ParticlesLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Niagara"), STAT_NiagaraLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("GPUSort"), STAT_GPUSortLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("GC"), STAT_GCLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("UI"), STAT_UILLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("NavigationRecast"), STAT_NavigationRecastLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Physics"), STAT_PhysicsLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysX"), STAT_PhysXLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysXGeometry"), STAT_PhysXGeometryLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysXLandscape"), STAT_PhysXLandscapeLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysXTrimesh"), STAT_PhysXTrimeshLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysXConvex"), STAT_PhysXConvexLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("PhysXAllocator"), STAT_PhysXAllocatorLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Chaos"), STAT_ChaosLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosGeometry"), STAT_ChaosGeometryLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosAcceleration"), STAT_ChaosAccelerationLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosParticles"), STAT_ChaosParticlesLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosLandscape"), STAT_ChaosLandscapeLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosTrimesh"), STAT_ChaosTrimeshLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosConvex"), STAT_ChaosConvexLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosScene"), STAT_ChaosSceneLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosUpdate"), STAT_ChaosUpdateLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosActor"), STAT_ChaosActorLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosBody"), STAT_ChaosBodyLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosConstraint"), STAT_ChaosConstraintLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ChaosMaterial"), STAT_ChaosMaterialLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("EnginePreInit"), STAT_EnginePreInitLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("EngineInit"), STAT_EngineInitLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Rendering Thread"), STAT_RenderingThreadLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("LoadMap Misc"), STAT_LoadMapMiscLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("StreamingManager"), STAT_StreamingManagerLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Graphics"), STAT_GraphicsPlatformLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("FileSystem"), STAT_FileSystemLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Localization"), STAT_LocalizationLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("AssetRegistry"), STAT_AssetRegistryLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ConfigSystem"), STAT_ConfigSystemLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("InitUObject"), STAT_InitUObjectLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("VideoRecording"), STAT_VideoRecordingLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Replays"), STAT_ReplaysLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("CsvProfiler"), STAT_CsvProfilerLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("MaterialInstance"), STAT_MaterialInstanceLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("SkeletalMesh"), STAT_SkeletalMeshLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("InstancedMesh"), STAT_InstancedMeshLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Landscape"), STAT_LandscapeLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("MediaStreaming"), STAT_MediaStreamingLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("ElectraPlayer"), STAT_ElectraPlayerLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("WMFPlayer"), STAT_WMFPlayerLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("MMIO"), STAT_PlatformMMIOLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("VirtualMemory"), STAT_PlatformVMLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("CustomName"), STAT_CustomName, STATGROUP_LLMFULL);

namespace UE
{
namespace LLMPrivate
{
	/**
	 * FLLMCsvWriter: class for writing out the LLM tag sizes to a csv file every few seconds
	 */
	class FLLMCsvWriter
	{
	public:
		FLLMCsvWriter();
		~FLLMCsvWriter();

		void SetTracker(ELLMTracker InTracker);
		void Clear();

		void Publish(FLowLevelMemTracker& LLMRef, const FTrackerTagSizeMap& TagSizes, const FTagData* OverrideTrackedTotalTagData, const FTagData* OverrideUntaggedTagData, int64 TrackedTotal, bool bTrackPeaks);

		void OnPreFork();

	private:
		void Write(FStringView Text);
		static const TCHAR* GetTrackerCsvName(ELLMTracker InTracker);

		bool CreateArchive();
		bool UpdateColumns(const FTrackerTagSizeMap& TagSizes);
		void WriteHeader(const FTagData* OverrideTrackedTotalTagData, const FTagData* OverrideUntaggedTagData);
		void AddRow(FLowLevelMemTracker& LLMRef, const FTrackerTagSizeMap& TagSizes, const FTagData* OverrideTrackedTotalName, const FTagData* OverrideUntaggedName, int64 TrackedTotal, bool bTrackPeaks);

		FConstTagDataArray Columns;
		TFastPointerLLMSet<const FTagData*> ExistingColumns;
		FArchive* Archive;
		double LastWriteTime;
		int32 WriteCount;
		ELLMTracker Tracker;
	};

	/**
	 * Outputs the LLM tags and sizes to TraceLog events.
	 */
	class FLLMTraceWriter
	{
	public:
		FLLMTraceWriter();
		void SetTracker(ELLMTracker InTracker);
		void Clear();
		void Publish(FLowLevelMemTracker& LLMRef, const FTrackerTagSizeMap& TagSizes, const FTagData* OverrideTrackedTotalName, const FTagData* OverrideUntaggedName, int64 TrackedTotal, bool bTrackPeaks);
		static void TraceGenericTags(FLowLevelMemTracker& LLMRef);

	private:
		static const void* GetTagId(const FTagData* TagData);

		ELLMTracker				Tracker;
		TFastPointerLLMSet<const FTagData*> DeclaredTags;
		bool					bTrackerSpecSent = false;
	};

	/**
	 * FLLMCsvWriter: class for writing out LLM stats to the Csv Profiler
	 */
	class FLLMCsvProfilerWriter
	{
	public:
		FLLMCsvProfilerWriter();
		void Clear();
		void SetTracker(ELLMTracker InTracker);
		void Publish(FLowLevelMemTracker& LLMRef, const FTrackerTagSizeMap& TagSizes, const FTagData* OverrideTrackedTotalName, const FTagData* OverrideUntaggedName, int64 TrackedTotal, bool bTrackPeaks);
	protected:
		void RecordTagToCsv(int32 CsvCategoryIndex, const FTagData* TagData, int64 TagSize);
#if LLM_CSV_PROFILER_WRITER_ENABLED 
		ELLMTracker Tracker;
		TFastPointerLLMMap<const FTagData*, FName> TagDataToCsvStatName;
#endif
	};


	/** per thread state in an LLMTracker */
	class FLLMThreadState
	{
	public:
		FLLMThreadState();

		void Clear();

		void PushTag(const FTagData* TagData);
		void PopTag();
		const FTagData* GetTopTag();
#if LLM_ALLOW_ASSETS_TAGS
		void PushAssetTag(const FTagData* TagData);
		void PopAssetTag();
		const FTagData* GetTopAssetTag();
#endif
		void TrackAllocation(const void* Ptr, int64 Size, ELLMTracker Tracker, ELLMAllocType AllocType, const FTagData* TagData, const FTagData* AssetTagData, bool bTrackInMemPro);
		void TrackFree(const void* Ptr, int64 Size, ELLMTracker Tracker, ELLMAllocType AllocType, const FTagData* TagData, const FTagData* AssetTagData, bool bTrackInMemPro);
		void TrackMoved(const void* Dest, const void* Source, int64 Size, ELLMTracker Tracker, const FTagData* TagData);
		void IncrTag(const FTagData* Tag, int64 Amount);

		void PropagateChildSizesToParents();
		void OnTagsResorted(FTagDataArray& OldTagDatas);
		void LockTags(bool bLock);
		void FetchAndClearTagSizes(FTrackerTagSizeMap& TagSizes, int64* InAllocTypeAmounts, bool bTrimAllocations);

		void ClearAllocTypeAmounts();

		FConstTagDataArray TagStack;
#if LLM_ALLOW_ASSETS_TAGS
		FConstTagDataArray AssetTagStack;
#endif

		FThreadTagSizeMap Allocations;
		FCriticalSection TagSection;

		int8 PausedCounter[(int32)ELLMAllocType::Count];
		int64 AllocTypeAmounts[(int32)ELLMAllocType::Count];
	};

	/*
	 * this is really the main LLM class. It owns the thread state objects.
	 */
	class FLLMTracker
	{
	public:

		FLLMTracker(FLowLevelMemTracker& InLLM);
		~FLLMTracker();

		void Initialise(ELLMTracker Tracker, FLLMAllocator* InAllocator);

		void PushTag(ELLMTag EnumTag);
		void PushTag(FName Tag, bool bInIsStatTag);
		void PushTag(const FTagData* TagData);
		void PopTag();
#if LLM_ALLOW_ASSETS_TAGS
		void PushAssetTag(FName Tag);
		void PushAssetTag(const FTagData* Tag);
		void PopAssetTag();
#endif
		void TrackAllocation(const void* Ptr, int64 Size, FName DefaultTag, ELLMTracker Tracker, ELLMAllocType AllocType, bool bTrackInMemPro);
		void TrackAllocation(const void* Ptr, int64 Size, ELLMTag DefaultTag, ELLMTracker Tracker, ELLMAllocType AllocType, bool bTrackInMemPro);
		void TrackFree(const void* Ptr, ELLMTracker Tracker, ELLMAllocType AllocType, bool bTrackInMemPro);
		void OnAllocMoved(const void* Dest, const void* Source, ELLMTracker Tracker, ELLMAllocType AllocType);

		void TrackMemory(ELLMTag EnumTag, int64 Amount, ELLMAllocType AllocType);
		void TrackMemory(FName TagName, int64 Amount, ELLMAllocType AllocType);
		void TrackMemory(const FTagData* TagData, int64 Amount, ELLMAllocType AllocType);

		// This will pause/unpause tracking, and also manually increment a given tag
		void PauseAndTrackMemory(FName TagName, bool bInIsStatTag, int64 Amount, ELLMAllocType AllocType);
		void PauseAndTrackMemory(ELLMTag EnumTag, int64 Amount, ELLMAllocType AllocType);
		void PauseAndTrackMemory(const FTagData* TagData, int64 Amount, ELLMAllocType AllocType);
		void Pause(ELLMAllocType AllocType);
		void Unpause(ELLMAllocType AllocType);
		bool IsPaused(ELLMAllocType AllocType);

		void Clear();

		void PublishStats(bool bTrackPeaks);
		void PublishCsv(bool bTrackPeaks);
		void PublishTrace(bool bTrackPeaks);
		void PublishCsvProfiler(bool bTrackPeaks);

		void OnPreFork();

		struct FLowLevelAllocInfo
		{
		public:
			void SetTag(const FTagData* InTag, FLowLevelMemTracker& InLLMRef)
			{
#if LLM_ENABLED_FULL_TAGS
				Tag = InTag->GetIndex();
#else
				Tag = InTag->GetEnumTag();
#endif
			}

			const FTagData* GetTag(FLowLevelMemTracker& InLLMRef) const
			{
#if LLM_ENABLED_FULL_TAGS
				FReadScopeLock TagDataScopeLock(InLLMRef.TagDataLock);
				return (*InLLMRef.TagDatas)[Tag];
#else
				return InLLMRef.FindTagData(Tag);
#endif
			}

#if LLM_ALLOW_ASSETS_TAGS
			void SetAssetTag(const FTagData* InTag, FLowLevelMemTracker& InLLMRef)
			{
				AssetTag = InTag;
			}

			const FTagData* GetAssetTag(FLowLevelMemTracker& InLLMRef) const
			{
				return AssetTag;
			}
#endif

#if LLM_ENABLED_FULL_TAGS
			int32 GetCompressedTag() { return Tag; }
			void SetCompressedTag(int32 InTag) { Tag = InTag; }
#else
			ELLMTag GetCompressedTag() { return Tag; }
			void SetCompressedTag(ELLMTag InTag) { Tag = InTag; }
#endif

		private:
#if LLM_ALLOW_ASSETS_TAGS
			const FTagData* AssetTag = nullptr;
#endif
#if LLM_ENABLED_FULL_TAGS
			// Even with arbitrary tags we are still partially compressed - the allocation records the tag's index (4 bytes) rather than the full tag pointer (8 bytes) or the tag's name (12 bytes)
			int32 Tag = 0;
#else
			ELLMTag Tag = ELLMTag::Untagged;
#endif
		};

		typedef LLMMap<PointerKey, uint32, FLowLevelAllocInfo, LLMNumAllocsType> FLLMAllocMap;	// pointer, size, info, Capacity SizeType

		void SetTotalTags(const FTagData* InOverrideUntaggedTagData, const FTagData* InOverrideTrackedTotalTagData);
		void Update();
		void UpdateThreads();
		void OnTagsResorted(FTagDataArray& OldTagDatas);
		void LockAllThreadTags(bool bLock);

		int64 GetTagAmount(const FTagData* TagData, bool bPeakAmount = false) const;
		void SetTagAmountExternal(const FTagData* TagData, int64 Amount, bool bAddToTotal);
		void SetTagAmountInUpdate(const FTagData* TagData, int64 Amount, bool bAddToTotal);
		const FTagData* GetActiveTagData();
		const FTagData* FindTagForPtr(void* Ptr);
		TArray<const FTagData*> GetTagDatas();

		int64 GetAllocTypeAmount(ELLMAllocType AllocType);

		int64 GetTrackedTotal() const
		{
			return TrackedTotal;
		}

	protected:

		FLLMThreadState* GetOrCreateState();
		FLLMThreadState* GetState();
		void TrackAllocation(const void* Ptr, int64 Size, const FTagData* ActiveTagData, ELLMTracker Tracker, ELLMAllocType AllocType, FLLMThreadState* State, bool bTrackInMemPro);

		FLowLevelMemTracker& LLMRef;

		uint32 TlsSlot;

		TArray<FLLMThreadState*, FDefaultLLMAllocator> ThreadStates;

		FCriticalSection PendingThreadStatesGuard;
		TArray<FLLMThreadState*, FDefaultLLMAllocator> PendingThreadStates;

		int64 TrackedTotal GCC_ALIGN(8);

		FLLMAllocMap AllocationMap;

		FTrackerTagSizeMap TagSizes;

		const FTagData* OverrideUntaggedTagData;
		const FTagData* OverrideTrackedTotalTagData;

		FLLMCsvWriter CsvWriter;
		FLLMTraceWriter TraceWriter;
		FLLMCsvProfilerWriter CsvProfilerWriter;

		double LastTrimTime;

		int64 AllocTypeAmounts[(int32)ELLMAllocType::Count];
	};

	const TCHAR* ToString(ETagReferenceSource ReferenceSource);
	void SetMemoryStatByFName(FName Name, int64 Amount);
	void ValidateUniqueName(FStringView UniqueName);
}
}

static FName GetTagName_CustomName()
{
	// This FName can be read before c++ global constructors are complete, by LLM_SCOPE_BY_BOOTSTRAP_TAG
	// so it needs to be a function static rather than a global.
	static FName TagName_CustomName(TEXT("CustomName"));
	return TagName_CustomName;
}
static FName TagName_Untagged(TEXT("Untagged"));

FName LLMGetTagUniqueName(ELLMTag Tag)
{
#define LLM_TAG_NAME_ARRAY(Enum,Str,Stat,Group,ParentTag) FName(TEXT(#Enum)),
	static const FName UniqueNames[] = { LLM_ENUM_GENERIC_TAGS(LLM_TAG_NAME_ARRAY) };
#undef LLM_TAG_NAME_ARRAY

	int32 Index = static_cast<int32>(Tag);
	if (Index < 0)
	{
		return NAME_None;
	}
	else if (Index < UE_ARRAY_COUNT(UniqueNames))
	{
		return UniqueNames[Index];
	}
	else if (Index < LLM_CUSTOM_TAG_START)
	{
		return NAME_None;
	}
	else if (Index <= LLM_CUSTOM_TAG_END)
	{
		static FName CustomNames[LLM_CUSTOM_TAG_COUNT];
		static bool bCustomNamesInitialized = false;
		if (!bCustomNamesInitialized)
		{
			TStringBuilder<256> UniqueNameBuffer;

			for (int32 CreateIndex = LLM_CUSTOM_TAG_START; CreateIndex <= LLM_CUSTOM_TAG_END; ++CreateIndex)
			{
				UniqueNameBuffer.Reset();
				UniqueNameBuffer.Appendf(TEXT("ELLMTag%d"), CreateIndex);
				CustomNames[CreateIndex - LLM_CUSTOM_TAG_START] = FName(UniqueNameBuffer);
			}
		}
		return CustomNames[Index - LLM_CUSTOM_TAG_START];
	}
	else
	{
		return NAME_None;
	}
}

extern const TCHAR* LLMGetTagName(ELLMTag Tag)
{
#define LLM_TAG_NAME_ARRAY(Enum,Str,Stat,Group,ParentTag) TEXT(Str),
	static TCHAR const* Names[] = { LLM_ENUM_GENERIC_TAGS(LLM_TAG_NAME_ARRAY) };
#undef LLM_TAG_NAME_ARRAY

	int32 Index = static_cast<int32>(Tag);
	if (Index >= 0 && Index < UE_ARRAY_COUNT(Names))
	{
		return Names[Index];
	}
	else
	{
		return nullptr;
	}
}

extern const ANSICHAR* LLMGetTagNameANSI(ELLMTag Tag)
{
#define LLM_TAG_NAME_ARRAY(Enum,Str,Stat,Group,ParentTag) Str,
	static ANSICHAR const* Names[] = { LLM_ENUM_GENERIC_TAGS(LLM_TAG_NAME_ARRAY) };
#undef LLM_TAG_NAME_ARRAY

	int32 Index = static_cast<int32>(Tag);
	if (Index >= 0 && Index < UE_ARRAY_COUNT(Names))
	{
		return Names[Index];
	}
	else
	{
		return nullptr;
	}
}

extern FName LLMGetTagStat(ELLMTag Tag)
{
#define LLM_TAG_STAT_ARRAY(Enum,Str,Stat,Group,ParentTag) Stat,
	static FName Names[] = { LLM_ENUM_GENERIC_TAGS(LLM_TAG_STAT_ARRAY) };
#undef LLM_TAG_STAT_ARRAY

	int32 Index = static_cast<int32>(Tag);
	if (Index >= 0 && Index < UE_ARRAY_COUNT(Names))
	{
		return Names[Index];
	}
	else
	{
		return NAME_None;
	}
}

extern FName LLMGetTagStatGroup(ELLMTag Tag)
{
#define LLM_TAG_STATGROUP_ARRAY(Enum,Str,Stat,Group,ParentTag) Group,
	static FName Names[] = { LLM_ENUM_GENERIC_TAGS(LLM_TAG_STATGROUP_ARRAY) };
#undef LLM_TAG_STAT_ARRAY

	int32 Index = static_cast<int32>(Tag);
	if (Index >= 0 && Index < UE_ARRAY_COUNT(Names))
	{
		return Names[Index];
	}
	else
	{
		return NAME_None;
	}
}

FLowLevelMemTracker& FLowLevelMemTracker::Construct()
{
	static FLowLevelMemTracker Tracker;
	TrackerInstance = &Tracker;
	return Tracker;
}

bool FLowLevelMemTracker::IsEnabled()
{
	return !bIsDisabled;
}

FLowLevelMemTracker* FLowLevelMemTracker::TrackerInstance = nullptr;
bool FLowLevelMemTracker::bIsDisabled = false; // must start off enabled because allocations happen before the command line enables/disables us

static const TCHAR* InvalidLLMTagName = TEXT("?");

FLowLevelMemTracker::FLowLevelMemTracker()
	: TagDatas(nullptr)
	, TagDataNameMap(nullptr)
	, TagDataEnumMap(nullptr)
	, ProgramSize(0)
	, MemoryUsageCurrentOverhead(0)
	, MemoryUsagePlatformTotalUntracked(0)
	, bFirstTimeUpdating(true)
	, bCanEnable(true)
	, bCsvWriterEnabled(false)
	, bTraceWriterEnabled(false)
	, bInitialisedTracking(false)
	, bIsBootstrapping(false)
	, bFullyInitialised(false)
	, bConfigurationComplete(false)
	, bTagAdded(false)
	, bAutoPublish(true)
	, bPublishSingleFrame(false)
{
	using namespace UE::LLMPrivate;

	// set the alloc functions
	LLMAllocFunction PlatformLLMAlloc = NULL;
	LLMFreeFunction PlatformLLMFree = NULL;
	int32 Alignment = 0;
	if (!FPlatformMemory::GetLLMAllocFunctions(PlatformLLMAlloc, PlatformLLMFree, Alignment))
	{
		bIsDisabled = true;
		bCanEnable = false;
		bConfigurationComplete = true;
		return;
	}
	LLMCheck(FMath::IsPowerOfTwo(Alignment));

	Allocator.Initialise(PlatformLLMAlloc, PlatformLLMFree, Alignment);
	FLLMAllocator::Get() = &Allocator;

	// only None is on by default
	for (int32 Index = 0; Index < static_cast<int32>(ELLMTagSet::Max); Index++)
	{
		ActiveSets[Index] = Index == static_cast<int32>(ELLMTagSet::None);
	}
}

FLowLevelMemTracker::~FLowLevelMemTracker()
{
	using namespace UE::LLMPrivate;

	bIsDisabled = true;
	Clear();
	FLLMAllocator::Get() = nullptr;
}

void FLowLevelMemTracker::BootstrapInitialise()
{
	using namespace UE::LLMPrivate;
	if (bInitialisedTracking)
	{
		return;
	}
	bInitialisedTracking = true;

	for (int32 TrackerIndex = 0; TrackerIndex < static_cast<int32>(ELLMTracker::Max); ++TrackerIndex)
	{
		FLLMTracker* Tracker = Allocator.New<FLLMTracker>(*this);

		Trackers[TrackerIndex] = Tracker;

		Tracker->Initialise(static_cast<ELLMTracker>(TrackerIndex), &Allocator);
	}

	BootstrapTagDatas();
	static_assert((uint8)ELLMTracker::Max == 2, "You added a tracker, without updating FLowLevelMemTracker::BootstrapInitialise (and probably need to update macros)");
	GetTracker(ELLMTracker::Platform)->SetTotalTags(FindOrAddTagData(ELLMTag::PlatformUntaggedTotal), FindOrAddTagData(ELLMTag::PlatformTrackedTotal));
	GetTracker(ELLMTracker::Default)->SetTotalTags(FindOrAddTagData(ELLMTag::UntaggedTotal), FindOrAddTagData(ELLMTag::TrackedTotal));


	// calculate program size early on... the platform can call SetProgramSize later if it sees fit
	InitialiseProgramSize();
}

void FLowLevelMemTracker::Clear()
{
	using namespace UE::LLMPrivate;

	if (!bInitialisedTracking)
	{
		return;
	}

	LLMCheck(bIsDisabled); // tracking must be stopped at this point or it will crash while tracking its own destruction
	for (int32 TrackerIndex = 0; TrackerIndex < static_cast<int32>(ELLMTracker::Max); TrackerIndex++)
	{
		GetTracker((ELLMTracker)TrackerIndex)->Clear();
		Allocator.Delete(Trackers[TrackerIndex]);
		Trackers[TrackerIndex] = nullptr;
	}

	ClearTagDatas();
	Allocator.Clear();
	bFullyInitialised = false;
	bInitialisedTracking = false;
}

void FLowLevelMemTracker::OnPreFork()
{
	using namespace UE::LLMPrivate;

	if (!bIsDisabled)
	{
		FLLMTracker& DefaultTracker = *GetTracker(ELLMTracker::Default);
		FLLMTracker& PlatformTracker = *GetTracker(ELLMTracker::Platform);

		DefaultTracker.OnPreFork();
		PlatformTracker.OnPreFork();
	}
}

void FLowLevelMemTracker::UpdateStatsPerFrame(const TCHAR* LogName)
{
	if (bIsDisabled)
	{
		if (bFirstTimeUpdating)
		{
			// UpdateStatsPerFrame is usually called from the game thread, but can sometimes be called from the async loading thread, so enter a lock for it
			FScopeLock UpdateScopeLock(&UpdateLock);
			if (bFirstTimeUpdating)
			{
				// Write the saved overhead value to the stats system; this allows us to see the overhead that is always there even when disabled
				// (unless the #define completely removes support, of course)
				bFirstTimeUpdating = false;
				// Don't call Update since we have cleared the Trackers by this point. But do publish the recorded values
				PublishDataPerFrame(LogName);
			}
		}
		return;
	}

	// UpdateStatsPerFrame is usually called from the game thread, but can sometimes be called from the async loading thread, so enter a lock for it
	FScopeLock UpdateScopeLock(&UpdateLock);
	BootstrapInitialise();

	if (bFirstTimeUpdating)
	{
		// Nothing needed here yet
		UE_LOG(LogInit, Log, TEXT("First time updating LLM stats..."));
		bFirstTimeUpdating = false;
	}
	TickInternal();

	if (bAutoPublish || bPublishSingleFrame)
	{
		PublishDataPerFrame(LogName);
		bPublishSingleFrame = false;
	}
}

void FLowLevelMemTracker::Tick()
{
	if (bIsDisabled)
	{
		return;
	}
	// TickInternal is usually called from the game thread, but can sometimes be called from the async loading thread, so enter a lock for it
	FScopeLock UpdateScopeLock(&UpdateLock);
	TickInternal();
}

void FLowLevelMemTracker::TickInternal()
{
	using namespace UE::LLMPrivate;

	if (bFullyInitialised)
	{
		// We call tick when not fully initialised to get the overhead when disabled. When not initialised, we have to avoid the portion of the tick that uses tags.

		// get the platform to update any custom tags; this must be done before the aggregation that occurs in GetTracker()->Update
		FPlatformMemory::UpdateCustomLLMTags();

		UpdateTags();

		// update the trackers
		for (int32 TrackerIndex = 0; TrackerIndex < static_cast<int32>(ELLMTracker::Max); TrackerIndex++)
		{
			GetTracker((ELLMTracker)TrackerIndex)->Update();
		}
	}
	FLLMTracker& DefaultTracker = *Trackers[static_cast<int32>(ELLMTracker::Default)];
	FLLMTracker& PlatformTracker = *Trackers[static_cast<int32>(ELLMTracker::Platform)];

	// calculate FMalloc unused stat and set it in the Default tracker
	int64 FMallocAmount = DefaultTracker.GetAllocTypeAmount(ELLMAllocType::FMalloc);
	int64 FMallocPlatformAmount = PlatformTracker.GetTagAmount(FindOrAddTagData(ELLMTag::FMalloc));
	int64 FMallocUnused = FMallocPlatformAmount - FMallocAmount;
	if (FMallocPlatformAmount == 0)
	{
		// We do not have instrumentation for this allocator, and so can not calculate how much memory it is using internally. Set unused to 0 for this case.
		FMallocUnused = 0;
	}
	DefaultTracker.SetTagAmountInUpdate(FindOrAddTagData(ELLMTag::FMallocUnused), FMallocUnused, true);

	int64 StaticOverhead = sizeof(FLowLevelMemTracker);
	MemoryUsageCurrentOverhead = StaticOverhead + Allocator.GetTotal();
	PlatformTracker.SetTagAmountInUpdate(FindOrAddTagData(ELLMTag::PlatformOverhead), MemoryUsageCurrentOverhead, true);

	// calculate memory the platform thinks we have allocated, compared to what we have tracked, including the program memory
	FPlatformMemoryStats PlatformStats = FPlatformMemory::GetStats();
#if PLATFORM_DESKTOP
	int64 PlatformProcessMemory = static_cast<int64>(PlatformStats.UsedVirtual);  // virtual is working set + paged out memory
#elif PLATFORM_ANDROID || PLATFORM_IOS || UE_SERVER
	int64 PlatformProcessMemory = static_cast<int64>(PlatformStats.UsedPhysical);
#else
	int64 PlatformProcessMemory = static_cast<int64>(PlatformStats.TotalPhysical) - static_cast<int64>(PlatformStats.AvailablePhysical);
#endif
	int64 PlatformTrackedTotal = PlatformTracker.GetTrackedTotal();
	MemoryUsagePlatformTotalUntracked = FMath::Max<int64>(0, PlatformProcessMemory - PlatformTrackedTotal);

	PlatformTracker.SetTagAmountInUpdate(FindOrAddTagData(ELLMTag::PlatformTotal), PlatformProcessMemory, false);
	PlatformTracker.SetTagAmountInUpdate(FindOrAddTagData(ELLMTag::PlatformUntracked), MemoryUsagePlatformTotalUntracked, false);
	PlatformTracker.SetTagAmountInUpdate(FindOrAddTagData(ELLMTag::PlatformOSAvailable), PlatformStats.AvailablePhysical, false);

	int64 TrackedTotal = DefaultTracker.GetTrackedTotal();
	// remove the MemoryUsageCurrentOverhead from the "Total" for the default LLM as it's not something anyone needs to investigate when finding what to reduce
	// the platform LLM will have the info 
	int64 DefaultProcessMemory = PlatformProcessMemory - MemoryUsageCurrentOverhead;
	int64 DefaultUntracked = FMath::Max<int64>(0, DefaultProcessMemory - TrackedTotal);
	DefaultTracker.SetTagAmountInUpdate(FindOrAddTagData(ELLMTag::Total), DefaultProcessMemory, false);
	DefaultTracker.SetTagAmountInUpdate(FindOrAddTagData(ELLMTag::Untracked), DefaultUntracked, false);

#if PLATFORM_WINDOWS
	DefaultTracker.SetTagAmountInUpdate(FindOrAddTagData(ELLMTag::WorkingSetSize), PlatformStats.UsedPhysical, false);
	DefaultTracker.SetTagAmountInUpdate(FindOrAddTagData(ELLMTag::PagefileUsed), PlatformStats.UsedVirtual, false);
#endif
}

void FLowLevelMemTracker::UpdateTags()
{
	using namespace UE::LLMPrivate;

	if (!bTagAdded)
	{
		return;
	}

	bTagAdded = false;
	bool bNeedsResort = false;
	{
		FReadScopeLock ScopeLock(TagDataLock);
		// Cannot use a ranged-for because FinishConstruct can drop our lock and add elements to TagDatas
		// Check the valid-index condition on every loop iteration
		for (int32 TagIndex = 0; TagIndex < TagDatas->Num(); ++TagIndex)
		{
			FTagData* TagData = (*TagDatas)[TagIndex];
			FinishConstruct(TagData, UE::LLMPrivate::ETagReferenceSource::FunctionAPI);
			const FTagData* Parent = TagData->GetParent();
			if (Parent && Parent->GetIndex() > TagData->GetIndex())
			{
				bNeedsResort = true;
			}
		}
	}
	if (bNeedsResort)
	{
		// Prevent threads from reading their tags while we are mutating tags
		for (int32 TrackerIndex = 0; TrackerIndex < static_cast<int32>(ELLMTracker::Max); TrackerIndex++)
		{
			GetTracker(static_cast<ELLMTracker>(TrackerIndex))->LockAllThreadTags(true);
		}

		FTagDataArray* OldTagDatas;
		FWriteScopeLock ScopeLock(TagDataLock);
		SortTags(OldTagDatas);

		for (int32 TrackerIndex = 0; TrackerIndex < static_cast<int32>(ELLMTracker::Max); TrackerIndex++)
		{
			GetTracker(static_cast<ELLMTracker>(TrackerIndex))->OnTagsResorted(*OldTagDatas);
			GetTracker(static_cast<ELLMTracker>(TrackerIndex))->LockAllThreadTags(false);
		}

		Allocator.Delete(OldTagDatas);
	}
}

void FLowLevelMemTracker::SortTags(UE::LLMPrivate::FTagDataArray*& OutOldTagDatas)
{
	using namespace UE::LLMPrivate;

	// Caller is responsible for holding a WriteLock on TagDataLock
	OutOldTagDatas = TagDatas;
	TagDatas = Allocator.New<FTagDataArray>();
	FTagDataArray& LocalTagDatas = *TagDatas;
	LocalTagDatas.Reserve(OutOldTagDatas->Num());
	for (FTagData* TagData : *OutOldTagDatas)
	{
		LocalTagDatas.Add(TagData);
	}

	auto GetEdges = [&LocalTagDatas](int32 Vertex, int* Edges, int& NumEdges)
	{
		NumEdges = 0;
		const FTagData* Parent = LocalTagDatas[Vertex]->GetParent();
		if (Parent)
		{
			Edges[NumEdges++] = Parent->GetIndex();
		}
	};

	LLMAlgo::TopologicalSortLeafToRoot(LocalTagDatas, GetEdges);

	// Set each tag's index to its new position in the sort order
	for (int32 n = 0; n < LocalTagDatas.Num(); ++n)
	{
		LocalTagDatas[n]->SetIndex(n);
	}
}

void FLowLevelMemTracker::PublishDataPerFrame(const TCHAR* LogName)
{
	using namespace UE::LLMPrivate;

	// set overhead stats
	SET_MEMORY_STAT(STAT_LLMOverheadTotal, MemoryUsageCurrentOverhead);
	if (!bIsDisabled)
	{
		FLLMTracker& DefaultTracker = *GetTracker(ELLMTracker::Default);
		FLLMTracker& PlatformTracker = *GetTracker(ELLMTracker::Platform);

		bool bTrackPeaks = CVarLLMTrackPeaks.GetValueOnAnyThread() != 0;
#if !LLM_ENABLED_TRACK_PEAK_MEMORY
		if (bTrackPeaks)
		{
			static bool bWarningGiven = false;
			if (!bWarningGiven)
			{
				UE_LOG(LogHAL, Warning, TEXT("Attempted to enable LLM.TrackPeaks, but LLM_ENABLED_TRACK_PEAK_MEMORY is not defined to 1. You will need to enable the define"));
				bWarningGiven = true;
			}
		}
#endif

		DefaultTracker.PublishStats(bTrackPeaks);
		PlatformTracker.PublishStats(bTrackPeaks);

		if (bCsvWriterEnabled)
		{
			DefaultTracker.PublishCsv(bTrackPeaks);
			PlatformTracker.PublishCsv(bTrackPeaks);
		}

		if (bTraceWriterEnabled)
		{
			DefaultTracker.PublishTrace(bTrackPeaks);
			PlatformTracker.PublishTrace(bTrackPeaks);
		}

#if LLM_CSV_PROFILER_WRITER_ENABLED
		if (FCsvProfiler::Get()->IsCapturing())
		{
			DefaultTracker.PublishCsvProfiler(bTrackPeaks);
			PlatformTracker.PublishCsvProfiler(bTrackPeaks);
		}
#endif
	}

	if (LogName != nullptr)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("---> Untracked memory at %s = %.2f mb\n"), LogName, (double)MemoryUsagePlatformTotalUntracked / (1024.0 * 1024.0));
	}
}

void FLowLevelMemTracker::InitialiseProgramSize()
{
	if (!ProgramSize)
	{
		FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
		ProgramSize = Stats.TotalPhysical - Stats.AvailablePhysical;

		Trackers[static_cast<int32>(ELLMTracker::Platform)]->TrackMemory(ELLMTag::ProgramSizePlatform, ProgramSize, ELLMAllocType::System);
		Trackers[static_cast<int32>(ELLMTracker::Default)]->TrackMemory(ELLMTag::ProgramSize, ProgramSize, ELLMAllocType::System);
	}
}

void FLowLevelMemTracker::SetProgramSize(uint64 InProgramSize)
{
	if (bIsDisabled)
	{
		return;
	}
	BootstrapInitialise();

	int64 ProgramSizeDiff = static_cast<int64>(InProgramSize) - ProgramSize;

	ProgramSize = static_cast<int64>(InProgramSize);

	GetTracker(ELLMTracker::Platform)->TrackMemory(ELLMTag::ProgramSizePlatform, ProgramSizeDiff, ELLMAllocType::System);
	GetTracker(ELLMTracker::Default)->TrackMemory(ELLMTag::ProgramSize, ProgramSizeDiff, ELLMAllocType::System);
}

void FLowLevelMemTracker::ProcessCommandLine(const TCHAR* CmdLine)
{
	CSV_METADATA(TEXT("LLM"), TEXT("0"));

#if LLM_AUTO_ENABLE
	// LLM is always on, regardless of command line
	bool bShouldDisable = false;
#elif LLM_COMMANDLINE_ENABLES_FUNCTIONALITY
	// if we require commandline to enable it, then we are disabled if it's not there
	bool bShouldDisable = FParse::Param(CmdLine, TEXT("LLM")) == false;
#else
	// if we allow commandline to disable us, then we are disabled if it's there
	bool bShouldDisable = FParse::Param(CmdLine, TEXT("NOLLM")) == true;
#endif

	bool bLocalCsvWriterEnabled = FParse::Param(CmdLine, TEXT("LLMCSV"));
	bool bLocalTraceWriterEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(MemTagChannel);
	// automatically enable LLM if only csv or trace output is active
	if (bLocalCsvWriterEnabled || bLocalTraceWriterEnabled)
	{
		if (bLocalTraceWriterEnabled)
		{
			UE_LOG(LogInit, Log, TEXT("LLM enabled due to UE_TRACE MemTagChannel being enabled"));
		}
		bShouldDisable = false;
	}
	
	bAutoPublish = FParse::Param(CmdLine, TEXT("LLMDISABLEAUTOPUBLISH")) == false;

	if (!bCanEnable)
	{
		LLMCheck(bIsDisabled);
		if (!bShouldDisable)
		{
			UE_LOG(LogInit, Log, TEXT("LLM - Ignoring request to enable LLM; it is not available on the current platform"));
		}
		return;
	}
	bConfigurationComplete = true;

	if (bShouldDisable)
	{
		// Before we shutdown, update once so we can publish the overhead-when-disabled later during the first call to UpdateStatsPerFrame
		if (!bIsDisabled)
		{
			Tick();
		}
		bIsDisabled = true;
		bCsvWriterEnabled = false;
		bTraceWriterEnabled = false;
		bCanEnable = false; // Reenabling after a clear is not implemented
		Clear();
		return;
	}
	CSV_METADATA(TEXT("LLM"), TEXT("1"));
	bIsDisabled = false;
	bCsvWriterEnabled = bLocalCsvWriterEnabled;
	bTraceWriterEnabled = bLocalTraceWriterEnabled;
	BootstrapInitialise();
	FinishInitialise();

	// activate tag sets (we ignore None set, it's always on)
	FString SetList;
	static_assert((uint8)ELLMTagSet::Max == 3, "You added a tagset, without updating FLowLevelMemTracker::ProcessCommandLine");
	if (FParse::Value(CmdLine, TEXT("LLMTAGSETS="), SetList, false /* bShouldStopOnSeparator */))
	{
		TArray<FString> Sets;
		SetList.ParseIntoArray(Sets, TEXT(","), true);
		for (FString& Set : Sets)
		{
			if (Set == TEXT("Assets"))
			{
#if LLM_ALLOW_ASSETS_TAGS // asset tracking has a per-thread memory overhead, so we have a #define to completely disable it - warn if we don't match
				ActiveSets[static_cast<int32>(ELLMTagSet::Assets)] = true;
#else
				UE_LOG(LogInit, Warning, TEXT("Attempted to use LLM to track assets, but LLM_ALLOW_ASSETS_TAGS is not defined to 1. You will need to enable the define"));
#endif
			}
			else if (Set == TEXT("AssetClasses"))
			{
				ActiveSets[static_cast<int32>(ELLMTagSet::AssetClasses)] = true;
			}
		}
	}

	// Commandline overrides for console variables
	int TrackPeaks = 0;
	if (FParse::Value(CmdLine, TEXT("LLMTrackPeaks="), TrackPeaks))
	{
		CVarLLMTrackPeaks->Set(TrackPeaks);
	}

	UE_LOG(LogInit, Log, TEXT("LLM enabled CsvWriter: %s TraceWriter: %s"), bCsvWriterEnabled ? TEXT("on") : TEXT("off"), bTraceWriterEnabled ? TEXT("on") : TEXT("off"));
}

// Return the total amount of memory being tracked
uint64 FLowLevelMemTracker::GetTotalTrackedMemory(ELLMTracker Tracker)
{
	if (bIsDisabled)
	{
		return 0;
	}
	BootstrapInitialise();

	return static_cast<uint64>(GetTracker(Tracker)->GetTrackedTotal());
}

void FLowLevelMemTracker::OnLowLevelAlloc(ELLMTracker Tracker, const void* Ptr, uint64 Size, ELLMTag DefaultTag, ELLMAllocType AllocType, bool bTrackInMemPro)
{
	if (bIsDisabled)
	{
		return;
	}
	BootstrapInitialise();

	GetTracker(Tracker)->TrackAllocation(Ptr, static_cast<int64>(Size), DefaultTag, Tracker, AllocType, bTrackInMemPro);
}

void FLowLevelMemTracker::OnLowLevelAlloc(ELLMTracker Tracker, const void* Ptr, uint64 Size, FName DefaultTag, ELLMAllocType AllocType, bool bTrackInMemPro)
{
	if (bIsDisabled)
	{
		return;
	}
	BootstrapInitialise();

	GetTracker(Tracker)->TrackAllocation(Ptr, static_cast<int64>(Size), DefaultTag, Tracker, AllocType, bTrackInMemPro);
}

void FLowLevelMemTracker::OnLowLevelFree(ELLMTracker Tracker, const void* Ptr, ELLMAllocType AllocType, bool bTrackInMemPro)
{
	if (bIsDisabled)
	{
		return;
	}
	BootstrapInitialise();

	if (Ptr != nullptr)
	{
		GetTracker(Tracker)->TrackFree(Ptr, Tracker, AllocType, bTrackInMemPro);
	}
}

void FLowLevelMemTracker::OnLowLevelAllocMoved(ELLMTracker Tracker, const void* Dest, const void* Source, ELLMAllocType AllocType)
{
	if (bIsDisabled)
	{
		return;
	}
	BootstrapInitialise();

	//update the allocation map
	GetTracker(Tracker)->OnAllocMoved(Dest, Source, Tracker, AllocType);
}

UE::LLMPrivate::FLLMTracker* FLowLevelMemTracker::GetTracker(ELLMTracker Tracker)
{
	return Trackers[static_cast<int32>(Tracker)];
}

bool FLowLevelMemTracker::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (bIsDisabled)
	{
		return false;
	}

	if (FParse::Command(&Cmd, TEXT("LLMEM")))
	{
		BootstrapInitialise();
		if (FParse::Command(&Cmd, TEXT("SPAMALLOC")))
		{
			int32 NumAllocs = 128;
			int64 MaxSize = FCString::Atoi(Cmd);
			if (MaxSize == 0)
			{
				MaxSize = 128 * 1024;
			}

			UpdateStatsPerFrame(TEXT("Before spam"));
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("----> Spamming %d allocations, from %d..%d bytes\n"), NumAllocs, MaxSize/2, MaxSize);

			TArray<void*> Spam;
			Spam.Reserve(NumAllocs);
			SIZE_T TotalSize = 0;
			for (int32 Index = 0; Index < NumAllocs; Index++)
			{
				SIZE_T Size = (FPlatformMath::Rand() % MaxSize / 2) + MaxSize / 2;
				TotalSize += Size;
				Spam.Add(FMemory::Malloc(Size));
			}
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("----> Allocated %d total bytes\n"), TotalSize);

			UpdateStatsPerFrame(TEXT("After spam"));

			for (int32 Index = 0; Index < Spam.Num(); Index++)
			{
				FMemory::Free(Spam[Index]);
			}

			UpdateStatsPerFrame(TEXT("After cleanup"));
		}
		return true;
	}

	return false;
}

bool FLowLevelMemTracker::IsTagSetActive(ELLMTagSet Set)
{
	if (bIsDisabled)
	{
		return false;
	}
	BootstrapInitialise();

	return ActiveSets[static_cast<int32>(Set)];
}

bool FLowLevelMemTracker::ShouldReduceThreads()
{
#if LLM_ENABLED_REDUCE_THREADS
	if (bIsDisabled)
	{
		return false;
	}
	BootstrapInitialise();
	LLMCheckf(bConfigurationComplete, TEXT("ShouldReduceThreads has been called too early, before we processed the configuration settings required for it."));

	return IsTagSetActive(ELLMTagSet::Assets) || IsTagSetActive(ELLMTagSet::AssetClasses);
#else
	return false;
#endif
}

static bool IsAssetTagForAssets(ELLMTagSet Set)
{
	return Set == ELLMTagSet::Assets || Set == ELLMTagSet::AssetClasses;
}

void FLowLevelMemTracker::RegisterCustomTagInternal(int32 Tag, const TCHAR* InDisplayName, FName StatName, FName SummaryStatName, int32 ParentTag)
{
	using namespace UE::LLMPrivate;

	LLMCheckf(Tag >= LLM_CUSTOM_TAG_START && Tag <= LLM_CUSTOM_TAG_END, TEXT("Tag %d out of range"), Tag);
	LLMCheckf(InDisplayName != nullptr, TEXT("Tag %d has no name"), Tag);
	LLMCheckf(ParentTag == -1 || ParentTag < LLM_TAG_COUNT, TEXT("Parent tag %d out of range"), ParentTag);

	FName DisplayName = InDisplayName ? InDisplayName : InvalidLLMTagName;
	ELLMTag EnumTag = static_cast<ELLMTag>(Tag);
	FName ParentName = ParentTag >= 0 ? LLMGetTagUniqueName(static_cast<ELLMTag>(ParentTag)) : NAME_None;

	RegisterTagData(LLMGetTagUniqueName(EnumTag), DisplayName, ParentName, StatName, SummaryStatName, true, EnumTag, false, ETagReferenceSource::CustomEnumTag);
}

void FLowLevelMemTracker::RegisterPlatformTag(int32 Tag, const TCHAR* Name, FName StatName, FName SummaryStatName, int32 ParentTag)
{
	MemoryTrace_AnnounceCustomTag(Tag, ParentTag, Name);

	if (bIsDisabled)
	{
		return;
	}
	BootstrapInitialise();

	LLMCheck(Tag >= static_cast<int32>(ELLMTag::PlatformTagStart) && Tag <= static_cast<int32>(ELLMTag::PlatformTagEnd));
	RegisterCustomTagInternal(Tag, Name, StatName, SummaryStatName, ParentTag);
}

void FLowLevelMemTracker::RegisterProjectTag(int32 Tag, const TCHAR* Name, FName StatName, FName SummaryStatName, int32 ParentTag)
{
	MemoryTrace_AnnounceCustomTag(Tag, ParentTag, Name);

	if (bIsDisabled)
	{
		return;
	}
	BootstrapInitialise();

	LLMCheck(Tag >= static_cast<int32>(ELLMTag::ProjectTagStart) && Tag <= static_cast<int32>(ELLMTag::ProjectTagEnd));
	RegisterCustomTagInternal(Tag, Name, StatName, SummaryStatName, ParentTag);
}

void GlobalRegisterTagDeclaration(FLLMTagDeclaration& TagDeclaration)
{
	if (FLowLevelMemTracker::bIsDisabled)
	{
		return;
	}
	FLowLevelMemTracker::Get().RegisterTagDeclaration(TagDeclaration);
}

void FLowLevelMemTracker::BootstrapTagDatas()
{
	using namespace UE::LLMPrivate;

	// While bootstrapping we are not allowed to construct any FNames because the FName system may not yet have been constructed.
	// Construct not-fully-initialized TagDatas for the central list of ELLMTags
	{
		FWriteScopeLock ScopeLock(TagDataLock);
		bIsBootstrapping = true;

		TagDatas = Allocator.New<FTagDataArray>();
		TagDataNameMap = Allocator.New<FTagDataNameMap>();
		TagDataEnumMap = reinterpret_cast<FTagData**>(Allocator.Alloc(sizeof(FTagData*) * LLM_TAG_COUNT));
		FMemory::Memset(TagDataEnumMap, 0, sizeof(FTagData*) * LLM_TAG_COUNT);

#define REGISTER_ELLMTAG(Enum,Str,Stat,Group,ParentTag) \
		{ \
			ELLMTag EnumTag = ELLMTag::Enum; \
			int32 Index = static_cast<int32>(EnumTag); \
			LLMCheck(0 <= Index && Index < LLM_TAG_COUNT); \
			FTagData* TagData = Allocator.New<FTagData>(EnumTag); \
			TagData->SetIndex(TagDatas->Num()); \
			TagDatas->Add(TagData); \
			LLMCheck(TagDataEnumMap[Index] == nullptr); \
			TagDataEnumMap[Index] = TagData; \
		}
		LLM_ENUM_GENERIC_TAGS(REGISTER_ELLMTAG);
#undef REGISTER_ELLMTAG

		// The CustomName tag is an adapter for connecting LLM_SCOPE_BYNAME tags with platforms that used the ELLMTag-based reporting.
		// We want to hide this procedural tags in systems that use FName-based reporting; if it is displayed it confusingly just displays 
		// a sum of every LLM_SCOPE_BYNAME tag.
		TagDataEnumMap[static_cast<int32>(ELLMTag::CustomName)]->SetIsReportable(false);
	}
}

void FLowLevelMemTracker::FinishInitialise()
{
	if (bFullyInitialised)
	{
		return;
	}
	bFullyInitialised = true;
	// Make sure that FNames and Malloc have already been initialised, since we will use them during InitialiseTagDatas
	// We force this by calling LLMGetTagUniqueName, which initializes FNames internally, and will therein trigger FName system construction, which will itself trigger Malloc construction
	(void)LLMGetTagUniqueName(ELLMTag::Untagged);
	InitialiseTagDatas();
}
 
void FLowLevelMemTracker::InitialiseTagDatas_SetLLMTagNames()
{
	using namespace UE::LLMPrivate;

	TStringBuilder<256> NameBuffer;
	// Load all the names for the central list of ELLMTags (recording the allocations the FName system makes for the construction of the names)
#define SET_ELLMTAG_NAMES(Enum,Str,Stat,Group,ParentTag) \
	{ \
		ELLMTag EnumTag = ELLMTag::Enum; \
		int32 Index = static_cast<int32>(EnumTag); \
		FTagData* TagData = TagDataEnumMap[Index]; \
		FName TagName = LLMGetTagUniqueName(EnumTag); \
		TagName.ToString(NameBuffer); \
		ValidateUniqueName(NameBuffer); \
		TagData->SetName(LLMGetTagUniqueName(EnumTag)); \
		TagData->SetDisplayName(Str); \
		TagData->SetStatName(Stat); \
		TagData->SetSummaryStatName(Group); \
		TagData->SetParentName(static_cast<int32>(ParentTag) == -1 ? NAME_None : LLMGetTagUniqueName(static_cast<ELLMTag>(ParentTag))); \
	}
	LLM_ENUM_GENERIC_TAGS(SET_ELLMTAG_NAMES);
#undef SET_ELLMTAG_NAMES
}

void FLowLevelMemTracker::InitialiseTagDatas_FinishRegister()
{
	using namespace UE::LLMPrivate;

	// Record the central list of ELLMTags in TagDataNameMap, and mark that bootstrapping is complete
	{
		FWriteScopeLock ScopeLock(TagDataLock);

#define FINISH_REGISTER(Enum,Str,Stat,Group,ParentTag) \
		{ \
			ELLMTag EnumTag = ELLMTag::Enum; \
			int32 Index = static_cast<int32>(EnumTag); \
			FTagData* TagData = TagDataEnumMap[Index]; \
			FTagData*& ExistingTagData = TagDataNameMap->FindOrAdd(TagData->GetName(), nullptr); \
			if (ExistingTagData != nullptr) \
			{ \
				ReportDuplicateTagName(ExistingTagData, ETagReferenceSource::EnumTag); \
			} \
			ExistingTagData = TagData; \
		}
		LLM_ENUM_GENERIC_TAGS(FINISH_REGISTER);
#undef FINISH_REGISTER

		bIsBootstrapping = false;
	}
}

void FLowLevelMemTracker::InitialiseTagDatas()
{
	using namespace UE::LLMPrivate;

	InitialiseTagDatas_SetLLMTagNames();

	InitialiseTagDatas_FinishRegister();

	// Construct the remaining startup tags; allocations when constructing these tags are known to consist only of the central list of ELLMTags so we do not need to bootstrap
	{
		// Construct LLM_DECLARE_TAGs
		FLLMTagDeclaration* List = FLLMTagDeclaration::GetList();
		while (List)
		{
			RegisterTagDeclaration(*List);
			List = List->Next;
		}
		FLLMTagDeclaration::AddCreationCallback(GlobalRegisterTagDeclaration);
	}

	// now let the platform add any custom tags
	FPlatformMemory::RegisterCustomLLMTags();

	// All parents in the ELLMTags and the initial modules' list of LLM_DEFINE_TAG must be contained within that same set, so we can FinishConstruct them now, which we do in UpdateTags
	bTagAdded = true;
	UpdateTags();
}

void FLowLevelMemTracker::ClearTagDatas()
{
	using namespace UE::LLMPrivate;

	FWriteScopeLock ScopeLock(TagDataLock);
	FLLMTagDeclaration::ClearCreationCallbacks();

	Allocator.Free(TagDataEnumMap, sizeof(FTagData*) * LLM_TAG_COUNT);
	TagDataEnumMap = nullptr;
	Allocator.Delete(TagDataNameMap);
	TagDataNameMap = nullptr;
	for (FTagData* TagData : *TagDatas)
	{
		Allocator.Delete(TagData);
	}
	Allocator.Delete(TagDatas);
	TagDatas = nullptr;
}

void FLowLevelMemTracker::RegisterTagDeclaration(FLLMTagDeclaration& TagDeclaration)
{
	TagDeclaration.ConstructUniqueName();
	RegisterTagData(TagDeclaration.UniqueName, TagDeclaration.DisplayName, TagDeclaration.ParentTagName, TagDeclaration.StatName, TagDeclaration.SummaryStatName, false, ELLMTag::CustomName, false, UE::LLMPrivate::ETagReferenceSource::Declare);
}

UE::LLMPrivate::FTagData& FLowLevelMemTracker::RegisterTagData(FName Name, FName DisplayName, FName ParentName, FName StatName, FName SummaryStatName, bool bHasEnumTag, ELLMTag EnumTag, bool bIsStatTag, UE::LLMPrivate::ETagReferenceSource ReferenceSource)
{
	LLMCheckf(!bIsBootstrapping, TEXT("A tag outside of LLM_ENUM_GENERIC_TAGS was requested from LLM_SCOPE or allocation while bootstrapping the names for LLM_ENUM_GENERIC_TAGS, this is not supported."));

	using namespace UE::LLMPrivate;

	TStringBuilder<256> NameBuffer; // If this allocates, that is okay. Set it to something small-as-possible-to-avoid-normally-allocating to prevent adding a lot of stack space in the calling LLM_SCOPE code
	Name.ToString(NameBuffer);

	if (bHasEnumTag)
	{
		ValidateUniqueName(NameBuffer);
		// EnumTags can specify DisplayName (if they are central or if CustomTag registration provided it); if not, they set DisplayName = UniqueName
		// Enum tags only specify ParentName explicitly; if no ParentName is provided, they have no parent
		if (DisplayName.IsNone())
		{
			DisplayName = Name;
		}
	}
	else if (bIsStatTag)
	{
		// Stat tag unique names do not have to be validated, because they are never used as parent tags
		// Stat tag unique names are of the form //GroupName//StatUniqueName///StatDisplayName///<OtherData>. We set LLM UniqueName = <TheEntireString> and LLM DisplayName = StatDisplayName
		// Stat tags do not specify their parent, and their parent is set to the CustomName aggregator
		LLMCheck(DisplayName.IsNone());
		LLMCheck(ParentName.IsNone());
		DisplayName = Name;
		ParentName = GetTagName_CustomName();

		const TCHAR* Start = FCString::Strstr(NameBuffer.ToString(), TEXT("///"));
		if (Start)
		{
			Start += 3;
			const TCHAR* End = FCString::Strstr(Start, TEXT("///"));
			if (End)
			{
				DisplayName = FName(FStringView(Start, UE_PTRDIFF_TO_INT32(End - Start)));
			}
		}
	}
	else
	{
		ValidateUniqueName(NameBuffer);
		// Normal defined-by-name tags supply unique names of the form Grandparent/.../Parent/Name
		// ParentName and  DisplayName can be provided

		// If both ParentName and /Parent/ are supplied, it is an error if they do not match
		// All custom name tags have to be children of an ELLMTag. If no parent is set, it defaults to the the proxy parent CustomName
		const TCHAR* LeafStart = NameBuffer.ToString();
		while (true)
		{
			const TCHAR* NextDivider = FCString::Strstr(LeafStart, TEXT("/"));
			if (!NextDivider)
			{
				break;
			}
			LeafStart = NextDivider + 1;
		}
		LLMCheckf(LeafStart[0] != '\0', TEXT("Invalid LLM custom name tag '%s'. Tag names must not end with /."), NameBuffer.ToString());
		if (LeafStart != NameBuffer.ToString())
		{
			FName ParsedParentName = FName(FStringView(NameBuffer.ToString(), UE_PTRDIFF_TO_INT32(LeafStart - 1 - NameBuffer.ToString())));
			if (!ParentName.IsNone() && ParentName != ParsedParentName)
			{
				TStringBuilder<128> ParentBuffer;
				ParentName.ToString(ParentBuffer);
				LLMCheckf(false, TEXT("Invalid LLM tag: parent supplied in tag declaration is '%s', which does not match the parent parsed from the tag unique name '%s'"), ParentBuffer.ToString(), NameBuffer.ToString());
			}
			ParentName = ParsedParentName;
		}
		else if (ParentName.IsNone())
		{
			ParentName = GetTagName_CustomName();
		}

		// Display name is set to the leaf /Name portion of the unique name, and is overridden if DisplayName is provided
		if (DisplayName.IsNone())
		{
			DisplayName = FName(LeafStart);
		}
	}

	FWriteScopeLock ScopeLock(TagDataLock);
	FTagData*& TagDataForName = TagDataNameMap->FindOrAdd(Name, nullptr);
	if (TagDataForName)
	{
		// The tag already exists; this can happen because two formal registrations
		// have tried to add the same name, but it can also happen due to a race condition
		// when auto-adding a tag from LLM_SCOPE, if the tag is added by another thread
		// in between FindOrAddTagData's check of TagDataNameMap->Find and now.
		// Note that it is not valid for an LLM_SCOPE to be called before a formal registration
		// (e.g. LLM_DECLARE_TAG). If a formal registration exists for a tag, it must precede its use
		// in any LLM_SCOPE calls.
		if ((ParentName.IsNone() && TagDataForName->GetParent() != nullptr) ||
			(!ParentName.IsNone() && (TagDataForName->GetParent() == nullptr || TagDataForName->GetParent()->GetName() != ParentName)) ||
			StatName != TagDataForName->GetStatName() ||
			SummaryStatName != TagDataForName->GetSummaryStatName() ||
			bHasEnumTag != TagDataForName->HasEnumTag() ||
			(bHasEnumTag && EnumTag != TagDataForName->GetEnumTag()))
		{
			if (ReferenceSource != UE::LLMPrivate::ETagReferenceSource::Scope &&
				ReferenceSource != UE::LLMPrivate::ETagReferenceSource::ImplicitParent &&
				ReferenceSource != UE::LLMPrivate::ETagReferenceSource::FunctionAPI)
			{
				ReportDuplicateTagName(TagDataForName, ReferenceSource);
			}
		}

		// Abandon the string work we've done and return the version that was added by the other source.
		return *TagDataForName;
	}

	FTagData* ParentData = nullptr;
	if (!ParentName.IsNone())
	{
		FTagData** ParentPtr = TagDataNameMap->Find(ParentName);
		if (ParentPtr)
		{
			ParentData = *ParentPtr;
		}
	}

	FTagData* TagData;
	if (ParentName.IsNone() || ParentData)
	{
		TagData = Allocator.New<FTagData>(Name, DisplayName, ParentData, StatName, SummaryStatName, bHasEnumTag, EnumTag, ReferenceSource);
	}
	else
	{
		TagData = Allocator.New<FTagData>(Name, DisplayName, ParentName, StatName, SummaryStatName, bHasEnumTag, EnumTag, ReferenceSource);
	}
	TagData->SetIndex(TagDatas->Num());
	TagDatas->Add(TagData);

	TagDataForName = TagData;

	if (bHasEnumTag)
	{
		int32 Index = static_cast<int32>(EnumTag);
		LLMCheck(0 <= Index && Index < LLM_TAG_COUNT);
		FTagData*& TagDataForEnum = TagDataEnumMap[Index];
		if (TagDataForEnum != nullptr)
		{
			LLMCheckf(false, TEXT("LLM Error: Duplicate copies of enumtag %d"), Index);
		}
		TagDataForEnum = TagData;
	}

	bTagAdded = true;
	return *TagData;
}

void FLowLevelMemTracker::ReportDuplicateTagName(UE::LLMPrivate::FTagData* TagData, UE::LLMPrivate::ETagReferenceSource ReferenceSource)
{
	using namespace UE::LLMPrivate;

	// We're inside the LLM lock, so do not allow these GetName calls to allocate memory from FMalloc.
	if (ReferenceSource == ETagReferenceSource::FunctionAPI || ReferenceSource == ETagReferenceSource::Scope || ReferenceSource == ETagReferenceSource::ImplicitParent)
	{
		LLMCheckf(false, TEXT("LLM Error: Unexpected call to RegisterTagData(%s) from LLM_SCOPE or function call when the tag already exists."),
			*WriteToString<FName::StringBufferSize>(TagData->GetName()));
	}
	else if (TagData->GetReferenceSource() == ETagReferenceSource::FunctionAPI || TagData->GetReferenceSource() == ETagReferenceSource::Scope)
	{
		LLMCheckf(false, TEXT("LLM Error: Tag %s parsed from %s after it was already used in an LLM_SCOPE or LLM api call."),
			*WriteToString<FName::StringBufferSize>(TagData->GetName()), ToString(ReferenceSource));
	}
	else if (TagData->GetReferenceSource() == ETagReferenceSource::ImplicitParent)
	{
		LLMCheckf(false, TEXT("LLM Error: Tag %s parsed from %s after it was already used as an implicit parent in another tag. Add LLM_DEFINE_TAG(%s) in cpp, or move it to the same module as the child tag using it, so that it will be defined before the child tag tries to use it."),
			*WriteToString<FName::StringBufferSize>(TagData->GetName()), ToString(ReferenceSource),
			*WriteToString<FName::StringBufferSize>(TagData->GetName()));
	}
	else
	{
		LLMCheckf(false, TEXT("LLM Error: Multiple occurrences of a unique tag name %s in ELLMTag or LLM_DEFINE_TAG. First occurrence: %s. Second occurrence: %s."),
			*WriteToString<FName::StringBufferSize>(TagData->GetName()), ToString(TagData->GetReferenceSource()), ToString(ReferenceSource));
	}
}

void FLowLevelMemTracker::FinishConstruct(UE::LLMPrivate::FTagData* TagData, UE::LLMPrivate::ETagReferenceSource ReferenceSource)
{
	using namespace UE::LLMPrivate;
	// Caller is responsible for holding a ReadLock (NOT a WriteLock) on TagDataLock

	LLMCheck(TagData);
	if (TagData->IsFinishConstructed())
	{
		return;
	}
	if (bIsBootstrapping)
	{
		// Can't access Names yet; run the FinishConstruct later
		return;
	}

	if (!TagData->IsParentConstructed())
	{
		FName ParentName = TagData->GetParentName();
		if (ParentName.IsNone())
		{
			TagData->SetParent(nullptr);
		}
		else
		{
			FTagData** ParentDataPtr = TagDataNameMap->Find(ParentName);
			if (!ParentDataPtr)
			{
				// We have to drop the ReadLock to call RegisterTagData, which takes a WriteLock
				TagDataLock.ReadUnlock();
				FTagData* ParentTagData = &RegisterTagData(ParentName, NAME_None, NAME_None, NAME_None, NAME_None, false, ELLMTag::CustomName, false, ETagReferenceSource::ImplicitParent);
				TagDataLock.ReadLock();
				if (TagData->IsFinishConstructed())
				{
					// Another thread got in and finished construction while we were outside of the lock
					return;
				}
				ParentDataPtr = TagDataNameMap->Find(ParentName);
				LLMCheck(ParentDataPtr);
			}
			FTagData* ParentData = *ParentDataPtr;
			TagData->SetParent(ParentData);
		}
	}
	TagData->SetFinishConstructed();

	FTagData* ParentData = const_cast<FTagData*>(TagData->GetParent());
	if (ParentData)
	{
		// Make sure the parent chain is FinishConstructed as well, since GetContainingEnum or GetDisplayPath will be called and walk up the parent chain
		FinishConstruct(ParentData, ReferenceSource);
	}
}

TArray<const UE::LLMPrivate::FTagData*> FLowLevelMemTracker::GetTrackedTags()
{
	using namespace UE::LLMPrivate;

	if (bIsDisabled)
	{
		return TArray<const FTagData*>();
	}

	BootstrapInitialise();

	FReadScopeLock TagDataScopeLock(TagDataLock);
	return TArray<const FTagData*>(*TagDatas);
}

TArray<const UE::LLMPrivate::FTagData*> FLowLevelMemTracker::GetTrackedTags(ELLMTracker Tracker)
{
	using namespace UE::LLMPrivate;

	if (bIsDisabled)
	{
		return TArray<const FTagData*>();
	}
	
	BootstrapInitialise();

	FScopeLock UpdateScopeLock(&UpdateLock); // uses of TagSizes are guarded by the UpdateLock
	return GetTracker(Tracker)->GetTagDatas();
}

bool FLowLevelMemTracker::FindTagByName( const TCHAR* Name, uint64& OutTag ) const
{
	using namespace UE::LLMPrivate;

	if (bIsDisabled)
	{
		return false;
	}
	LLMCheck(bFullyInitialised); // Cannot call BootstrapInitialise and FinishInitialise without shenanigans because this function is const

	if (Name != nullptr)
	{
		FReadScopeLock ScopeLock(TagDataLock);

		// Search by Name
		FName SearchName(Name);
		FTagData** TagDataPtr = TagDataNameMap->Find(SearchName);
		if (TagDataPtr)
		{
			const FTagData* TagData = *TagDataPtr;
			OutTag = static_cast<uint64>(TagData->GetContainingEnum());
			return true;
		}

		// Search by ELLMTag's DisplayName
		for (int32 Index = 0; Index < LLM_TAG_COUNT; ++Index)
		{
			const FTagData* TagData = TagDataEnumMap[Index];
			if (!TagData)
			{
				continue;
			}
			if (TCString<TCHAR>::Stricmp(*TagData->GetDisplayName().ToString(), Name))
			{
				OutTag = static_cast<uint64>(TagData->GetContainingEnum());
				return true;
			}
		}
	}

	return false;
}

const TCHAR* FLowLevelMemTracker::FindTagName(uint64 Tag) const
{
	if (bIsDisabled)
	{
		return nullptr;
	}
	LLMCheck(bInitialisedTracking); // Cannot call BootstrapInitialise without shenanigans because this function is const

	static TMap<uint64, FString> FoundTags;
	FString* Cached = FoundTags.Find(Tag);
	if (Cached)
	{
		return **Cached;
	}

	FName DisplayName = FindTagDisplayName(Tag);
	if (DisplayName.IsNone())
	{
		return nullptr;
	}

	FString& AddedCache = FoundTags.Add(Tag);
	AddedCache = DisplayName.ToString();
	return *AddedCache;
}

FName FLowLevelMemTracker::FindTagDisplayName(uint64 Tag) const
{
	using namespace UE::LLMPrivate;

	if (bIsDisabled)
	{
		return NAME_None;
	}
	LLMCheck(bInitialisedTracking); // Cannot call BootstrapInitialise without shenanigans because this function is const

	int32 Index = static_cast<int32>(Tag);
	if (0 <= Index && Index < LLM_CUSTOM_TAG_START)
	{
		const FTagData* TagData = TagDataEnumMap[Index];
		if (TagData)
		{
			return TagData->GetDisplayName();
		}
	}
	return NAME_None;
}

FName FLowLevelMemTracker::GetTagDisplayName(const UE::LLMPrivate::FTagData* TagData) const
{
	return TagData->GetDisplayName();
}

FString FLowLevelMemTracker::GetTagDisplayPathName(const UE::LLMPrivate::FTagData* TagData) const
{
	return TagData->GetDisplayPath();
}

FName FLowLevelMemTracker::GetTagUniqueName(const UE::LLMPrivate::FTagData* TagData) const
{
	return TagData->GetName();
}

int64 FLowLevelMemTracker::GetTagAmountForTracker(ELLMTracker Tracker, ELLMTag Tag, bool bPeakAmount/* = false*/)
{
	if (bIsDisabled)
	{
		return 0;
	}

	BootstrapInitialise();

	return GetTagAmountForTracker(Tracker, FindTagData(Tag), bPeakAmount);
}

int64 FLowLevelMemTracker::GetTagAmountForTracker(ELLMTracker Tracker, const UE::LLMPrivate::FTagData* TagData, bool bPeakAmount/* = false*/)
{
	using namespace UE::LLMPrivate;

	if (bIsDisabled)
	{
		return 0;
	}

	BootstrapInitialise();

	if (TagData == nullptr)
	{
		return 0;
	}

	FScopeLock UpdateScopeLock(&UpdateLock); // uses of TagSizes are guarded by the UpdateLock
	return GetTracker(Tracker)->GetTagAmount(TagData, bPeakAmount);
}

void FLowLevelMemTracker::SetTagAmountForTracker(ELLMTracker Tracker, ELLMTag Tag, int64 Amount, bool bAddToTotal)
{
	using namespace UE::LLMPrivate;

	if (bIsDisabled)
	{
		return;
	}
	BootstrapInitialise();
	const FTagData* TagData = FindOrAddTagData(Tag);

	FScopeLock UpdateScopeLock(&UpdateLock); // uses of TagSizes are guarded by the UpdateLock
	GetTracker(Tracker)->SetTagAmountExternal(TagData, Amount, bAddToTotal);
}

int64 FLowLevelMemTracker::GetActiveTag(ELLMTracker Tracker)
{
	using namespace UE::LLMPrivate;

	if (bIsDisabled)
	{
		return static_cast<int64>(ELLMTag::Untagged);
	}
	BootstrapInitialise();

	const FTagData* TagData = GetActiveTagData(Tracker);
	if (TagData)
	{
		return static_cast<int64>(TagData->GetContainingEnum());
	}
	else
	{
		return static_cast<int64>(ELLMTag::Untagged);
	}
}

const UE::LLMPrivate::FTagData* FLowLevelMemTracker::GetActiveTagData(ELLMTracker Tracker)
{
	if (bIsDisabled)
	{
		return nullptr;
	}
	BootstrapInitialise();

	return GetTracker(Tracker)->GetActiveTagData();
}

uint64 FLowLevelMemTracker::DumpTag( ELLMTracker Tracker, const char* FileName, int LineNumber )
{
	using namespace UE::LLMPrivate;
	if (bIsDisabled)
	{
		return static_cast<int64>(ELLMTag::Untagged);
	}
	BootstrapInitialise();

	const FTagData* TagData = GetActiveTagData(Tracker);
	if (TagData)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LLM TAG: %s (%lld) @ %s:%d\n"), *TagData->GetDisplayName().ToString(), TagData->GetContainingEnum(), FileName ? ANSI_TO_TCHAR(FileName) : TEXT("?"), LineNumber);
		return static_cast<uint64>(TagData->GetContainingEnum());
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LLM TAG: No Active Tag"));
		return static_cast<uint64>(ELLMTag::Untagged);
	}
}

void FLowLevelMemTracker::PublishDataSingleFrame()
{
	if (bAutoPublish)
	{
		UE_LOG(LogHAL, Error, TEXT("Command must be used with the -llmdisableautopublish command line parameter."));
	}
	else
	{
		bPublishSingleFrame = true;
	}	
}

const UE::LLMPrivate::FTagData* FLowLevelMemTracker::FindOrAddTagData(ELLMTag EnumTag, UE::LLMPrivate::ETagReferenceSource ReferenceSource)
{
	using namespace UE::LLMPrivate;

	int32 Index = static_cast<int32>(EnumTag);
	LLMCheckf(0 <= Index && Index < LLM_TAG_COUNT, TEXT("Out of range ELLMTag %d"), Index);

	{
		FReadScopeLock ScopeLock(TagDataLock);
		FTagData* TagData = TagDataEnumMap[Index];
		if (TagData)
		{
			FinishConstruct(TagData, ReferenceSource);
			return TagData;
		}
	}

	// If we have not initialised tags yet, we have to initialise now to potentially create the custom ELLMTag that we are trying to read
	if (!bFullyInitialised)
	{
		FinishInitialise();
		// Reeneter this function so that we retry the find above; note we avoid infinite recursion because bFullyInitialised is now true
		return FindOrAddTagData(EnumTag, ReferenceSource);
	}
	LLMCheckf(!bIsBootstrapping, TEXT("LLM Error: Invalid use of custom ELLMTag when initialising tags."));

	// Add the new Tag
	FName TagName = LLMGetTagUniqueName(EnumTag);
	{
		FTagData* TagData = &RegisterTagData(TagName, NAME_None, NAME_None, NAME_None, NAME_None, true, EnumTag, false, ReferenceSource);
		FReadScopeLock ScopeLock(TagDataLock);
		FinishConstruct(TagData, ReferenceSource);
		return TagData;
	}
}

const UE::LLMPrivate::FTagData* FLowLevelMemTracker::FindOrAddTagData(FName TagName, bool bIsStatTag, UE::LLMPrivate::ETagReferenceSource ReferenceSource)
{
	using namespace UE::LLMPrivate;

	{
		FReadScopeLock ScopeLock(TagDataLock);
		FTagData** TagDataPtr = TagDataNameMap->Find(TagName);
		if (TagDataPtr)
		{
			FTagData* TagData = *TagDataPtr;
			FinishConstruct(TagData, ReferenceSource);
			return TagData;
		}
	}

	// If we have not initialised tags yet, we have to initialise now to potentially create the TagName that we are trying to read
	if (!bFullyInitialised)
	{
		FinishInitialise();
		// Reeneter this function so that we retry the find above; note we avoid infinite recursion because bFullyInitialised is now true
		return FindOrAddTagData(TagName, bIsStatTag, ReferenceSource);
	}
	LLMCheckf(!bIsBootstrapping, TEXT("LLM Error: Invalid use of FName tag when initialising tags."));

	// Add the new Tag
	FName StatName = bIsStatTag ? TagName : NAME_None;
	FTagData* TagData = &RegisterTagData(TagName, NAME_None, NAME_None, StatName, NAME_None, false, ELLMTag::CustomName, bIsStatTag, ReferenceSource);
	{
		FReadScopeLock ScopeLock(TagDataLock);
		FinishConstruct(TagData, ReferenceSource);
		return TagData;
	}
}

const UE::LLMPrivate::FTagData* FLowLevelMemTracker::FindTagData(ELLMTag EnumTag, UE::LLMPrivate::ETagReferenceSource ReferenceSource)
{
	using namespace UE::LLMPrivate;

	int32 Index = static_cast<int32>(EnumTag);
	LLMCheckf(0 <= Index && Index < LLM_TAG_COUNT, TEXT("Out of range ELLMTag %d"), Index);

	FReadScopeLock ScopeLock(TagDataLock);
	FTagData* TagData = TagDataEnumMap[Index];
	if (TagData)
	{
		FinishConstruct(TagData, ReferenceSource);
		return TagData;
	}
	else
	{
		return nullptr;
	}
}

const UE::LLMPrivate::FTagData* FLowLevelMemTracker::FindTagData(FName TagName, UE::LLMPrivate::ETagReferenceSource ReferenceSource)
{
	using namespace UE::LLMPrivate;

	FReadScopeLock ScopeLock(TagDataLock);

	FTagData** TagDataPtr = TagDataNameMap->Find(TagName);
	if (TagDataPtr)
	{
		FTagData* TagData = *TagDataPtr;
		FinishConstruct(TagData, ReferenceSource);
		return TagData;
	}
	else
	{
		return nullptr;
	}
}

void FLLMScope::Init(ELLMTag TagEnum, bool bInIsStatTag, ELLMTagSet InTagSet, ELLMTracker InTracker)
{
	LLMCheck(!bInIsStatTag && InTagSet == ELLMTagSet::None);
	FLowLevelMemTracker& LLMRef = FLowLevelMemTracker::Get();
	// We have to check bIsDisabled again after calling Get, because the constructor is called from Get, and will set bIsDisabled=false if the platform doesn't support it
	if (FLowLevelMemTracker::bIsDisabled)
	{
		bEnabled = false;
		return;
	}
	LLMRef.BootstrapInitialise();

	bEnabled = true;
	Tracker = InTracker;
#if LLM_ALLOW_ASSETS_TAGS
	bIsAssetTag = false;
#endif
	LLMRef.GetTracker(Tracker)->PushTag(TagEnum);
}

void FLLMScope::Init(FName TagName, bool bInIsStatTag, ELLMTagSet InTagSet, ELLMTracker InTracker)
{
	FLowLevelMemTracker& LLMRef = FLowLevelMemTracker::Get();
	// We have to check bIsDisabled again after calling Get, because the constructor is called from Get, and will set bIsDisabled=false if the platform doesn't support it
	if (FLowLevelMemTracker::bIsDisabled)
	{
		bEnabled = false;
		return;
	}
	LLMRef.BootstrapInitialise();
	if (!LLMRef.IsTagSetActive(InTagSet))
	{
		bEnabled = false;
		return;
	}

	bEnabled = true;
	Tracker = InTracker;

#if LLM_ALLOW_ASSETS_TAGS
	bIsAssetTag = bInIsStatTag && IsAssetTagForAssets(InTagSet);
	if (bIsAssetTag)
	{
		LLMRef.GetTracker(Tracker)->PushAssetTag(TagName);
	}
	else
#endif
	{
		LLMRef.GetTracker(Tracker)->PushTag(TagName, bInIsStatTag);
	}
}

void FLLMScope::Init(const UE::LLMPrivate::FTagData* TagData, bool bInIsStatTag, ELLMTagSet InTagSet, ELLMTracker InTracker)
{
	LLMCheck(!bInIsStatTag && InTagSet == ELLMTagSet::None);
	FLowLevelMemTracker& LLMRef = FLowLevelMemTracker::Get();
	// We have to check bIsDisabled again after calling Get, because the constructor is called from Get, and will set bIsDisabled=false if the platform doesn't support it
	if (FLowLevelMemTracker::bIsDisabled)
	{
		bEnabled = false;
		return;
	}
	LLMRef.BootstrapInitialise();

	bEnabled = true;
	Tracker = InTracker;
#if LLM_ALLOW_ASSETS_TAGS
	bIsAssetTag = false;
#endif
	LLMRef.GetTracker(Tracker)->PushTag(TagData);
}

void FLLMScope::Destruct()
{
	FLowLevelMemTracker& LLMRef = FLowLevelMemTracker::Get();
#if LLM_ALLOW_ASSETS_TAGS
	if (bIsAssetTag)
	{
		LLMRef.GetTracker(Tracker)->PopAssetTag();
	}
	else
#endif
	{
		LLMRef.GetTracker(Tracker)->PopTag();
	}
}

FLLMPauseScope::FLLMPauseScope(FName TagName, bool bIsStatTag, uint64 Amount, ELLMTracker TrackerToPause, ELLMAllocType InAllocType)
{
	if (FLowLevelMemTracker::bIsDisabled)
	{
		bEnabled = false;
		return;
	}
	Init(TagName, ELLMTag::Untagged, false, bIsStatTag, Amount, TrackerToPause, InAllocType);
}

FLLMPauseScope::FLLMPauseScope(ELLMTag TagEnum, bool bIsStatTag, uint64 Amount, ELLMTracker TrackerToPause, ELLMAllocType InAllocType)
{
	if (FLowLevelMemTracker::bIsDisabled)
	{
		bEnabled = false;
		return;
	}
	LLMCheck(!bIsStatTag);
	Init(NAME_None, TagEnum, true, false, Amount, TrackerToPause, InAllocType);
}

void FLLMPauseScope::Init(FName TagName, ELLMTag EnumTag, bool bIsEnumTag, bool bIsStatTag, uint64 Amount, ELLMTracker TrackerToPause, ELLMAllocType InAllocType)
{
	FLowLevelMemTracker& LLMRef = FLowLevelMemTracker::Get();
	// We have to check bIsDisabled again after calling Get, because the constructor is called from Get, and will set bIsDisabled=false if the platform doesn't support it
	if (FLowLevelMemTracker::bIsDisabled)
	{
		bEnabled = false;
		return;
	}
	LLMRef.BootstrapInitialise();
	if (!LLMRef.IsTagSetActive(ELLMTagSet::None))
	{
		bEnabled = false;
		return;
	}

	bEnabled = true;
	PausedTracker = TrackerToPause;
	AllocType = InAllocType;

	for (int32 TrackerIndex = 0; TrackerIndex < static_cast<int32>(ELLMTracker::Max); TrackerIndex++)
	{
		ELLMTracker Tracker = (ELLMTracker)TrackerIndex;

		if (PausedTracker == ELLMTracker::Max || PausedTracker == Tracker)
		{
			if (Amount == 0)
			{
				LLMRef.GetTracker(Tracker)->Pause(InAllocType);
			}
			else
			{
				if (bIsEnumTag)
				{
					LLMRef.GetTracker(Tracker)->PauseAndTrackMemory(EnumTag, static_cast<int64>(Amount), InAllocType);
				}
				else
				{
					LLMRef.GetTracker(Tracker)->PauseAndTrackMemory(TagName, bIsStatTag, static_cast<int64>(Amount), InAllocType);
				}
			}
		}
	}
}

FLLMPauseScope::~FLLMPauseScope()
{
	if (!bEnabled)
	{
		return;
	}
	FLowLevelMemTracker& LLMRef = FLowLevelMemTracker::Get();

	for (int32 TrackerIndex = 0; TrackerIndex < static_cast<int32>(ELLMTracker::Max); TrackerIndex++)
	{
		ELLMTracker Tracker = static_cast<ELLMTracker>(TrackerIndex);

		if (PausedTracker == ELLMTracker::Max || Tracker == PausedTracker)
		{
			LLMRef.GetTracker(Tracker)->Unpause(AllocType);
		}
	}
}


FLLMScopeFromPtr::FLLMScopeFromPtr(void* Ptr, ELLMTracker InTracker )
{
	using namespace UE::LLMPrivate;
	if (FLowLevelMemTracker::bIsDisabled)
	{
		bEnabled = false;
		return;
	}
	if(Ptr == nullptr)
	{
		bEnabled = false;
		return;
	}

	FLowLevelMemTracker& LLMRef = FLowLevelMemTracker::Get();
	// We have to check bIsDisabled again after calling Get, because the constructor is called from Get, and will set bIsDisabled=false if the platform doesn't support it
	if (FLowLevelMemTracker::bIsDisabled)
	{
		bEnabled = false;
		return;
	}
	LLMRef.BootstrapInitialise();

	FLLMTracker* TrackerData = LLMRef.GetTracker(InTracker);
	const FTagData* TagData = TrackerData->FindTagForPtr(Ptr);
	if (!TagData)
	{
		bEnabled = false;
		return;
	}

	Tracker = InTracker;
	bEnabled = true;

	TrackerData->PushTag(TagData);
}

FLLMScopeFromPtr::~FLLMScopeFromPtr()
{
	if (!bEnabled)
	{
		return;
	}

	FLowLevelMemTracker& LLMRef = FLowLevelMemTracker::Get();
	LLMRef.GetTracker(Tracker)->PopTag();
}

FLLMTagDeclaration::FLLMTagDeclaration(const TCHAR* InCPPName, FName InDisplayName, FName InParentTagName, FName InStatName, FName InSummaryStatName)
	:CPPName(InCPPName), UniqueName(NAME_None), DisplayName(InDisplayName), ParentTagName(InParentTagName), StatName(InStatName), SummaryStatName(InSummaryStatName)
{
	Register();
}

void FLLMTagDeclaration::ConstructUniqueName()
{
	FString NameBuffer(CPPName);
	NameBuffer.ReplaceCharInline(TEXT('_'), TEXT('/'));
	UniqueName = FName(*NameBuffer);
}

namespace UE
{
namespace LLMPrivate
{
namespace LLMTagDeclarationInternal
{

TArray<FLLMTagDeclaration::FCreationCallback, TInlineAllocator<2>>& GetCreationCallbacks()
{
	static TArray<FLLMTagDeclaration::FCreationCallback, TInlineAllocator<2>> CreationCallbacks;
	return CreationCallbacks;
}

FLLMTagDeclaration*& GetList()
{
	static FLLMTagDeclaration* List = nullptr;
	return List;
}

}
}
}

void FLLMTagDeclaration::AddCreationCallback(FCreationCallback InCallback)
{
	TArray<FCreationCallback, TInlineAllocator<2>>& CreationCallbacks =
		UE::LLMPrivate::LLMTagDeclarationInternal::GetCreationCallbacks();
	if (CreationCallbacks.Num() >= CreationCallbacks.Max())
	{
		check(false); // We are not allowed to allocate memory here; you need to increase the allocation size
	}
	else
	{
		CreationCallbacks.Add(InCallback);
	}
}

void FLLMTagDeclaration::ClearCreationCallbacks()
{
	TArray<FCreationCallback, TInlineAllocator<2>>& CreationCallbacks =
		UE::LLMPrivate::LLMTagDeclarationInternal::GetCreationCallbacks();
	CreationCallbacks.Empty();
}

TArrayView<FLLMTagDeclaration::FCreationCallback> FLLMTagDeclaration::GetCreationCallbacks()
{
	return UE::LLMPrivate::LLMTagDeclarationInternal::GetCreationCallbacks();
}

FLLMTagDeclaration* FLLMTagDeclaration::GetList()
{
	return UE::LLMPrivate::LLMTagDeclarationInternal::GetList();
}

void FLLMTagDeclaration::Register()
{
	for (FCreationCallback CreationCallback : GetCreationCallbacks())
	{
		CreationCallback(*this);
	}
	FLLMTagDeclaration*& List = UE::LLMPrivate::LLMTagDeclarationInternal::GetList();
	Next = List;
	List = this;
}

namespace UE
{
namespace LLMPrivate
{
#if DO_CHECK

	bool HandleAssert(bool bLog, const TCHAR* Format, ...)
	{
		if (bLog)
		{
			TCHAR DescriptionString[4096];
			GET_VARARGS(DescriptionString, UE_ARRAY_COUNT(DescriptionString), UE_ARRAY_COUNT(DescriptionString) - 1, Format, Format);

			FPlatformMisc::LowLevelOutputDebugString(DescriptionString);

			if (FPlatformMisc::IsDebuggerPresent())
				FPlatformMisc::PromptForRemoteDebugging(true);

			UE_DEBUG_BREAK();
		}
		return false;
	}

#endif

	const TCHAR* ToString(UE::LLMPrivate::ETagReferenceSource ReferenceSource)
	{
		switch (ReferenceSource)
		{
		case ETagReferenceSource::Scope:
			return TEXT("LLM_SCOPE");
		case ETagReferenceSource::Declare:
			return TEXT("LLM_DEFINE_TAG");
		case ETagReferenceSource::EnumTag:
			return TEXT("LLM_ENUM_GENERIC_TAGS");
		case ETagReferenceSource::CustomEnumTag:
			return TEXT("RegisterPlatformTag/RegisterProjectTag");
		case ETagReferenceSource::FunctionAPI:
			return TEXT("DefaultName/InternalCall");
		case ETagReferenceSource::ImplicitParent:
			return TEXT("ImplicitParent");
		default:
			return TEXT("Invalid");
		}
	}

	void ValidateUniqueName(FStringView UniqueName)
	{
		// Characters that are invalid for c++ names are invalid (other than /), since we use uniquenames (with / replaced by _) as part of the name of the auto-constructed FLLMTagDeclaration variables
		// _ is invalid since we use an _ to indicate a / in FLLMTagDeclaration
		// So only Alnum characters or / are allowed, and the first character can not be a number
		if (UniqueName.Len() == 0)
		{
			LLMCheckf(false, TEXT("Invalid length-zero Tag Unique Name"));
		}
		else
		{
			LLMCheckf(!TChar<TCHAR>::IsDigit(UniqueName[0]), TEXT("Invalid first character is digit in Tag Unique Name '%.*s'"), UniqueName.Len(), UniqueName.GetData());
		}
		for (TCHAR c : UniqueName)
		{
			if (!TChar<TCHAR>::IsAlnum(c) && c != TEXT('/'))
			{
				LLMCheckf(false, TEXT("Invalid character %c in Tag Unique Name '%.*s'"), c, UniqueName.Len(), UniqueName.GetData());
			}
		}
	}


	namespace AllocatorPrivate
	{
		/**
		 * When a Page is allocated, it splits the memory of the page up into blocks, and creates an FAlloc at the start of each block. All the FAllocs are joined together in a FreeList linked list.
		 * When a Page allocates memory, it takes an FAlloc from the freelist and gives it to the caller, and forgets about it.
		 * When the caller returns a pointer, the Page restores the FAlloc at the beginning of the block and puts it back on the FreeList.
		 */
		struct FAlloc
		{
			FAlloc* Next;
		};

		/**
		 * An FPage holds a single page of memory received from the OS; all pages are of the same size.
		 * FPages are owned by FBins, and the FPages for an FBin divide the page up into blocks of the FBin's size.
		 * An FPage keeps track of the blocks it has not yet given out so it can allocate, and keeps track of how many blocks it has given out, so that it can be freed when no longer used.
		 * Pages that are neither free nor empty (and thus are available for allocating from) are kept in a doubly-linked list on the FBin.
		 */
		struct FPage
		{
			FPage(int32 PageSize, int32 BinSize);
			void* Allocate();
			void Free(void* Ptr);
			bool IsFull() const;
			bool IsEmpty() const;
			void AddToList(FPage*& Head);
			void RemoveFromList(FPage*& Head);

			FAlloc* FreeList;
			FPage* Prev;
			FPage* Next;
			int32 UsedCount;
		};

		/**
		 * An FBin handles all allocations that fit into its size range. It's size is set to the power of two at the top of its size range.
		 * The FBin allocates one FPage at a time from the OS; the FPage gets split up into blocks and handles providing a block for callers requesting a pointer.
		 * The FBin has a doubly-linked list of pages that are in use but are not yet full. It provides allocations from these pages.
		 * When an FPage gets full, the FBin forgets about it, counting on the caller to give the pointer to the page back when it frees the pointer and the page becomes non-full again.
		 * When an FBin has no more non-full pages and needs to satisfy an alloc, it allocates a new page.
		 * When a page becomes unused due to a free, the FBin frees the page, returning it to the OS.
		 */
		struct FBin
		{
			FBin(int32 InBinSize);
			void* Allocate(FLLMAllocator& Allocator);
			void Free(void* Ptr, FLLMAllocator& Allocator);

			FPage* FreePages;
			int32 UsedCount;
			int32 BinSize;
		};
	}

	FLLMAllocator*& FLLMAllocator::Get()
	{
		static FLLMAllocator* Allocator = nullptr;
		return Allocator;
	}

	FLLMAllocator::FLLMAllocator()
		: PlatformAlloc(nullptr)
		, PlatformFree(nullptr)
		, Bins(nullptr)
		, Total(0)
		, PageSize(0)
		, NumBins(0)
	{
	}

	FLLMAllocator::~FLLMAllocator()
	{
		Clear();
	}

	void FLLMAllocator::Initialise(LLMAllocFunction InAlloc, LLMFreeFunction InFree, int32 InPageSize)
	{
		using namespace UE::LLMPrivate::AllocatorPrivate;

		PlatformAlloc = InAlloc;
		PlatformFree = InFree;
		PageSize = InPageSize;

		if (PlatformAlloc)
		{
			constexpr int32 MinBinSizeForAlignment = 16;
			constexpr int32 MinBinSizeForAllocStorage = static_cast<int32>(sizeof(FAlloc));
			constexpr int32 MultiplierBetweenBins = 2;
			// Setting MultiplierAfterLastBin=2 would be useless because the PageSize/2 bin would only get a single allocation out of each page due to the FPage data taking up the first half
			// TODO: For bins >= 4*FPage size, allocate FPages in a separate list rather than embedding them. This will require allocating extra space in each allocation to store its page pointer.
			constexpr int32 MultiplierAfterLastBin = 4;

			int32 MinBinSize = FMath::Max(MinBinSizeForAllocStorage, MinBinSizeForAlignment);
			int32 MaxBinSize = InPageSize / MultiplierAfterLastBin;
			int32 BinSize = MinBinSize;
			while (BinSize <= MaxBinSize)
			{
				BinSize *= MultiplierBetweenBins;
				++NumBins;
			}
			if (NumBins > 0)
			{
				Bins = reinterpret_cast<FBin*>(AllocPages(NumBins * sizeof(FBin)));
				BinSize = MinBinSize;
				for (int32 BinIndex = 0; BinIndex < NumBins; ++BinIndex)
				{
					new (&Bins[BinIndex]) FBin(BinSize);
					BinSize *= MultiplierBetweenBins;
				}
			}
		}
	}

	void FLLMAllocator::Clear()
	{
		using namespace UE::LLMPrivate::AllocatorPrivate;

		if (NumBins)
		{
			for (int32 BinIndex = 0; BinIndex < NumBins; ++BinIndex)
			{
				LLMCheck(Bins[BinIndex].UsedCount == 0);
				Bins[BinIndex].~FBin();
			}
			FreePages(Bins, NumBins * sizeof(FBin));
			Bins = nullptr;
			NumBins = 0;
		}
	}

	void* FLLMAllocator::Malloc(size_t Size)
	{
		return Alloc(Size);
	}

	void* FLLMAllocator::Alloc(size_t Size)
	{
		using namespace UE::LLMPrivate::AllocatorPrivate;

		if (Size == 0)
		{
			return nullptr;
		}
		int32 BinIndex = GetBinIndex(Size);
		FScopeLock Lock(&CriticalSection);
		if (BinIndex == NumBins)
		{
			return AllocPages(Size);
		}
		return Bins[BinIndex].Allocate(*this);
	}

	void FLLMAllocator::Free(void* Ptr, size_t Size)
	{
		using namespace UE::LLMPrivate::AllocatorPrivate;

		if (Ptr != nullptr)
		{
			int32 BinIndex = GetBinIndex(Size);
			FScopeLock Lock(&CriticalSection);
			if (BinIndex == NumBins)
			{
				FreePages(Ptr, Size);
			}
			else
			{
				Bins[BinIndex].Free(Ptr, *this);
			}
		}
	}

	void* FLLMAllocator::Realloc(void* Ptr, size_t OldSize, size_t NewSize)
	{
		void* NewPtr;
		if (NewSize)
		{
			NewPtr = Alloc(NewSize);
			if (OldSize)
			{
				size_t CopySize = FMath::Min(OldSize, NewSize);
				FMemory::Memcpy(NewPtr, Ptr, CopySize);
			}
		}
		else
		{
			NewPtr = nullptr;
		}
		Free(Ptr, OldSize);
		return NewPtr;
	}

	int64 FLLMAllocator::GetTotal() const
	{
		FScopeLock Lock((FCriticalSection*)&CriticalSection);
		return Total;
	}

	void* FLLMAllocator::AllocPages(size_t Size)
	{
		Size = Align(Size, PageSize);
		void* Ptr = PlatformAlloc(Size);
		LLMCheck(Ptr);
		LLMCheck((reinterpret_cast<intptr_t>(Ptr) & (PageSize - 1)) == 0);
		Total += Size;
		return Ptr;
	}

	void FLLMAllocator::FreePages(void* Ptr, size_t Size)
	{
		Size = Align(Size, PageSize);
		PlatformFree(Ptr, Size);
		Total -= Size;
	}

	int32 FLLMAllocator::GetBinIndex(size_t Size) const
	{
		int BinIndex = 0;
		while (BinIndex < NumBins && static_cast<size_t>(Bins[BinIndex].BinSize) < Size)
		{
			++BinIndex;
		}
		return BinIndex;
	}

	namespace AllocatorPrivate
	{
		FPage::FPage(int32 PageSize, int32 BinSize)
		{
			Next = Prev = nullptr;
			UsedCount = 0;
			int32 NumHeaderBins = (FMath::Max(static_cast<int32>(sizeof(FPage)), BinSize) + BinSize - 1) / BinSize;
			int32 FreeCount = PageSize / BinSize - NumHeaderBins;

			// Divide the rest of the page after this header into FAllocs, and add all the FAllocs into the free list
			FreeList = reinterpret_cast<FAlloc*>(reinterpret_cast<intptr_t>(this) + NumHeaderBins*BinSize);
			FAlloc* EndAlloc = reinterpret_cast<FAlloc*>(reinterpret_cast<intptr_t>(FreeList) + (FreeCount-1) * BinSize);
			FAlloc* Alloc = FreeList;
			while (Alloc != EndAlloc)
			{
				Alloc->Next = reinterpret_cast<FAlloc*>(reinterpret_cast<intptr_t>(Alloc) + BinSize);
				Alloc = Alloc->Next;
			}
			EndAlloc->Next = nullptr;
		}

		void* FPage::Allocate()
		{
			LLMCheck(FreeList);
			FAlloc* Alloc = FreeList;
			FreeList = Alloc->Next;
			++UsedCount;
			return Alloc;
		}

		void FPage::Free(void* Ptr)
		{
			LLMCheck(UsedCount > 0);
			FAlloc* Alloc = reinterpret_cast<FAlloc*>(Ptr);
			Alloc->Next = FreeList;
			FreeList = Alloc;
			--UsedCount;
		}

		bool FPage::IsFull() const
		{
			return FreeList == nullptr;
		}

		bool FPage::IsEmpty() const
		{
			return UsedCount == 0;
		}

		void FPage::AddToList(FPage*& Head)
		{
			Next = Head;
			Prev = nullptr;
			Head = this;
			if (Next)
			{
				Next->Prev = this;
			}
		}

		void FPage::RemoveFromList(FPage*& Head)
		{
			if (Prev)
			{
				Prev->Next = Next;
				if (Next)
				{
					Next->Prev = Prev;
				}
			}
			else
			{
				Head = Next;
				if (Next)
				{
					Next->Prev = nullptr;
				}
			}
			Next = Prev = nullptr;
		}

		FBin::FBin(int32 InBinSize)
		{
			FreePages = nullptr;
			UsedCount = 0;
			BinSize = InBinSize;
		}

		void* FBin::Allocate(FLLMAllocator& Allocator)
		{
			if (!FreePages)
			{
				FPage* Page = reinterpret_cast<FPage*>(Allocator.AllocPages(Allocator.PageSize));
				++UsedCount;
				LLMCheck(Page);
				new (Page) FPage(Allocator.PageSize, BinSize); // The FPage is at the beginning of the array of PageSize bytes
				Page->AddToList(FreePages);
			}

			void* Result = FreePages->Allocate();
			if (FreePages->IsFull())
			{
				FreePages->RemoveFromList(FreePages); //-V678
			}
			return Result;
		}

		void FBin::Free(void* Ptr, FLLMAllocator& Allocator)
		{
			FPage* Page = reinterpret_cast<FPage*>(reinterpret_cast<intptr_t>(Ptr) & ~(static_cast<intptr_t>(Allocator.PageSize) - 1));
			if (Page->IsFull())
			{
				Page->AddToList(FreePages);
			}
			Page->Free(Ptr);
			if (Page->IsEmpty())
			{
				Page->RemoveFromList(FreePages);
				--UsedCount;
				Allocator.FreePages(Page, Allocator.PageSize);
			}
		}
	}

	FTagData::FTagData(FName InName, FName InDisplayName, FName InParentName, FName InStatName, FName InSummaryStatName, bool bInHasEnumTag, ELLMTag InEnumTag, ETagReferenceSource InReferenceSource)
		: Name(InName), DisplayName(InDisplayName), ParentName(InParentName), StatName(InStatName)
		, SummaryStatName(InSummaryStatName), EnumTag(InEnumTag), ReferenceSource(InReferenceSource)
		, bIsFinishConstructed(false), bParentIsName(true), bHasEnumTag(bInHasEnumTag), bIsReportable(true)
	{
	}

	FTagData::FTagData(FName InName, FName InDisplayName, const FTagData* InParent, FName InStatName, FName InSummaryStatName, bool bInHasEnumTag, ELLMTag InEnumTag, ETagReferenceSource InReferenceSource)
		: FTagData(InName, InDisplayName, NAME_None, InStatName, InSummaryStatName, bInHasEnumTag, InEnumTag, InReferenceSource)
	{
		SetParent(InParent);
	}

	FTagData::FTagData(ELLMTag InEnumTag)
		: FTagData(NAME_None, NAME_None, NAME_None, NAME_None, NAME_None, true, InEnumTag, ETagReferenceSource::EnumTag)
	{
	}

	FTagData::~FTagData()
	{
		if (bParentIsName)
		{
			ParentName.~FName();
			bParentIsName = false;
		}
	}

	bool FTagData::IsFinishConstructed() const
	{
		return bIsFinishConstructed;
	}

	bool FTagData::IsParentConstructed() const
	{
		return !bParentIsName;
	}

	FName FTagData::GetName() const
	{
		return Name;
	}

	FName FTagData::GetDisplayName() const
	{
		return DisplayName;
	}

	FString FTagData::GetDisplayPath() const
	{
		TStringBuilder<256> NameBuffer;
		AppendDisplayPath(NameBuffer);
		return FString(NameBuffer);
	}

	void FTagData::AppendDisplayPath(FStringBuilderBase& Result) const
	{
		if (Parent && Parent->IsUsedAsDisplayParent())
		{
			Parent->AppendDisplayPath(Result);
			Result << TEXT("/");
		}
		DisplayName.AppendString(Result);
	}

	const FTagData* FTagData::GetParent() const
	{
		LLMCheckf(!bParentIsName, TEXT("GetParent called on TagData %s before SetParent was called"), *WriteToString<FName::StringBufferSize>(Name));
		return Parent;
	}

	FName FTagData::GetParentName() const
	{
		LLMCheckf(bParentIsName, TEXT("GetParentName called on TagData %s after SetParent was called"), *WriteToString<FName::StringBufferSize>(Name));
		return ParentName;
	}

	FName FTagData::GetStatName() const
	{
		return StatName;
	}

	FName FTagData::GetSummaryStatName() const
	{
		return SummaryStatName;
	}

	ELLMTag FTagData::GetEnumTag() const
	{
		return EnumTag;
	}

	bool FTagData::HasEnumTag() const
	{
		return bHasEnumTag;
	}

	const FTagData* FTagData::GetContainingEnumTagData() const
	{
		const FTagData* TagData = this;
		do
		{
			if (TagData->bHasEnumTag)
			{
				return TagData;
			}
			TagData = TagData->GetParent();
		} while (TagData);
		LLMCheckf(false, TEXT("TagData is not a descendent of an ELLMTag TagData. All TagDatas must be descendents of ELLMTag::CustomName if they are not descendets of any other ELLMTag"));
		return this;
	}

	ELLMTag FTagData::GetContainingEnum() const
	{
		const FTagData* TagData = GetContainingEnumTagData();
		return TagData->EnumTag;
	}

	ETagReferenceSource FTagData::GetReferenceSource() const
	{
		return ReferenceSource;
	}

	int32 FTagData::GetIndex() const
	{
		return Index;
	}

	bool FTagData::IsReportable() const
	{
		return bIsReportable;
	}

	void FTagData::SetParent(const FTagData* InParent)
	{
		if (bParentIsName)
		{
			ParentName.~FName();
			bParentIsName = false;
		}
		Parent = InParent;
	}

	void FTagData::SetName(FName InName)
	{
		Name = InName;
	}

	void FTagData::SetDisplayName(FName InDisplayName)
	{
		DisplayName = InDisplayName;
	}

	void FTagData::SetStatName(FName InStatName)
	{
		StatName = InStatName;
	}

	void FTagData::SetSummaryStatName(FName InSummaryStatName)
	{
		SummaryStatName = InSummaryStatName;
	}

	void FTagData::SetParentName(FName InParentName)
	{
		LLMCheck(bParentIsName);
		ParentName = InParentName;
	}

	void FTagData::SetFinishConstructed()
	{
		bIsFinishConstructed = true;
	}

	void FTagData::SetIndex(int32 InIndex)
	{
		Index = InIndex;
	}

	void FTagData::SetIsReportable(bool bInIsReportable)
	{
		bIsReportable = bInIsReportable;
	}

	bool FTagData::IsUsedAsDisplayParent() const
	{
		// All Tags but one are UsedAsDisplayParent - their name is prepended during GetDisplayPath
		// ELLMTag::CustomName is the exception. It is set for FName tags that do not have a real parent to provide a containing ELLMTag for them to provide to systems that do not support FName tags.
		// When FName tags without a real parent are displayed, their path should display as parentless despite having the CustomName tag as their parent.
		return !(bHasEnumTag && EnumTag == ELLMTag::CustomName);
	}

	FLLMTracker::FLLMTracker(FLowLevelMemTracker& InLLM)
		: LLMRef(InLLM)
		, TrackedTotal(0)
		, OverrideUntaggedTagData(nullptr)
		, OverrideTrackedTotalTagData(nullptr)
		, LastTrimTime(0.0)
	{
		TlsSlot = FPlatformTLS::AllocTlsSlot();

		for (int32 Index = 0; Index < static_cast<int32>(ELLMAllocType::Count); ++Index)
		{
			AllocTypeAmounts[Index] = 0;
		}
	}

	FLLMTracker::~FLLMTracker()
	{
		Clear();

		FPlatformTLS::FreeTlsSlot(TlsSlot);
	}

	void FLLMTracker::Initialise(
		ELLMTracker Tracker,
		FLLMAllocator* InAllocator)
	{
		CsvWriter.SetTracker(Tracker);
		TraceWriter.SetTracker(Tracker);
		CsvProfilerWriter.SetTracker(Tracker);

		AllocationMap.SetAllocator(InAllocator);
	}

	FLLMThreadState* FLLMTracker::GetOrCreateState()
	{
		// look for already allocated thread state
		FLLMThreadState* State = (FLLMThreadState*)FPlatformTLS::GetTlsValue(TlsSlot);
		// get one if needed
		if (State == nullptr)
		{
			State = LLMRef.Allocator.New<FLLMThreadState>();

			// Add to pending thread states, these will be consumed on the GT
			{
				FScopeLock Lock(&PendingThreadStatesGuard);
				PendingThreadStates.Add(State);
			}

			// push to Tls
			FPlatformTLS::SetTlsValue(TlsSlot, State);
		}
		return State;
	}

	FLLMThreadState* FLLMTracker::GetState()
	{
		return (FLLMThreadState*)FPlatformTLS::GetTlsValue(TlsSlot);
	}

	void FLLMTracker::PushTag(ELLMTag EnumTag)
	{
		const FTagData* TagData = LLMRef.FindOrAddTagData(EnumTag, ETagReferenceSource::Scope);

		// pass along to the state object
		GetOrCreateState()->PushTag(TagData);
	}

	void FLLMTracker::PushTag(FName Tag, bool bInIsStatData)
	{
		const FTagData* TagData = LLMRef.FindOrAddTagData(Tag, bInIsStatData, ETagReferenceSource::Scope);

		// pass along to the state object
		GetOrCreateState()->PushTag(TagData);
	}

	void FLLMTracker::PushTag(const FTagData* TagData)
	{
		// pass along to the state object
		GetOrCreateState()->PushTag(TagData);
	}

	void FLLMTracker::PopTag()
	{
		// look for already allocated thread state
		FLLMThreadState* State = GetState();

		LLMCheckf(State != nullptr, TEXT("Called PopTag but PushTag was never called!"));

		State->PopTag();
	}

#if LLM_ALLOW_ASSETS_TAGS
	void FLLMTracker::PushAssetTag(FName Tag)
	{
		const FTagData* TagData = LLMRef.FindOrAddTagData(Tag, true /* bIsStatTag */, ETagReferenceSource::Scope);

		// pass along to the state object
		GetOrCreateState()->PushAssetTag(TagData);
	}

	void FLLMTracker::PushAssetTag(const FTagData* TagData)
	{
		// pass along to the state object
		GetOrCreateState()->PushAssetTag(TagData);
	}

	void FLLMTracker::PopAssetTag()
	{
		// look for already allocated thread state
		FLLMThreadState* State = GetState();

		LLMCheckf(State != nullptr, TEXT("Called PopTag but PushTag was never called!"));

		State->PopAssetTag();
	}
#endif

	void FLLMTracker::TrackAllocation(const void* Ptr, int64 Size, ELLMTag DefaultEnumTag, ELLMTracker Tracker, ELLMAllocType AllocType, bool bTrackInMemPro)
	{
		FLLMThreadState* State = GetOrCreateState();
		const FTagData* TagData = State->GetTopTag();
		if (!TagData)
		{
			TagData = LLMRef.FindOrAddTagData(DefaultEnumTag);
		}
		TrackAllocation(Ptr, Size, TagData, Tracker, AllocType, State, bTrackInMemPro);
	}

	void FLLMTracker::TrackAllocation(const void* Ptr, int64 Size, FName DefaultTag, ELLMTracker Tracker, ELLMAllocType AllocType, bool bTrackInMemPro)
	{
		FLLMThreadState* State = GetOrCreateState();
		const FTagData* TagData = State->GetTopTag();
		if (!TagData)
		{
			TagData = LLMRef.FindOrAddTagData(DefaultTag);
		}
		TrackAllocation(Ptr, Size, TagData, Tracker, AllocType, State, bTrackInMemPro);
	}

	void FLLMTracker::TrackAllocation(const void* Ptr, int64 Size, const FTagData* ActiveTagData, ELLMTracker Tracker, ELLMAllocType AllocType, FLLMThreadState* State, bool bTrackInMemPro)
	{
		if (IsPaused(AllocType))
		{
			// When Paused, we do not track any new allocations and we we do not update the counters for the memory they use; the code that triggered the pause is responsible for updating those counters
			// Since we do not track the allocations, TrackFree will likewise not update the counters when those allocations are freed
			return;
		}

		// track the total quickly
		FPlatformAtomics::InterlockedAdd(&TrackedTotal, Size);
	
#if !LLM_ENABLED_FULL_TAGS
		// When full tags are disabled, we instead store the top-level enumtag parent of the tag used by each allocation
		ActiveTagData = ActiveTagData->GetContainingEnumTagData();
#endif

#if LLM_ALLOW_ASSETS_TAGS
		const FTagData* AssetTagData = State->GetTopAssetTag();
#else
		const FTagData* AssetTagData = nullptr;
#endif

		// track on the thread state
		State->TrackAllocation(Ptr, Size, Tracker, AllocType, ActiveTagData, AssetTagData, bTrackInMemPro);

		// tracking a nullptr with a Size is allowed, but we don't need to remember it, since we can't free it ever
		if (Ptr != nullptr)
		{
			// remember the size and tag info
			FLLMTracker::FLowLevelAllocInfo AllocInfo;
			AllocInfo.SetTag(ActiveTagData, LLMRef);
#if LLM_ALLOW_ASSETS_TAGS
			AllocInfo.SetAssetTag(AssetTagData, LLMRef);
#endif
			LLMCheck(Size <= 0x0000'ffff'ffff'ffff);
			uint32 SizeLow = uint32(Size);
			uint16 SizeHigh = uint16(Size >> 32ull);
			PointerKey Key(Ptr, SizeHigh);
			AllocationMap.Add(Key, SizeLow, AllocInfo);
		}
	}

	void FLLMTracker::TrackFree(const void* Ptr, ELLMTracker Tracker, ELLMAllocType AllocType, bool bTrackInMemPro)
	{
		// look up the pointer in the tracking map
		FLLMAllocMap::Values Values;
		{
			if (!AllocationMap.Remove(PointerKey(Ptr), Values))
			{
				return;
			}
		}

		if (IsPaused(AllocType))
		{
			// When Paused, we remove our data for any freed allocations, but we do not update the counters for the memory they used; the code that triggered the pause is responsible for updating those counters
			return;
		}

		int64 SizeLow = static_cast<int64>(Values.Value1);
		int64 SizeHigh = Values.Key.GetExtraData();
		int64 Size = (SizeHigh << 32ull) | SizeLow;
		FLLMTracker::FLowLevelAllocInfo& AllocInfo = Values.Value2;

		// track the total quickly
		FPlatformAtomics::InterlockedAdd(&TrackedTotal, -Size);

		FLLMThreadState* State = GetOrCreateState();
		const FTagData* TagData = AllocInfo.GetTag(LLMRef);
#if LLM_ALLOW_ASSETS_TAGS
		const FTagData* AssetTagData = AllocInfo.GetAssetTag(LLMRef);
#else
		const FTagData* AssetTagData = nullptr;
#endif

		State->TrackFree(Ptr, Size, Tracker, AllocType, TagData, AssetTagData, bTrackInMemPro);
	}

	void FLLMTracker::OnAllocMoved(const void* Dest, const void* Source, ELLMTracker Tracker, ELLMAllocType AllocType)
	{
		FLLMAllocMap::Values Values;
		{
			if (!AllocationMap.Remove(PointerKey(Source), Values))
			{
				return;
			}

			PointerKey Key(Dest, uint16(Values.Key.GetExtraData()));
			AllocationMap.Add(Key, Values.Value1, Values.Value2);
		}

		if (IsPaused(AllocType))
		{
			// When Paused, don't update counters in case any of the external tracking systems are not available
			return;
		}

		int64 SizeLow = static_cast<int64>(Values.Value1);
		int64 SizeHigh = Values.Key.GetExtraData();
		int64 Size = (SizeHigh << 32ull) | SizeLow;
		const FLLMTracker::FLowLevelAllocInfo& AllocInfo = Values.Value2;
		const FTagData* TagData = AllocInfo.GetTag(LLMRef);

		FLLMThreadState* State = GetOrCreateState();
		State->TrackMoved(Dest, Source, Size, Tracker, TagData);
	}

	void FLLMTracker::TrackMemory(ELLMTag Tag, int64 Amount, ELLMAllocType AllocType)
	{
		TrackMemory(LLMRef.FindOrAddTagData(Tag), Amount, AllocType);
	}

	void FLLMTracker::TrackMemory(FName Tag, int64 Amount, ELLMAllocType AllocType)
	{
		TrackMemory(LLMRef.FindOrAddTagData(Tag), Amount, AllocType);
	}

	void FLLMTracker::TrackMemory(const FTagData* TagData, int64 Amount, ELLMAllocType AllocType)
	{
		FLLMThreadState* State = GetOrCreateState();
		FScopeLock Lock(&State->TagSection);
		State->IncrTag(TagData, Amount);
		State->AllocTypeAmounts[static_cast<int32>(AllocType)] += Amount;
		FPlatformAtomics::InterlockedAdd(&TrackedTotal, Amount);
	}

	void FLLMTracker::PauseAndTrackMemory(FName TagName, bool bInIsStatTag, int64 Amount, ELLMAllocType AllocType)
	{
		const FTagData* TagData = LLMRef.FindOrAddTagData(TagName, bInIsStatTag);
		PauseAndTrackMemory(TagData, Amount, AllocType);
	}

	void FLLMTracker::PauseAndTrackMemory(ELLMTag EnumTag, int64 Amount, ELLMAllocType AllocType)
	{
		const FTagData* TagData = LLMRef.FindOrAddTagData(EnumTag);
		PauseAndTrackMemory(TagData, Amount, AllocType);
	}

	// This will pause/unpause tracking, and also manually increment a given tag
	void FLLMTracker::PauseAndTrackMemory(const FTagData* TagData, int64 Amount, ELLMAllocType AllocType)
	{
		FLLMThreadState* State = GetOrCreateState();
		FScopeLock Lock(&State->TagSection);
		State->PausedCounter[static_cast<int32>(AllocType)]++;
		State->IncrTag(TagData, Amount);
		State->AllocTypeAmounts[static_cast<int32>(AllocType)] += Amount;
		FPlatformAtomics::InterlockedAdd(&TrackedTotal, Amount);
	}

	void FLLMTracker::Pause(ELLMAllocType AllocType)
	{
		FLLMThreadState* State = GetOrCreateState();
		State->PausedCounter[static_cast<int32>(AllocType)]++;
	}

	void FLLMTracker::Unpause(ELLMAllocType AllocType)
	{
		FLLMThreadState* State = GetOrCreateState();
		State->PausedCounter[static_cast<int32>(AllocType)]--;
		LLMCheck( State->PausedCounter[static_cast<int32>(AllocType)] >= 0 );
	}

	bool FLLMTracker::IsPaused(ELLMAllocType AllocType)
	{
		FLLMThreadState* State = GetState();
		// pause during shutdown, as the external trackers might not be able to robustly handle tracking once we start shutting down
		return IsEngineExitRequested() || (State == nullptr ? false : (State->PausedCounter[static_cast<int32>(ELLMAllocType::None)]>0) || (State->PausedCounter[static_cast<int32>(AllocType)])>0);
	}

	void FLLMTracker::Clear()
	{
		{
			FScopeLock Lock(&PendingThreadStatesGuard);
			for (FLLMThreadState* ThreadState : PendingThreadStates)
				LLMRef.Allocator.Delete(ThreadState);
			PendingThreadStates.Empty();
		}

		for (FLLMThreadState* ThreadState : ThreadStates)
			LLMRef.Allocator.Delete(ThreadState);
		ThreadStates.Empty();

		AllocationMap.Clear();

		CsvWriter.Clear();
		TraceWriter.Clear();
		CsvProfilerWriter.Clear();
	}

	void FLLMTracker::OnPreFork()
	{
		CsvWriter.OnPreFork();
	}

	void FLLMTracker::SetTotalTags(const FTagData* InOverrideUntaggedTagData, const FTagData* InOverrideTrackedTotalTagData)
	{
		OverrideUntaggedTagData = InOverrideUntaggedTagData;
		OverrideTrackedTotalTagData = InOverrideTrackedTotalTagData;
	}

	void FLLMTracker::Update()
	{
		UpdateThreads();
		double CurrentTime = FPlatformTime::Seconds();
		constexpr double UpdateTrimPeriod = 10.0;
		bool bTrimAllocations = CurrentTime - LastTrimTime > UpdateTrimPeriod;
		if (bTrimAllocations)
		{
			LastTrimTime = CurrentTime;
			{
				AllocationMap.Trim();
			}
		}

		// Add the values from each thread to the central repository
		bool bTrimThreads = bTrimAllocations;
		for (FLLMThreadState* ThreadState : ThreadStates)
		{
			ThreadState->PropagateChildSizesToParents();
			ThreadState->FetchAndClearTagSizes(TagSizes, AllocTypeAmounts, bTrimThreads);
		}

		// Update peak sizes and external sizes in the central repository
		for (TPair<const FTagData*, FTrackerTagSizeData>& It : TagSizes)
		{
			FTrackerTagSizeData& AllocationData = It.Value;

			// Update external amount
			if (AllocationData.bExternalValid)
			{
				if (AllocationData.bExternalAddToTotal)
				{
					FPlatformAtomics::InterlockedAdd(&TrackedTotal, AllocationData.ExternalAmount - AllocationData.Size);
				}
				AllocationData.Size = AllocationData.ExternalAmount;
				AllocationData.bExternalValid = false;
			}

			// Calculate peaks
#if LLM_ENABLED_TRACK_PEAK_MEMORY
			// @todo we should be keeping track of the intra-frame memory peak for the total tracked memory.
			// For now we will just use the memory at the time the update happens since there are threading implications to being accurate.
			AllocationData.PeakSize = FMath::Max(AllocationData.PeakSize, AllocationData.Size);
#endif
		}
	}

	void FLLMTracker::UpdateThreads()
	{
		// Consume pending thread states
		// We must be careful to do all allocations outside of the PendingThreadStatesGuard guard as that can lead to a deadlock due to contention with PendingThreadStatesGuard & Locks inside the underlying allocator (i.e. MallocBinned2 -> Mutex)
		{
			PendingThreadStatesGuard.Lock();
			const int32 NumPendingThreadStatesToConsume = PendingThreadStates.Num();
			if (NumPendingThreadStatesToConsume > 0)
			{
				PendingThreadStatesGuard.Unlock();
				ThreadStates.Reserve(ThreadStates.Num() + NumPendingThreadStatesToConsume);
				PendingThreadStatesGuard.Lock();

				for (int32 i = 0; i < NumPendingThreadStatesToConsume; ++i)
				{
					ThreadStates.Add(PendingThreadStates.Pop(false /*bAllowShrinking*/));
				}
			}
			PendingThreadStatesGuard.Unlock();
		}
	}

	void FLLMTracker::PublishStats(bool bTrackPeaks)
	{
		if (OverrideTrackedTotalTagData)
		{
			SetMemoryStatByFName(OverrideTrackedTotalTagData->GetStatName(), TrackedTotal);
			SetMemoryStatByFName(OverrideTrackedTotalTagData->GetSummaryStatName(), TrackedTotal);
		}

		if (OverrideUntaggedTagData)
		{
			const FTagData* TagData = LLMRef.FindTagData(TagName_Untagged);
			const FTrackerTagSizeData* AllocationData = TagData ? TagSizes.Find(TagData) : nullptr;
			SetMemoryStatByFName(OverrideUntaggedTagData->GetStatName(), AllocationData ? AllocationData->GetSize(bTrackPeaks) : 0);
			SetMemoryStatByFName(OverrideUntaggedTagData->GetSummaryStatName(), AllocationData ? AllocationData->GetSize(bTrackPeaks) : 0);
		}

		for (const TPair<const FTagData*, FTrackerTagSizeData>& It : TagSizes)
		{
			const FTagData* TagData = It.Key;
			if (!TagData->IsReportable())
			{
				continue;
			}
			if (OverrideUntaggedTagData && TagData->GetName() == TagName_Untagged)
			{
				// Handled separately by OverrideUntaggedTagData
				continue;
			}
			const FTrackerTagSizeData& AllocationData = It.Value;
			int64 Amount = AllocationData.GetSize(bTrackPeaks);

			SetMemoryStatByFName(TagData->GetStatName(), Amount);
			SetMemoryStatByFName(TagData->GetSummaryStatName(), Amount);
		}
	}

	void FLLMTracker::PublishCsv(bool bTrackPeaks)
	{
		CsvWriter.Publish(LLMRef, TagSizes, OverrideTrackedTotalTagData, OverrideUntaggedTagData, TrackedTotal, bTrackPeaks);
	}

	void FLLMTracker::PublishTrace(bool bTrackPeaks)
	{
		TraceWriter.Publish(LLMRef, TagSizes, OverrideTrackedTotalTagData, OverrideUntaggedTagData, TrackedTotal, bTrackPeaks);
	}

	void FLLMTracker::PublishCsvProfiler(bool bTrackPeaks)
	{
		CsvProfilerWriter.Publish(LLMRef, TagSizes, OverrideTrackedTotalTagData, OverrideUntaggedTagData, TrackedTotal, bTrackPeaks);
	}

	void FLLMTracker::OnTagsResorted(FTagDataArray& OldTagDatas)
	{
#if LLM_ENABLED_FULL_TAGS
		{
			// Each allocation references the tag by its index, which we have just remapped.
			// Remap each allocation's tag index to the new index for the tag
			AllocationMap.LockAll();
			for (const FLLMAllocMap::FTuple& Tuple : AllocationMap)
			{
				Tuple.Value2.SetCompressedTag(OldTagDatas[Tuple.Value2.GetCompressedTag()]->GetIndex());
			}
			AllocationMap.UnlockAll();
		}
#else
		// Values in AllocationMap are ELLMTags, and don't depend on the Index of the tagdatas
#endif

		// Update the uses of Index in the ThreadStates
		for (FLLMThreadState* ThreadState : ThreadStates)
		{
			ThreadState->OnTagsResorted(OldTagDatas);
		}
	}

	void FLLMTracker::LockAllThreadTags(bool bLock)
	{
		if (bLock)
		{
			UpdateThreads();
			PendingThreadStatesGuard.Lock();
		}

		for (FLLMThreadState* ThreadState : ThreadStates)
		{
			ThreadState->LockTags(bLock);
		}

		if (!bLock)
		{
			PendingThreadStatesGuard.Unlock();
		}
	}

	const FTagData* FLLMTracker::GetActiveTagData()
	{
		FLLMThreadState* State = GetOrCreateState();
		return State->GetTopTag();
	}

	const FTagData* FLLMTracker::FindTagForPtr( void* Ptr )
	{
		FLLMThreadState* State = GetOrCreateState();
		FLowLevelAllocInfo AllocInfo;
		{
			uint32* Size;
			FLowLevelAllocInfo* AllocInfoPtr;
			AllocationMap.Find(PointerKey(Ptr), Size, AllocInfoPtr);
			if (!AllocInfoPtr)
			{
				return nullptr;
			}
			AllocInfo = *AllocInfoPtr;
		}
		return AllocInfo.GetTag(LLMRef);
	}

	TArray<const FTagData*> FLLMTracker::GetTagDatas()
	{
		TArray<const FTagData*> FoundTagDatas;
		TagSizes.GetKeys(FoundTagDatas);
		return FoundTagDatas;
	}

	int64 FLLMTracker::GetTagAmount(const FTagData* TagData, bool bPeakAmount/* = false*/) const
	{
		const FTrackerTagSizeData* AllocationData = TagSizes.Find(TagData);
		if (AllocationData)
		{
			return AllocationData->GetSize(bPeakAmount);
		}
		else
		{
			return 0;
		}
	}

	void FLLMTracker::SetTagAmountExternal(const FTagData* TagData, int64 Amount, bool bAddToTotal)
	{
		FTrackerTagSizeData& AllocationData = TagSizes.FindOrAdd(TagData);
		AllocationData.bExternalValid = true;
		AllocationData.bExternalAddToTotal = bAddToTotal;
		AllocationData.ExternalAmount = Amount;
	}

	void FLLMTracker::SetTagAmountInUpdate(const FTagData* TagData, int64 Amount, bool bAddToTotal)
	{
		FTrackerTagSizeData& AllocationData = TagSizes.FindOrAdd(TagData);
		if (bAddToTotal)
		{
			FPlatformAtomics::InterlockedAdd(&TrackedTotal, Amount - AllocationData.Size);
		}
		AllocationData.Size = Amount;
#if LLM_ENABLED_TRACK_PEAK_MEMORY
		AllocationData.PeakSize = FMath::Max(AllocationData.PeakSize, AllocationData.Size);
#endif
	}

	int64 FLLMTracker::GetAllocTypeAmount(ELLMAllocType AllocType)
	{
		return AllocTypeAmounts[static_cast<int32>(AllocType)];
	}

	FLLMThreadState::FLLMThreadState()
	{
		for (int32 Index = 0; Index < static_cast<int32>(ELLMAllocType::Count); ++Index)
		{
			PausedCounter[Index] = 0;
		}

		ClearAllocTypeAmounts();
	}

	void FLLMThreadState::Clear()
	{
		TagStack.Empty();
#if LLM_ALLOW_ASSETS_TAGS
		AssetTagStack.Empty();
#endif
		Allocations.Empty();
		ClearAllocTypeAmounts();
	}

	void FLLMThreadState::PushTag(const FTagData* TagData)
	{
		FScopeLock Lock(&TagSection);

		// push a tag
		TagStack.Add(TagData);
	}

	void FLLMThreadState::PopTag()
	{
		FScopeLock Lock(&TagSection);

		LLMCheckf(TagStack.Num() > 0, TEXT("Called FLLMThreadState::PopTag without a matching Push (stack was empty on pop)"));
		TagStack.Pop(false /* bAllowShrinking */);
	}

	const FTagData* FLLMThreadState::GetTopTag()
	{
		return TagStack.Num() ? TagStack.Last() : nullptr;
	}

#if LLM_ALLOW_ASSETS_TAGS
	void FLLMThreadState::PushAssetTag(const FTagData* TagData)
	{
		FScopeLock Lock(&TagSection);

		// push a tag
		AssetTagStack.Add(TagData);
	}

	void FLLMThreadState::PopAssetTag()
	{
		FScopeLock Lock(&TagSection);

		LLMCheckf(AssetTagStack.Num() > 0, TEXT("Called FLLMThreadState::PopTag without a matching Push (stack was empty on pop)"));
		AssetTagStack.Pop(false /* bAllowShrinking */);
	}

	const FTagData* FLLMThreadState::GetTopAssetTag()
	{
		FScopeLock Lock(&TagSection);
		return AssetTagStack.Num() ? AssetTagStack.Last() : nullptr;
	}
#endif

	void FLLMThreadState::IncrTag(const FTagData* TagData, int64 Amount)
	{
		// Caller is responsible for holding a lock on TagSection
		FThreadTagSizeData& AllocationSize = Allocations.FindOrAdd(TagData->GetIndex());
		AllocationSize.TagData = TagData;
		AllocationSize.Size += Amount;
	}

	void FLLMThreadState::TrackAllocation(const void* Ptr, int64 Size, ELLMTracker Tracker, ELLMAllocType AllocType, const FTagData* TagData, const FTagData* AssetTagData, bool bTrackInMemPro)
	{
		FScopeLock Lock(&TagSection);

		AllocTypeAmounts[static_cast<int32>(AllocType)] += Size;

		IncrTag(TagData, Size);
#if LLM_ALLOW_ASSETS_TAGS
		if (AssetTagData)
		{
			IncrTag(AssetTagData, Size);
		}
#endif

		ELLMTag EnumTag = TagData->GetContainingEnum();
		if (Tracker == ELLMTracker::Default)
		{
			FPlatformMemory::OnLowLevelMemory_Alloc(Ptr, static_cast<uint64>(Size), static_cast<uint64>(EnumTag));
		}

#if MEMPRO_ENABLED
		if (FMemProProfiler::IsTrackingTag(EnumTag) && bTrackInMemPro)
		{
			MEMPRO_TRACK_ALLOC(const_cast<void*>(Ptr), static_cast<size_t>(Size));
		}
#endif
	}

	void FLLMThreadState::TrackFree(const void* Ptr, int64 Size, ELLMTracker Tracker, ELLMAllocType AllocType, const FTagData* TagData, const FTagData* AssetTagData, bool bTrackInMemPro)
	{
		FScopeLock Lock(&TagSection);

		AllocTypeAmounts[static_cast<int32>(AllocType)] -= Size;

		IncrTag(TagData, -Size);
#if LLM_ALLOW_ASSETS_TAGS
		if (AssetTagData)
		{
			IncrTag(AssetTagData, -Size);
		}
#endif
		ELLMTag EnumTag = TagData->GetContainingEnum();
		if (Tracker == ELLMTracker::Default)
		{
			FPlatformMemory::OnLowLevelMemory_Free(Ptr, static_cast<uint64>(Size), static_cast<uint64>(EnumTag));
		}

#if MEMPRO_ENABLED
		if (FMemProProfiler::IsTrackingTag(EnumTag) && bTrackInMemPro)
		{
			MEMPRO_TRACK_FREE(const_cast<void*>(Ptr));
		}
#endif
	}

	void FLLMThreadState::TrackMoved(const void* Dest, const void* Source, int64 Size, ELLMTracker Tracker, const FTagData* TagData)
	{
		// update external memory trackers (ideally would want a proper 'move' option on these)
		ELLMTag EnumTag = TagData->GetContainingEnum();
		if (Tracker == ELLMTracker::Default)
		{
			FPlatformMemory::OnLowLevelMemory_Free(Source, static_cast<uint64>(Size), static_cast<uint64>(EnumTag));
			FPlatformMemory::OnLowLevelMemory_Alloc(Dest, static_cast<uint64>(Size), static_cast<uint64>(EnumTag));
		}

#if MEMPRO_ENABLED
		if (FMemProProfiler::IsTrackingTag(EnumTag))
		{
			MEMPRO_TRACK_FREE(const_cast<void*>(Source));
			MEMPRO_TRACK_ALLOC(const_cast<void*>(Dest), static_cast<size_t>(Size));
		}
#endif
	}

	void FLLMThreadState::PropagateChildSizesToParents()
	{
		FScopeLock Lock(&TagSection);

		// Make sure all parents of any TagDatas in the Allocations are also present
		FConstTagDataArray ParentsToAdd;
		for (const TPair<int32, FThreadTagSizeData>& It : Allocations)
		{
			const FTagData* TagData = It.Value.TagData;
			const FTagData* ParentData = TagData->GetParent();
			while (ParentData && !Allocations.Contains(ParentData->GetIndex()))
			{
				ParentsToAdd.Add(ParentData);
				ParentData = ParentData->GetParent();
			}
		}
		for (const FTagData* TagData : ParentsToAdd)
		{
			FThreadTagSizeData& Info = Allocations.FindOrAdd(TagData->GetIndex());
			Info.TagData = TagData;
		}

		// Tags are sorted topologically from parent to child, so we can accumulate children into parents recursively by reverse iterating the map
		for (FThreadTagSizeMap::TConstReverseIterator It(Allocations); It; ++It)
		{
			const FThreadTagSizeData& Info = It->Value;
			const FTagData* ParentData = Info.TagData->GetParent();
			if (Info.Size && ParentData)
			{
				Allocations.FindChecked(ParentData->GetIndex()).Size += Info.Size;
			}
		}
	}

	void FLLMThreadState::OnTagsResorted(FTagDataArray& OldTagDatas)
	{
		FScopeLock Lock(&TagSection);
		TArray<FThreadTagSizeData, FDefaultLLMAllocator> AllocationDatas;
		AllocationDatas.Reserve(Allocations.Num());
		for (const TPair<int32, FThreadTagSizeData>& It : Allocations)
		{
			AllocationDatas.Add(It.Value);
		}
		Allocations.Reset();
		for (const FThreadTagSizeData& AllocationData : AllocationDatas)
		{
			Allocations.Add(AllocationData.TagData->GetIndex(), AllocationData);
		}
	}

	void FLLMThreadState::LockTags(bool bLock)
	{
		if (bLock)
		{
			TagSection.Lock();
		}
		else
		{
			TagSection.Unlock();
		}
	}

	void FLLMThreadState::FetchAndClearTagSizes(FTrackerTagSizeMap& TagSizes, int64* InAllocTypeAmounts, bool bTrimAllocations)
	{
		FScopeLock Lock(&TagSection);
		for (TPair<int32, FThreadTagSizeData>& Item : Allocations)
		{
			FThreadTagSizeData& ThreadInfo = Item.Value;
			if (ThreadInfo.Size)
			{
				const FTagData* TagData = ThreadInfo.TagData;
				FTrackerTagSizeData& TrackerInfo = TagSizes.FindOrAdd(TagData);
				TrackerInfo.Size += ThreadInfo.Size;
				ThreadInfo.Size = 0;
			}
		}
		if (bTrimAllocations)
		{
			Allocations.Empty();
		}

		for (int32 Index = 0; Index < static_cast<int32>(ELLMAllocType::Count); ++Index)
		{
			InAllocTypeAmounts[Index] += AllocTypeAmounts[Index];
			AllocTypeAmounts[Index] = 0;
		}
	}

	void SetMemoryStatByFName(FName Name, int64 Amount)
	{
		if (Name != NAME_None)
		{
			SET_MEMORY_STAT_FName(Name, Amount);
		}
	}

	void FLLMThreadState::ClearAllocTypeAmounts()
	{
		for (int32 Index = 0; Index < static_cast<int32>(ELLMAllocType::Count); ++Index)
		{
			AllocTypeAmounts[Index] = 0;
		}
	}

	/*
	 * FLLMCsvWriter implementation
	*/

	FLLMCsvWriter::FLLMCsvWriter()
		: Archive(nullptr)
		, LastWriteTime(FPlatformTime::Seconds())
		, WriteCount(0)
	{
	}

	FLLMCsvWriter::~FLLMCsvWriter()
	{
		delete Archive;
	}

	void FLLMCsvWriter::Clear()
	{
		Columns.Empty();
		ExistingColumns.Empty();
	}

	void FLLMCsvWriter::OnPreFork()
	{
		if (Archive)
		{
			Archive->Flush();
			delete Archive;
			Archive = nullptr;
		}
	}

	void FLLMCsvWriter::SetTracker(ELLMTracker InTracker)
	{
		Tracker = InTracker;
	}

	void FLLMCsvWriter::Publish(FLowLevelMemTracker& LLMRef, const FTrackerTagSizeMap& TagSizes, const FTagData* OverrideTrackedTotalTagData, const FTagData* OverrideUntaggedTagData, int64 TrackedTotal, bool bTrackPeaks)
	{
		const double Now = FPlatformTime::Seconds();

		if ((FLowLevelMemTracker::Get().bPublishSingleFrame == false) && 
			(Now - LastWriteTime < (double)CVarLLMWriteInterval.GetValueOnAnyThread()))
		{
			return;
		}

		LastWriteTime = Now;

		const bool bCreatedArchive = CreateArchive();
		const bool bColumnsUpdated = UpdateColumns(TagSizes);

		if (bCreatedArchive || bColumnsUpdated)
		{
			// The column names are written at the start of the archive; when they change we seek back to the start of the file and rewrite the column names.
			WriteHeader(OverrideTrackedTotalTagData, OverrideUntaggedTagData);
		}

		AddRow(LLMRef, TagSizes, OverrideTrackedTotalTagData, OverrideUntaggedTagData, TrackedTotal, bTrackPeaks);
	}

	const TCHAR* FLLMCsvWriter::GetTrackerCsvName(ELLMTracker InTracker)
	{
		switch (InTracker)
		{
			case ELLMTracker::Default: return TEXT("LLM");
			case ELLMTracker::Platform: return TEXT("LLMPlatform");
			default: LLMCheck(false); return TEXT("");
		}
	}

	/*
	 * Archive is a binary stream, so we can't just serialise an FString using <<
	*/
	void FLLMCsvWriter::Write(FStringView Text)
	{
		Archive->Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(Text.GetData(), Text.Len()).Get()), Text.Len() * sizeof(ANSICHAR));
	}

	bool FLLMCsvWriter::CreateArchive()
	{
		if (Archive)
		{
			return false;
		}

		// create the csv file
		FString Directory = FPaths::ProfilingDir() + TEXT("LLM/");
		IFileManager::Get().MakeDirectory(*Directory, true);

		const TCHAR* TrackerName = GetTrackerCsvName(Tracker);
		const FDateTime FileDate = FDateTime::Now();
#if PLATFORM_DESKTOP
		FString PlatformName = FPlatformProperties::PlatformName();
#else // Use the CPU for consoles so we can differentiate things like different SKUs of a console generation
		FString PlatformName = FPlatformMisc::GetCPUBrand().TrimStartAndEnd();
#endif
		PlatformName.ReplaceCharInline(' ', '_');
		PlatformName = FPaths::MakeValidFileName(PlatformName);
#if WITH_SERVER_CODE
		FString Filename = FString::Printf(TEXT("%s/%s_Pid%d_%s_%s.csv"), *Directory, TrackerName, FPlatformProcess::GetCurrentProcessId(), *FileDate.ToString(), *PlatformName);
#else
		FString Filename = FString::Printf(TEXT("%s/%s_%s_%s.csv"), *Directory, TrackerName, *FileDate.ToString(), *PlatformName);
#endif
		Archive = IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_AllowRead);
		LLMCheck(Archive);

		// create space for column titles that are filled in as we get them
		Write(FString::ChrN(CVarLLMHeaderMaxSize.GetValueOnAnyThread(), ' '));
		Write(TEXT("\n"));

		return true;
	}

	bool FLLMCsvWriter::UpdateColumns(const FTrackerTagSizeMap& TagSizes)
	{
		bool bUpdated = false;

		for (const TPair<const FTagData*, FTrackerTagSizeData>& It : TagSizes)
		{
			const FTagData* TagData = It.Key;
			if (!TagData->IsReportable())
			{
				continue;
			}
			if (TagData->GetName() == TagName_Untagged)
			{
				continue; // Handled by OverrideUntaggedName
			}
			if (ExistingColumns.Contains(TagData))
			{
				continue;
			}

			ExistingColumns.Add(TagData);
			Columns.Add(TagData);
			bUpdated = true;
		}
		return bUpdated;
	}

	void FLLMCsvWriter::WriteHeader(const FTagData* OverrideTrackedTotalTagData, const FTagData* OverrideUntaggedTagData)
	{
		int64 OriginalOffset = Archive->Tell();
		Archive->Seek(0);

		TStringBuilder<256> NameBuffer;
		auto WriteTagData = [this, &NameBuffer](const FTagData* TagData)
		{
			if (!TagData)
			{
				return;
			}

			NameBuffer.Reset();
			TagData->AppendDisplayPath(NameBuffer);
			NameBuffer << TEXT(",");
			Write(NameBuffer);
		};

		WriteTagData(OverrideTrackedTotalTagData);
		WriteTagData(OverrideUntaggedTagData);
		for (const FTagData* TagData : Columns)
		{
			WriteTagData(TagData);
		}

		int64 ColumnTitleTotalSize = Archive->Tell();
		if (ColumnTitleTotalSize >= CVarLLMHeaderMaxSize.GetValueOnAnyThread())
		{
			UE_LOG(LogHAL, Error, TEXT("LLM column titles have overflowed, LLM CSM data will be corrupted. Increase CVarLLMHeaderMaxSize > %d"), ColumnTitleTotalSize);
		}

		Archive->Seek(OriginalOffset);
	}

	void FLLMCsvWriter::AddRow(FLowLevelMemTracker& LLMRef, const FTrackerTagSizeMap& TagSizes, const FTagData* OverrideTrackedTotalTagData, const FTagData* OverrideUntaggedTagData, int64 TrackedTotal, bool bTrackPeaks)
	{
		TStringBuilder<256> TextBuffer;
		auto WriteValue = [this, &TextBuffer](int64 Value)
		{
			TextBuffer.Reset();
			TextBuffer.Appendf(TEXT("%0.2f,"), (float)Value / 1024.0f / 1024.0f);
			Write(TextBuffer);
		};
		auto WriteTag = [&WriteValue, &TagSizes, bTrackPeaks](const FTagData* TagData)
		{
			if (!TagData)
			{
				WriteValue(0);
			}
			else
			{
				const FTrackerTagSizeData* AllocationData = TagSizes.Find(TagData);
				if (!AllocationData)
				{
					WriteValue(0);
				}
				else
				{
					WriteValue(AllocationData->GetSize(bTrackPeaks));
				}
			}
		};

		if (OverrideTrackedTotalTagData)
		{
			WriteValue(TrackedTotal);
		}
		if (OverrideUntaggedTagData)
		{
			WriteTag(LLMRef.FindTagData(TagName_Untagged));
		}

		for (const FTagData* TagData : Columns)
		{
			WriteTag(TagData);
		}
		Write(TEXTVIEW("\n"));

		WriteCount++;

		if (CVarLLMWriteInterval.GetValueOnAnyThread())
		{
			UE_LOG(LogHAL, Log, TEXT("Wrote LLM csv line %d"), WriteCount);
		}

		Archive->Flush();
	}

	/*
	 * FLLMTraceWriter implementation
	*/


	FLLMTraceWriter::FLLMTraceWriter()
	{
	}

	inline void FLLMTraceWriter::SetTracker(ELLMTracker InTracker)
	{ 
		Tracker = InTracker;
	}

	void FLLMTraceWriter::Clear()
	{
		DeclaredTags.Empty();
	}

	const void* FLLMTraceWriter::GetTagId(const FTagData* TagData)
	{
		if (!TagData)
		{
			return nullptr;
		}
		return reinterpret_cast<const void*>(TagData);
	}

	void FLLMTraceWriter::Publish(FLowLevelMemTracker& LLMRef, const FTrackerTagSizeMap& TagSizes, const FTagData* OverrideTrackedTotalTagData, const FTagData* OverrideUntaggedTagData, int64 TrackedTotal, bool bTrackPeaks)
	{
		if (!UE_TRACE_CHANNELEXPR_IS_ENABLED(MemTagChannel))
		{
			return;
		}

		if (!bTrackerSpecSent)
		{
			bTrackerSpecSent = true;
			static struct {
				const ANSICHAR* Name;
				uint32 Len;
			} TrackerNames[] = {
				{ "Platform", 8 },
				{ "Default", 7 },
			};
			static_assert(UE_ARRAY_COUNT(TrackerNames) == int(ELLMTracker::Max), "");
			uint32 NameLen = TrackerNames[(uint8)Tracker].Len;
			UE_TRACE_LOG(LLM, TrackerSpec, MemTagChannel, NameLen * sizeof(ANSICHAR))
				<< TrackerSpec.TrackerId((uint8)Tracker)
				<< TrackerSpec.Name(TrackerNames[(uint8)Tracker].Name, NameLen);
		}

		TStringBuilder<1024> NameBuffer;
		auto SendTagDeclaration = [this, &NameBuffer](const FTagData* TagData)
		{
			if (!TagData || DeclaredTags.Contains(TagData))
			{
				return;
			}
			DeclaredTags.Add(TagData);

			const FTagData* Parent = TagData->GetParent();
			NameBuffer.Reset();
			TagData->AppendDisplayPath(NameBuffer);
			UE_TRACE_LOG(LLM, TagsSpec, MemTagChannel, NameBuffer.Len() * sizeof(ANSICHAR))
				<< TagsSpec.TagId(GetTagId(TagData))
				<< TagsSpec.ParentId(GetTagId(Parent))
				<< TagsSpec.Name(*NameBuffer, NameBuffer.Len());
		};
		SendTagDeclaration(OverrideTrackedTotalTagData);
		SendTagDeclaration(OverrideUntaggedTagData);
		for (const TPair<const FTagData*, FTrackerTagSizeData>& It : TagSizes)
		{
			const FTagData* TagData = It.Key;
			if (!TagData->IsReportable())
			{
				continue;
			}
			if (OverrideUntaggedTagData != nullptr && TagData->GetName() == TagName_Untagged)
			{
				continue; // Handled by OverrideUntaggedTagData
			}
			SendTagDeclaration(TagData);
		}

		TArray<const void*, FDefaultLLMAllocator> TagIds;
		TArray<int64, FDefaultLLMAllocator> TagValues;
		TagIds.Reserve(TagSizes.Num() + 2);
		TagValues.Reserve(TagSizes.Num() + 2);
		auto AddValue = [&TagIds, &TagValues](const FTagData* TagData, int64 Value)
		{
			if (!TagData)
			{
				return;
			}
			TagIds.Add(GetTagId(TagData));
			TagValues.Add(Value);
		};

		AddValue(OverrideTrackedTotalTagData, TrackedTotal);
		if (OverrideUntaggedTagData)
		{
			const FTagData* TagData = LLMRef.FindTagData(TagName_Untagged);
			if (!TagData)
			{
				AddValue(OverrideUntaggedTagData, 0);
			}
			else
			{
				const FTrackerTagSizeData* AllocationData = TagSizes.Find(TagData);
				AddValue(OverrideUntaggedTagData, AllocationData ? AllocationData->GetSize(bTrackPeaks) : 0);
			}
		}

		for (const TPair<const FTagData*, FTrackerTagSizeData>& It : TagSizes)
		{
			const FTagData* TagData = It.Key;
			if (!TagData->IsReportable())
			{
				continue;
			}
			if (OverrideUntaggedTagData && TagData->GetName() == TagName_Untagged)
			{
				continue; // Handled by OverrideUntaggedTagData
			}
			AddValue(TagData, It.Value.GetSize(bTrackPeaks));
		}

		int TagCount = TagIds.Num();
		LLMCheck(TagCount == TagValues.Num());
		const uint64 Cycle = FPlatformTime::Cycles64();
		UE_TRACE_LOG(LLM, TagValue, MemTagChannel)
			<< TagValue.TrackerId((uint8)Tracker)
			<< TagValue.Cycle(Cycle)
			<< TagValue.Tags(TagIds.GetData(), TagCount)
			<< TagValue.Values(TagValues.GetData(), TagCount);
	}

	void FLLMTraceWriter::TraceGenericTags(FLowLevelMemTracker& LLMRef)
	{
		for (int32 GenericTagIndex = 0; GenericTagIndex < static_cast<int32>(ELLMTag::GenericTagCount); GenericTagIndex++)
		{
			const FTagData* TagData = LLMRef.FindTagData(static_cast<ELLMTag>(GenericTagIndex));
			FString TagName = TagData->GetDisplayPath();
			UE_TRACE_LOG(LLM, TagsSpec, MemTagChannel, TagName.Len() * sizeof(ANSICHAR))
				<< TagsSpec.TagId(GetTagId(TagData))
				<< TagsSpec.ParentId(GetTagId(TagData->GetParent()))
				<< TagsSpec.Name(*TagName, TagName.Len());
		}
	}

	/*
	 * FLLMCsvProfilerWriter implementation
	*/
	FLLMCsvProfilerWriter::FLLMCsvProfilerWriter()
	{
	}

	void FLLMCsvProfilerWriter::Clear()
	{
#if LLM_CSV_PROFILER_WRITER_ENABLED
		TagDataToCsvStatName.Empty();
#endif
	}

	void FLLMCsvProfilerWriter::SetTracker(ELLMTracker InTracker)
	{
#if LLM_CSV_PROFILER_WRITER_ENABLED
		check(InTracker == ELLMTracker::Platform || InTracker == ELLMTracker::Default);
		Tracker = InTracker;
#endif
	}

	void FLLMCsvProfilerWriter::Publish(FLowLevelMemTracker& LLMRef, const FTrackerTagSizeMap& TagSizes, const FTagData* OverrideTrackedTotalTagData, const FTagData* OverrideUntaggedTagData, int64 TrackedTotal, bool bTrackPeaks)
	{
#if LLM_CSV_PROFILER_WRITER_ENABLED
		int32 CsvCategoryIndex = (Tracker == ELLMTracker::Platform) ? CSV_CATEGORY_INDEX(LLMPlatform) : CSV_CATEGORY_INDEX(LLM);

		if (OverrideTrackedTotalTagData)
		{
			RecordTagToCsv(CsvCategoryIndex, OverrideTrackedTotalTagData, TrackedTotal);
		}

		if (OverrideUntaggedTagData)
		{
			const FTagData* TagData = LLMRef.FindTagData(TagName_Untagged);
			const FTrackerTagSizeData* AllocationData = TagData ? TagSizes.Find(TagData) : nullptr;
			RecordTagToCsv(CsvCategoryIndex, OverrideUntaggedTagData, AllocationData ? AllocationData->GetSize(bTrackPeaks) : 0);
		}

		for (const TPair<const FTagData*, FTrackerTagSizeData>& It : TagSizes)
		{
			const FTagData* TagData = It.Key;
			if (!TagData->IsReportable())
			{
				continue;
			}
			if (OverrideUntaggedTagData && TagData->GetName() == TagName_Untagged)
			{
				// Handled separately by OverrideUntaggedTagData
				continue;
			}
			const FTrackerTagSizeData* AllocationData = TagSizes.Find(TagData);
			RecordTagToCsv(CsvCategoryIndex, TagData, AllocationData ? AllocationData->GetSize(bTrackPeaks) : 0);
		}
#endif
	}

	void FLLMCsvProfilerWriter::RecordTagToCsv(int32 CsvCategoryIndex, const FTagData* TagData, int64 TagSize)
	{
#if LLM_CSV_PROFILER_WRITER_ENABLED
		FName NewCsvStatName;
		FName* CsvStatNamePtr = TagDataToCsvStatName.Find(TagData);
		if (CsvStatNamePtr == nullptr)
		{
			NewCsvStatName = FName(TagData->GetDisplayPath());
			TagDataToCsvStatName.Add(TagData, NewCsvStatName);
			CsvStatNamePtr = &NewCsvStatName;
		}
		FCsvProfiler::RecordCustomStat(*CsvStatNamePtr, CsvCategoryIndex, (float)((double)TagSize / (1024.0 * 1024.0)), ECsvCustomStatOp::Set);
#endif
	}


}
}

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER
