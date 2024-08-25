// Copyright Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidStats.h"
#include "Async/TaskGraphInterfaces.h"
#include "CoreMinimal.h"
#include "Android/AndroidPlatformMisc.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"

#if !UE_BUILD_SHIPPING && PLATFORM_ANDROID_ARM64
#	define HWCPIPE_SUPPORTED 1
#else
#	define HWCPIPE_SUPPORTED 0
#endif

#if HWCPIPE_SUPPORTED
#	include "hwcpipe.h"
#endif
#include "Android/AndroidPlatformThermal.h"

DECLARE_STATS_GROUP(TEXT("Android CPU stats"), STATGROUP_AndroidCPU, STATCAT_Advanced);
CSV_DEFINE_CATEGORY(AndroidCPU, true);
CSV_DEFINE_CATEGORY(AndroidMemory, true);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Frequency Groups"), STAT_NumFreqGroups, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 0 : Max frequency (MHz)"), STAT_FreqGroup0MaxFrequency, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 0 : Min frequency (MHz)"), STAT_FreqGroup0MinFrequency, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 0 : Current frequency (MHz)"), STAT_FreqGroup0CurrentFrequency, STATGROUP_AndroidCPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Freq Group 0 : Current frequency (% from Max)"), STAT_FreqGroup0CurrentFrequencyPercentage, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 0 : Num Cores"), STAT_FreqGroup0NumCores, STATGROUP_AndroidCPU);
CSV_DEFINE_STAT(AndroidCPU, CPUFreqMHzGroup0);
CSV_DEFINE_STAT(AndroidCPU, CPUFreqPercentageGroup0);

DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 1 : Max frequency (MHz)"), STAT_FreqGroup1MaxFrequency, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 1 : Min frequency (MHz)"), STAT_FreqGroup1MinFrequency, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 1 : Current frequency (MHz)"), STAT_FreqGroup1CurrentFrequency, STATGROUP_AndroidCPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Freq Group 1 : Current frequency (% from Max)"), STAT_FreqGroup1CurrentFrequencyPercentage, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 1 : Num Cores"), STAT_FreqGroup1NumCores, STATGROUP_AndroidCPU);
CSV_DEFINE_STAT(AndroidCPU, CPUFreqMHzGroup1);
CSV_DEFINE_STAT(AndroidCPU, CPUFreqPercentageGroup1);

DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 2 : Max frequency (MHz)"), STAT_FreqGroup2MaxFrequency, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 2 : Min frequency (MHz)"), STAT_FreqGroup2MinFrequency, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 2 : Current frequency (MHz)"), STAT_FreqGroup2CurrentFrequency, STATGROUP_AndroidCPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Freq Group 2 : Current frequency (% from Max)"), STAT_FreqGroup2CurrentFrequencyPercentage, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 2 : Num Cores"), STAT_FreqGroup2NumCores, STATGROUP_AndroidCPU);
CSV_DEFINE_STAT(AndroidCPU, CPUFreqMHzGroup2);
CSV_DEFINE_STAT(AndroidCPU, CPUFreqPercentageGroup2);

DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 3 : Max frequency (MHz)"), STAT_FreqGroup3MaxFrequency, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 3 : Min frequency (MHz)"), STAT_FreqGroup3MinFrequency, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 3 : Current frequency (MHz)"), STAT_FreqGroup3CurrentFrequency, STATGROUP_AndroidCPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Freq Group 3 : Current frequency (% from Max)"), STAT_FreqGroup3CurrentFrequencyPercentage, STATGROUP_AndroidCPU);
DECLARE_DWORD_COUNTER_STAT(TEXT("Freq Group 3 : Num Cores"), STAT_FreqGroup3NumCores, STATGROUP_AndroidCPU);
CSV_DEFINE_STAT(AndroidCPU, CPUFreqMHzGroup3);
CSV_DEFINE_STAT(AndroidCPU, CPUFreqPercentageGroup3);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num CPU Cores"), STAT_NumCPUCores, STATGROUP_AndroidCPU);

DECLARE_FLOAT_COUNTER_STAT(TEXT("Freq Group 0 : highest core utilization %"), STAT_FreqGroup0MaxUtilization, STATGROUP_AndroidCPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Freq Group 1 : highest core utilization %"), STAT_FreqGroup1MaxUtilization, STATGROUP_AndroidCPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Freq Group 2 : highest core utilization %"), STAT_FreqGroup2MaxUtilization, STATGROUP_AndroidCPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Freq Group 3 : highest core utilization %"), STAT_FreqGroup3MaxUtilization, STATGROUP_AndroidCPU);

CSV_DEFINE_STAT(AndroidCPU, CPUTemp);
DECLARE_FLOAT_COUNTER_STAT(TEXT("CPU Temperature"), STAT_CPUTemp, STATGROUP_AndroidCPU);

CSV_DEFINE_STAT(AndroidCPU, ThermalStatus);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Thermal Status"), STAT_ThermalStatus, STATGROUP_AndroidCPU);

CSV_DEFINE_STAT(AndroidCPU, ThermalStress);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Thermal Stress"), STAT_ThermalStress, STATGROUP_AndroidCPU);

#if STATS

#define SET_DWORD_STAT_BY_FNAME(Stat, Amount) \
{\
	if (Amount != 0 && FThreadStats::IsCollectingData()) \
	{ \
		FThreadStats::AddMessage(Stat, EStatOperation::Set, int64(Amount));\
		TRACE_STAT_SET(Stat, int64(Amount)); \
	} \
}

#define SET_FLOAT_STAT_BY_FNAME(Stat, Amount) \
{\
	if (Amount != 0 && FThreadStats::IsCollectingData()) \
	{ \
		FThreadStats::AddMessage(Stat, EStatOperation::Set, double(Amount));\
		TRACE_STAT_SET(Stat, double(Amount)); \
	} \
}

#endif

#if CSV_PROFILER

#define CSV_STAT_PTR(StatName)									&_GCsvStat_##StatName
#define CSV_CUSTOM_STAT_DEFINED_BY_PTR(StatPtr,Value,Op)		FCsvProfiler::RecordCustomStat(StatPtr->Name, StatPtr->CategoryIndex, Value, Op);
FCsvDeclaredStat* GCPUFreqStats[] = {
	CSV_STAT_PTR(CPUFreqMHzGroup0),
	CSV_STAT_PTR(CPUFreqMHzGroup1),
	CSV_STAT_PTR(CPUFreqMHzGroup2),
	CSV_STAT_PTR(CPUFreqMHzGroup3)
};

FCsvDeclaredStat* GCPUFreqPercentageStats[] = {
	CSV_STAT_PTR(CPUFreqPercentageGroup0),
	CSV_STAT_PTR(CPUFreqPercentageGroup1),
	CSV_STAT_PTR(CPUFreqPercentageGroup2),
	CSV_STAT_PTR(CPUFreqPercentageGroup3)
};
#undef CSV_STAT_PTR

#else
#define CSV_CUSTOM_STAT_DEFINED_BY_PTR(StatPtr,Value,Op)
#endif


#if HWCPIPE_SUPPORTED

CSV_DEFINE_CATEGORY(AndroidGPU, true);
CSV_DEFINE_STAT(AndroidGPU, GPUCyclesMln);
CSV_DEFINE_STAT(AndroidGPU, VertexCyclesMln);
CSV_DEFINE_STAT(AndroidGPU, FragmentCyclesMln);
CSV_DEFINE_STAT(AndroidGPU, PixelsMln);
CSV_DEFINE_STAT(AndroidGPU, ShaderCyclesMln);
CSV_DEFINE_STAT(AndroidGPU, ShaderArithmeticCyclesMln);
CSV_DEFINE_STAT(AndroidGPU, ShaderLoadStoreCyclesMln);
CSV_DEFINE_STAT(AndroidGPU, ShaderTextureCyclesMln);
CSV_DEFINE_STAT(AndroidGPU, ExternalMemoryReadMB);
CSV_DEFINE_STAT(AndroidGPU, ExternalMemoryWriteMB);

DECLARE_STATS_GROUP(TEXT("Android GPU stats"), STATGROUP_AndroidGPU, STATCAT_Advanced);
DECLARE_FLOAT_COUNTER_STAT(TEXT("GPU Cycles (Mln)"), STAT_GPUCycles, STATGROUP_AndroidGPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Vertex Cycles (Mln)"), STAT_VertexCycles, STATGROUP_AndroidGPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Fragment Cycles (Mln)"), STAT_FragmentCycles, STATGROUP_AndroidGPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Pixels (Mln)"), STAT_Pixels, STATGROUP_AndroidGPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Shader Cycles (Mln)"), STAT_ShaderCycles, STATGROUP_AndroidGPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Shader Arithmetic Cycles (Mln)"), STAT_ShaderArithmeticCycles, STATGROUP_AndroidGPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Shader Load\\Store Cycles (Mln)"), STAT_ShaderLoadStoreCycles, STATGROUP_AndroidGPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Shader Texture Cycles (Mln)"), STAT_ShaderTextureCycles, STATGROUP_AndroidGPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("External Memory Read (MB)"), STAT_ExternalMemoryRead, STATGROUP_AndroidGPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("External Memory Write (MB)"), STAT_ExternalMemoryWrite, STATGROUP_AndroidGPU);

static hwcpipe::HWCPipe* GHWCPipe = nullptr;
#endif

static void UpdateGPUStats();


static float GAndroidCPUStatsUpdateRate = 0.100f;
static FAutoConsoleVariableRef CVarAndroidCollectCPUStatsRate(
	TEXT("Android.CPUStatsUpdateRate"),
	GAndroidCPUStatsUpdateRate,
	TEXT("Update rate in seconds for collecting CPU Stats (Default: 0.1)\n")
	TEXT("0 to disable."),
	ECVF_Default);

static int GThermalStatus = 0;
static int GTrimMemoryBackgroundLevel = 0;
CSV_DEFINE_STAT(AndroidMemory, TrimMemoryBackgroundLevel);

CSV_DEFINE_STAT(AndroidMemory, Mem_RSS);
CSV_DEFINE_STAT(AndroidMemory, Mem_Swap);
CSV_DEFINE_STAT(AndroidMemory, Mem_TotalUsed);

static int GTrimMemoryForegroundLevel = 0;
CSV_DEFINE_STAT(AndroidMemory, TrimMemoryForegroundLevel);

CSV_DEFINE_STAT(AndroidMemory, MemAdvisor_DeviceMemFreeMb);
CSV_DEFINE_STAT(AndroidMemory, MemAdvisor_DeviceMemUsedMb);
static FAndroidPlatformMemory::FMemAdviceStats GDeviceMemAdviceStats;

void FAndroidStats::Init(bool bEnableHWCPipe)
{
	(void)bEnableHWCPipe;
#if HWCPIPE_SUPPORTED
	if (bEnableHWCPipe)
	{
		static hwcpipe::HWCPipe HWCPipe({}, {hwcpipe::GpuCounter::GpuCycles,
											 hwcpipe::GpuCounter::VertexComputeCycles,
											 hwcpipe::GpuCounter::FragmentCycles,
											 hwcpipe::GpuCounter::Pixels,
											 hwcpipe::GpuCounter::ShaderCycles,
											 hwcpipe::GpuCounter::ShaderArithmeticCycles,
											 hwcpipe::GpuCounter::ShaderLoadStoreCycles,
											 hwcpipe::GpuCounter::ShaderTextureCycles,
											 hwcpipe::GpuCounter::ExternalMemoryReadBytes,
											 hwcpipe::GpuCounter::ExternalMemoryWriteBytes,});
		if (hwcpipe::get_last_error() == nullptr)
		{
			GHWCPipe = &HWCPipe;
			GHWCPipe->run();
		}
	}
#endif
}

void FAndroidStats::OnThermalStatusChanged(int Status)
{
	GThermalStatus = Status;
}

void FAndroidStats::OnTrimMemory(int TrimLevel)
{
	// https://developer.android.com/reference/android/content/ComponentCallbacks2#constants_1
	enum ETrimLevel
	{
		TRIM_MEMORY_BACKGROUND = 40,
		TRIM_MEMORY_COMPLETE = 80,
		TRIM_MEMORY_MODERATE = 60,
		TRIM_MEMORY_RUNNING_CRITICAL = 15,
		TRIM_MEMORY_RUNNING_LOW = 10,
		TRIM_MEMORY_RUNNING_MODERATE = 5,
		TRIM_MEMORY_UI_HIDDEN = 20,
	};

	GTrimMemoryBackgroundLevel = 0;
	GTrimMemoryForegroundLevel = 0;

	switch (TrimLevel)
	{
	case TRIM_MEMORY_UI_HIDDEN:
		GTrimMemoryBackgroundLevel = 1;
		break;
	case TRIM_MEMORY_BACKGROUND:
		GTrimMemoryBackgroundLevel = 2;
		break;
	case TRIM_MEMORY_MODERATE:
		GTrimMemoryBackgroundLevel = 3;
		break;
	case TRIM_MEMORY_COMPLETE:
		GTrimMemoryBackgroundLevel = 4;
		break;

	case TRIM_MEMORY_RUNNING_LOW:
		GTrimMemoryForegroundLevel = 1;
		break;
	case TRIM_MEMORY_RUNNING_MODERATE:
		GTrimMemoryForegroundLevel = 2;
		break;
	case TRIM_MEMORY_RUNNING_CRITICAL:
		GTrimMemoryForegroundLevel = 3;
		break;
	default:
		GTrimMemoryForegroundLevel = -1;
		GTrimMemoryBackgroundLevel = -1;
		break;
}
}

void FAndroidStats::UpdateAndroidStats()
{
	if (GAndroidCPUStatsUpdateRate <= 0.0f)
	{
		return;
	}

	static float CPUTemp = 0.0f;
	static uint64 LastCollectionTime = FPlatformTime::Cycles64();
	const uint64 CurrentTime = FPlatformTime::Cycles64();
	const bool bUpdateStats = ((FPlatformTime::ToSeconds64(CurrentTime - LastCollectionTime) >= GAndroidCPUStatsUpdateRate));
	static FPlatformMemoryStats MemStats = FAndroidPlatformMemory::GetStats();
	if (bUpdateStats)
	{
		LastCollectionTime = CurrentTime;
		CPUTemp = FAndroidMisc::GetCPUTemperature();

		GDeviceMemAdviceStats = FAndroidPlatformMemory::GetDeviceMemAdviceStats();
		MemStats = FAndroidPlatformMemory::GetStats();
	}

	CSV_CUSTOM_STAT_DEFINED(CPUTemp, CPUTemp, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(ThermalStatus, GThermalStatus, ECsvCustomStatOp::Set);


	float Thermals5S = FAndroidPlatformThermal::GetThermalStress(FAndroidPlatformThermal::EForecastPeriod::FIVE_SEC);
	CSV_CUSTOM_STAT_DEFINED(ThermalStress, Thermals5S, ECsvCustomStatOp::Set);
	
	CSV_CUSTOM_STAT_DEFINED(TrimMemoryBackgroundLevel, GTrimMemoryBackgroundLevel, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(Mem_Swap, (int)(MemStats.VMSwap / (1024 * 1024)), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(Mem_RSS, (int)(MemStats.VMRss / (1024 * 1024)), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(Mem_TotalUsed, (int)(MemStats.UsedPhysical / (1024 * 1024)), ECsvCustomStatOp::Set);
	
	if(GDeviceMemAdviceStats.IsValid())
	{
		CSV_CUSTOM_STAT_DEFINED(MemAdvisor_DeviceMemFreeMb, (int)(GDeviceMemAdviceStats.MemFree/(1024*1024)), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(MemAdvisor_DeviceMemUsedMb, (int)(GDeviceMemAdviceStats.MemUsed/(1024*1024)), ECsvCustomStatOp::Set);
	}

	static const uint32 MaxFrequencyGroupStats = 4;
	const int32 MaxCoresStatsSupport = 16;
	int32 NumCores = FMath::Min(FAndroidMisc::NumberOfCores(), MaxCoresStatsSupport);

	struct FFrequencyGroup
	{
		uint32 MinFrequency;
		uint32 MaxFrequency;
		uint32 CoreCount;
	};

	static uint32 UnInitializedCores = NumCores;
	static TArray<FFrequencyGroup> FrequencyGroups;
	static uint32 CoreFrequencyGroupIndex[MaxCoresStatsSupport] = {
		0xFFFFFFFF, 0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
		0xFFFFFFFF, 0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
		0xFFFFFFFF, 0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
		0xFFFFFFFF, 0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
	};

	if (UnInitializedCores != 0)
	{
		for (int32 CoreIndex = 0; CoreIndex < NumCores; CoreIndex++)
		{
			if (CoreFrequencyGroupIndex[CoreIndex] == 0xFFFFFFFF)
			{
				uint32 MinFreq = FAndroidMisc::GetCoreFrequency(CoreIndex, FAndroidMisc::ECoreFrequencyProperty::MinFrequency) / 1000;
				uint32 MaxFreq = FAndroidMisc::GetCoreFrequency(CoreIndex, FAndroidMisc::ECoreFrequencyProperty::MaxFrequency) / 1000;
				if (MaxFreq > 0)
				{
					UnInitializedCores--;
					int32 FoundIndex = FrequencyGroups.IndexOfByPredicate([&MinFreq, &MaxFreq](const FFrequencyGroup& s) { return s.MinFrequency == MinFreq && s.MaxFrequency == MaxFreq; });
					if (FoundIndex == INDEX_NONE)
					{
						FFrequencyGroup NewGroup = { MinFreq,MaxFreq, 1 };
						CoreFrequencyGroupIndex[CoreIndex] = FrequencyGroups.Add(NewGroup);
					}
					else
					{
						CoreFrequencyGroupIndex[CoreIndex] = FoundIndex;
						FrequencyGroups[FoundIndex].CoreCount++;
					}
				}
			}
		}
	}

	auto GetFrequencyGroupCurrentFrequency = [&](int32 FrequencyGroupIdx)
	{
		for (int32 CoreIdx = 0; CoreIdx < NumCores; CoreIdx++)
		{
			if (CoreFrequencyGroupIndex[CoreIdx] == FrequencyGroupIdx)
			{
				return FAndroidMisc::GetCoreFrequency(CoreIdx, FAndroidMisc::ECoreFrequencyProperty::CurrentFrequency) / 1000;
			}
		}
		return 0u;
	};

	static int32 CurrentFrequencies[MaxFrequencyGroupStats] = { 0,0,0,0 };
	static float CurrentFrequenciesPercentage[MaxFrequencyGroupStats] = { 0,0,0,0 };
	for (int32 FrequencyGroupIndex = 0; FrequencyGroupIndex < FrequencyGroups.Num(); FrequencyGroupIndex++)
	{
		if (bUpdateStats)
		{
			CurrentFrequencies[FrequencyGroupIndex] = GetFrequencyGroupCurrentFrequency(FrequencyGroupIndex);
			CurrentFrequenciesPercentage[FrequencyGroupIndex] = ((float)CurrentFrequencies[FrequencyGroupIndex] / (float)FrequencyGroups[FrequencyGroupIndex].MaxFrequency) * 100.0f;
		}
		CSV_CUSTOM_STAT_DEFINED_BY_PTR(GCPUFreqStats[FrequencyGroupIndex], CurrentFrequencies[FrequencyGroupIndex], ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED_BY_PTR(GCPUFreqPercentageStats[FrequencyGroupIndex], CurrentFrequenciesPercentage[FrequencyGroupIndex], ECsvCustomStatOp::Set);
	}

#if STATS
	static const FName AndroidFrequencyGroupMaxFreqStats[] = {
		GET_STATFNAME(STAT_FreqGroup0MaxFrequency),
		GET_STATFNAME(STAT_FreqGroup1MaxFrequency),
		GET_STATFNAME(STAT_FreqGroup2MaxFrequency),
		GET_STATFNAME(STAT_FreqGroup3MaxFrequency),
	};

	static const FName AndroidFrequencyGroupMinFreqStats[] = {
		GET_STATFNAME(STAT_FreqGroup0MinFrequency),
		GET_STATFNAME(STAT_FreqGroup1MinFrequency),
		GET_STATFNAME(STAT_FreqGroup2MinFrequency),
		GET_STATFNAME(STAT_FreqGroup3MinFrequency),
	};

	static const FName AndroidFrequencyGroupCurrentFreqStats[] = {
		GET_STATFNAME(STAT_FreqGroup0CurrentFrequency),
		GET_STATFNAME(STAT_FreqGroup1CurrentFrequency),
		GET_STATFNAME(STAT_FreqGroup2CurrentFrequency),
		GET_STATFNAME(STAT_FreqGroup3CurrentFrequency),
	};

	
	static const FName AndroidFrequencyGroupCurrentFreqPercentageStats[] = {
		GET_STATFNAME(STAT_FreqGroup0CurrentFrequencyPercentage),
		GET_STATFNAME(STAT_FreqGroup1CurrentFrequencyPercentage),
		GET_STATFNAME(STAT_FreqGroup2CurrentFrequencyPercentage),
		GET_STATFNAME(STAT_FreqGroup3CurrentFrequencyPercentage),
	};

	static const FName AndroidFrequencyGroupNumCoresStats[] = {
		GET_STATFNAME(STAT_FreqGroup0NumCores),
		GET_STATFNAME(STAT_FreqGroup1NumCores),
		GET_STATFNAME(STAT_FreqGroup2NumCores),
		GET_STATFNAME(STAT_FreqGroup3NumCores),
	};

	static const FName AndroidFrequencyGroupMaxCoresUtilizationStats[] = {
		GET_STATFNAME(STAT_FreqGroup0MaxUtilization),
		GET_STATFNAME(STAT_FreqGroup1MaxUtilization),
		GET_STATFNAME(STAT_FreqGroup2MaxUtilization),
		GET_STATFNAME(STAT_FreqGroup3MaxUtilization),
	};

	static float MaxSingleCoreUtilization[MaxFrequencyGroupStats] = { 0.0f };
	if (bUpdateStats)
	{
		FAndroidMisc::FCPUState& AndroidCPUState = FAndroidMisc::GetCPUState();
		for (int32 CoreIndex = 0; CoreIndex < NumCores; CoreIndex++)
		{
			uint32 FrequencyGroupIndex = CoreFrequencyGroupIndex[CoreIndex];
			if (FrequencyGroupIndex != 0xFFFFFFFF)
			{
				float& MaxCoreUtilization = MaxSingleCoreUtilization[FrequencyGroupIndex];
				MaxCoreUtilization = FMath::Max((float)AndroidCPUState.Utilization[CoreIndex], MaxCoreUtilization);
			}
		}
	}

	for (int32 FrequencyGroupIndex = 0; FrequencyGroupIndex < FrequencyGroups.Num(); FrequencyGroupIndex++)
	{
		const FFrequencyGroup& FrequencyGroup = FrequencyGroups[FrequencyGroupIndex];
		SET_DWORD_STAT_BY_FNAME(AndroidFrequencyGroupMaxFreqStats[FrequencyGroupIndex], FrequencyGroup.MaxFrequency);
		SET_DWORD_STAT_BY_FNAME(AndroidFrequencyGroupNumCoresStats[FrequencyGroupIndex], FrequencyGroup.CoreCount);
		//SET_DWORD_STAT_BY_FNAME(AndroidFrequencyGroupMinFreqStats[FrequencyGroupIndex], FrequencyGroup.MinFrequency);
		SET_DWORD_STAT_BY_FNAME(AndroidFrequencyGroupCurrentFreqStats[FrequencyGroupIndex], CurrentFrequencies[FrequencyGroupIndex]);
		SET_FLOAT_STAT_BY_FNAME(AndroidFrequencyGroupCurrentFreqPercentageStats[FrequencyGroupIndex], CurrentFrequencies[FrequencyGroupIndex]);
		SET_FLOAT_STAT_BY_FNAME(AndroidFrequencyGroupMaxCoresUtilizationStats[FrequencyGroupIndex], MaxSingleCoreUtilization[FrequencyGroupIndex]);
	}

	static const FName CPUStatName = GET_STATFNAME(STAT_CPUTemp);
	static const FName ThermalStatus = GET_STATFNAME(STAT_ThermalStatus);
	SET_FLOAT_STAT_BY_FNAME(CPUStatName, CPUTemp);
	SET_FLOAT_STAT_BY_FNAME(ThermalStatus, GThermalStatus);
#endif

	UpdateGPUStats();
}

static void UpdateGPUStats()
{
#if HWCPIPE_SUPPORTED
	if (!GHWCPipe)
	{
		return;
	}

	FFunctionGraphTask::CreateAndDispatchWhenReady([]()
	{
		const double Mln = 1000000.0;
		const double MB = 1024.0 * 1024.0;
		hwcpipe::GpuMeasurements counters = GHWCPipe->gpu_profiler()->sample();
		for (hwcpipe::GpuMeasurements::iterator it = counters.begin(); it != counters.end(); ++it)
		{
			float value;
			switch (it->first)
			{
			case hwcpipe::GpuCounter::GpuCycles:
				value = float(it->second.get<double>() / Mln);
				CSV_CUSTOM_STAT_DEFINED(GPUCyclesMln, value, ECsvCustomStatOp::Set);
				SET_FLOAT_STAT(STAT_GPUCycles, value);
				break;
			case hwcpipe::GpuCounter::VertexComputeCycles:
				value = float(it->second.get<double>() / Mln);
				CSV_CUSTOM_STAT_DEFINED(VertexCyclesMln, value, ECsvCustomStatOp::Set);
				SET_FLOAT_STAT(STAT_VertexCycles, value);
				break;
			case hwcpipe::GpuCounter::FragmentCycles:
				value = float(it->second.get<double>() / Mln);
				CSV_CUSTOM_STAT_DEFINED(FragmentCyclesMln, value, ECsvCustomStatOp::Set);
				SET_FLOAT_STAT(STAT_FragmentCycles, value);
				break;
			case hwcpipe::GpuCounter::Pixels:
				value = float(it->second.get<double>() / Mln);
				CSV_CUSTOM_STAT_DEFINED(PixelsMln, value, ECsvCustomStatOp::Set);
				SET_FLOAT_STAT(STAT_Pixels, value);
				break;
			case hwcpipe::GpuCounter::ShaderCycles:
				value = float(it->second.get<double>() / Mln);
				CSV_CUSTOM_STAT_DEFINED(ShaderCyclesMln, value, ECsvCustomStatOp::Set);
				SET_FLOAT_STAT(STAT_ShaderCycles, value);
				break;
			case hwcpipe::GpuCounter::ShaderArithmeticCycles:
				value = float(it->second.get<double>() / Mln);
				CSV_CUSTOM_STAT_DEFINED(ShaderArithmeticCyclesMln, value, ECsvCustomStatOp::Set);
				SET_FLOAT_STAT(STAT_ShaderArithmeticCycles, value);
				break;
			case hwcpipe::GpuCounter::ShaderLoadStoreCycles:
				value = float(it->second.get<double>() / Mln);
				CSV_CUSTOM_STAT_DEFINED(ShaderLoadStoreCyclesMln, value, ECsvCustomStatOp::Set);
				SET_FLOAT_STAT(STAT_ShaderLoadStoreCycles, value);
				break;
			case hwcpipe::GpuCounter::ShaderTextureCycles:
				value = float(it->second.get<double>() / Mln);
				CSV_CUSTOM_STAT_DEFINED(ShaderTextureCyclesMln, value, ECsvCustomStatOp::Set);
				SET_FLOAT_STAT(STAT_ShaderTextureCycles, value);
				break;
			case hwcpipe::GpuCounter::ExternalMemoryReadBytes:
				value = float(it->second.get<double>() / MB);
				CSV_CUSTOM_STAT_DEFINED(ExternalMemoryReadMB, value, ECsvCustomStatOp::Set);
				SET_FLOAT_STAT(STAT_ExternalMemoryRead, value);
				break;
			case hwcpipe::GpuCounter::ExternalMemoryWriteBytes:
				value = float(it->second.get<double>() / MB);
				CSV_CUSTOM_STAT_DEFINED(ExternalMemoryWriteMB, value, ECsvCustomStatOp::Set);
				SET_FLOAT_STAT(STAT_ExternalMemoryWrite, value);
				break;
			default:
				break;
			}
		}
	}, TStatId(), nullptr, ENamedThreads::AnyThread);
#endif
}
