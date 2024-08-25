// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12RayTracing.h"

#if D3D12_RHI_RAYTRACING

#include "D3D12Resources.h"
#include "D3D12Util.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "Experimental/Containers/SherwoodHashTable.h"
#include "BuiltInRayTracingShaders.h"
#include "RayTracingValidationShaders.h"
#include "Hash/xxhash.h"
#include "HAL/CriticalSection.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManagerGeneric.h"
#include "Misc/ScopeLock.h"
#include "Async/ParallelFor.h"
#include "Misc/BufferedOutputDevice.h"
#include "String/LexFromString.h"
#include "GlobalRenderResources.h"
#include "D3D12RayTracingDebug.h"
#include "D3D12ExplicitDescriptorCache.h"

extern int32 GD3D12ExplicitViewDescriptorHeapSize;
extern int32 GD3D12ExplicitViewDescriptorHeapOverflowReported;

static int32 GRayTracingDebugForceBuildMode = 0;
static FAutoConsoleVariableRef CVarRayTracingDebugForceFastTrace(
	TEXT("r.D3D12.RayTracing.DebugForceBuildMode"),
	GRayTracingDebugForceBuildMode,
	TEXT("Forces specific acceleration structure build mode (not runtime-tweakable).\n")
	TEXT("0: Use build mode requested by high-level code (Default)\n")
	TEXT("1: Force fast build mode\n")
	TEXT("2: Force fast trace mode\n"),
	ECVF_ReadOnly
);

static int32 GRayTracingCacheShaderRecords = 1;
static FAutoConsoleVariableRef CVarRayTracingShaderRecordCache(
	TEXT("r.D3D12.RayTracing.CacheShaderRecords"),
	GRayTracingCacheShaderRecords,
	TEXT("Automatically cache and re-use SBT hit group records. This significantly improves CPU performance in large scenes with many identical mesh instances. (default = 1)\n")
	TEXT("This mode assumes that contents of uniform buffers does not change during ray tracing resource binding.")
);

static int32 GD3D12RayTracingAllowCompaction = 1;
static FAutoConsoleVariableRef CVarD3D12RayTracingAllowCompaction(
	TEXT("r.D3D12.RayTracing.AllowCompaction"),
	GD3D12RayTracingAllowCompaction,
	TEXT("Whether to automatically perform compaction for static acceleration structures to save GPU memory. (default = 1)\n"),
	ECVF_ReadOnly
);

static int32 GD3D12RayTracingMaxBatchedCompaction = 64;
static FAutoConsoleVariableRef CVarD3D12RayTracingMaxBatchedCompaction(
	TEXT("r.D3D12.RayTracing.MaxBatchedCompaction"),
	GD3D12RayTracingMaxBatchedCompaction,
	TEXT("Maximum of amount of compaction requests and rebuilds per frame. (default = 64)\n"),
	ECVF_ReadOnly
);

static int32 GRayTracingSpecializeStateObjects = 0;
static FAutoConsoleVariableRef CVarRayTracingSpecializeStateObjects(
	TEXT("r.D3D12.RayTracing.SpecializeStateObjects"),
	GRayTracingSpecializeStateObjects,
	TEXT("Whether to create specialized unique ray tracing pipeline state objects for each ray generation shader. (default = 0)\n")
	TEXT("This option can produce more more efficient PSOs for the GPU at the cost of longer creation times and more memory. Requires DXR 1.1.\n"),
	ECVF_ReadOnly
);

static int32 GRayTracingAllowSpecializedStateObjects = 1;
static FAutoConsoleVariableRef CVarRayTracingAllowSpecializedStateObjects(
	TEXT("r.D3D12.RayTracing.AllowSpecializedStateObjects"),
	GRayTracingAllowSpecializedStateObjects,
	TEXT("Whether to use specialized RTPSOs if they have been created. ")
	TEXT("This is intended for performance testingand has no effect if r.D3D12.RayTracing.SpecializeStateObjects is 0. (default = 1)\n")
);

static int32 GD3D12RayTracingGPUValidation = 0;
static FAutoConsoleVariableRef CVarD3D12RayTracingGPUValidation(
	TEXT("r.D3D12.RayTracing.GPUValidation"),
	GD3D12RayTracingGPUValidation,
	TEXT("Whether to perform validation of ray tracing geometry and other structures on the GPU. Requires Shader Model 6. (default = 0)")
);

// Ray tracing stat counters

DECLARE_STATS_GROUP(TEXT("D3D12RHI: Ray Tracing"), STATGROUP_D3D12RayTracing, STATCAT_Advanced);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Created pipelines (total)"), STAT_D3D12RayTracingCreatedPipelines, STATGROUP_D3D12RayTracing);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Compiled shaders (total)"), STAT_D3D12RayTracingCompiledShaders, STATGROUP_D3D12RayTracing);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Allocated bottom level acceleration structures"), STAT_D3D12RayTracingAllocatedBLAS, STATGROUP_D3D12RayTracing);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Allocated top level acceleration structures"), STAT_D3D12RayTracingAllocatedTLAS, STATGROUP_D3D12RayTracing);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Triangles in all BL acceleration structures"), STAT_D3D12RayTracingTrianglesBLAS, STATGROUP_D3D12RayTracing);

DECLARE_DWORD_COUNTER_STAT(TEXT("Built BL AS (per frame)"), STAT_D3D12RayTracingBuiltBLAS, STATGROUP_D3D12RayTracing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Updated BL AS (per frame)"), STAT_D3D12RayTracingUpdatedBLAS, STATGROUP_D3D12RayTracing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Built TL AS (per frame)"), STAT_D3D12RayTracingBuiltTLAS, STATGROUP_D3D12RayTracing);

DECLARE_MEMORY_STAT(TEXT("Total BL AS Memory"), STAT_D3D12RayTracingBLASMemory, STATGROUP_D3D12RayTracing);
DECLARE_MEMORY_STAT(TEXT("Static BL AS Memory"), STAT_D3D12RayTracingStaticBLASMemory, STATGROUP_D3D12RayTracing);
DECLARE_MEMORY_STAT(TEXT("Dynamic BL AS Memory"), STAT_D3D12RayTracingDynamicBLASMemory, STATGROUP_D3D12RayTracing);
DECLARE_MEMORY_STAT(TEXT("TL AS Memory"), STAT_D3D12RayTracingTLASMemory, STATGROUP_D3D12RayTracing);
DECLARE_MEMORY_STAT(TEXT("Total Used Video Memory"), STAT_D3D12RayTracingUsedVideoMemory, STATGROUP_D3D12RayTracing);

DECLARE_CYCLE_STAT(TEXT("RTPSO Compile Shader"), STAT_RTPSO_CompileShader, STATGROUP_D3D12RayTracing);
DECLARE_CYCLE_STAT(TEXT("RTPSO Create Pipeline"), STAT_RTPSO_CreatePipeline, STATGROUP_D3D12RayTracing);

DECLARE_CYCLE_STAT(TEXT("SetRayTracingBindings"), STAT_D3D12SetRayTracingBindings, STATGROUP_D3D12RayTracing);
DECLARE_CYCLE_STAT(TEXT("CreateShaderTable"), STAT_D3D12CreateShaderTable, STATGROUP_D3D12RayTracing);
DECLARE_CYCLE_STAT(TEXT("BuildTopLevel"), STAT_D3D12BuildTLAS, STATGROUP_D3D12RayTracing);
DECLARE_CYCLE_STAT(TEXT("BuildBottomLevel"), STAT_D3D12BuildBLAS, STATGROUP_D3D12RayTracing);
DECLARE_CYCLE_STAT(TEXT("DispatchRays"), STAT_D3D12DispatchRays, STATGROUP_D3D12RayTracing);

static ERayTracingAccelerationStructureFlags GetRayTracingAccelerationStructureBuildFlags(const FRayTracingGeometryInitializer& Initializer);

#if UE_BUILD_SHIPPING
inline void RegisterD3D12RayTracingGeometry(FD3D12RayTracingGeometry* Geometry) {};
inline void UnregisterD3D12RayTracingGeometry(FD3D12RayTracingGeometry* Geometry) {};
#else
struct FD3D12RayTracingGeometryTracker
{
	TSet<FD3D12RayTracingGeometry*> Geometries;
	uint64 TotalBLASSize = 0;
	uint64 MaxTotalBLASSize = 0;
	FCriticalSection CS;

	uint64 GetGeometrySize(FD3D12RayTracingGeometry& Geometry)
	{
		if (Geometry.AccelerationStructureCompactedSize != 0)
		{
			return Geometry.AccelerationStructureCompactedSize;
		}
		else
		{
			return Geometry.SizeInfo.ResultSize;
		}
	}

	void Add(FD3D12RayTracingGeometry* Geometry)
	{
		uint64 BLASSize = GetGeometrySize(*Geometry);

		FScopeLock Lock(&CS);
		Geometries.Add(Geometry);
		TotalBLASSize += BLASSize;

		MaxTotalBLASSize = FMath::Max(MaxTotalBLASSize, TotalBLASSize);
	}

	void Remove(FD3D12RayTracingGeometry* Geometry)
	{
		uint64 BLASSize = GetGeometrySize(*Geometry);

		FScopeLock Lock(&CS);
		Geometries.Remove(Geometry);

		TotalBLASSize -= BLASSize;
	}
};

static FD3D12RayTracingGeometryTracker& GetD3D12RayTracingGeometryTracker()
{
	static FD3D12RayTracingGeometryTracker Instance;
	return Instance;
}

enum class EDumpRayTracingGeometryMode
{
	Top,
	All,
};

static void DumpRayTracingGeometries(EDumpRayTracingGeometryMode Mode, int32 NumEntriesToShow, const FString& NameFilter, bool bCSV, FBufferedOutputDevice& BufferedOutput)
{
	FD3D12RayTracingGeometryTracker& Tracker = GetD3D12RayTracingGeometryTracker();
	FScopeLock Lock(&Tracker.CS);
	
	auto GetGeometrySize = [](FD3D12RayTracingGeometry& Geometry)
	{
		if (Geometry.AccelerationStructureCompactedSize != 0)
		{
			return Geometry.AccelerationStructureCompactedSize;
		}
		else
		{
			return Geometry.SizeInfo.ResultSize;
		}
	};

	TArray<FD3D12RayTracingGeometry*> Geometries = Tracker.Geometries.Array();
	Geometries.Sort([GetGeometrySize](FD3D12RayTracingGeometry& A, FD3D12RayTracingGeometry& B)
	{
		return GetGeometrySize(A) > GetGeometrySize(B);
	});

	FName CategoryName(TEXT("D3D12RayTracing"));
	uint64 TotalSizeBytes = 0;
	uint64 TopSizeBytes = 0;
	BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Tracked FD3D12RayTracingGeometry objects"));

	if (NumEntriesToShow < 0 || NumEntriesToShow > Geometries.Num())
	{
		NumEntriesToShow = Geometries.Num();
	}

	if (NumEntriesToShow != Geometries.Num())
	{
		BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Showing %d out of %d"), NumEntriesToShow, Geometries.Num());
	}

	auto ShouldShow = [&NameFilter](FD3D12RayTracingGeometry* Entry)
	{
		if (NameFilter.IsEmpty())
		{
			return true;
		}

		FString DebugName = Entry->DebugName.ToString();
		if (DebugName.Find(NameFilter, ESearchCase::IgnoreCase) != INDEX_NONE)
		{
			return true;
		}
		else
		{
			return false;
		}
	};

	FArchive* CSVFile{ nullptr };
	if (bCSV)
	{
		const FString Filename = FString::Printf(TEXT("%sd3d12DumpRayTracingGeometries-%s.csv"), *FPaths::ProfilingDir(), *FDateTime::Now().ToString());
		CSVFile = IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_AllowRead);

		const TCHAR* Header = TEXT("Name,Size (MBs),Prims,Segments,Compaction,Update,MarkedForDelete\n");
		CSVFile->Serialize(TCHAR_TO_ANSI(Header), FPlatformString::Strlen(Header));
	}

	int32 ShownEntries = 0;
	for (int32 i=0; i< Geometries.Num(); ++i)
	{
		FD3D12RayTracingGeometry* Geometry = Geometries[i];
		uint64 SizeBytes = GetGeometrySize(*Geometry);

		ERayTracingAccelerationStructureFlags GeometryBuildFlags = GetRayTracingAccelerationStructureBuildFlags(Geometry->Initializer);

		if (ShownEntries < NumEntriesToShow && ShouldShow(Geometry))
		{
			if (bCSV)
			{
				const FString Row = FString::Printf(TEXT("%s,%.3f,%d,%d,%d,%d,%d\n"),
					!Geometry->DebugName.IsNone() ? *Geometry->DebugName.ToString() : TEXT("*UNKNOWN*"),
					SizeBytes / double(1 << 20),
					Geometry->Initializer.TotalPrimitiveCount,
					Geometry->Initializer.Segments.Num(),
					(int32)EnumHasAllFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction),
					(int32)EnumHasAllFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate),
					!Geometry->IsValid());
				CSVFile->Serialize(TCHAR_TO_ANSI(*Row), Row.Len());
			}
			else
			{
				BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Name: %s - Size: %.3f MB - Prims: %d - Segments: %d -  Compaction: %d - Update: %d"),
					!Geometry->DebugName.IsNone() ? *Geometry->DebugName.ToString() : TEXT("*UNKNOWN*"),
					SizeBytes / double(1 << 20),
					Geometry->Initializer.TotalPrimitiveCount,
					Geometry->Initializer.Segments.Num(),
					(int32)EnumHasAllFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction),
					(int32)EnumHasAllFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate));
			}
			TopSizeBytes += SizeBytes;
			++ShownEntries;
		}

		TotalSizeBytes += SizeBytes;
	}

	if (bCSV)
	{
		delete CSVFile;
		CSVFile = nullptr;
	}
	else
	{
		double TotalSizeF = double(TotalSizeBytes) / double(1 << 20);
		double TopSizeF = double(TopSizeBytes) / double(1 << 20);

		if (ShownEntries != Geometries.Num() && ShownEntries)
		{
			BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log,
				TEXT("Use command `D3D12.DumpRayTracingGeometries all/N [name]` to dump all or N objects. ")
				TEXT("Optionally add 'name' to filter entries, such as 'skm_'."));
			BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Shown %d entries. Size: %.3f MB (%.2f%% of total)"),
				ShownEntries, TopSizeF, 100.0 * TopSizeF / TotalSizeF);
		}

		BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Total size: %.3f MB"), TotalSizeF);
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GD3D12DumpRayTracingGeometriesCmd(
	TEXT("D3D12.DumpRayTracingGeometries"),
	TEXT("Dump memory allocations for ray tracing resources."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic([](const TArray<FString>& Args, UWorld*, FOutputDevice& OutputDevice)
{
	// Default: show top 50 largest objects.
	EDumpRayTracingGeometryMode Mode = EDumpRayTracingGeometryMode::Top;
	int32 NumEntriesToShow = 50;
	bool bCSV = false;

	FString NameFilter;

	if (Args.Num())
	{
		if (Args[0] == TEXT("all"))
		{
			Mode = EDumpRayTracingGeometryMode::All;
			NumEntriesToShow = -1;
		}
		else if (FCString::IsNumeric(*Args[0]))
		{
			Mode = EDumpRayTracingGeometryMode::Top;
			LexFromString(NumEntriesToShow, *Args[0]);
		}

		if (Args.Num() > 1)
		{
			NameFilter = Args[1];
		}
	}

	FBufferedOutputDevice BufferedOutput;
	DumpRayTracingGeometries(Mode, NumEntriesToShow, NameFilter, bCSV, BufferedOutput);
	BufferedOutput.RedirectTo(OutputDevice);
}));


static FAutoConsoleCommandWithWorldArgsAndOutputDevice GD3D12DumpRayTracingGeometriesToCSVCmd(
	TEXT("D3D12.DumpRayTracingGeometriesToCSV"),
	TEXT("Dump all memory allocations for ray tracing resources to a CSV file on disc."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic([](const TArray<FString>& Args, UWorld*, FOutputDevice& OutputDevice)
{
	// CSV dumps all entries
	EDumpRayTracingGeometryMode Mode = EDumpRayTracingGeometryMode::All;
	int32 NumEntriesToShow = -1;
	bool bCSV = true;
	FString NameFilter;

	FBufferedOutputDevice BufferedOutput;
	DumpRayTracingGeometries(Mode, NumEntriesToShow, NameFilter, bCSV, BufferedOutput);
	BufferedOutput.RedirectTo(OutputDevice);
}));

inline void RegisterD3D12RayTracingGeometry(FD3D12RayTracingGeometry* Geometry)
{
	GetD3D12RayTracingGeometryTracker().Add(Geometry);
}
inline void UnregisterD3D12RayTracingGeometry(FD3D12RayTracingGeometry* Geometry)
{
	GetD3D12RayTracingGeometryTracker().Remove(Geometry);
}
#endif // UE_BUILD_SHIPPING

struct FD3D12ShaderIdentifier
{
	uint64 Data[4] = {~0ull, ~0ull, ~0ull, ~0ull};

	// No shader is executed if a shader binding table record with null identifier is encountered.
	static const FD3D12ShaderIdentifier Null;

	bool operator == (const FD3D12ShaderIdentifier& Other) const
	{
		return Data[0] == Other.Data[0]
			&& Data[1] == Other.Data[1]
			&& Data[2] == Other.Data[2]
			&& Data[3] == Other.Data[3];
	}

	bool operator != (const FD3D12ShaderIdentifier& Other) const
	{
		return !(*this == Other);
	}

	bool IsValid() const
	{
		return *this != FD3D12ShaderIdentifier();
	}

	void SetData(const void* InData)
	{
		FMemory::Memcpy(Data, InData, sizeof(Data));
	}
};

const FD3D12ShaderIdentifier FD3D12ShaderIdentifier::Null = { 0, 0, 0, 0 };

static_assert(sizeof(FD3D12ShaderIdentifier) == D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, "Unexpected shader identifier size");

static bool ShouldRunRayTracingGPUValidation()
{
	// Wave ops are required to run ray tracing validation shaders
	const bool bSupportsWaveOps = GRHISupportsWaveOperations && RHISupportsWaveOperations(GMaxRHIShaderPlatform);
	return GD3D12RayTracingGPUValidation && bSupportsWaveOps;
}

static D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS TranslateRayTracingAccelerationStructureFlags(ERayTracingAccelerationStructureFlags Flags)
{
	uint32 Result = {};

	auto HandleFlag = [&Flags, &Result](ERayTracingAccelerationStructureFlags Engine, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS Native)
	{
		if (EnumHasAllFlags(Flags, Engine))
		{
			Result |= (uint32)Native;
			EnumRemoveFlags(Flags, Engine);
		}
	};

	HandleFlag(ERayTracingAccelerationStructureFlags::AllowUpdate, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE);
	HandleFlag(ERayTracingAccelerationStructureFlags::AllowCompaction, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION);
	HandleFlag(ERayTracingAccelerationStructureFlags::FastTrace, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);
	HandleFlag(ERayTracingAccelerationStructureFlags::FastBuild, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD);
	HandleFlag(ERayTracingAccelerationStructureFlags::MinimizeMemory, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY);

	checkf(!EnumHasAnyFlags(Flags, Flags), TEXT("Some ERayTracingAccelerationStructureFlags entries were not handled"));

	return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS(Result);
}

static D3D12_RAYTRACING_GEOMETRY_TYPE TranslateRayTracingGeometryType(ERayTracingGeometryType GeometryType)
{
	switch (GeometryType)
	{
	case RTGT_Triangles:
		return D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		break;
	case RTGT_Procedural:
		return D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
		break;
	default:
		checkf(false, TEXT("Unexpected ray tracing geometry type"));
		return D3D12_RAYTRACING_GEOMETRY_TYPE(0);
	}
}

struct FDXILLibrary
{
	// No copy assignment or move because FDXILLibrary points to internal struct memory
	UE_NONCOPYABLE(FDXILLibrary)

	FDXILLibrary() = default;

	void InitFromDXIL(const void* Bytecode, uint32 BytecodeLength, const LPCWSTR* InEntryNames, const LPCWSTR* InExportNames, uint32 NumEntryNames)
	{
		check(NumEntryNames != 0);
		check(InEntryNames);
		check(InExportNames);

		EntryNames.SetNum(NumEntryNames);
		ExportNames.SetNum(NumEntryNames);
		ExportDesc.SetNum(NumEntryNames);

		for (uint32 EntryIndex = 0; EntryIndex < NumEntryNames; ++EntryIndex)
		{
			EntryNames[EntryIndex] = InEntryNames[EntryIndex];
			ExportNames[EntryIndex] = InExportNames[EntryIndex];

			ExportDesc[EntryIndex].ExportToRename = *(EntryNames[EntryIndex]);
			ExportDesc[EntryIndex].Flags = D3D12_EXPORT_FLAG_NONE;
			ExportDesc[EntryIndex].Name = *(ExportNames[EntryIndex]);
		}

		Desc.DXILLibrary.pShaderBytecode = Bytecode;
		Desc.DXILLibrary.BytecodeLength = BytecodeLength;
		Desc.NumExports = ExportDesc.Num();
		Desc.pExports = ExportDesc.GetData();
	}

	void InitFromDXIL(const D3D12_SHADER_BYTECODE& ShaderBytecode, LPCWSTR* InEntryNames, LPCWSTR* InExportNames, uint32 NumEntryNames)
	{
		InitFromDXIL(ShaderBytecode.pShaderBytecode, ShaderBytecode.BytecodeLength, InEntryNames, InExportNames, NumEntryNames);
	}

	D3D12_STATE_SUBOBJECT GetSubobject() const
	{
		D3D12_STATE_SUBOBJECT Subobject = {};
		Subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		Subobject.pDesc = &Desc;
		return Subobject;
	}

	// NOTE: typical DXIL library may contain up to 3 entry points (i.e. hit groups with closest hit, any hit and intersection shaders)
	// Typical case is 1 (RGS, MS or CHS only) or 2 (CHS + AHS for shaders with alpha masking)
	static constexpr uint32 ExpectedEntryPoints = 3;
	TArray<D3D12_EXPORT_DESC, TInlineAllocator<ExpectedEntryPoints>> ExportDesc;
	TArray<FString, TInlineAllocator<ExpectedEntryPoints>> EntryNames;
	TArray<FString, TInlineAllocator<ExpectedEntryPoints>> ExportNames;

	D3D12_DXIL_LIBRARY_DESC Desc = {};
};

static TRefCountPtr<ID3D12StateObject> CreateRayTracingStateObject(
	ID3D12Device5* RayTracingDevice,
	const TArrayView<const FDXILLibrary*>& ShaderLibraries,
	const TArrayView<LPCWSTR>& Exports,
	uint32 MaxAttributeSizeInBytes,
	uint32 MaxPayloadSizeInBytes,
	const TArrayView<const D3D12_HIT_GROUP_DESC>& HitGroups,
	const ID3D12RootSignature* GlobalRootSignature,
	const TArrayView<ID3D12RootSignature*>& LocalRootSignatures,
	const TArrayView<uint32>& LocalRootSignatureAssociations, // indices into LocalRootSignatures, one per export (may be empty, which assumes single root signature used for everything)
	const TArrayView<D3D12_EXISTING_COLLECTION_DESC>& ExistingCollections,
	D3D12_STATE_OBJECT_TYPE StateObjectType // Full RTPSO or a Collection
)
{
	checkf((LocalRootSignatureAssociations.Num() == 0 && LocalRootSignatures.Num() == 1)
		|| (LocalRootSignatureAssociations.Num() == Exports.Num()),
		TEXT("There must be exactly one local root signature association per export."));

	TRefCountPtr<ID3D12StateObject> Result;

	// There are several pipeline sub-objects that are always required:
	// 1) D3D12_RAYTRACING_SHADER_CONFIG
	// 2) D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION
	// 3) D3D12_RAYTRACING_PIPELINE_CONFIG
	// 4) D3D12_STATE_OBJECT_CONFIG
	// 5) Global root signature
	static constexpr uint32 NumRequiredSubobjects = 5;

	TArray<D3D12_STATE_SUBOBJECT> Subobjects;
	Subobjects.SetNumUninitialized(NumRequiredSubobjects
		+ ShaderLibraries.Num()
		+ HitGroups.Num()
		+ LocalRootSignatures.Num()
		+ Exports.Num()
		+ ExistingCollections.Num()
	);

	TArray<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> ExportAssociations;
	ExportAssociations.SetNumUninitialized(Exports.Num());

	uint32 Index = 0;

	const uint32 NumExports = Exports.Num();

	// Shader libraries

	for (const FDXILLibrary* Library : ShaderLibraries)
	{
		Subobjects[Index++] = Library->GetSubobject();
	}

	// Shader config

	D3D12_RAYTRACING_SHADER_CONFIG ShaderConfig = {};
	ShaderConfig.MaxAttributeSizeInBytes = MaxAttributeSizeInBytes;
	check(ShaderConfig.MaxAttributeSizeInBytes <= RAY_TRACING_MAX_ALLOWED_ATTRIBUTE_SIZE);
	ShaderConfig.MaxPayloadSizeInBytes = MaxPayloadSizeInBytes;

	const uint32 ShaderConfigIndex = Index;
	Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &ShaderConfig};

	// Shader config association

	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION ShaderConfigAssociation = {};
	ShaderConfigAssociation.NumExports = Exports.Num();
	ShaderConfigAssociation.pExports = Exports.GetData();
	ShaderConfigAssociation.pSubobjectToAssociate = &Subobjects[ShaderConfigIndex];
	Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &ShaderConfigAssociation };

	// Hit groups

	for (const D3D12_HIT_GROUP_DESC& HitGroupDesc : HitGroups)
	{
		Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &HitGroupDesc };
	}

	// Pipeline config

	D3D12_RAYTRACING_PIPELINE_CONFIG PipelineConfig = {};
	PipelineConfig.MaxTraceRecursionDepth = RAY_TRACING_MAX_ALLOWED_RECURSION_DEPTH;
	const uint32 PipelineConfigIndex = Index;
	Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &PipelineConfig };

	// State object config

	D3D12_STATE_OBJECT_CONFIG StateObjectConfig = {};
	if (GRHISupportsRayTracingPSOAdditions)
	{
		StateObjectConfig.Flags = D3D12_STATE_OBJECT_FLAG_ALLOW_STATE_OBJECT_ADDITIONS;
	}
	Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG, &StateObjectConfig };

	// Global root signature

	Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &GlobalRootSignature };

	// Local root signatures

	const uint32 LocalRootSignatureBaseIndex = Index;
	for (int32 SignatureIndex = 0; SignatureIndex < LocalRootSignatures.Num(); ++SignatureIndex)
	{
		checkf(LocalRootSignatures[SignatureIndex], TEXT("All local root signatures must be valid"));
		Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &LocalRootSignatures[SignatureIndex] };
	}

	// Local root signature associations

	for (int32 ExportIndex = 0; ExportIndex < Exports.Num(); ++ExportIndex)
	{
		// If custom LocalRootSignatureAssociations data is not provided, then assume same default local RS association.
		const int32 LocalRootSignatureIndex = LocalRootSignatureAssociations.Num() != 0
			? LocalRootSignatureAssociations[ExportIndex]
			: 0;

		D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION& Association = ExportAssociations[ExportIndex];
		Association = D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION{};
		Association.NumExports = 1;
		Association.pExports = &Exports[ExportIndex];

		check(LocalRootSignatureIndex < LocalRootSignatures.Num());
		Association.pSubobjectToAssociate = &Subobjects[LocalRootSignatureBaseIndex + LocalRootSignatureIndex];

		Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &ExportAssociations[ExportIndex] };
	}

	// Existing collection objects

	for (int32 CollectionIndex = 0; CollectionIndex < ExistingCollections.Num(); ++CollectionIndex)
	{
		Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION, &ExistingCollections[CollectionIndex] };
	}

	// Done!

	checkf(Index == Subobjects.Num(), TEXT("All pipeline subobjects must be initialized."));

	// Create ray tracing pipeline state object

	D3D12_STATE_OBJECT_DESC Desc = {};
	Desc.NumSubobjects = Index;
	Desc.pSubobjects = &Subobjects[0];
	Desc.Type = StateObjectType;

	VERIFYD3D12RESULT(RayTracingDevice->CreateStateObject(&Desc, IID_PPV_ARGS(Result.GetInitReference())));

	INC_DWORD_STAT(STAT_D3D12RayTracingCreatedPipelines);
	INC_DWORD_STAT_BY(STAT_D3D12RayTracingCompiledShaders, NumExports);

	return Result;
}

inline uint64 GetShaderHash64(FRHIRayTracingShader* ShaderRHI)
{
	uint64 ShaderHash; // 64 bits from the shader SHA1
	FMemory::Memcpy(&ShaderHash, ShaderRHI->GetHash().Hash, sizeof(ShaderHash));
	return ShaderHash;
}

// Generates a stable symbol name for a ray tracing shader, used for RT PSO creation.

inline FString GenerateShaderName(const TCHAR* Prefix, uint64 Hash)
{
	return FString::Printf(TEXT("%s_%016llx"), Prefix, Hash);
}

inline FString GenerateShaderName(FRHIRayTracingShader* ShaderRHI)
{
	const FD3D12RayTracingShader* Shader = FD3D12DynamicRHI::ResourceCast(ShaderRHI);
	uint64 ShaderHash = GetShaderHash64(ShaderRHI);
	return GenerateShaderName(*(Shader->EntryPoint), ShaderHash);
}

static FD3D12ShaderIdentifier GetShaderIdentifier(ID3D12StateObjectProperties* PipelineProperties, const TCHAR* ExportName)
{
	const void* ShaderIdData = PipelineProperties->GetShaderIdentifier(ExportName);
	checkf(ShaderIdData, TEXT("Couldn't find requested export in the ray tracing shader pipeline"));

	FD3D12ShaderIdentifier Result;
	Result.SetData(ShaderIdData);

	return Result;
}

static FD3D12ShaderIdentifier GetShaderIdentifier(ID3D12StateObject* StateObject, const TCHAR* ExportName)
{
	TRefCountPtr<ID3D12StateObjectProperties> PipelineProperties;
	HRESULT QueryInterfaceResult = StateObject->QueryInterface(IID_PPV_ARGS(PipelineProperties.GetInitReference()));
	checkf(SUCCEEDED(QueryInterfaceResult), TEXT("Failed to query pipeline properties from the ray tracing pipeline state object. Result=%08x"), QueryInterfaceResult);

	return GetShaderIdentifier(PipelineProperties, ExportName);
}

static bool TryGetShaderIdentifier(ID3D12StateObject* StateObject, const TCHAR* ExportName, FD3D12ShaderIdentifier& OutShaderIdentifier)
{
	TRefCountPtr<ID3D12StateObjectProperties> PipelineProperties;
	HRESULT QueryInterfaceResult = StateObject->QueryInterface(IID_PPV_ARGS(PipelineProperties.GetInitReference()));
	if (!SUCCEEDED(QueryInterfaceResult))
	{
		return false;
	}
	const void* ShaderIdData = PipelineProperties->GetShaderIdentifier(ExportName);
	if (ShaderIdData)
	{
		OutShaderIdentifier.SetData(ShaderIdData);
		return true;
	}
	else
	{
		return false;
	}
}

FD3D12RayTracingCompactionRequestHandler::FD3D12RayTracingCompactionRequestHandler(FD3D12Device* Device)
	: FD3D12DeviceChild(Device)
{
	D3D12_RESOURCE_DESC PostBuildInfoBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(GD3D12RayTracingMaxBatchedCompaction * sizeof(uint64), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	FRHIGPUMask GPUMask = FRHIGPUMask::FromIndex(GetParentDevice()->GetGPUIndex());
	ID3D12ResourceAllocator* ResourceAllocator = nullptr;
	bool bHasInitialData = false;
	PostBuildInfoBuffer = GetParentDevice()->GetParentAdapter()->CreateRHIBuffer(PostBuildInfoBufferDesc, 8,
		FRHIBufferDesc(PostBuildInfoBufferDesc.Width, 0, BUF_UnorderedAccess | BUF_SourceCopy), ED3D12ResourceStateMode::MultiState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, bHasInitialData, GPUMask, ResourceAllocator, TEXT("PostBuildInfoBuffer"));
	SetName(PostBuildInfoBuffer->GetResource(), TEXT("PostBuildInfoBuffer"));

	PostBuildInfoStagingBuffer = RHICreateStagingBuffer();
}

void FD3D12RayTracingCompactionRequestHandler::RequestCompact(FD3D12RayTracingGeometry* InRTGeometry)
{
	uint32 GPUIndex = GetParentDevice()->GetGPUIndex();
	check(InRTGeometry->AccelerationStructureBuffers[GPUIndex]);
	ERayTracingAccelerationStructureFlags GeometryBuildFlags = GetRayTracingAccelerationStructureBuildFlags(InRTGeometry->Initializer);
	check(EnumHasAllFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction) &&
		EnumHasAllFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::FastTrace) &&
		!EnumHasAnyFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate));

	FScopeLock Lock(&CS);
	PendingRequests.Add(InRTGeometry);
}

bool FD3D12RayTracingCompactionRequestHandler::ReleaseRequest(FD3D12RayTracingGeometry* InRTGeometry)
{
	FScopeLock Lock(&CS);

	// Remove from pending list, not found then try active requests
	if (PendingRequests.Remove(InRTGeometry) <= 0)
	{
		// If currently enqueued, then clear pointer to not handle the compaction request anymore			
		for (int32 BLASIndex = 0; BLASIndex < ActiveBLASGPUAddresses.Num(); ++BLASIndex)
		{
			if (ActiveRequests[BLASIndex] == InRTGeometry)
			{
				ActiveRequests[BLASIndex] = nullptr;
				return true;
			}
		}

		return false;
	}
	else
	{
		return true;
	}
}

void FD3D12RayTracingCompactionRequestHandler::Update(FD3D12CommandContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("FD3D12RT/Compaction"));
	FScopeLock Lock(&CS);

	// process previous build request data retrieval
	uint32 GPUIndex = GetParentDevice()->GetGPUIndex();

	if (ActiveBLASGPUAddresses.Num() > 0)
	{
		// Ensure that our builds & copies have finished on GPU when enqueued - if still busy then wait until done
		if (PostBuildInfoBufferReadbackSyncPoint && !PostBuildInfoBufferReadbackSyncPoint->IsComplete())
		{
			return;
		}

		// Readback the sizes from the readback buffer and schedule new builds ops on the RTGeometry objects
		uint64* SizesAfterCompaction = (uint64*)PostBuildInfoStagingBuffer->Lock(0, ActiveBLASGPUAddresses.Num() * sizeof(uint64));
		for (int32 BLASIndex = 0; BLASIndex < ActiveBLASGPUAddresses.Num(); ++BLASIndex)
		{
			if (ActiveRequests[BLASIndex] != nullptr)
			{
				ActiveRequests[BLASIndex]->CompactAccelerationStructure(Context, GPUIndex, SizesAfterCompaction[BLASIndex]);
			}
		}
		PostBuildInfoStagingBuffer->Unlock();

		// reset working values
		PostBuildInfoBufferReadbackSyncPoint = nullptr;
		ActiveRequests.Empty(ActiveRequests.Num());
		ActiveBLASGPUAddresses.Empty(ActiveBLASGPUAddresses.Num());
	}

	// build a new set of build requests to extract the build data	
	for (FD3D12RayTracingGeometry* RTGeometry : PendingRequests)
	{
		ActiveRequests.Add(RTGeometry);

		FD3D12ResourceLocation& ResourceLocation = RTGeometry->AccelerationStructureBuffers[GPUIndex].GetReference()->ResourceLocation;
		ActiveBLASGPUAddresses.Add(ResourceLocation.GetGPUVirtualAddress());

		Context.UpdateResidency(ResourceLocation.GetResource());

		// enqueued enough requests for this update round
		if (ActiveRequests.Num() >= GD3D12RayTracingMaxBatchedCompaction)
		{
			break;
		}
	}

	// Do we have requests?
	if (ActiveRequests.Num() > 0)
	{
		// clear out all of the pending requests, don't allow the array to shrink
		PendingRequests.RemoveAt(0, ActiveRequests.Num(), EAllowShrinking::No);

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC PostBuildInfoDesc = {};
		PostBuildInfoDesc.DestBuffer = PostBuildInfoBuffer->ResourceLocation.GetGPUVirtualAddress();
		PostBuildInfoDesc.InfoType = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE;

		Context.TransitionResource(PostBuildInfoBuffer->GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0);

		// Force UAV barrier to make sure all previous builds ops are finished
		Context.AddUAVBarrier();
		Context.FlushResourceBarriers();

		// Emit the RT post build info from the selected requests
		Context.RayTracingCommandList()->EmitRaytracingAccelerationStructurePostbuildInfo(&PostBuildInfoDesc, ActiveBLASGPUAddresses.Num(), ActiveBLASGPUAddresses.GetData());

		// Transition to copy source and perform the copy to readback
		Context.TransitionResource(PostBuildInfoBuffer.GetReference()->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
		Context.FlushResourceBarriers();

		Context.RHICopyToStagingBuffer(PostBuildInfoBuffer, PostBuildInfoStagingBuffer, 0, sizeof(uint64) * ActiveBLASGPUAddresses.Num());

		// Update the sync point
		PostBuildInfoBufferReadbackSyncPoint = Context.GetContextSyncPoint();
	}
}


// Cache for ray tracing pipeline collection objects, containing single shaders that can be linked into full pipelines.
class FD3D12RayTracingPipelineCache : FD3D12DeviceChild
{
public:

	UE_NONCOPYABLE(FD3D12RayTracingPipelineCache)

	FD3D12RayTracingPipelineCache(FD3D12Device* Device)
		: FD3D12DeviceChild(Device)
		, DefaultLocalRootSignature(Device->GetParentAdapter())
	{
		// Default empty local root signature
		LLM_SCOPE_BYNAME(TEXT("FD3D12RT/PipelineCache"));
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC LocalRootSignatureDesc = {};
		if (Device->GetParentAdapter()->GetRootSignatureVersion() >= D3D_ROOT_SIGNATURE_VERSION_1_1)
		{
			LocalRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
			LocalRootSignatureDesc.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
		}
		else
		{
		LocalRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
		LocalRootSignatureDesc.Desc_1_0.Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
		}
		
		DefaultLocalRootSignature.Init(LocalRootSignatureDesc);
	}

	~FD3D12RayTracingPipelineCache()
	{
		Reset();
	}

	struct FKey
	{
		uint64 ShaderHash = 0;
		uint32 MaxAttributeSizeInBytes = 0;
		uint32 MaxPayloadSizeInBytes = 0;
		ID3D12RootSignature* GlobalRootSignature = nullptr;
		ID3D12RootSignature* LocalRootSignature = nullptr;

		bool operator == (const FKey& Other) const
		{
			return ShaderHash == Other.ShaderHash
				&& MaxAttributeSizeInBytes == Other.MaxAttributeSizeInBytes
				&& MaxPayloadSizeInBytes == Other.MaxPayloadSizeInBytes
				&& GlobalRootSignature == Other.GlobalRootSignature
				&& LocalRootSignature == Other.LocalRootSignature;
		}

		inline friend uint32 GetTypeHash(const FKey& Key)
		{
			return Key.ShaderHash;
		}
	};

	enum class ECollectionType
	{
		Unknown,
		RayGen,
		Miss,
		HitGroup,
		Callable,
	};

	struct FEntry
	{
		// Move-only type
		FEntry() = default;
		FEntry(FEntry&& Other) = default;

		FEntry(const FEntry&) = delete;
		FEntry& operator = (const FEntry&) = delete;
		FEntry& operator = (FEntry&& Other) = delete;

		D3D12_EXISTING_COLLECTION_DESC GetCollectionDesc()
		{
			check(bDeserialized || (CompileEvent.IsValid() && CompileEvent->IsComplete()));
			check(StateObject);

			D3D12_EXISTING_COLLECTION_DESC Result = {};
			Result.pExistingCollection = StateObject;

			return Result;
		}

		const TCHAR* GetPrimaryExportNameChars()
		{
			checkf(ExportNames.Num()!=0, TEXT("This ray tracing shader collection does not export any symbols."));
			return *(ExportNames[0]);
		}

		ECollectionType CollectionType = ECollectionType::Unknown;

		TRefCountPtr<FD3D12RayTracingShader> Shader;

		TRefCountPtr<ID3D12StateObject> StateObject;
		FD3D12RayTracingPipelineInfo PipelineInfo;

		FGraphEventRef CompileEvent;
		bool bDeserialized = false;

		static constexpr uint32 MaxExports = 4;
		TArray<FString, TFixedAllocator<MaxExports>> ExportNames;

		FD3D12ShaderIdentifier Identifier;

		float CompileTimeMS = 0.0f;
	};

	static const TCHAR* GetCollectionTypeName(ECollectionType Type)
	{
		switch (Type)
		{
		case ECollectionType::Unknown:
			return TEXT("Unknown");
		case ECollectionType::RayGen:
			return TEXT("RayGen");
		case ECollectionType::Miss:
			return TEXT("Miss");
		case ECollectionType::HitGroup:
			return TEXT("HitGroup");
		case ECollectionType::Callable:
			return TEXT("Callable");
		default:
			return TEXT("");
		}
	}

	class FShaderCompileTask
	{
	public:

		UE_NONCOPYABLE(FShaderCompileTask)

		FShaderCompileTask(
				FEntry& InEntry,
				FKey InCacheKey,
				FD3D12Device* InDevice,
				ECollectionType InCollectionType)
			: Entry(InEntry)
			, CacheKey(InCacheKey)
			, Device(InDevice)
			, RayTracingDevice(InDevice->GetDevice5())
			, CollectionType(InCollectionType)
		{
		}

		static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			SCOPE_CYCLE_COUNTER(STAT_RTPSO_CompileShader);
			TRACE_CPUPROFILER_EVENT_SCOPE(ShaderCompileTask);

			uint64 CompileTimeCycles = 0;
			CompileTimeCycles -= FPlatformTime::Cycles64();

			FD3D12RayTracingShader* Shader = Entry.Shader;

			static constexpr uint32 MaxEntryPoints = 3; // CHS+AHS+IS for HitGroup or just a single entry point for other collection types
			TArray<LPCWSTR, TFixedAllocator<MaxEntryPoints>> OriginalEntryPoints;
			TArray<LPCWSTR, TFixedAllocator<MaxEntryPoints>> RenamedEntryPoints;

			const uint32 NumHitGroups = CollectionType == ECollectionType::HitGroup ? 1 : 0;
			const uint64 ShaderHash = CacheKey.ShaderHash;
			ID3D12RootSignature* GlobalRootSignature = CacheKey.GlobalRootSignature;
			ID3D12RootSignature* LocalRootSignature = CacheKey.LocalRootSignature;
			const uint32 DefaultLocalRootSignatureIndex = 0;
			uint32 MaxAttributeSizeInBytes = CacheKey.MaxAttributeSizeInBytes;
			uint32 MaxPayloadSizeInBytes = CacheKey.MaxPayloadSizeInBytes;

			D3D12_HIT_GROUP_DESC HitGroupDesc = {};

			if (CollectionType == ECollectionType::HitGroup)
			{
				HitGroupDesc.HitGroupExport = Entry.GetPrimaryExportNameChars();
				HitGroupDesc.Type = Shader->IntersectionEntryPoint.IsEmpty() ? D3D12_HIT_GROUP_TYPE_TRIANGLES : D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;

				{
					const FString& ExportName = Entry.ExportNames.Add_GetRef(GenerateShaderName(TEXT("CHS"), ShaderHash));

					HitGroupDesc.ClosestHitShaderImport = *ExportName;

					OriginalEntryPoints.Add(*(Shader->EntryPoint));
					RenamedEntryPoints.Add(*ExportName);
				}

				if (!Shader->AnyHitEntryPoint.IsEmpty())
				{
					const FString& ExportName = Entry.ExportNames.Add_GetRef(GenerateShaderName(TEXT("AHS"), ShaderHash));

					HitGroupDesc.AnyHitShaderImport = *ExportName;

					OriginalEntryPoints.Add(*(Shader->AnyHitEntryPoint));
					RenamedEntryPoints.Add(*ExportName);
				}

				if (!Shader->IntersectionEntryPoint.IsEmpty())
				{
					const FString& ExportName = Entry.ExportNames.Add_GetRef(GenerateShaderName(TEXT("IS"), ShaderHash));

					HitGroupDesc.IntersectionShaderImport = *ExportName;

					OriginalEntryPoints.Add(*(Shader->IntersectionEntryPoint));
					RenamedEntryPoints.Add(*ExportName);
				}
			}
			else
			{
				checkf(CollectionType == ECollectionType::Miss || CollectionType == ECollectionType::RayGen || CollectionType == ECollectionType::Callable, TEXT("Unexpected RT sahder collection type"));

				OriginalEntryPoints.Add(*(Shader->EntryPoint));
				RenamedEntryPoints.Add(Entry.GetPrimaryExportNameChars());
			}

			// Validate that memory reservation was correct

			check(Entry.ExportNames.Num() <= Entry.MaxExports);

			FDXILLibrary Library;
			Library.InitFromDXIL(Shader->GetShaderBytecode(), OriginalEntryPoints.GetData(), RenamedEntryPoints.GetData(), OriginalEntryPoints.Num());

			const FDXILLibrary* LibraryPtr = &Library;

			Entry.StateObject = CreateRayTracingStateObject(
				RayTracingDevice,
				MakeArrayView(&LibraryPtr, 1),
				RenamedEntryPoints,
				MaxAttributeSizeInBytes,
				MaxPayloadSizeInBytes,
				MakeArrayView(&HitGroupDesc, NumHitGroups),
				GlobalRootSignature,
				MakeArrayView(&LocalRootSignature, 1),
				{}, // LocalRootSignatureAssociations (single RS will be used for all exports since this is null)
				{}, // ExistingCollections
				D3D12_STATE_OBJECT_TYPE_COLLECTION);

			if (Entry.StateObject)
			{
				Device->GetRayTracingPipelineInfo(Entry.StateObject, &Entry.PipelineInfo);
			}

			// Shader identifier can be queried immediately here per PSO collection, however this does not work on old NVIDIA drivers (430.00).
			// Therefore shader identifiers need to be queried from the final linked pipeline (JIRA DH-2182) if a known bad driver is detected.
			if (GD3D12WorkaroundFlags.bAllowGetShaderIdentifierOnCollectionSubObject)
			{
				// Some DXR implementations (such as VKD3D) may not support shader ID queries from collections.
				// If shader ID query fails, we will attempt to query it from the final linked RTPSO later.
				TryGetShaderIdentifier(Entry.StateObject, Entry.GetPrimaryExportNameChars(), Entry.Identifier);
			}

			CompileTimeCycles += FPlatformTime::Cycles64();

			Entry.CompileTimeMS = float(FPlatformTime::ToMilliseconds64(CompileTimeCycles));

			if (Entry.CompileTimeMS >= 1000.0f)
			{
				// Log compilations of individual shaders that took more than 1 second
				UE_LOG(LogD3D12RHI, Log, TEXT("Compiled %s for RTPSO in %.2f ms."), OriginalEntryPoints[0], Entry.CompileTimeMS);
			}
		}

		FORCEINLINE TStatId GetStatId() const
		{
			return GET_STATID(STAT_RTPSO_CompileShader);
		}

		ENamedThreads::Type GetDesiredThread()
		{
			return ENamedThreads::AnyHiPriThreadHiPriTask;
		}

		FEntry& Entry;
		FKey CacheKey;
		FD3D12Device* Device;
		ID3D12Device5* RayTracingDevice;
		ECollectionType CollectionType;

	};

	FEntry* GetOrCompileShader(
		FD3D12Device* Device,
		FD3D12RayTracingShader* Shader,
		ID3D12RootSignature* GlobalRootSignature,
		uint32 MaxAttributeSizeInBytes,
		uint32 MaxPayloadSizeInBytes,
		ECollectionType CollectionType,
		FGraphEventArray& CompletionList,
		bool* bOutCacheHit = nullptr)
	{
		FScopeLock Lock(&CriticalSection);

		const uint64 ShaderHash = GetShaderHash64(Shader);

		ID3D12RootSignature* LocalRootSignature = nullptr;
		if (CollectionType == ECollectionType::RayGen)
		{
			// RayGen shaders use a default empty local root signature as all their resources bound via global RS.
			LocalRootSignature = DefaultLocalRootSignature.GetRootSignature();
		}
		else
		{
			// All other shaders (hit groups, miss, callable) use custom root signatures.
			LocalRootSignature = Shader->pRootSignature->GetRootSignature();
		}

		FKey CacheKey;
		CacheKey.ShaderHash = ShaderHash;
		CacheKey.MaxAttributeSizeInBytes = MaxAttributeSizeInBytes;
		CacheKey.MaxPayloadSizeInBytes = MaxPayloadSizeInBytes;
		CacheKey.GlobalRootSignature = GlobalRootSignature;
		CacheKey.LocalRootSignature = LocalRootSignature;

		FEntry*& FindResult = Cache.FindOrAdd(CacheKey);

		if (FindResult)
		{
			if (bOutCacheHit) *bOutCacheHit = true;
		}
		else
		{
			if (bOutCacheHit) *bOutCacheHit = false;

			if (FindResult == nullptr)
			{
				FindResult = new FEntry;
			}

			FEntry& Entry = *FindResult;

			Entry.CollectionType = CollectionType;
			Entry.Shader = Shader;

			if (Shader->bPrecompiledPSO)
			{
				D3D12_SHADER_BYTECODE Bytecode = Shader->GetShaderBytecode();
				Entry.StateObject = Device->DeserializeRayTracingStateObject(Bytecode, GlobalRootSignature);
				if (Entry.StateObject)
				{
					Device->GetRayTracingPipelineInfo(Entry.StateObject, &Entry.PipelineInfo);
				}

				checkf(Entry.StateObject != nullptr, TEXT("Failed to deserialize RTPSO"));

				Entry.ExportNames.Add(Shader->EntryPoint);
				Entry.Identifier = GetShaderIdentifier(Entry.StateObject, *Shader->EntryPoint);
				Entry.bDeserialized = true;
			}
			else
			{
				// Generate primary export name, which is immediately required on the PSO creation thread.
				Entry.ExportNames.Add(GenerateShaderName(GetCollectionTypeName(CollectionType), ShaderHash));
				checkf(Entry.ExportNames.Num() == 1, TEXT("Primary export name must always be first."));

				// Defer actual compilation to another task, as there may be many shaders that may be compiled in parallel.
				// Result of the compilation (the collection PSO) is not needed until final RT PSO is linked.
				Entry.CompileEvent = TGraphTask<FShaderCompileTask>::CreateTask().ConstructAndDispatchWhenReady(
					Entry,
					CacheKey,
					Device,
					CollectionType
				);
			}
		}

		if (FindResult->CompileEvent.IsValid())
		{
			if (!FindResult->CompileEvent->IsComplete())
			{
				CompletionList.Add(FindResult->CompileEvent);
			}
		}
		else
		{
			check(FindResult->StateObject != nullptr);
		}

		return FindResult;
	}

	void Reset()
	{
		FScopeLock Lock(&CriticalSection);

		for (auto It : Cache)
		{
			delete It.Value;
		}

		Cache.Reset();
	}

	ID3D12RootSignature* GetGlobalRootSignature()
	{
		FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
		const FD3D12RootSignature* RootSignature = Adapter->GetGlobalRayTracingRootSignature();
		return RootSignature->GetRootSignature();
	}

private:

	FCriticalSection CriticalSection;
	TMap<FKey, FEntry*> Cache;
	FD3D12RootSignature DefaultLocalRootSignature; // Default empty root signature used for default hit shaders.
};

class FD3D12RayTracingShaderTable
{
private:
	void WriteData(uint32 WriteOffset, const void* InData, uint32 InDataSize)
	{
#if DO_CHECK && DO_GUARD_SLOW
		Data.RangeCheck(WriteOffset);
		Data.RangeCheck(WriteOffset + InDataSize - 1);
#endif // DO_CHECK && DO_GUARD_SLOW

		FMemory::Memcpy(Data.GetData() + WriteOffset, InData, InDataSize);
	}

	void WriteLocalShaderRecord(uint32 ShaderTableOffset, uint32 RecordIndex, uint32 OffsetWithinRecord, const void* InData, uint32 InDataSize)
	{
		checkfSlow(OffsetWithinRecord % 4 == 0, TEXT("SBT record parameters must be written on DWORD-aligned boundary"));
		checkfSlow(InDataSize % 4 == 0, TEXT("SBT record parameters must be DWORD-aligned"));
		checkfSlow(OffsetWithinRecord + InDataSize <= LocalRecordSizeUnaligned, TEXT("SBT record write request is out of bounds"));

		const uint32 WriteOffset = ShaderTableOffset + LocalRecordStride * RecordIndex + OffsetWithinRecord;

		WriteData(WriteOffset, InData, InDataSize);
	}

public:

	UE_NONCOPYABLE(FD3D12RayTracingShaderTable)

	static constexpr uint32 MaxBindingWorkers = FD3D12RayTracingScene::MaxBindingWorkers;

	struct FInitializer
	{
		uint32 NumRayGenShaders = 0;
		uint32 NumMissShaders = 0;
		uint32 NumMissRecords = 0;
		uint32 NumHitRecords = 0;
		uint32 NumCallableRecords = 0;
		uint32 LocalRootDataSize = 0;
		uint32 MaxViewDescriptorsPerRecord = 0;
	};

	FD3D12RayTracingShaderTable()
	{
	}

	~FD3D12RayTracingShaderTable()
	{
		delete DescriptorCache;
	#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		for (FWorkerThreadData& ThisWorkerData : WorkerData)
		{
			for (FD3D12ConstantBufferView* CBV : ThisWorkerData.TransientCBVs)
			{
				delete CBV;
			}
		}
	#endif // D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
	}

	void Init(const FInitializer& Initializer, FD3D12Device* Device, const TArrayView<const FD3D12ShaderIdentifier>& RaygenIdentifiers, const FD3D12ShaderIdentifier& DefaultHitGroupIdentifier)
	{
		checkf(Initializer.LocalRootDataSize <= 4096, TEXT("The maximum size of a local root signature is 4KB.")); // as per section 4.22.1 of DXR spec v1.0
		checkf(Initializer.NumRayGenShaders >= 1, TEXT("All shader tables must contain at least one raygen shader."));

		LocalRecordSizeUnaligned = ShaderIdentifierSize + Initializer.LocalRootDataSize;
		LocalRecordStride = RoundUpToNextMultiple(LocalRecordSizeUnaligned, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		// Custom descriptor cache is only required when local resources may be bound.
		// If only global resources are used, then transient descriptor cache can be used.
		const bool bNeedsDescriptorCache = (Initializer.NumHitRecords + Initializer.NumCallableRecords + Initializer.NumMissRecords) * Initializer.LocalRootDataSize != 0;

		if (bNeedsDescriptorCache)
		{
			// #dxr_todo UE-72158: Remove this when RT descriptors are sub-allocated from the global view descriptor heap.

			if (GD3D12ExplicitViewDescriptorHeapOverflowReported)
			{
				GD3D12ExplicitViewDescriptorHeapSize = GD3D12ExplicitViewDescriptorHeapSize * 2;
				GD3D12ExplicitViewDescriptorHeapOverflowReported = 0;
			}

			// D3D12 is guaranteed to support 1M (D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1) descriptors in a CBV/SRV/UAV heap, so clamp the size to this.
			// https://docs.microsoft.com/en-us/windows/desktop/direct3d12/hardware-support
			const uint32 NumViewDescriptors = FMath::Min(D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1, GD3D12ExplicitViewDescriptorHeapSize);
			const uint32 NumSamplerDescriptors = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;

			DescriptorCache = new FD3D12ExplicitDescriptorCache(Device, FD3D12RayTracingScene::MaxBindingWorkers);
			DescriptorCache->Init(NumViewDescriptors, NumSamplerDescriptors, ERHIBindlessConfiguration::RayTracingShaders);
		}

		NumRayGenShaders = Initializer.NumRayGenShaders;
		NumMissRecords = Initializer.NumMissRecords;
		NumHitRecords = Initializer.NumHitRecords;
		NumCallableRecords = Initializer.NumCallableRecords;

		uint32 TotalDataSize = 0;

		RayGenShaderTableOffset = TotalDataSize;
		TotalDataSize += NumRayGenShaders * RayGenRecordStride;
		TotalDataSize = RoundUpToNextMultiple(TotalDataSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		DefaultHitGroupShaderTableOffset = TotalDataSize;
		TotalDataSize += ShaderIdentifierSize;
		TotalDataSize = RoundUpToNextMultiple(TotalDataSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		HitGroupShaderTableOffset = TotalDataSize;
		TotalDataSize += Initializer.NumHitRecords * LocalRecordStride;
		TotalDataSize = RoundUpToNextMultiple(TotalDataSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		CallableShaderTableOffset = TotalDataSize;
		TotalDataSize += Initializer.NumCallableRecords * LocalRecordStride;
		TotalDataSize = RoundUpToNextMultiple(TotalDataSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		MissShaderTableOffset = TotalDataSize;
		TotalDataSize += Initializer.NumMissRecords * LocalRecordStride;
		TotalDataSize = RoundUpToNextMultiple(TotalDataSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		Data.SetNumZeroed(TotalDataSize);
#if DO_CHECK
		bWasDefaultMissShaderSet = false;
#endif
		SetRayGenIdentifiers(RaygenIdentifiers);
		SetDefaultHitGroupIdentifier(DefaultHitGroupIdentifier);
		SetDefaultMissShaderIdentifier(FD3D12ShaderIdentifier::Null);
		SetDefaultCallableShaderIdentifier(FD3D12ShaderIdentifier::Null);

		// Keep CPU-side data after upload
		Data.SetAllowCPUAccess(true);
	}

	template <typename T>
	void SetLocalShaderParameters(uint32 ShaderTableOffset, uint32 RecordIndex, uint32 InOffsetWithinRootSignature, const T& Parameters)
	{
		WriteLocalShaderRecord(ShaderTableOffset, RecordIndex, ShaderIdentifierSize + InOffsetWithinRootSignature, &Parameters, sizeof(Parameters));
	}

	void SetLocalShaderParameters(uint32 ShaderTableOffset, uint32 RecordIndex, uint32 InOffsetWithinRootSignature, const void* InData, uint32 InDataSize)
	{
		WriteLocalShaderRecord(ShaderTableOffset, RecordIndex, ShaderIdentifierSize + InOffsetWithinRootSignature, InData, InDataSize);
	}

	template <typename T>
	void SetMissShaderParameters(uint32 RecordIndex, uint32 InOffsetWithinRootSignature, const T& Parameters)
	{
		const uint32 ShaderTableOffset = MissShaderTableOffset;
		WriteLocalShaderRecord(ShaderTableOffset, RecordIndex, ShaderIdentifierSize + InOffsetWithinRootSignature, &Parameters, sizeof(Parameters));
	}

	template <typename T>
	void SetCallableShaderParameters(uint32 RecordIndex, uint32 InOffsetWithinRootSignature, const T& Parameters)
	{
		const uint32 ShaderTableOffset = CallableShaderTableOffset;
		WriteLocalShaderRecord(ShaderTableOffset, RecordIndex, ShaderIdentifierSize + InOffsetWithinRootSignature, &Parameters, sizeof(Parameters));
	}
	
	void CopyLocalShaderParameters(uint32 InShaderTableOffset, uint32 InDestRecordIndex, uint32 InSourceRecordIndex, uint32 InOffsetWithinRootSignature)
	{
		const uint32 BaseOffset = InShaderTableOffset + ShaderIdentifierSize + InOffsetWithinRootSignature;
		const uint32 DestOffset   = BaseOffset + LocalRecordStride * InDestRecordIndex;
		const uint32 SourceOffset = BaseOffset + LocalRecordStride * InSourceRecordIndex;
		const uint32 CopySize = LocalRecordStride - ShaderIdentifierSize - InOffsetWithinRootSignature;
		checkSlow(CopySize <= LocalRecordStride);

		FMemory::Memcpy(
			Data.GetData() + DestOffset,
			Data.GetData() + SourceOffset,
			CopySize);
	}

	void CopyHitGroupParameters(uint32 InDestRecordIndex, uint32 InSourceRecordIndex, uint32 InOffsetWithinRootSignature)
	{
		const uint32 ShaderTableOffset = HitGroupShaderTableOffset;
		CopyLocalShaderParameters(ShaderTableOffset, InDestRecordIndex, InSourceRecordIndex, InOffsetWithinRootSignature);
	}

	void SetRayGenIdentifier(uint32 RecordIndex, const FD3D12ShaderIdentifier& ShaderIdentifier)
	{
		const uint32 WriteOffset = RayGenShaderTableOffset + RecordIndex * RayGenRecordStride;
		WriteData(WriteOffset, ShaderIdentifier.Data, ShaderIdentifierSize);
	}

	void SetMissIdentifier(uint32 RecordIndex, const FD3D12ShaderIdentifier& ShaderIdentifier)
	{
		const uint32 WriteOffset = MissShaderTableOffset + RecordIndex * LocalRecordStride;
#if DO_CHECK
		if (RecordIndex == 0)
		{
			bWasDefaultMissShaderSet = true;
		}
#endif
		WriteData(WriteOffset, ShaderIdentifier.Data, ShaderIdentifierSize);
	}

	void SetCallableIdentifier(uint32 RecordIndex, const FD3D12ShaderIdentifier& ShaderIdentifier)
	{
		const uint32 WriteOffset = CallableShaderTableOffset + RecordIndex * LocalRecordStride;
		WriteData(WriteOffset, ShaderIdentifier.Data, ShaderIdentifierSize);
	}

	void SetDefaultHitGroupIdentifier(const FD3D12ShaderIdentifier& ShaderIdentifier)
	{
		const uint32 WriteOffset = DefaultHitGroupShaderTableOffset;
		WriteData(WriteOffset, ShaderIdentifier.Data, ShaderIdentifierSize);
	}

	void SetHitGroupSystemParameters(uint32 RecordIndex, const FHitGroupSystemParameters& SystemParameters)
	{
		const uint32 OffsetWithinRootSignature = 0; // System parameters are always first in the RS.
		const uint32 ShaderTableOffset = HitGroupShaderTableOffset;
		SetLocalShaderParameters(ShaderTableOffset, RecordIndex, OffsetWithinRootSignature, SystemParameters);
	}

	void SetHitGroupIdentifier(uint32 RecordIndex, const FD3D12ShaderIdentifier& ShaderIdentifier)
	{
		checkfSlow(ShaderIdentifier.IsValid(), TEXT("Shader identifier must be initialized FD3D12RayTracingPipelineState::GetShaderIdentifier() before use."));
		checkSlow(sizeof(ShaderIdentifier.Data) >= ShaderIdentifierSize);

		const uint32 WriteOffset = HitGroupShaderTableOffset + RecordIndex * LocalRecordStride;
		WriteData(WriteOffset, ShaderIdentifier.Data, ShaderIdentifierSize);
	}

	void SetRayGenIdentifiers(const TArrayView<const FD3D12ShaderIdentifier>& Identifiers)
	{
		check(Identifiers.Num() == NumRayGenShaders);
		for (int32 Index = 0; Index < Identifiers.Num(); ++Index)
		{
			SetRayGenIdentifier(Index, Identifiers[Index]);
		}
	}

	void SetDefaultMissShaderIdentifier(const FD3D12ShaderIdentifier& ShaderIdentifier)
	{
		// Set all slots to the same default
		for (uint32 Index = 0; Index < NumMissRecords; ++Index)
		{
			SetMissIdentifier(Index, ShaderIdentifier);
		}

#if DO_CHECK
		bWasDefaultMissShaderSet = false;
#endif
	}

	void SetDefaultCallableShaderIdentifier(const FD3D12ShaderIdentifier& ShaderIdentifier)
	{
		for (uint32 Index = 0; Index < NumCallableRecords; ++Index)
		{
			SetCallableIdentifier(Index, ShaderIdentifier);
		}
	}

	void CopyToGPU(FD3D12CommandContext& Context)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ShaderTableCopyToGPU);

		check(IsInRHIThread() || !IsRunningRHIInSeparateThread());

		checkf(Data.Num(), TEXT("Shader table is expected to be initialized before copying to GPU."));

		checkf(bWasDefaultMissShaderSet, TEXT("At least the first miss shader must have been set before copying to GPU."));

		FD3D12Device* Device = Context.GetParentDevice();
		FD3D12Adapter* Adapter = Device->GetParentAdapter();
		D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Data.GetResourceDataSize(), D3D12_RESOURCE_FLAG_NONE, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
		FRHIGPUMask GPUMask = FRHIGPUMask::FromIndex(Device->GetGPUIndex());
		bool bHasInitialData = true;

		ID3D12ResourceAllocator* ResourceAllocator = nullptr;
		Buffer = Adapter->CreateRHIBuffer(
			BufferDesc, BufferDesc.Alignment, FRHIBufferDesc(BufferDesc.Width, 0, BUF_Static), ED3D12ResourceStateMode::MultiState,
			D3D12_RESOURCE_STATE_COPY_DEST, bHasInitialData, GPUMask, ResourceAllocator, TEXT("Shader binding table"));

		// Use copy queue for uploading the data
		Context.BatchedSyncPoints.ToWait.Emplace(Buffer->UploadResourceDataViaCopyQueue(&Data));

		// Enqueue transition to SRV
		Context.TransitionResource(
			Buffer->GetResource(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			0);

		bIsDirty = false;
	}

	D3D12_GPU_VIRTUAL_ADDRESS GetShaderTableAddress() const
	{
		checkf(!bIsDirty, TEXT("Shader table update is pending, therefore GPU address is not available. Use CopyToGPU() to upload data and acquire a valid GPU buffer address."));
		return Buffer->ResourceLocation.GetGPUVirtualAddress();
	}

	D3D12_DISPATCH_RAYS_DESC GetDispatchRaysDesc(uint32 RayGenShaderIndex, bool bAllowHitGroupIndexing) const
	{
		D3D12_GPU_VIRTUAL_ADDRESS ShaderTableAddress = GetShaderTableAddress();

		D3D12_DISPATCH_RAYS_DESC Desc = {};

		Desc.RayGenerationShaderRecord.StartAddress = ShaderTableAddress + RayGenShaderTableOffset + RayGenShaderIndex * RayGenRecordStride;
		Desc.RayGenerationShaderRecord.SizeInBytes = RayGenRecordStride;

		Desc.MissShaderTable.StartAddress = ShaderTableAddress + MissShaderTableOffset;
		Desc.MissShaderTable.StrideInBytes = LocalRecordStride;
		Desc.MissShaderTable.SizeInBytes = LocalRecordStride * NumMissRecords;


		if (NumCallableRecords)
		{
			Desc.CallableShaderTable.StartAddress = ShaderTableAddress + CallableShaderTableOffset;
			Desc.CallableShaderTable.StrideInBytes = LocalRecordStride;
			Desc.CallableShaderTable.SizeInBytes = NumCallableRecords * LocalRecordStride;
		}

		if (bAllowHitGroupIndexing)
		{
			Desc.HitGroupTable.StartAddress = ShaderTableAddress + HitGroupShaderTableOffset;
			Desc.HitGroupTable.StrideInBytes = LocalRecordStride;
			Desc.HitGroupTable.SizeInBytes = NumHitRecords * LocalRecordStride;
		}
		else
		{
			Desc.HitGroupTable.StartAddress = ShaderTableAddress + DefaultHitGroupShaderTableOffset;
			Desc.HitGroupTable.StrideInBytes = 0; // Zero stride effectively disables SBT indexing
			Desc.HitGroupTable.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT; // Minimal table with only one record
		}

		return Desc;
	}

	static constexpr uint32 ShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	uint32 NumHitRecords = 0;
	uint32 NumRayGenShaders = 0;
	uint32 NumCallableRecords = 0;
	uint32 NumMissRecords = 0;
	uint32 NumLocalRecords = 0;

	uint32 RayGenShaderTableOffset = 0;
	uint32 MissShaderTableOffset = 0;
	uint32 DefaultHitGroupShaderTableOffset = 0;
	uint32 HitGroupShaderTableOffset = 0;
	uint32 CallableShaderTableOffset = 0;

	uint64 LastCommandListID = 0;

	// Note: TABLE_BYTE_ALIGNMENT is used instead of RECORD_BYTE_ALIGNMENT to allow arbitrary switching 
	// between multiple RayGen and Miss shaders within the same underlying table.
	static constexpr uint32 RayGenRecordStride = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

	uint32 LocalRecordSizeUnaligned = 0; // size of the shader identifier + local root parameters, not aligned to SHADER_RECORD_BYTE_ALIGNMENT (used for out-of-bounds access checks)
	uint32 LocalRecordStride = 0; // size of shader identifier + local root parameters, aligned to SHADER_RECORD_BYTE_ALIGNMENT (same for hit groups and callable shaders)
	TResourceArray<uint8, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT> Data;

	bool bIsDirty = true;
	TRefCountPtr<FD3D12Buffer> Buffer;
#if DO_CHECK
	bool bWasDefaultMissShaderSet = false;
#endif

	// SBTs have their own descriptor heaps
	FD3D12ExplicitDescriptorCache* DescriptorCache = nullptr;

	struct FShaderRecordCacheKey
	{
		static constexpr uint32 MaxUniformBuffers = 6;
		FRHIUniformBuffer* const* UniformBuffers[MaxUniformBuffers];
		uint64 Hash = 0;
		uint32 NumUniformBuffers = 0;
		uint32 ShaderIndex = 0;

		FShaderRecordCacheKey() = default;
		FShaderRecordCacheKey(uint32 InNumUniformBuffers, FRHIUniformBuffer* const* InUniformBuffers, uint32 InShaderIndex)
		{
			ShaderIndex = InShaderIndex;

			check(InNumUniformBuffers <= MaxUniformBuffers);
			NumUniformBuffers = FMath::Min(MaxUniformBuffers, InNumUniformBuffers);

			const uint64 DataSizeInBytes = sizeof(FRHIUniformBuffer*) * NumUniformBuffers;
			FMemory::Memcpy(UniformBuffers, InUniformBuffers, DataSizeInBytes);
			Hash = FXxHash64::HashBuffer(UniformBuffers, DataSizeInBytes).Hash;
		}

		bool operator == (const FShaderRecordCacheKey& Other) const
		{
			if (Hash != Other.Hash) return false;
			if (ShaderIndex != Other.ShaderIndex) return false;
			if (NumUniformBuffers != Other.NumUniformBuffers) return false;

			for (uint32 BufferIndex = 0; BufferIndex < NumUniformBuffers; ++BufferIndex)
			{
				if (UniformBuffers[BufferIndex] != Other.UniformBuffers[BufferIndex]) return false;
			}

			return true;
		}

		friend uint32 GetTypeHash(const FShaderRecordCacheKey& Key)
		{
			return uint32(Key.Hash);
		}
	};
	
	void AddResourceReference(FD3D12Resource* D3D12Resource, uint32 WorkerIndex)
	{
		bool bIsAlreadyInSet = false;
		WorkerData[WorkerIndex].ReferencedD3D12ResourceSet.Add(D3D12Resource, &bIsAlreadyInSet);
		if (!bIsAlreadyInSet)
		{
			WorkerData[WorkerIndex].ReferencedD3D12Resources.Add(D3D12Resource);
		}
	}

	void UpdateResidency(FD3D12CommandContext& CommandContext)
	{
		// Skip redundant resource residency updates when a shader table is repeatedly used on the same command list
		if (LastCommandListID == CommandContext.GetCommandListID())
		{
			return;
		}

		// Merge all data from worker threads into the main set

		for (uint32 WorkerIndex = 1; WorkerIndex < MaxBindingWorkers; ++WorkerIndex)
		{
			for (FD3D12Resource* Resource : WorkerData[WorkerIndex].ReferencedD3D12Resources)
			{
				AddResourceReference(Resource, 0);
			}
			
			WorkerData[WorkerIndex].ReferencedD3D12Resources.Empty();
		}

		// Use the main (merged) set data to actually update resource residency

		for (FD3D12Resource* Resource : WorkerData[0].ReferencedD3D12Resources)
		{
			CommandContext.UpdateResidency(Resource);
		}

		CommandContext.UpdateResidency(Buffer->GetResource());

		LastCommandListID = CommandContext.GetCommandListID();
	}

	void AddResourceTransition(FD3D12ShaderResourceView* SRV, uint32 WorkerIndex)
	{
		bool bAlreadyInSet = false;
		WorkerData[WorkerIndex].TransitionViewSet.Add(SRV, &bAlreadyInSet);
		if (!bAlreadyInSet)
		{
			WorkerData[WorkerIndex].TransitionSRVs.Add(SRV);
		}
	}

	void AddResourceTransition(FD3D12UnorderedAccessView* UAV, uint32 WorkerIndex)
	{
		bool bAlreadyInSet = false;
		WorkerData[WorkerIndex].TransitionViewSet.Add(UAV, &bAlreadyInSet);

		if (!bAlreadyInSet)
		{
			WorkerData[WorkerIndex].TransitionUAVs.Add(UAV);
		}
	}

	void TransitionResources(FD3D12CommandContext& CommandContext)
	{
		// Merge all data from worker threads into the main set

		for (uint32 WorkerIndex = 1; WorkerIndex < MaxBindingWorkers; ++WorkerIndex)
		{
			for (FD3D12ShaderResourceView* SRV : WorkerData[WorkerIndex].TransitionSRVs)
			{
				AddResourceTransition(SRV, 0);
			}

			for (FD3D12UnorderedAccessView* UAV : WorkerData[WorkerIndex].TransitionUAVs)
			{
				AddResourceTransition(UAV, 0);
			}

			WorkerData[WorkerIndex].TransitionSRVs.Empty();
			WorkerData[WorkerIndex].TransitionUAVs.Empty();
			WorkerData[WorkerIndex].TransitionViewSet.Empty();
		}

		// Use the main (merged) set data to perform resource transitions

		for (FD3D12ShaderResourceView* SRV : WorkerData[0].TransitionSRVs)
		{
			CommandContext.TransitionResource(SRV, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}

		for (FD3D12UnorderedAccessView* UAV : WorkerData[0].TransitionUAVs)
		{
			CommandContext.TransitionResource(UAV, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}
	}

	struct alignas(PLATFORM_CACHE_LINE_SIZE) FWorkerThreadData
	{
		Experimental::TSherwoodMap<FShaderRecordCacheKey, uint32> ShaderRecordCache;

		// A set of all resources referenced by this shader table for the purpose of updating residency before ray tracing work dispatch.
		Experimental::TSherwoodSet<void*> ReferencedD3D12ResourceSet;
		TArray<TRefCountPtr<FD3D12Resource>> ReferencedD3D12Resources;

		// Some resources referenced in SBT may be dynamic (written on GPU timeline) and may require transition barriers.
		// We save such resources while we fill the SBT and issue transitions before the SBT is used.
		Experimental::TSherwoodSet<FD3D12View*> TransitionViewSet;
		TArray<FD3D12ShaderResourceView*> TransitionSRVs;
		TArray<FD3D12UnorderedAccessView*> TransitionUAVs;

#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		TArray<FD3D12ConstantBufferView*> TransientCBVs;
#endif // D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
	};

	FWorkerThreadData WorkerData[MaxBindingWorkers];
};

struct FD3D12RayTracingShaderLibrary
{
	void Reserve(uint32 NumShaders)
	{
		Shaders.Reserve(NumShaders);
		Identifiers.Reserve(NumShaders);
	}

	int32 Find(FSHAHash Hash) const
	{
		for (int32 Index = 0; Index < Shaders.Num(); ++Index)
		{
			if (Hash == Shaders[Index]->GetHash())
			{
				return Index;
			}
		}

		return INDEX_NONE;
	}

	TArray<TRefCountPtr<FD3D12RayTracingShader>> Shaders;
	TArray<FD3D12ShaderIdentifier> Identifiers;
};

static void CreateSpecializedStateObjects(
	ID3D12Device5* RayTracingDevice,
	ID3D12RootSignature* GlobalRootSignature,
	uint32 MaxAttributeSizeInBytes,
	uint32 MaxPayloadSizeInBytes,
	const FD3D12RayTracingShaderLibrary& RayGenShaders,
	const TArray<FD3D12RayTracingPipelineCache::FEntry*>& UniqueShaderCollections,
	const TMap<FSHAHash, int32>& RayGenShaderIndexByHash,
	TArray<TRefCountPtr<ID3D12StateObject>>& OutSpecializedStateObjects,
	TArray<int32>& OutSpecializationIndices)
{
	static constexpr uint32 MaxSpecializationBuckets = FD3D12RayTracingPipelineInfo::MaxPerformanceGroups;

	if (RayGenShaders.Shaders.Num() <= 1)
	{
		// No specializations needed
		return;
	}

	// Initialize raygen shader PSO specialization map to default values
	OutSpecializationIndices.Reserve(RayGenShaders.Shaders.Num());
	for (int32 It = 0; It < RayGenShaders.Shaders.Num(); ++It)
	{
		OutSpecializationIndices.Add(INDEX_NONE);
	}

	struct FRayGenShaderSpecialization
	{
		D3D12_EXISTING_COLLECTION_DESC Desc = {};
		int32 ShaderIndex = INDEX_NONE;
	};
	TArray<FRayGenShaderSpecialization> RayGenShaderCollectionBuckets[MaxSpecializationBuckets];
	TArray<D3D12_EXISTING_COLLECTION_DESC> ShaderCollectionDescs;

	// Find useful performance group range for non-raygen shaders.
	// It is not necessary to create PSO specializations for high-occupancy RGS if overall PSO will be limited by low-occupancy hit shaders.
	// Also not necessary to create specializations if all raygen shaders are already in the same group.
	uint32 MaxPerformanceGroupRGS = 0;
	uint32 MinPerformanceGroupRGS = MaxSpecializationBuckets - 1;
	uint32 MaxPerformanceGroupOther = 0;
	uint32 MinPerformanceGroupOther = MaxSpecializationBuckets - 1;
	int32 LastRayGenShaderCollectionIndex = INDEX_NONE;

	for (int32 EntryIndex = 0; EntryIndex < UniqueShaderCollections.Num(); ++EntryIndex)
	{
		FD3D12RayTracingPipelineCache::FEntry* Entry = UniqueShaderCollections[EntryIndex];

		const uint32 Group = FMath::Min<uint32>(Entry->PipelineInfo.PerformanceGroup, MaxSpecializationBuckets);

		if (Entry->CollectionType == FD3D12RayTracingPipelineCache::ECollectionType::RayGen)
		{
			MaxPerformanceGroupRGS = FMath::Max<uint32>(MaxPerformanceGroupRGS, Group);
			MinPerformanceGroupRGS = FMath::Min<uint32>(MinPerformanceGroupRGS, Group);
			LastRayGenShaderCollectionIndex = EntryIndex;
		}
		else
		{
			checkf(EntryIndex > LastRayGenShaderCollectionIndex, TEXT("Ray generation shaders are expected to be first in the UniqueShaderCollections list."));

			MaxPerformanceGroupOther = FMath::Max<uint32>(MaxPerformanceGroupOther, Group);
			MinPerformanceGroupOther = FMath::Min<uint32>(MinPerformanceGroupOther, Group);

			// This is a hit/miss/callable shader which will be common for all specialized RTPSOs.
			ShaderCollectionDescs.Add(Entry->GetCollectionDesc());
		}
	}

	if (MinPerformanceGroupRGS == MaxPerformanceGroupRGS)
	{
		// No need to create a specialized PSO if all raygen shaders are already in the same group
		return;
	}

	// Split RGS collections into a separate lists, organized by performance group
	for (int32 EntryIndex = 0; EntryIndex <= LastRayGenShaderCollectionIndex; ++EntryIndex)
	{
		FD3D12RayTracingPipelineCache::FEntry* Entry = UniqueShaderCollections[EntryIndex];

		check(Entry->CollectionType == FD3D12RayTracingPipelineCache::ECollectionType::RayGen);

		// Don't create specializations for raygen shaders that have better occupancy than worst non-raygen shader
		const uint32 SpecializationBucket = FMath::Min<uint32>(Entry->PipelineInfo.PerformanceGroup, MinPerformanceGroupOther);

		// Don't create extra specialized pipelines for group 0 (worst-performing) and just use the default RTPSO.
		if (SpecializationBucket > 0)
		{
			FRayGenShaderSpecialization Specialization;
			Specialization.Desc = Entry->GetCollectionDesc();
			Specialization.ShaderIndex = RayGenShaderIndexByHash.FindChecked(Entry->Shader->GetHash());
			RayGenShaderCollectionBuckets[SpecializationBucket].Add(Specialization);
		}
	}

	OutSpecializedStateObjects.Reserve(MaxSpecializationBuckets);

	const uint32 ShaderCollectionDescsSize = ShaderCollectionDescs.Num();

	for (const TArray<FRayGenShaderSpecialization>& SpecializationBucket : RayGenShaderCollectionBuckets)
	{
		if (SpecializationBucket.IsEmpty())
		{
			continue;
		}

		const int32 SpecializationIndex = OutSpecializedStateObjects.Num();

		for (const FRayGenShaderSpecialization& Specialization : SpecializationBucket)
		{
			// Temporarily add the RGSs to complete shader collection
			ShaderCollectionDescs.Add(Specialization.Desc);
			OutSpecializationIndices[Specialization.ShaderIndex] = SpecializationIndex;
		}

		TRefCountPtr<ID3D12StateObject> SpecializedPSO = CreateRayTracingStateObject(
			RayTracingDevice,
			{}, // Libraries,
			{}, // LibraryExports,
			MaxAttributeSizeInBytes,
			MaxPayloadSizeInBytes,
			{}, // HitGroups
			GlobalRootSignature,
			{}, // LocalRootSignatures
			{}, // LocalRootSignatureAssociations,
			ShaderCollectionDescs,
			D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

		OutSpecializedStateObjects.Add(SpecializedPSO);

		// Remove the temporary RGSs
		ShaderCollectionDescs.SetNum(ShaderCollectionDescsSize);
	}
}

class FD3D12RayTracingPipelineState : public FRHIRayTracingPipelineState
{
public:

	UE_NONCOPYABLE(FD3D12RayTracingPipelineState)

	FD3D12RayTracingPipelineState(FD3D12Device* Device, const FRayTracingPipelineStateInitializer& Initializer)
	{
		SCOPE_CYCLE_COUNTER(STAT_RTPSO_CreatePipeline);
		TRACE_CPUPROFILER_EVENT_SCOPE(RTPSO_CreatePipeline);

		checkf(Initializer.GetRayGenTable().Num() > 0 || Initializer.bPartial, TEXT("Ray tracing pipelines must have at leat one ray generation shader."));

		uint64 TotalCreationTime = 0;
		uint64 CompileTime = 0;
		uint64 LinkTime = 0;
		uint32 NumCacheHits = 0;

		TotalCreationTime -= FPlatformTime::Cycles64();

		ID3D12Device5* RayTracingDevice = Device->GetDevice5();

		TArrayView<FRHIRayTracingShader*> InitializerHitGroups = Initializer.GetHitGroupTable();
		TArrayView<FRHIRayTracingShader*> InitializerMissShaders = Initializer.GetMissTable();
		TArrayView<FRHIRayTracingShader*> InitializerRayGenShaders = Initializer.GetRayGenTable();
		TArrayView<FRHIRayTracingShader*> InitializerCallableShaders = Initializer.GetCallableTable();

		const uint32 MaxTotalShaders = InitializerRayGenShaders.Num() + InitializerMissShaders.Num() + InitializerHitGroups.Num() + InitializerCallableShaders.Num();
		checkf(MaxTotalShaders >= 1, TEXT("Ray tracing pipelines are expected to contain at least one shader"));

		FD3D12RayTracingPipelineCache* PipelineCache = Device->GetRayTracingPipelineCache();

		// All raygen shaders must share the same global root signature (this is validated below)

		GlobalRootSignature = PipelineCache->GetGlobalRootSignature();

		const FD3D12RayTracingPipelineState* BasePipeline = GRHISupportsRayTracingPSOAdditions 
			? FD3D12DynamicRHI::ResourceCast(Initializer.BasePipeline.GetReference())
			: nullptr;

		if (BasePipeline)
		{
			PipelineShaderHashes = BasePipeline->PipelineShaderHashes;
		}
		PipelineShaderHashes.Reserve(MaxTotalShaders);

		TArray<FD3D12RayTracingPipelineCache::FEntry*> UniqueShaderCollections;
		UniqueShaderCollections.Reserve(MaxTotalShaders);

		FGraphEventArray CompileCompletionList;
		CompileCompletionList.Reserve(MaxTotalShaders);

		// Helper function to acquire a D3D12_EXISTING_COLLECTION_DESC for a compiled shader via cache

		auto AddShaderCollection = [Device, GlobalRootSignature = this->GlobalRootSignature, PipelineCache,
										&UniqueShaderHashes = this->PipelineShaderHashes, &UniqueShaderCollections, &Initializer, &NumCacheHits, &CompileTime,
										&CompileCompletionList]
			(FD3D12RayTracingShader* Shader, FD3D12RayTracingPipelineCache::ECollectionType CollectionType)
		{
			bool bIsAlreadyInSet = false;
			const uint64 ShaderHash = GetShaderHash64(Shader);
			UniqueShaderHashes.Add(ShaderHash, &bIsAlreadyInSet);

			bool bCacheHit = false;

			CompileTime -= FPlatformTime::Cycles64();

			FD3D12RayTracingPipelineCache::FEntry* ShaderCacheEntry = PipelineCache->GetOrCompileShader(
				Device, Shader, GlobalRootSignature,
				Initializer.MaxAttributeSizeInBytes,
				Initializer.MaxPayloadSizeInBytes,
				CollectionType, CompileCompletionList,
				&bCacheHit);

			CompileTime += FPlatformTime::Cycles64();

			if (!bIsAlreadyInSet)
			{
				UniqueShaderCollections.Add(ShaderCacheEntry);

				if (bCacheHit) NumCacheHits++;
			}

			return ShaderCacheEntry;
		};

		// If no custom hit groups were provided, then disable SBT indexing and force default shader on all primitives

		bAllowHitGroupIndexing = Initializer.GetHitGroupTable().Num() ? Initializer.bAllowHitGroupIndexing : false;

		// Add ray generation shaders

		TArray<FD3D12RayTracingPipelineCache::FEntry*> RayGenShaderEntries;

		RayGenShaders.Reserve(InitializerRayGenShaders.Num());
		RayGenShaderEntries.Reserve(InitializerRayGenShaders.Num());
		TMap<FSHAHash, int32> RayGenShaderIndexByHash;

		checkf(UniqueShaderCollections.Num() == 0, TEXT("Ray generation shaders are expected to be first in the UniqueShaderCollections list."));

		for (FRHIRayTracingShader* ShaderRHI : InitializerRayGenShaders)
		{
			FD3D12RayTracingShader* Shader = FD3D12DynamicRHI::ResourceCast(ShaderRHI);
			checkf(Shader->pRootSignature->GetRootSignature() == GlobalRootSignature, TEXT("All raygen and miss shaders must share the same root signature"));
			checkf(!Shader->UsesGlobalUniformBuffer(), TEXT("Global uniform buffers are not implemented for ray generation shaders"));

			FD3D12RayTracingPipelineCache::FEntry* ShaderCacheEntry = AddShaderCollection(Shader, FD3D12RayTracingPipelineCache::ECollectionType::RayGen);

			RayGenShaderEntries.Add(ShaderCacheEntry);
			RayGenShaderIndexByHash.Add(Shader->GetHash(), RayGenShaders.Shaders.Num());
			RayGenShaders.Shaders.Add(Shader);
		}

		MaxHitGroupViewDescriptors = 0;
		MaxLocalRootSignatureSize = 0;


		// Add miss shaders

		TArray<FD3D12RayTracingPipelineCache::FEntry*> MissShaderEntries;
		MissShaders.Reserve(InitializerMissShaders.Num());
		MissShaderEntries.Reserve(InitializerMissShaders.Num());

		for (FRHIRayTracingShader* ShaderRHI : InitializerMissShaders)
		{
			FD3D12RayTracingShader* Shader = FD3D12DynamicRHI::ResourceCast(ShaderRHI);

			checkf(Shader, TEXT("A valid ray tracing shader must be provided for all elements in the FRayTracingPipelineStateInitializer miss shader table."));
			checkf(!Shader->UsesGlobalUniformBuffer(), TEXT("Global uniform buffers are not implemented for ray tracing miss shaders"));

			const uint32 ShaderViewDescriptors = Shader->ResourceCounts.NumSRVs + Shader->ResourceCounts.NumUAVs;
			MaxHitGroupViewDescriptors = FMath::Max(MaxHitGroupViewDescriptors, ShaderViewDescriptors);
			MaxLocalRootSignatureSize = FMath::Max(MaxLocalRootSignatureSize, Shader->pRootSignature->GetTotalRootSignatureSizeInBytes());

			FD3D12RayTracingPipelineCache::FEntry* ShaderCacheEntry = AddShaderCollection(Shader, FD3D12RayTracingPipelineCache::ECollectionType::Miss);

			MissShaderEntries.Add(ShaderCacheEntry);
			MissShaders.Shaders.Add(Shader);
		}

		// Add hit groups

		TArray<FD3D12RayTracingPipelineCache::FEntry*> HitGroupEntries;
		HitGroupShaders.Reserve(InitializerHitGroups.Num());
		HitGroupEntries.Reserve(InitializerHitGroups.Num());

		for (FRHIRayTracingShader* ShaderRHI : InitializerHitGroups)
		{
			FD3D12RayTracingShader* Shader = FD3D12DynamicRHI::ResourceCast(ShaderRHI);

			checkf(Shader, TEXT("A valid ray tracing hit group shader must be provided for all elements in the FRayTracingPipelineStateInitializer hit group table."));

			const uint32 ShaderViewDescriptors = Shader->ResourceCounts.NumSRVs + Shader->ResourceCounts.NumUAVs;
			MaxHitGroupViewDescriptors = FMath::Max(MaxHitGroupViewDescriptors, ShaderViewDescriptors);
			MaxLocalRootSignatureSize = FMath::Max(MaxLocalRootSignatureSize, Shader->pRootSignature->GetTotalRootSignatureSizeInBytes());

			FD3D12RayTracingPipelineCache::FEntry* ShaderCacheEntry = AddShaderCollection(Shader, FD3D12RayTracingPipelineCache::ECollectionType::HitGroup);

			HitGroupEntries.Add(ShaderCacheEntry);
			HitGroupShaders.Shaders.Add(Shader);
		}

		// Add callable shaders

		TArray<FD3D12RayTracingPipelineCache::FEntry*> CallableShaderEntries;
		CallableShaders.Reserve(InitializerCallableShaders.Num());
		CallableShaderEntries.Reserve(InitializerCallableShaders.Num());

		for (FRHIRayTracingShader* ShaderRHI : InitializerCallableShaders)
		{
			FD3D12RayTracingShader* Shader = FD3D12DynamicRHI::ResourceCast(ShaderRHI);

			checkf(Shader, TEXT("A valid ray tracing shader must be provided for all elements in the FRayTracingPipelineStateInitializer callable shader table."));
			checkf(!Shader->UsesGlobalUniformBuffer(), TEXT("Global uniform buffers are not implemented for ray tracing callable shaders"));

			const uint32 ShaderViewDescriptors = Shader->ResourceCounts.NumSRVs + Shader->ResourceCounts.NumUAVs;
			MaxHitGroupViewDescriptors = FMath::Max(MaxHitGroupViewDescriptors, ShaderViewDescriptors);
			MaxLocalRootSignatureSize = FMath::Max(MaxLocalRootSignatureSize, Shader->pRootSignature->GetTotalRootSignatureSizeInBytes());

			FD3D12RayTracingPipelineCache::FEntry* ShaderCacheEntry = AddShaderCollection(Shader, FD3D12RayTracingPipelineCache::ECollectionType::Callable);

			CallableShaderEntries.Add(ShaderCacheEntry);
			CallableShaders.Shaders.Add(Shader);
		}

		// Wait for all compilation tasks to be complete and then gather the compiled collection descriptors

		CompileTime -= FPlatformTime::Cycles64();

		FTaskGraphInterface::Get().WaitUntilTasksComplete(CompileCompletionList);

		CompileTime += FPlatformTime::Cycles64();

		if (Initializer.bPartial)
		{
			// Partial pipelines don't have a linking phase, so exit immediately after compilation tasks are complete.
			return;
		}

		TArray<D3D12_EXISTING_COLLECTION_DESC> UniqueShaderCollectionDescs;
		UniqueShaderCollectionDescs.Reserve(MaxTotalShaders);
		for (FD3D12RayTracingPipelineCache::FEntry* Entry : UniqueShaderCollections)
		{
			UniqueShaderCollectionDescs.Add(Entry->GetCollectionDesc());
		}

		// Link final RTPSO from shader collections

		LinkTime -= FPlatformTime::Cycles64();

		// Extending RTPSOs is currently not compatible with PSO specializations
		if (BasePipeline && GRayTracingSpecializeStateObjects == 0)
		{
			if (UniqueShaderCollectionDescs.Num() == 0)
			{
				// New PSO does not actually have any new shaders that were not in the base
				StateObject = BasePipeline->StateObject.GetReference();
			}
			else
			{

				TArray<D3D12_STATE_SUBOBJECT> Subobjects;

				int32 SubobjectIndex = 0;
				Subobjects.Reserve(UniqueShaderCollectionDescs.Num() + 1);

				D3D12_STATE_OBJECT_CONFIG StateObjectConfig = {};
				StateObjectConfig.Flags = D3D12_STATE_OBJECT_FLAG_ALLOW_STATE_OBJECT_ADDITIONS;
				Subobjects.Add(D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG, &StateObjectConfig });

				for (const D3D12_EXISTING_COLLECTION_DESC& Collection : UniqueShaderCollectionDescs)
				{
					Subobjects.Add(D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION, &Collection });
				}

				D3D12_STATE_OBJECT_DESC Desc = {};
				Desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
				Desc.NumSubobjects = Subobjects.Num();
				Desc.pSubobjects = Subobjects.GetData();

				ID3D12Device7* Device7 = Device->GetDevice7();

				VERIFYD3D12RESULT(Device7->AddToStateObject(&Desc,
					BasePipeline->StateObject.GetReference(),
					IID_PPV_ARGS(StateObject.GetInitReference())));
			}
		}
		else
		{
			StateObject = CreateRayTracingStateObject(
				RayTracingDevice,
				{}, // Libraries,
				{}, // LibraryExports,
				Initializer.MaxAttributeSizeInBytes,
				Initializer.MaxPayloadSizeInBytes,
				{}, // HitGroups
				GlobalRootSignature,
				{}, // LocalRootSignatures
				{}, // LocalRootSignatureAssociations,
				UniqueShaderCollectionDescs,
				D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);
		}

		if (GRayTracingSpecializeStateObjects != 0 && Initializer.GetRayGenTable().Num() > 1)
		{
			CreateSpecializedStateObjects(
				RayTracingDevice,
				GlobalRootSignature,
				Initializer.MaxAttributeSizeInBytes,
				Initializer.MaxPayloadSizeInBytes,
				RayGenShaders,
				UniqueShaderCollections,
				RayGenShaderIndexByHash,
				SpecializedStateObjects, // out param
				SpecializationIndices // out param
			);
		}

		LinkTime += FPlatformTime::Cycles64();

		HRESULT QueryInterfaceResult = StateObject->QueryInterface(IID_PPV_ARGS(PipelineProperties.GetInitReference()));
		checkf(SUCCEEDED(QueryInterfaceResult), TEXT("Failed to query pipeline properties from the ray tracing pipeline state object. Result=%08x"), QueryInterfaceResult);

		// Query shader identifiers from the pipeline state object

		check(HitGroupEntries.Num() == InitializerHitGroups.Num());

		auto GetEntryShaderIdentifier = [Properties = PipelineProperties.GetReference()](FD3D12RayTracingPipelineCache::FEntry* Entry) -> FD3D12ShaderIdentifier
		{
			if (Entry->Identifier.IsValid())
			{
				return Entry->Identifier;
			}
			else
			{
				return GetShaderIdentifier(Properties, Entry->GetPrimaryExportNameChars());
			}
		};

		HitGroupShaders.Identifiers.SetNumUninitialized(InitializerHitGroups.Num());
		for (int32 HitGroupIndex = 0; HitGroupIndex < HitGroupEntries.Num(); ++HitGroupIndex)
		{
			HitGroupShaders.Identifiers[HitGroupIndex] = GetEntryShaderIdentifier(HitGroupEntries[HitGroupIndex]);
		}

		RayGenShaders.Identifiers.SetNumUninitialized(RayGenShaderEntries.Num());
		for (int32 ShaderIndex = 0; ShaderIndex < RayGenShaderEntries.Num(); ++ShaderIndex)
		{
			RayGenShaders.Identifiers[ShaderIndex] = GetEntryShaderIdentifier(RayGenShaderEntries[ShaderIndex]);
		}

		MissShaders.Identifiers.SetNumUninitialized(MissShaderEntries.Num());
		for (int32 ShaderIndex = 0; ShaderIndex < MissShaderEntries.Num(); ++ShaderIndex)
		{
			MissShaders.Identifiers[ShaderIndex] = GetEntryShaderIdentifier(MissShaderEntries[ShaderIndex]);
		}

		CallableShaders.Identifiers.SetNumUninitialized(CallableShaderEntries.Num());
		for (int32 ShaderIndex = 0; ShaderIndex < CallableShaderEntries.Num(); ++ShaderIndex)
		{
			CallableShaders.Identifiers[ShaderIndex] = GetEntryShaderIdentifier(CallableShaderEntries[ShaderIndex]);
		}

		PipelineStackSize = PipelineProperties->GetPipelineStackSize();

		TotalCreationTime += FPlatformTime::Cycles64();

		// Report stats for pipelines that take a long time to create

#if !NO_LOGGING

		// Gather PSO stats
		ShaderStats.Reserve(UniqueShaderCollections.Num());
		for (FD3D12RayTracingPipelineCache::FEntry* Entry : UniqueShaderCollections)
		{
			FShaderStats Stats;
			Stats.Name = *(Entry->Shader->EntryPoint);
			Stats.ShaderSize = Entry->Shader->Code.Num();
			Stats.CompileTimeMS = Entry->CompileTimeMS;

		#if PLATFORM_WINDOWS
			if (Entry->Shader->GetFrequency() == SF_RayGen)
			{
				Stats.StackSize = uint32(PipelineProperties->GetShaderStackSize(*(Entry->ExportNames[0])));
			}
		#endif // PLATFORM_WINDOWS

			ShaderStats.Add(Stats);
		}

		ShaderStats.Sort([](const FShaderStats& A, const FShaderStats& B) { return B.CompileTimeMS < A.CompileTimeMS; });

		const double TotalCreationTimeMS = 1000.0 * FPlatformTime::ToSeconds64(TotalCreationTime);
		const float CreationTimeWarningThresholdMS = 10.0f;
		const bool bAllowLogSlowCreation = !Initializer.bBackgroundCompilation; // Only report creation stalls on the critical path
		if (bAllowLogSlowCreation && TotalCreationTimeMS > CreationTimeWarningThresholdMS)
		{
			const double CompileTimeMS = 1000.0 * FPlatformTime::ToSeconds64(CompileTime);
			const double LinkTimeMS = 1000.0 * FPlatformTime::ToSeconds64(LinkTime);
			const uint32 NumUniqueShaders = UniqueShaderCollections.Num();
			UE_LOG(LogD3D12RHI, Log,
				TEXT("Creating RTPSO with %d shaders (%d cached, %d new) took %.2f ms. Compile time %.2f ms, link time %.2f ms."),
				PipelineShaderHashes.Num(), NumCacheHits, NumUniqueShaders - NumCacheHits, (float)TotalCreationTimeMS, (float)CompileTimeMS, (float)LinkTimeMS);
		}

#endif //!NO_LOGGING
	}

	FD3D12RayTracingShaderLibrary RayGenShaders;
	FD3D12RayTracingShaderLibrary MissShaders;
	FD3D12RayTracingShaderLibrary HitGroupShaders;
	FD3D12RayTracingShaderLibrary CallableShaders;

	ID3D12RootSignature* GlobalRootSignature = nullptr;

	TRefCountPtr<ID3D12StateObject> StateObject;
	TRefCountPtr<ID3D12StateObjectProperties> PipelineProperties;

	// Maps raygen shader index to a specialized state object (may be -1 if no specialization is used for a shader)
	TArray<int32> SpecializationIndices;

	// State objects with raygen shaders grouped by occupancy
	TArray<TRefCountPtr<ID3D12StateObject>> SpecializedStateObjects;

	static constexpr uint32 ShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	bool bAllowHitGroupIndexing = true;

	uint32 MaxLocalRootSignatureSize = 0;
	uint32 MaxHitGroupViewDescriptors = 0;

	TSet<uint64> PipelineShaderHashes;

	uint32 PipelineStackSize = 0;

#if !NO_LOGGING
	struct FShaderStats
	{
		const TCHAR* Name = nullptr;
		float CompileTimeMS = 0;
		uint32 StackSize = 0;
		uint32 ShaderSize = 0;
	};
	TArray<FShaderStats> ShaderStats;
#endif // !NO_LOGGING
};

struct FD3D12RHICommandInitializeRayTracingGeometryString
{
	static const TCHAR* TStr() { return TEXT("FD3D12RHICommandInitializeRayTracingGeometryString"); }
};
struct FD3D12RHICommandInitializeRayTracingGeometry final : public FRHICommand<FD3D12RHICommandInitializeRayTracingGeometry, FD3D12RHICommandInitializeRayTracingGeometryString>
{
	TRefCountPtr<FD3D12RayTracingGeometry> Geometry;
	FD3D12ResourceLocation SrcResourceLoc;
	bool bForRendering;

	FORCEINLINE_DEBUGGABLE FD3D12RHICommandInitializeRayTracingGeometry(TRefCountPtr<FD3D12RayTracingGeometry>&& InGeometry, FD3D12ResourceLocation& InSrcResourceLoc, bool bForRendering)
		: Geometry(MoveTemp(InGeometry))
		, SrcResourceLoc(InSrcResourceLoc.GetParentDevice())
		, bForRendering(bForRendering)
	{
		FD3D12ResourceLocation::TransferOwnership(SrcResourceLoc, InSrcResourceLoc);
	}

	void Execute(FRHICommandListBase& /* unused */)
	{
		ExecuteNoCmdList();
	}

	void ExecuteNoCmdList()
	{
		for (uint32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS && GPUIndex < GNumExplicitGPUsForRendering; ++GPUIndex)
		{
			FD3D12Buffer* AccelerationStructure = Geometry->AccelerationStructureBuffers[GPUIndex];
			FD3D12Resource* Destination = AccelerationStructure->ResourceLocation.GetResource();
			FD3D12Device* Device = Destination->GetParentDevice();

			FD3D12CommandContext& Context = Device->GetDefaultCommandContext();

			Context.RayTracingCommandList()->CopyRaytracingAccelerationStructure(
				AccelerationStructure->ResourceLocation.GetGPUVirtualAddress(),
				SrcResourceLoc.GetGPUVirtualAddress(),
				D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_DESERIALIZE
			);

			Context.UpdateResidency(SrcResourceLoc.GetResource());
			Context.ConditionalSplitCommandList();
			
			if (bForRendering)
			{
				Geometry->RegisterAsRenameListener(GPUIndex);
				Geometry->SetupHitGroupSystemParameters(GPUIndex);
			}

			Geometry->SetDirty(FRHIGPUMask::FromIndex(GPUIndex), false);
		}
	}
};

void FD3D12Device::InitRayTracing()
{
	LLM_SCOPE_BYNAME(TEXT("FD3D12RT"));
	check(RayTracingPipelineCache == nullptr);
	RayTracingPipelineCache = new FD3D12RayTracingPipelineCache(this);
}

void FD3D12Device::CleanupRayTracing()
{
	delete RayTracingPipelineCache;
	RayTracingPipelineCache = nullptr;

	delete RayTracingDispatchRaysDescBuffer;
	RayTracingDispatchRaysDescBuffer = nullptr;
}

static D3D12_RAYTRACING_INSTANCE_FLAGS TranslateRayTracingInstanceFlags(ERayTracingInstanceFlags InFlags)
{
	D3D12_RAYTRACING_INSTANCE_FLAGS Result = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

	if (EnumHasAnyFlags(InFlags, ERayTracingInstanceFlags::TriangleCullDisable))
	{
		Result |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
	}

	if (!EnumHasAnyFlags(InFlags, ERayTracingInstanceFlags::TriangleCullReverse))
	{
		// Counterclockwise is the default for UE. Reversing culling is achieved by *not* setting this flag.
		Result |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
	}

	if (EnumHasAnyFlags(InFlags, ERayTracingInstanceFlags::ForceOpaque))
	{
		Result |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
	}

	if (EnumHasAnyFlags(InFlags, ERayTracingInstanceFlags::ForceNonOpaque))
	{
		Result |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
	}

	return Result;
}

FRayTracingAccelerationStructureSize FD3D12DynamicRHI::RHICalcRayTracingSceneSize(uint32 MaxInstances, ERayTracingAccelerationStructureFlags Flags)
{
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS BuildInputs = {};
	BuildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	BuildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	BuildInputs.NumDescs = MaxInstances;
	BuildInputs.Flags = TranslateRayTracingAccelerationStructureFlags(Flags);

	FD3D12Adapter& Adapter = GetAdapter();

	FRayTracingAccelerationStructureSize SizeInfo = {};
	for (uint32 GPUIndex = 0; GPUIndex < GNumExplicitGPUsForRendering; ++GPUIndex)
	{
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};
		Adapter.GetDevice(GPUIndex)->GetRaytracingAccelerationStructurePrebuildInfo(&BuildInputs, &PrebuildInfo);

		SizeInfo.ResultSize = FMath::Max(SizeInfo.ResultSize, PrebuildInfo.ResultDataMaxSizeInBytes);
		SizeInfo.BuildScratchSize  = FMath::Max(SizeInfo.BuildScratchSize, PrebuildInfo.ScratchDataSizeInBytes);
		SizeInfo.UpdateScratchSize = FMath::Max(SizeInfo.UpdateScratchSize, PrebuildInfo.UpdateScratchDataSizeInBytes);
	}

	return SizeInfo;
}

static ERayTracingAccelerationStructureFlags GetRayTracingAccelerationStructureBuildFlags(const FRayTracingGeometryInitializer& Initializer)
{
	ERayTracingAccelerationStructureFlags BuildFlags = ERayTracingAccelerationStructureFlags::None;

	if (Initializer.bFastBuild)
	{
		BuildFlags = ERayTracingAccelerationStructureFlags::FastBuild;
	}
	else
	{
		BuildFlags = ERayTracingAccelerationStructureFlags::FastTrace;
	}

	if (Initializer.bAllowUpdate)
	{
		EnumAddFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate);
	}

	if (!Initializer.bFastBuild && !Initializer.bAllowUpdate && Initializer.bAllowCompaction && GD3D12RayTracingAllowCompaction)
	{
		EnumAddFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction);
	}

	if (GRayTracingDebugForceBuildMode == 1)
	{
		EnumAddFlags(BuildFlags, ERayTracingAccelerationStructureFlags::FastBuild);
		EnumRemoveFlags(BuildFlags, ERayTracingAccelerationStructureFlags::FastTrace);
	}
	else if (GRayTracingDebugForceBuildMode == 2)
	{
		EnumAddFlags(BuildFlags, ERayTracingAccelerationStructureFlags::FastTrace);
		EnumRemoveFlags(BuildFlags, ERayTracingAccelerationStructureFlags::FastBuild);
	}

	return BuildFlags;
}

void TranslateRayTracingGeometryDescs(const FRayTracingGeometryInitializer& Initializer, TArrayView<D3D12_RAYTRACING_GEOMETRY_DESC> Output)
{
	check(Output.Num() == Initializer.Segments.Num());

	D3D12_RAYTRACING_GEOMETRY_TYPE GeometryType = TranslateRayTracingGeometryType(Initializer.GeometryType);

	uint32 ComputedPrimitiveCountForValidation = 0;

	for (int32 SegmentIndex = 0; SegmentIndex < Initializer.Segments.Num(); ++SegmentIndex)
	{
		const FRayTracingGeometrySegment& Segment = Initializer.Segments[SegmentIndex];

		checkf(Segment.VertexBuffer, TEXT("Position vertex buffer is required for ray tracing geometry."));
		checkf(Segment.VertexBufferStride, TEXT("Non-zero position vertex buffer stride is required."));
		checkf(Segment.VertexBufferStride % 4 == 0, TEXT("Position vertex buffer stride must be aligned to 4 bytes for ByteAddressBuffer loads to work."));

		checkf(Segment.MaxVertices != 0 || Segment.NumPrimitives == 0,
			TEXT("FRayTracingGeometrySegment.MaxVertices for '%s' must contain number of positions in the vertex buffer or maximum index buffer value+1 if index buffer is provided."),
			*Initializer.DebugName.ToString());

		if (Initializer.GeometryType == RTGT_Triangles)
		{
			checkf(Segment.VertexBufferElementType == VET_Float3
				|| Segment.VertexBufferElementType == VET_Float4, TEXT("Only float3/4 vertex buffers are currently implemented.")); // #dxr_todo UE-72160: support other vertex buffer formats
			checkf(Segment.VertexBufferStride >= 12, TEXT("Only deinterleaved float3 position vertex buffers are currently implemented.")); // #dxr_todo UE-72160: support interleaved vertex buffers
		}
		else if (Initializer.GeometryType == RTGT_Procedural)
		{
			checkf(Segment.VertexBufferStride >= (2 * sizeof(FVector3f)), TEXT("Procedural geometry vertex buffer must contain at least 2xFloat3 that defines 3D bounding boxes of primitives."));
		}

		check(Segment.FirstPrimitive + Segment.NumPrimitives <= Initializer.TotalPrimitiveCount);

		if (Initializer.IndexBuffer)
		{
			uint32 IndexStride = Initializer.IndexBuffer->GetStride();
			check(Initializer.IndexBuffer->GetSize() >=
				(Segment.FirstPrimitive + Segment.NumPrimitives) * FD3D12RayTracingGeometry::IndicesPerPrimitive * IndexStride + Initializer.IndexBufferOffset);
		}

		D3D12_RAYTRACING_GEOMETRY_DESC Desc = {};

		Desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
		Desc.Type = GeometryType;

		if (Segment.bForceOpaque)
		{
			// Deny anyhit shader invocations when this segment is hit
			Desc.Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		}

		if (!Segment.bAllowDuplicateAnyHitShaderInvocation)
		{
			// Allow only a single any-hit shader invocation per primitive
			Desc.Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;
		}

		switch (GeometryType)
		{
		case D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES:
			switch (Segment.VertexBufferElementType)
			{
			case VET_Float4:
				// While the DXGI_FORMAT_R32G32B32A32_FLOAT format is not supported by DXR, since we manually load vertex 
				// data when we are building the BLAS, we can just rely on the vertex stride to offset the read index, 
				// and read only the 3 vertex components, and so use the DXGI_FORMAT_R32G32B32_FLOAT vertex format
			case VET_Float3:
				Desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				break;
			case VET_Float2:
				Desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32_FLOAT;
				break;
			case VET_Half2:
				Desc.Triangles.VertexFormat = DXGI_FORMAT_R16G16_FLOAT;
				break;
			default:
				checkNoEntry();
				break;
			}

			if (Initializer.IndexBuffer)
			{
				// In some cases the geometry is created with 16 bit index buffer, but it's 32 bit at build time.
				// We conservatively set this to 32 bit to allocate acceleration structure memory.
				Desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
				Desc.Triangles.IndexCount = Segment.NumPrimitives * FD3D12RayTracingGeometry::IndicesPerPrimitive;
				Desc.Triangles.VertexCount = Segment.MaxVertices;
			}
			else
			{
				// Non-indexed geometry
				checkf(Initializer.Segments.Num() == 1, TEXT("Non-indexed geometry with multiple segments is not implemented."));
				Desc.Triangles.IndexFormat = DXGI_FORMAT_UNKNOWN;
				Desc.Triangles.VertexCount = FMath::Min<uint32>(Segment.MaxVertices, Initializer.TotalPrimitiveCount * 3);
			}
			break;

		case D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS:
			Desc.AABBs.AABBCount = Segment.NumPrimitives;
			break;

		default:
			checkf(false, TEXT("Unexpected ray tracing geometry type"));
			break;
		}

		ComputedPrimitiveCountForValidation += Segment.NumPrimitives;


		Output[SegmentIndex] = Desc;
	}

	check(ComputedPrimitiveCountForValidation == Initializer.TotalPrimitiveCount);
}

FRayTracingAccelerationStructureSize FD3D12DynamicRHI::RHICalcRayTracingGeometrySize(FRHICommandListBase& RHICmdList, const FRayTracingGeometryInitializer& Initializer)
{
	FRayTracingAccelerationStructureSize SizeInfo = {};
	
	ERayTracingAccelerationStructureFlags BuildFlags = GetRayTracingAccelerationStructureBuildFlags(Initializer);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS PrebuildDescInputs = {};

	TArray<D3D12_RAYTRACING_GEOMETRY_DESC, TInlineAllocator<32>> GeometryDescs;
	GeometryDescs.SetNumUninitialized(Initializer.Segments.Num());
	TranslateRayTracingGeometryDescs(Initializer, GeometryDescs);

	D3D12_RAYTRACING_GEOMETRY_TYPE GeometryType = TranslateRayTracingGeometryType(Initializer.GeometryType);

	PrebuildDescInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	PrebuildDescInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	PrebuildDescInputs.NumDescs = GeometryDescs.Num();
	PrebuildDescInputs.pGeometryDescs = GeometryDescs.GetData();
	PrebuildDescInputs.Flags = TranslateRayTracingAccelerationStructureFlags(BuildFlags);

	FD3D12Adapter& Adapter = GetAdapter();

	// We don't know the final index buffer format, so take maximum of 16 and 32 bit.

	static const DXGI_FORMAT ValidIndexBufferFormats[] = { DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R32_UINT };
	static const DXGI_FORMAT NullIndexBufferFormats[] = { DXGI_FORMAT_UNKNOWN };

	TArrayView<const DXGI_FORMAT> IndexFormats = Initializer.IndexBuffer.IsValid() 
		? MakeArrayView(ValidIndexBufferFormats)
		: MakeArrayView(NullIndexBufferFormats);

	for (DXGI_FORMAT IndexFormat : IndexFormats)
	{
		for (D3D12_RAYTRACING_GEOMETRY_DESC& GeometryDesc : GeometryDescs)
		{
			if (GeometryDesc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES)
			{
				GeometryDesc.Triangles.IndexFormat = IndexFormat;
			}
		}

		// Get maximum buffer sizes for all GPUs in the system
		for (uint32 GPUIndex = 0; GPUIndex < GNumExplicitGPUsForRendering; ++GPUIndex)
		{
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};
			Adapter.GetDevice(GPUIndex)->GetRaytracingAccelerationStructurePrebuildInfo(&PrebuildDescInputs, &PrebuildInfo);

			SizeInfo.ResultSize = FMath::Max(SizeInfo.ResultSize, PrebuildInfo.ResultDataMaxSizeInBytes);
			SizeInfo.BuildScratchSize = FMath::Max(SizeInfo.BuildScratchSize, PrebuildInfo.ScratchDataSizeInBytes);
			SizeInfo.UpdateScratchSize = FMath::Max(SizeInfo.UpdateScratchSize, PrebuildInfo.UpdateScratchDataSizeInBytes);
		}
	}

	SizeInfo.ResultSize = Align(SizeInfo.ResultSize, GRHIRayTracingAccelerationStructureAlignment);
	SizeInfo.BuildScratchSize = Align(SizeInfo.BuildScratchSize, GRHIRayTracingScratchBufferAlignment);
	SizeInfo.UpdateScratchSize = Align(FMath::Max(1ULL, SizeInfo.UpdateScratchSize), GRHIRayTracingScratchBufferAlignment);

	return SizeInfo;
}

FRayTracingPipelineStateRHIRef FD3D12DynamicRHI::RHICreateRayTracingPipelineState(const FRayTracingPipelineStateInitializer& Initializer)
{
	FD3D12Device* Device = GetAdapter().GetDevice(0); // All pipelines are created on the first node, as they may be used on any other linked GPU.
	FD3D12RayTracingPipelineState* Result = new FD3D12RayTracingPipelineState(Device, Initializer);

	return Result;
}

FRayTracingGeometryRHIRef FD3D12DynamicRHI::RHICreateRayTracingGeometry(FRHICommandListBase& RHICmdList, const FRayTracingGeometryInitializer& Initializer)
{
	FD3D12Adapter& Adapter = GetAdapter();
	return new FD3D12RayTracingGeometry(RHICmdList, &Adapter, Initializer);
}

void FD3D12DynamicRHI::RHITransferRayTracingGeometryUnderlyingResource(FRHICommandListBase& RHICmdList, FRHIRayTracingGeometry* DestGeometry, FRHIRayTracingGeometry* SrcGeometry)
{
	check(DestGeometry);
	FD3D12RayTracingGeometry* Dest = ResourceCast(DestGeometry);
	if (!SrcGeometry)
	{
		Dest->ReleaseUnderlyingResource();
	}
	else
	{		
		FD3D12RayTracingGeometry* Src = ResourceCast(SrcGeometry);
		Dest->Swap(*Src);
	}
}

FRayTracingSceneRHIRef FD3D12DynamicRHI::RHICreateRayTracingScene(FRayTracingSceneInitializer2 Initializer)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CreateRayTracingScene);

	FD3D12Adapter& Adapter = GetAdapter();

	return new FD3D12RayTracingScene(&Adapter, MoveTemp(Initializer));
}

FBufferRHIRef FD3D12RayTracingGeometry::NullTransformBuffer;

enum class ERayTracingBufferType
{
	AccelerationStructure,
	Scratch
};

static TRefCountPtr<FD3D12Buffer> CreateRayTracingBuffer(FD3D12Adapter* Adapter, uint32 GPUIndex, uint64 Size, ERayTracingBufferType Type, const FDebugName& DebugName)
{
	FString DebugNameString = DebugName.ToString();

	checkf(Size != 0, TEXT("Attempting to create ray tracing %s buffer of zero size. Debug name: %s"),
		Type == ERayTracingBufferType::AccelerationStructure ? TEXT("AccelerationStructure") : TEXT("Scratch"),
		*DebugNameString);

	TRefCountPtr<FD3D12Buffer> Result;

	ID3D12ResourceAllocator* ResourceAllocator = nullptr;
	FRHIGPUMask GPUMask = FRHIGPUMask::FromIndex(GPUIndex);
	bool bHasInitialData = false;

	if (Type == ERayTracingBufferType::AccelerationStructure)
	{
		D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Size, D3D12_RESOURCE_FLAG_NONE);
		Result = Adapter->CreateRHIBuffer(
			BufferDesc, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
			FRHIBufferDesc(BufferDesc.Width, 0, BUF_AccelerationStructure),
			ED3D12ResourceStateMode::SingleState, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, bHasInitialData,
			GPUMask, ResourceAllocator, *DebugNameString);
	}
	else if (Type == ERayTracingBufferType::Scratch)
	{
		// Scratch doesn't need single state anymore because there are only a few scratch allocations left and allocating a 
		// dedicated single state heap for it wastes memory - ideally all scratch allocations should be transient
		D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		Result = Adapter->CreateRHIBuffer(
			BufferDesc, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
			FRHIBufferDesc(BufferDesc.Width, 0, BUF_UnorderedAccess),
			ED3D12ResourceStateMode::Default, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, bHasInitialData,
			GPUMask, ResourceAllocator, *DebugNameString);

		// Elevates the scratch buffer heap priority, which may help performance / stability in low memory conditions 
		// (Acceleration structure already boosted from allocation side)
		ID3D12Pageable* HeapResource = Result->GetResource()->GetPageable();
		Adapter->SetResidencyPriority(HeapResource, D3D12_RESIDENCY_PRIORITY_HIGH, GPUIndex);
	}
	else
	{
		checkNoEntry();
	}

	return Result;
}

FString GetGeometryInitializerDebugString(const FRayTracingGeometryInitializer& Initializer)
{
	TStringBuilder<128> Result;

	Result << "DebugName=" << Initializer.DebugName.ToString();
	Result << " NumSegments=" << Initializer.Segments.Num();
	Result << " NumPrims=" << Initializer.TotalPrimitiveCount;
	if (Initializer.IndexBuffer)
	{
		Result << " IndexStride=" << Initializer.IndexBuffer->GetStride();
	}
	else
	{
		Result << " NonIndexed";
	}

	if (Initializer.OfflineData)
	{
		Result << " HasOfflineData";
	}

	return Result.ToString();
}

FD3D12RayTracingGeometry::FD3D12RayTracingGeometry(FRHICommandListBase& RHICmdList, FD3D12Adapter* Adapter, const FRayTracingGeometryInitializer& InInitializer)
	: FRHIRayTracingGeometry(InInitializer), FD3D12AdapterChild(Adapter)
{
	INC_DWORD_STAT(STAT_D3D12RayTracingAllocatedBLAS);

	static const FName NAME_BLAS(TEXT("BLAS"));

	DebugName = !Initializer.DebugName.IsNone() ? Initializer.DebugName : NAME_BLAS;
	OwnerName = Initializer.OwnerName;
	
	FMemory::Memzero(bHasPendingCompactionRequests);
	FMemory::Memzero(bRegisteredAsRenameListener);

	if(!FD3D12RayTracingGeometry::NullTransformBuffer.IsValid())
	{
		TResourceArray<float> NullTransformData;
		NullTransformData.SetNumZeroed(12);

		FRHIResourceCreateInfo CreateInfo(TEXT("NullTransformBuffer"));
		CreateInfo.ResourceArray = &NullTransformData;

		FD3D12RayTracingGeometry::NullTransformBuffer = RHICmdList.CreateBuffer(NullTransformData.GetResourceDataSize(), BUF_VertexBuffer | BUF_Static, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
	}

	RegisterD3D12RayTracingGeometry(this);

	checkf(Initializer.Segments.Num() > 0, TEXT("Ray tracing geometry must be initialized with at least one segment."));

	// #yuriy_todo: get flags directly through the initializer
	ERayTracingAccelerationStructureFlags BuildFlags = GetRayTracingAccelerationStructureBuildFlags(Initializer);

	GeometryDescs.SetNumUninitialized(Initializer.Segments.Num());
	TranslateRayTracingGeometryDescs(Initializer, GeometryDescs);

	SetDirty(FRHIGPUMask::All(), true);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS PrebuildDescInputs = {};

	PrebuildDescInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	PrebuildDescInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	PrebuildDescInputs.NumDescs = GeometryDescs.Num();
	PrebuildDescInputs.pGeometryDescs = GeometryDescs.GetData();
	PrebuildDescInputs.Flags = TranslateRayTracingAccelerationStructureFlags(BuildFlags);

	struct FOfflineBVHHeader
	{
		uint64 ResultSize;
		uint64 BuildScratchSize;
		uint64 UpdateScratchSize;
	};

	if (Initializer.OfflineData)
	{
		FOfflineBVHHeader* DataHeader = (FOfflineBVHHeader*)Initializer.OfflineData->GetResourceData();

		SizeInfo.ResultSize = DataHeader->ResultSize;
		SizeInfo.BuildScratchSize = DataHeader->BuildScratchSize;
		SizeInfo.UpdateScratchSize = DataHeader->UpdateScratchSize;

		AccelerationStructureCompactedSize = DataHeader->ResultSize;
	}
	else
	{
		// Get maximum buffer sizes for all GPUs in the system
		SizeInfo = RHICmdList.CalcRayTracingGeometrySize(Initializer);
	}

	checkf(SizeInfo.ResultSize != 0,
		TEXT("Unexpected acceleration structure buffer size (0).\nGeometry initializer details:\n%s"),
		*GetGeometryInitializerDebugString(Initializer));

	// If this RayTracingGeometry going to be used as streaming destination 
	// we don't want to allocate its memory as it will be replaced later by streamed version
	// but we still need correct SizeInfo as it is used to estimate its memory requirements outside of RHI.
	if (Initializer.Type == ERayTracingGeometryInitializerType::StreamingDestination)
	{
		return;
	}

	// Allocate acceleration structure buffer
	for (uint32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS && GPUIndex < GNumExplicitGPUsForRendering; ++GPUIndex)
	{
		AccelerationStructureBuffers[GPUIndex] = CreateRayTracingBuffer(Adapter, GPUIndex, SizeInfo.ResultSize, ERayTracingBufferType::AccelerationStructure, DebugName);
		AccelerationStructureBuffers[GPUIndex]->SetOwnerName(OwnerName);

		INC_MEMORY_STAT_BY(STAT_D3D12RayTracingUsedVideoMemory, AccelerationStructureBuffers[GPUIndex]->GetSize());
		INC_MEMORY_STAT_BY(STAT_D3D12RayTracingBLASMemory, AccelerationStructureBuffers[GPUIndex]->GetSize());
		if (Initializer.bAllowUpdate)
		{
			INC_MEMORY_STAT_BY(STAT_D3D12RayTracingDynamicBLASMemory, AccelerationStructureBuffers[GPUIndex]->GetSize());
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_D3D12RayTracingStaticBLASMemory, AccelerationStructureBuffers[GPUIndex]->GetSize());
		}
	}

	INC_DWORD_STAT_BY(STAT_D3D12RayTracingTrianglesBLAS, Initializer.TotalPrimitiveCount);

	const bool bForRendering = Initializer.Type == ERayTracingGeometryInitializerType::Rendering;
	if (Initializer.OfflineData)
	{
		FD3D12Device* Device = Adapter->GetDevice(0);

		const uint8* Data = ((const uint8*)Initializer.OfflineData->GetResourceData()) + sizeof(FOfflineBVHHeader);
		uint32 Size = Initializer.OfflineData->GetResourceDataSize() - sizeof(FOfflineBVHHeader);

		FD3D12ResourceLocation SrcResourceLoc(Device);

		const bool bOnAsyncThread = !IsInRHIThread() && !IsInRenderingThread();
		uint8* DstDataBase;
		if (bOnAsyncThread)
		{
			DstDataBase = (uint8*)Adapter->GetUploadHeapAllocator(0).AllocUploadResource(Size, 256, SrcResourceLoc);
		}
		else
		{
			DstDataBase = (uint8*)Device->GetDefaultFastAllocator().Allocate(Size, 256, &SrcResourceLoc);
		}

		FMemory::Memcpy(DstDataBase, Data, Size);

		if (bOnAsyncThread)
		{
			check(Initializer.Type == ERayTracingGeometryInitializerType::StreamingSource);

			FD3D12ResourceLocation* SrcResourceLoc_Heap = new FD3D12ResourceLocation(SrcResourceLoc.GetParentDevice());
			FD3D12ResourceLocation::TransferOwnership(*SrcResourceLoc_Heap, SrcResourceLoc);

			TRefCountPtr<FD3D12RayTracingGeometry> ThisRef = this;
			ENQUEUE_RENDER_COMMAND(CmdD3D12InitializeRayTracingGeometry)(
				[DestinationGeometry = MoveTemp(ThisRef), SrcResourceLoc_Heap](FRHICommandListImmediate& RHICmdList) mutable
			{
				const bool bForRendering = false;
				if (RHICmdList.Bypass())
				{
					FD3D12RHICommandInitializeRayTracingGeometry Command(MoveTemp(DestinationGeometry), *SrcResourceLoc_Heap, bForRendering);
					Command.ExecuteNoCmdList();
				}
				else
				{
					ALLOC_COMMAND_CL(RHICmdList, FD3D12RHICommandInitializeRayTracingGeometry)(MoveTemp(DestinationGeometry), *SrcResourceLoc_Heap, bForRendering);
				}
				delete SrcResourceLoc_Heap;
			});
		}
		else if (!RHICmdList.Bypass() && IsRunningRHIInSeparateThread())
		{
			ALLOC_COMMAND_CL(RHICmdList, FD3D12RHICommandInitializeRayTracingGeometry)(this, SrcResourceLoc, bForRendering);
		}
		else
		{
			FD3D12RHICommandInitializeRayTracingGeometry Command(this, SrcResourceLoc, bForRendering);
			Command.ExecuteNoCmdList();
		}
		
		Initializer.OfflineData->Discard();
	}
	else
	{
		// Offline data already registered via FD3D12RHICommandInitializeRayTracingGeometry
		for (uint32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS && GPUIndex < GNumExplicitGPUsForRendering; ++GPUIndex)
		{
			RegisterAsRenameListener(GPUIndex);
		}
	}
}

void FD3D12RayTracingGeometry::Swap(FD3D12RayTracingGeometry& Other)
{
	for (uint32 i = 0; i < MAX_NUM_GPUS; i++)
	{
		::Swap(AccelerationStructureBuffers[i], Other.AccelerationStructureBuffers[i]);
		::Swap(bIsAccelerationStructureDirty[i], Other.bIsAccelerationStructureDirty[i]);
	}
	::Swap(AccelerationStructureCompactedSize, Other.AccelerationStructureCompactedSize);

	// The rest of the members should be updated using SetInitializer()
}

void FD3D12RayTracingGeometry::ReleaseUnderlyingResource()
{
	UnregisterD3D12RayTracingGeometry(this);

	// Remove compaction request if still pending
	for (uint32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS; ++GPUIndex)
	{
		if (bHasPendingCompactionRequests[GPUIndex])
		{
			check(AccelerationStructureBuffers[GPUIndex]);
			FD3D12Device* Device = AccelerationStructureBuffers[GPUIndex].GetReference()->GetParentDevice();
			bool bRequestFound = Device->GetRayTracingCompactionRequestHandler()->ReleaseRequest(this);
			check(bRequestFound);
			bHasPendingCompactionRequests[GPUIndex] = false;
		}
	}

	// Unregister as dependent resource on vertex and index buffers
	for (uint32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS; ++GPUIndex)
	{
		UnregisterAsRenameListener(GPUIndex);
	}

	for (TRefCountPtr<FD3D12Buffer>& Buffer : AccelerationStructureBuffers)
	{
		if (Buffer)
		{
			DEC_MEMORY_STAT_BY(STAT_D3D12RayTracingUsedVideoMemory, Buffer->GetSize());
			DEC_MEMORY_STAT_BY(STAT_D3D12RayTracingBLASMemory, Buffer->GetSize());

			ERayTracingAccelerationStructureFlags BuildFlags = GetRayTracingAccelerationStructureBuildFlags(Initializer);
			if (EnumHasAllFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate))
			{
				DEC_MEMORY_STAT_BY(STAT_D3D12RayTracingDynamicBLASMemory, Buffer->GetSize());
			}
			else
			{
				DEC_MEMORY_STAT_BY(STAT_D3D12RayTracingStaticBLASMemory, Buffer->GetSize());
			}
		}
	}

	if (Initializer.Type != ERayTracingGeometryInitializerType::StreamingSource)
	{
		DEC_DWORD_STAT_BY(STAT_D3D12RayTracingTrianglesBLAS, Initializer.TotalPrimitiveCount);
		DEC_DWORD_STAT(STAT_D3D12RayTracingAllocatedBLAS);
	}

	// Reset members
	for (TRefCountPtr<FD3D12Buffer>& Buffer : AccelerationStructureBuffers)
	{
		Buffer.SafeRelease();
	}

	Initializer = {};

	AccelerationStructureCompactedSize = 0;
	GeometryDescs = {};
	for (TArray<FHitGroupSystemParameters>& HitGroupParametersForGPU : HitGroupSystemParameters)
	{
		HitGroupParametersForGPU.Empty();
	}
}

FD3D12RayTracingGeometry::~FD3D12RayTracingGeometry()
{
	ReleaseUnderlyingResource();
}

void FD3D12RayTracingGeometry::RegisterAsRenameListener(uint32 InGPUIndex)
{
	check(!bRegisteredAsRenameListener[InGPUIndex]);

	FD3D12Buffer* IndexBuffer = FD3D12DynamicRHI::ResourceCast(Initializer.IndexBuffer.GetReference(), InGPUIndex);
	if (IndexBuffer)
	{
		IndexBuffer->AddRenameListener(this);
	}

	TArray<FD3D12Buffer*, TInlineAllocator<1>> UniqueVertexBuffers;
	UniqueVertexBuffers.Reserve(Initializer.Segments.Num());
	for (const FRayTracingGeometrySegment& Segment : Initializer.Segments)
	{
		FD3D12Buffer* VertexBuffer = FD3D12DynamicRHI::ResourceCast(Segment.VertexBuffer.GetReference(), InGPUIndex);
		if (VertexBuffer && !UniqueVertexBuffers.Contains(VertexBuffer))
		{
			VertexBuffer->AddRenameListener(this);
			UniqueVertexBuffers.Add(VertexBuffer);
		}
	}

	bRegisteredAsRenameListener[InGPUIndex] = true;
}

void FD3D12RayTracingGeometry::UnregisterAsRenameListener(uint32 InGPUIndex)
{
	if (!bRegisteredAsRenameListener[InGPUIndex])
	{
		return;
	}

	FD3D12Buffer* IndexBuffer = FD3D12DynamicRHI::ResourceCast(Initializer.IndexBuffer.GetReference(), InGPUIndex);
	if (IndexBuffer)
	{
		IndexBuffer->RemoveRenameListener(this);
	}

	TArray<FD3D12Buffer*, TInlineAllocator<1>> UniqueVertexBuffers;
	UniqueVertexBuffers.Reserve(Initializer.Segments.Num());
	for (const FRayTracingGeometrySegment& Segment : Initializer.Segments)
	{
		FD3D12Buffer* VertexBuffer = FD3D12DynamicRHI::ResourceCast(Segment.VertexBuffer.GetReference(), InGPUIndex);
		if (VertexBuffer && !UniqueVertexBuffers.Contains(VertexBuffer))
		{
			VertexBuffer->RemoveRenameListener(this);
			UniqueVertexBuffers.Add(VertexBuffer);
		}
	}

	bRegisteredAsRenameListener[InGPUIndex] = false;
}

void FD3D12RayTracingGeometry::ResourceRenamed(FRHICommandListBase& RHICmdList, FD3D12BaseShaderResource* InRenamedResource, FD3D12ResourceLocation* InNewResourceLocation)
{
	// Empty resource location is used on destruction of the base shader resource but this
	// shouldn't happen for RT Geometries because it keeps smart pointers to it's resources.
	check(InNewResourceLocation != nullptr);

	// Recreate the hit group parameters which cache the address to the index and vertex buffers directly if the geometry is fully valid
	uint32 GPUIndex = InRenamedResource->GetParentDevice()->GetGPUIndex();
	if (BuffersValid(GPUIndex))
	{
		SetupHitGroupSystemParameters(GPUIndex);
	}
}

bool FD3D12RayTracingGeometry::BuffersValid(uint32 GPUIndex) const
{
	if (Initializer.IndexBuffer)
	{
		const FD3D12Buffer* IndexBuffer = FD3D12DynamicRHI::ResourceCast(Initializer.IndexBuffer.GetReference(), GPUIndex);
		if (!IndexBuffer->ResourceLocation.IsValid())
		{
			return false;
		}
	}

	for (const FRayTracingGeometrySegment& Segment : Initializer.Segments)
	{
		const FD3D12Buffer* VertexBuffer = FD3D12DynamicRHI::ResourceCast(Segment.VertexBuffer.GetReference(), GPUIndex);
		if (!VertexBuffer->ResourceLocation.IsValid())
		{
			return false;
		}
	}

	return true;
}

void FD3D12RayTracingGeometry::TransitionBuffers(FD3D12CommandContext& CommandContext)
{
	// Transition vertex and index resources..
	if (Initializer.IndexBuffer)
	{
		FD3D12Buffer* IndexBuffer = CommandContext.RetrieveObject<FD3D12Buffer>(Initializer.IndexBuffer.GetReference());
		if (IndexBuffer->GetResource()->RequiresResourceStateTracking())
		{
			CommandContext.TransitionResource(
				IndexBuffer->GetResource(),
				D3D12_RESOURCE_STATE_TBD,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				0
			);
		}
	}

	for (const FRayTracingGeometrySegment& Segment : Initializer.Segments)
	{
		const FBufferRHIRef& RHIVertexBuffer = Segment.VertexBuffer;
		FD3D12Buffer* VertexBuffer = CommandContext.RetrieveObject<FD3D12Buffer>(RHIVertexBuffer.GetReference());
		if (VertexBuffer->GetResource()->RequiresResourceStateTracking())
		{
			CommandContext.TransitionResource(
				VertexBuffer->GetResource(),
				D3D12_RESOURCE_STATE_TBD,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				0
			);
		}
	}
}

void FD3D12RayTracingGeometry::UpdateResidency(FD3D12CommandContext& CommandContext)
{
	if (Initializer.IndexBuffer)
	{
		FD3D12Buffer* IndexBuffer = CommandContext.RetrieveObject<FD3D12Buffer>(Initializer.IndexBuffer.GetReference());
		CommandContext.UpdateResidency(IndexBuffer->GetResource());
	}

	for (const FRayTracingGeometrySegment& Segment : Initializer.Segments)
	{
		const FBufferRHIRef& RHIVertexBuffer = Segment.VertexBuffer;
		FD3D12Buffer* VertexBuffer = CommandContext.RetrieveObject<FD3D12Buffer>(RHIVertexBuffer.GetReference());		
		CommandContext.UpdateResidency(VertexBuffer->ResourceLocation.GetResource());
	}

	const uint32 GPUIndex = CommandContext.GetGPUIndex();
	CommandContext.UpdateResidency(AccelerationStructureBuffers[GPUIndex]->GetResource());
}

void FD3D12RayTracingGeometry::SetInitializer(const FRayTracingGeometryInitializer& InInitializer)
{
	checkf(InitializedType == ERayTracingGeometryInitializerType::StreamingDestination, TEXT("Only FD3D12RayTracingGeometry that was created as StreamingDestination can update their initializer."));
	Initializer = InInitializer;

	for (uint32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS && GPUIndex < GNumExplicitGPUsForRendering; ++GPUIndex)
	{
		UnregisterAsRenameListener(GPUIndex);
	}	

	DebugName = !Initializer.DebugName.IsNone() ? Initializer.DebugName : FName(TEXT("BLAS"));

	checkf(Initializer.Segments.Num() > 0, TEXT("Ray tracing geometry must be initialized with at least one segment."));

	GeometryDescs.SetNumUninitialized(Initializer.Segments.Num());
	TranslateRayTracingGeometryDescs(Initializer, GeometryDescs);
	
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	RHICmdList.EnqueueLambda(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			for (uint32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS && GPUIndex < GNumExplicitGPUsForRendering; ++GPUIndex)
			{				
				RegisterAsRenameListener(GPUIndex);
				SetupHitGroupSystemParameters(GPUIndex);		
			}
		}
	);
}

void FD3D12RayTracingGeometry::SetupHitGroupSystemParameters(uint32 InGPUIndex)
{
	D3D12_RAYTRACING_GEOMETRY_TYPE GeometryType = TranslateRayTracingGeometryType(Initializer.GeometryType);

	TArray<FHitGroupSystemParameters>& HitGroupSystemParametersForThisGPU = HitGroupSystemParameters[InGPUIndex];
	HitGroupSystemParametersForThisGPU.Reset(Initializer.Segments.Num());

	check(BuffersValid(InGPUIndex));
	FD3D12Buffer* IndexBuffer = FD3D12DynamicRHI::ResourceCast(Initializer.IndexBuffer.GetReference(), InGPUIndex);
	const uint32 IndexStride = IndexBuffer ? IndexBuffer->GetStride() : 0;
	for (const FRayTracingGeometrySegment& Segment : Initializer.Segments)
	{
		FD3D12Buffer* VertexBuffer = FD3D12DynamicRHI::ResourceCast(Segment.VertexBuffer.GetReference(), InGPUIndex);

		FHitGroupSystemParameters SystemParameters = {};
		SystemParameters.RootConstants.SetVertexAndIndexStride(Segment.VertexBufferStride, IndexStride);
		SystemParameters.VertexBuffer = VertexBuffer->ResourceLocation.GetGPUVirtualAddress() + Segment.VertexBufferOffset;

		if (GeometryType == D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES && IndexBuffer != nullptr)
		{
			SystemParameters.IndexBuffer = IndexBuffer->ResourceLocation.GetGPUVirtualAddress();
			SystemParameters.RootConstants.IndexBufferOffsetInBytes = Initializer.IndexBufferOffset + IndexStride * Segment.FirstPrimitive * FD3D12RayTracingGeometry::IndicesPerPrimitive;
			SystemParameters.RootConstants.FirstPrimitive = Segment.FirstPrimitive;
		}

		HitGroupSystemParametersForThisGPU.Add(SystemParameters);
	}
}

void FD3D12RayTracingGeometry::CreateAccelerationStructureBuildDesc(FD3D12CommandContext& CommandContext, EAccelerationStructureBuildMode BuildMode, D3D12_GPU_VIRTUAL_ADDRESS ScratchBufferAddress, D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& OutDesc, TArrayView<D3D12_RAYTRACING_GEOMETRY_DESC>& OutGeometryDescs) const
{
	if (Initializer.IndexBuffer)
	{
		checkf(Initializer.IndexBuffer->GetStride() == 2 || Initializer.IndexBuffer->GetStride() == 4, TEXT("Index buffer must be 16 or 32 bit."));
	}

	const uint32 GPUIndex = CommandContext.GetGPUIndex();
	const uint32 IndexStride = Initializer.IndexBuffer ? Initializer.IndexBuffer->GetStride() : 0;
	const bool bIsUpdate = BuildMode == EAccelerationStructureBuildMode::Update;

	// Use the pre-built descs as template and set the GPU resource pointers (current VB/IB).
	check(OutGeometryDescs.Num() == GeometryDescs.Num());
	checkf(BuffersValid(GPUIndex), TEXT("Index & vertex buffers should be valid (not streamed out) when building the acceleration structure"));

	FD3D12Buffer* IndexBuffer = CommandContext.RetrieveObject<FD3D12Buffer>(Initializer.IndexBuffer.GetReference());
	FD3D12Buffer* NullTransformBufferD3D12 = CommandContext.RetrieveObject<FD3D12Buffer>(NullTransformBuffer.GetReference());

	const TArray<FHitGroupSystemParameters>& HitGroupSystemParametersForThisGPU = HitGroupSystemParameters[GPUIndex];
	check(HitGroupSystemParametersForThisGPU.Num() == Initializer.Segments.Num());

	ERayTracingAccelerationStructureFlags BuildFlags = GetRayTracingAccelerationStructureBuildFlags(Initializer);
	D3D12_RAYTRACING_GEOMETRY_TYPE GeometryType = TranslateRayTracingGeometryType(Initializer.GeometryType);
	for (int32 SegmentIndex = 0; SegmentIndex < Initializer.Segments.Num(); ++SegmentIndex)
	{
		D3D12_RAYTRACING_GEOMETRY_DESC& Desc = OutGeometryDescs[SegmentIndex];
		Desc = GeometryDescs[SegmentIndex]; // Copy from template

		const FRayTracingGeometrySegment& Segment = Initializer.Segments[SegmentIndex];
		const FHitGroupSystemParameters& SystemParameters = HitGroupSystemParametersForThisGPU[SegmentIndex];

		FD3D12Buffer* VertexBuffer = CommandContext.RetrieveObject<FD3D12Buffer>(Segment.VertexBuffer.GetReference());

		switch (GeometryType)
		{
		case D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES:
			switch (Segment.VertexBufferElementType)
			{
			case VET_Float4:
				// While the DXGI_FORMAT_R32G32B32A32_FLOAT format is not supported by DXR, since we manually load vertex 
				// data when we are building the BLAS, we can just rely on the vertex stride to offset the read index, 
				// and read only the 3 vertex components, and so use the DXGI_FORMAT_R32G32B32_FLOAT vertex format
			case VET_Float3:
				check(Desc.Triangles.VertexFormat == DXGI_FORMAT_R32G32B32_FLOAT);
				break;
			case VET_Float2:
				check(Desc.Triangles.VertexFormat == DXGI_FORMAT_R32G32_FLOAT);
				break;
			case VET_Half2:
				check(Desc.Triangles.VertexFormat == DXGI_FORMAT_R16G16_FLOAT);
				break;
			default:
				checkNoEntry();
				break;
			}

			if (!Segment.bEnabled)
			{
				Desc.Triangles.IndexCount = 0;
			}

			checkf(Desc.Triangles.Transform3x4 == D3D12_GPU_VIRTUAL_ADDRESS(0), TEXT("BLAS geometry transforms are not supported!"));

			if (IndexBuffer)
			{
				check(Desc.Triangles.IndexCount <= Segment.NumPrimitives * FD3D12RayTracingGeometry::IndicesPerPrimitive);

				Desc.Triangles.IndexFormat = (IndexStride == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT);
				Desc.Triangles.IndexBuffer = SystemParameters.IndexBuffer + SystemParameters.RootConstants.IndexBufferOffsetInBytes;
			}
			else
			{
				// Non-indexed geometry
				checkf(Initializer.Segments.Num() == 1, TEXT("Non-indexed geometry with multiple segments is not implemented."));
				check(Desc.Triangles.IndexFormat == DXGI_FORMAT_UNKNOWN);
				check(Desc.Triangles.IndexCount == 0);
				check(Desc.Triangles.IndexBuffer == D3D12_GPU_VIRTUAL_ADDRESS(0));
			}

			Desc.Triangles.VertexBuffer.StartAddress = SystemParameters.VertexBuffer;
			Desc.Triangles.VertexBuffer.StrideInBytes = Segment.VertexBufferStride;
			break;

		case D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS:
			Desc.AABBs.AABBCount = Segment.NumPrimitives;
			Desc.AABBs.AABBs.StartAddress = SystemParameters.VertexBuffer;
			Desc.AABBs.AABBs.StrideInBytes = Segment.VertexBufferStride;
			break;

		default:
			checkf(false, TEXT("Unexpected ray tracing geometry type"));
			break;
		}

		if (GeometryType == D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES)
		{
			// #dxr_todo UE-72160: support various vertex buffer layouts (fetch/decode based on vertex stride and format)
			checkf(Segment.VertexBufferElementType == VET_Float3 || Segment.VertexBufferElementType == VET_Float4, TEXT("Only VET_Float3 and Float4 are currently implemented and tested. Other formats will be supported in the future."));
		}
	}

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS LocalBuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS(BuildFlags);

	if (bIsUpdate)
	{
		checkf(EnumHasAllFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate),
			TEXT("Acceleration structure must be created with FRayTracingGeometryInitializer::bAllowUpdate=true to perform refit / update."));

		LocalBuildFlags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS PrebuildDescInputs = {};

	PrebuildDescInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	PrebuildDescInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	PrebuildDescInputs.NumDescs = OutGeometryDescs.Num();
	PrebuildDescInputs.pGeometryDescs = OutGeometryDescs.GetData();
	PrebuildDescInputs.Flags = LocalBuildFlags;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};

	CommandContext.GetParentDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&PrebuildDescInputs, &PrebuildInfo);

	// Must make sure that values computed in the constructor are valid.
	check(PrebuildInfo.ResultDataMaxSizeInBytes <= SizeInfo.ResultSize);

	if (bIsUpdate)
	{
		check(PrebuildInfo.UpdateScratchDataSizeInBytes <= SizeInfo.UpdateScratchSize);
	}
	else
	{
		check(PrebuildInfo.ScratchDataSizeInBytes <= SizeInfo.BuildScratchSize);
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BuildDesc = {};
	BuildDesc.Inputs = PrebuildDescInputs;
	BuildDesc.DestAccelerationStructureData = AccelerationStructureBuffers[GPUIndex]->ResourceLocation.GetGPUVirtualAddress();
	BuildDesc.ScratchAccelerationStructureData = ScratchBufferAddress;
	BuildDesc.SourceAccelerationStructureData = bIsUpdate
		? AccelerationStructureBuffers[GPUIndex]->ResourceLocation.GetGPUVirtualAddress()
		: D3D12_GPU_VIRTUAL_ADDRESS(0);

	OutDesc = BuildDesc;
}

static bool ShouldCompactAfterBuild(ERayTracingAccelerationStructureFlags BuildFlags)
{
	return EnumHasAllFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction | ERayTracingAccelerationStructureFlags::FastTrace)
		&& !EnumHasAnyFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate);
}

void FD3D12RayTracingGeometry::CompactAccelerationStructure(FD3D12CommandContext& CommandContext, uint32 InGPUIndex, uint64 InSizeAfterCompaction)
{
	LLM_SCOPE_BYNAME(TEXT("FD3D12RT/CompactBLAS"));
	// Should have a pending request
	check(bHasPendingCompactionRequests[InGPUIndex]);
	bHasPendingCompactionRequests[InGPUIndex] = false;

	ensureMsgf(InSizeAfterCompaction > 0, TEXT("Compacted acceleration structure size is expected to be non-zero. This error suggests that GPU readback synchronization is broken."));
	if (InSizeAfterCompaction == 0)
	{
		return;
	}

	DEC_MEMORY_STAT_BY(STAT_D3D12RayTracingUsedVideoMemory, AccelerationStructureBuffers[InGPUIndex]->GetSize());
	DEC_MEMORY_STAT_BY(STAT_D3D12RayTracingBLASMemory, AccelerationStructureBuffers[InGPUIndex]->GetSize());
	DEC_MEMORY_STAT_BY(STAT_D3D12RayTracingStaticBLASMemory, AccelerationStructureBuffers[InGPUIndex]->GetSize());

	UnregisterD3D12RayTracingGeometry(this);

	// Move old AS into this temporary variable which gets released when this function returns
	TRefCountPtr<FD3D12Buffer> OldAccelerationStructure = AccelerationStructureBuffers[InGPUIndex];

	AccelerationStructureBuffers[InGPUIndex] = CreateRayTracingBuffer(CommandContext.GetParentAdapter(), InGPUIndex, InSizeAfterCompaction, ERayTracingBufferType::AccelerationStructure, DebugName);
	AccelerationStructureBuffers[InGPUIndex]->SetOwnerName(OwnerName);

	INC_MEMORY_STAT_BY(STAT_D3D12RayTracingUsedVideoMemory, AccelerationStructureBuffers[InGPUIndex]->GetSize());
	INC_MEMORY_STAT_BY(STAT_D3D12RayTracingBLASMemory, AccelerationStructureBuffers[InGPUIndex]->GetSize());
	INC_MEMORY_STAT_BY(STAT_D3D12RayTracingStaticBLASMemory, AccelerationStructureBuffers[InGPUIndex]->GetSize());

	CommandContext.UpdateResidency(OldAccelerationStructure->GetResource());
	CommandContext.UpdateResidency(AccelerationStructureBuffers[InGPUIndex]->GetResource());

	CommandContext.RayTracingCommandList()->CopyRaytracingAccelerationStructure(
		AccelerationStructureBuffers[InGPUIndex]->ResourceLocation.GetGPUVirtualAddress(),
		OldAccelerationStructure->ResourceLocation.GetGPUVirtualAddress(),
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT
	);

	AccelerationStructureCompactedSize = InSizeAfterCompaction;

	RegisterD3D12RayTracingGeometry(this);
}

FD3D12RayTracingScene::FD3D12RayTracingScene(FD3D12Adapter* Adapter, FRayTracingSceneInitializer2 InInitializer)
	: FD3D12AdapterChild(Adapter), Initializer(MoveTemp(InInitializer))
{
	INC_DWORD_STAT(STAT_D3D12RayTracingAllocatedTLAS);

	checkf(Initializer.NumMissShaderSlots >= 1, TEXT("Need at least 1 miss shader slot."));
	checkf(Initializer.Lifetime == RTSL_SingleFrame, TEXT("Only single-frame ray tracing scenes are currently implemented."));

	const ERayTracingAccelerationStructureFlags BuildFlags = ERayTracingAccelerationStructureFlags::FastTrace; // #yuriy_todo: pass this in

	SizeInfo = {};

	const uint32 NumLayers = Initializer.NumNativeInstancesPerLayer.Num();
	check(NumLayers > 0);

	Layers.SetNum(NumLayers);

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FLayerData& Layer = Layers[LayerIndex];

		Layer.BuildInputs = {};
		Layer.BuildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		Layer.BuildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		Layer.BuildInputs.NumDescs = Initializer.NumNativeInstancesPerLayer[LayerIndex];
		Layer.BuildInputs.Flags = TranslateRayTracingAccelerationStructureFlags(BuildFlags);

		// Get maximum buffer sizes for all GPUs in the system
		Layer.SizeInfo = RHICalcRayTracingSceneSize(Layer.BuildInputs.NumDescs, BuildFlags);

		Layer.BufferOffset = Align(SizeInfo.ResultSize, GRHIRayTracingAccelerationStructureAlignment);
		Layer.ScratchBufferOffset = Align(SizeInfo.BuildScratchSize, GRHIRayTracingScratchBufferAlignment);

		SizeInfo.ResultSize = Layer.BufferOffset + Layer.SizeInfo.ResultSize;
		SizeInfo.BuildScratchSize = Layer.ScratchBufferOffset + Layer.SizeInfo.BuildScratchSize;
	}
};

FD3D12RayTracingScene::~FD3D12RayTracingScene()
{
	for (auto& Table : ShaderTables)
	{
		for (auto Item : Table)
		{
			delete Item.Value;
		}
	}

	ReleaseBuffer();

	DEC_DWORD_STAT(STAT_D3D12RayTracingAllocatedTLAS);
}

void FD3D12RayTracingScene::ReleaseBuffer()
{
	for (auto& AccelerationStructureBuffer : AccelerationStructureBuffers)
	{
		if (AccelerationStructureBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_D3D12RayTracingUsedVideoMemory, AccelerationStructureBuffer->GetSize());
			DEC_MEMORY_STAT_BY(STAT_D3D12RayTracingTLASMemory, AccelerationStructureBuffer->GetSize());
		}

		AccelerationStructureBuffer = nullptr;
	}
}

void FD3D12RayTracingScene::BindBuffer(FRHIBuffer* InBuffer, uint32 InBufferOffset)
{
	check(SizeInfo.ResultSize + InBufferOffset <= InBuffer->GetSize());

	for (uint32 GPUIndex = 0; GPUIndex < GNumExplicitGPUsForRendering; ++GPUIndex)
	{
		if (AccelerationStructureBuffers[GPUIndex])
		{
			DEC_MEMORY_STAT_BY(STAT_D3D12RayTracingUsedVideoMemory, AccelerationStructureBuffers[GPUIndex]->GetSize());
			DEC_MEMORY_STAT_BY(STAT_D3D12RayTracingTLASMemory, AccelerationStructureBuffers[GPUIndex]->GetSize());
		}

		AccelerationStructureBuffers[GPUIndex] = FD3D12CommandContext::RetrieveObject<FD3D12Buffer>(InBuffer, GPUIndex);

		INC_MEMORY_STAT_BY(STAT_D3D12RayTracingUsedVideoMemory, AccelerationStructureBuffers[GPUIndex]->GetSize());
		INC_MEMORY_STAT_BY(STAT_D3D12RayTracingTLASMemory, AccelerationStructureBuffers[GPUIndex]->GetSize());
	}
	BufferOffset = InBufferOffset;

	for (auto& Layer : Layers)
	{
		const uint32 LayerOffset = InBufferOffset + Layer.BufferOffset;
		check(LayerOffset % GRHIRayTracingAccelerationStructureAlignment == 0);
	}
}

void FD3D12RayTracingScene::BuildAccelerationStructure(FD3D12CommandContext& CommandContext,
	FD3D12Buffer* ScratchBuffer, uint32 ScratchBufferOffset,
	FD3D12Buffer* InstanceBuffer, uint32 InstanceBufferOffset)
{
	check(InstanceBuffer != nullptr);

	TRACE_CPUPROFILER_EVENT_SCOPE(BuildAccelerationStructure_TopLevel);
	SCOPE_CYCLE_COUNTER(STAT_D3D12BuildTLAS);

	const uint32 GPUIndex = CommandContext.GetGPUIndex();
	FD3D12Adapter* Adapter = CommandContext.GetParentAdapter();

	TRefCountPtr<FD3D12Buffer> AutoScratchBuffer;
	if (ScratchBuffer == nullptr)
	{
		static const FName ScratchBufferName("AutoBuildScratchTLAS");
		AutoScratchBuffer = CreateRayTracingBuffer(Adapter, GPUIndex, SizeInfo.BuildScratchSize, ERayTracingBufferType::Scratch, ScratchBufferName);
		ScratchBuffer = AutoScratchBuffer.GetReference();
		ScratchBufferOffset = 0;
	}

	checkf(ScratchBuffer, TEXT("TLAS build requires scratch buffer of at least %lld bytes."), SizeInfo.BuildScratchSize);

	const uint32 NumLayers = Initializer.NumNativeInstancesPerLayer.Num();

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		const FLayerData& Layer = Layers[LayerIndex];

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};

		CommandContext.GetParentDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&Layer.BuildInputs, &PrebuildInfo);

		checkf(PrebuildInfo.ResultDataMaxSizeInBytes <= Layer.SizeInfo.ResultSize,
			TEXT("TLAS build result buffer now requires %lld bytes, but only %lld was calculated in the constructor."),
			PrebuildInfo.ResultDataMaxSizeInBytes, Layer.SizeInfo.ResultSize);

		checkf(PrebuildInfo.ScratchDataSizeInBytes <= Layer.SizeInfo.BuildScratchSize,
			TEXT("TLAS build scratch buffer now requires %lld bytes, but only %lld was calculated in the constructor."),
			PrebuildInfo.ScratchDataSizeInBytes, Layer.SizeInfo.BuildScratchSize);

		checkf(PrebuildInfo.ScratchDataSizeInBytes + ScratchBufferOffset <= ScratchBuffer->GetSize(),
			TEXT("TLAS scratch buffer size is %d bytes with offset %d (%d bytes available), but the build requires %lld bytes. ")
			TEXT("BuildInputs.NumDescs = %d, Instances.Num = %d, PerInstanceGeometries.Num = %d."),
			ScratchBuffer->GetSize(), ScratchBufferOffset, ScratchBuffer->GetSize() - ScratchBufferOffset,
			PrebuildInfo.ScratchDataSizeInBytes,
			Layer.BuildInputs.NumDescs, Initializer.NumNativeInstancesPerLayer[LayerIndex], Initializer.PerInstanceGeometries.Num());
	}

	{
		// - Set up acceleration structure pointers and make them resident.
		// - Generate HitGroupSystemParametersCache.

		CommandContext.UpdateResidency(InstanceBuffer->GetResource());

		{
			TArray<FD3D12ResidencyHandle*>& GeometryResidencyHandlesForThisGPU = GeometryResidencyHandles[GPUIndex];

			GeometryResidencyHandlesForThisGPU.Reset(0);

			Experimental::TSherwoodSet<FD3D12ResidencyHandle*> UniqueResidencyHandles;

			auto AddResidencyHandleForResource = [&UniqueResidencyHandles, &GeometryResidencyHandlesForThisGPU] (FD3D12Resource* Resource)
			{
			#if ENABLE_RESIDENCY_MANAGEMENT
				for (FD3D12ResidencyHandle* ResidencyHandle : Resource->GetResidencyHandles())
				{
					if (D3DX12Residency::IsInitialized(ResidencyHandle))
					{
						bool bIsAlreadyInSet = false;
						UniqueResidencyHandles.Add(ResidencyHandle, &bIsAlreadyInSet);
						if (!bIsAlreadyInSet)
						{
							GeometryResidencyHandlesForThisGPU.Add(ResidencyHandle);
						}
					}
				}
			#endif // ENABLE_RESIDENCY_MANAGEMENT
			};

			const int32 NumReferencedGeometries = Initializer.ReferencedGeometries.Num();
			for (int32 Index = 0; Index < NumReferencedGeometries; ++Index)
			{
				FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(Initializer.ReferencedGeometries[Index].GetReference());

				checkf(!Geometry->IsDirty(CommandContext.GetGPUIndex()),
					TEXT("Acceleration structures for all geometries must be built before building the top level acceleration structure for the scene."));
				checkf(Geometry->BuffersValid(CommandContext.GetGPUIndex()),
					TEXT("Index & vertex buffers for all geometries must be valid (streamed in) when adding geometry to the top level acceleration structure for the scene"));

				AddResidencyHandleForResource(Geometry->AccelerationStructureBuffers[GPUIndex]->GetResource());

				if (Geometry->Initializer.IndexBuffer)
				{
					FD3D12Buffer* IndexBuffer = CommandContext.RetrieveObject<FD3D12Buffer>(Geometry->Initializer.IndexBuffer.GetReference());
					AddResidencyHandleForResource(IndexBuffer->GetResource());
				}

				for (const FRayTracingGeometrySegment& Segment : Geometry->Initializer.Segments)
				{
					if (Segment.VertexBuffer)
					{
						FD3D12Buffer* VertexBuffer = CommandContext.RetrieveObject<FD3D12Buffer>(Segment.VertexBuffer.GetReference());
						AddResidencyHandleForResource(VertexBuffer->GetResource());
					}
				}
			}
		}

		// make a copy of system parameters to they can optimized fetch during SBT building (only done for GPU0)
		if (GPUIndex == 0)
		{
			HitGroupSystemParametersCache.Reserve(Initializer.NumTotalSegments);
			HitGroupSystemParametersCache.Empty(Initializer.NumTotalSegments);

			const int32 NumSceneInstances = Initializer.PerInstanceGeometries.Num();
			for (int32 InstanceIndex = 0; InstanceIndex < NumSceneInstances; ++InstanceIndex)
			{
				FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(Initializer.PerInstanceGeometries[InstanceIndex]);

				check(Geometry->HitGroupSystemParameters[GPUIndex].Num() > 0);
				HitGroupSystemParametersCache.Append(Geometry->HitGroupSystemParameters[GPUIndex]);
			}
		}
	}
	
	// Build the actual acceleration structure

	const int32 NumReferencedGeometries = Initializer.ReferencedGeometries.Num();
	for (int32 Index = 0; Index < NumReferencedGeometries; ++Index)
	{
		FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(Initializer.ReferencedGeometries[Index].GetReference());
		CommandContext.UpdateResidency(Geometry->AccelerationStructureBuffers[GPUIndex]->ResourceLocation.GetResource());
	}

	TRefCountPtr<FD3D12Buffer>& AccelerationStructureBuffer = AccelerationStructureBuffers[GPUIndex];
	checkf(AccelerationStructureBuffer.IsValid(), 
		TEXT("Acceleration structure buffer must be set for this scene using RHIBindAccelerationStructureMemory() before build command is issued."));

	CommandContext.UpdateResidency(AccelerationStructureBuffer->GetResource());
	CommandContext.UpdateResidency(ScratchBuffer->GetResource());

	// Enqueue transition to UAV/SRV
	CommandContext.TransitionResource(
		InstanceBuffer->GetResource(),
		D3D12_RESOURCE_STATE_TBD,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		0
	);

	if (ShouldRunRayTracingGPUValidation())
	{
		for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
		{
			const FLayerData& Layer = Layers[LayerIndex];
			TRHICommandList_RecursiveHazardous<FD3D12CommandContext> RHICmdList(&CommandContext);
			uint32 InstanceBufferStride = GRHIRayTracingInstanceDescriptorSize;
			uint32 TotalHitGroupSlots = Initializer.NumTotalSegments * Initializer.ShaderSlotsPerGeometrySegment;
			FRayTracingValidateSceneBuildParamsCS::Dispatch(RHICmdList,
				TotalHitGroupSlots, Layer.BuildInputs.NumDescs,
				InstanceBuffer, InstanceBufferOffset, InstanceBufferStride);
		}
	}

	// UAV barrier is used here to ensure that all bottom level acceleration structures are built
	CommandContext.AddUAVBarrier();
	CommandContext.FlushResourceBarriers();

	TArray<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC, TInlineAllocator<32>> BuildDescs;
	BuildDescs.Reserve(NumLayers);

	uint32 InstanceBaseOffset = 0;

	for(uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		const FLayerData& Layer = Layers[LayerIndex];

		const D3D12_GPU_VIRTUAL_ADDRESS BufferAddress = AccelerationStructureBuffer->ResourceLocation.GetGPUVirtualAddress() + BufferOffset + Layer.BufferOffset;
		const D3D12_GPU_VIRTUAL_ADDRESS ScratchAddress = ScratchBuffer->ResourceLocation.GetGPUVirtualAddress() + ScratchBufferOffset + Layer.ScratchBufferOffset;

		checkf(BufferAddress % GRHIRayTracingAccelerationStructureAlignment == 0,
			TEXT("TLAS  buffer (plus offset) must be aligned to %lld bytes."),
			GRHIRayTracingAccelerationStructureAlignment);

		checkf(ScratchAddress % GRHIRayTracingScratchBufferAlignment == 0,
			TEXT("TLAS scratch buffer (plus offset) must be aligned to %lld bytes."),
			GRHIRayTracingScratchBufferAlignment);

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& BuildDesc = BuildDescs.AddDefaulted_GetRef();
		BuildDesc.Inputs = Layer.BuildInputs;
		BuildDesc.Inputs.InstanceDescs = InstanceBuffer->ResourceLocation.GetGPUVirtualAddress() + InstanceBufferOffset + InstanceBaseOffset * GRHIRayTracingInstanceDescriptorSize;
		BuildDesc.DestAccelerationStructureData = BufferAddress;
		BuildDesc.ScratchAccelerationStructureData = ScratchAddress;
		BuildDesc.SourceAccelerationStructureData = D3D12_GPU_VIRTUAL_ADDRESS(0); // Null source TLAS as this is a build command

		INC_DWORD_STAT(STAT_D3D12RayTracingBuiltTLAS);

		InstanceBaseOffset += Initializer.NumNativeInstancesPerLayer[LayerIndex];
	}

	CommandContext.BuildAccelerationStructuresInternal(BuildDescs);

	// UAV barrier is used here to ensure that the acceleration structure build is complete before any rays are traced
	// #dxr_todo: these barriers should ideally be inserted by the high level code to allow more overlapped execution
	CommandContext.AddUAVBarrier();

	bBuilt = true;

#if D3D12_RHI_SUPPORT_RAYTRACING_SCENE_DEBUGGING
	D3D12RayTracingSceneDebugUpdate(*this, InstanceBuffer, InstanceBufferOffset, CommandContext);
#endif // D3D12_RHI_SUPPORT_RAYTRACING_SCENE_DEBUGGING
}

void FD3D12RayTracingScene::UpdateResidency(FD3D12CommandContext& CommandContext)
{
#if ENABLE_RESIDENCY_MANAGEMENT

	// Skip redundant resource residency updates when a scene is repeatedly used on the same command list
	if (LastCommandListID == CommandContext.GetCommandListID())
	{
		return;
	}

	const uint32 GPUIndex = CommandContext.GetGPUIndex();
	CommandContext.UpdateResidency(AccelerationStructureBuffers[GPUIndex]->GetResource());
	CommandContext.UpdateResidency(GeometryResidencyHandles[GPUIndex]);

	LastCommandListID = CommandContext.GetCommandListID();

#endif // ENABLE_RESIDENCY_MANAGEMENT
}

FD3D12RayTracingShaderTable* FD3D12RayTracingScene::FindExistingShaderTable(const FD3D12RayTracingPipelineState* Pipeline, FD3D12Device* Device) const
{
	const uint32 GPUIndex = Device->GetGPUIndex();
	FD3D12RayTracingShaderTable* const* FoundShaderTable = ShaderTables[GPUIndex].Find(Pipeline);
	if (FoundShaderTable)
	{
		return *FoundShaderTable;
	}
	else
	{
		return nullptr;
	}
}

FD3D12RayTracingShaderTable* FD3D12RayTracingScene::FindOrCreateShaderTable(const FD3D12RayTracingPipelineState* Pipeline, FD3D12Device* Device)
{
	FD3D12RayTracingShaderTable* FoundShaderTable = FindExistingShaderTable(Pipeline, Device);
	if (FoundShaderTable)
	{
		return FoundShaderTable;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FindOrCreateShaderTable);
	SCOPE_CYCLE_COUNTER(STAT_D3D12CreateShaderTable);

	FD3D12RayTracingShaderTable* CreatedShaderTable = new FD3D12RayTracingShaderTable();
	ID3D12Device5* RayTracingDevice = Device->GetDevice5();
	const uint32 GPUIndex = Device->GetGPUIndex();

	const uint32 NumHitGroupSlots = Pipeline->bAllowHitGroupIndexing ? Initializer.NumTotalSegments * Initializer.ShaderSlotsPerGeometrySegment : 0;

	checkf(Pipeline->MaxLocalRootSignatureSize >= sizeof(FHitGroupSystemParameters), TEXT("All local root signatures are expected to contain ray tracing system root parameters (2x root buffers + 4x root DWORD)"));

	FD3D12RayTracingShaderTable::FInitializer SBTInitializer = {};
	SBTInitializer.NumRayGenShaders = Pipeline->RayGenShaders.Identifiers.Num();
	SBTInitializer.NumMissShaders = Pipeline->MissShaders.Identifiers.Num();
	SBTInitializer.NumMissRecords = Initializer.NumMissShaderSlots;
	SBTInitializer.NumHitRecords = NumHitGroupSlots;
	SBTInitializer.NumCallableRecords = Initializer.NumCallableShaderSlots;
	SBTInitializer.LocalRootDataSize = Pipeline->MaxLocalRootSignatureSize;
	SBTInitializer.MaxViewDescriptorsPerRecord = Pipeline->MaxHitGroupViewDescriptors;

	CreatedShaderTable->Init(SBTInitializer, Device, Pipeline->RayGenShaders.Identifiers, Pipeline->HitGroupShaders.Identifiers[0]);

	ShaderTables[GPUIndex].Add(Pipeline, CreatedShaderTable);

	return CreatedShaderTable;
}

void FD3D12CommandContext::BuildAccelerationStructuresInternal(const TArrayView<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC> BuildDescs)
{
	for (const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& Desc : BuildDescs)
	{
		GraphicsCommandList4()->BuildRaytracingAccelerationStructure(&Desc, 0, nullptr);
	}
}

#if WITH_MGPU
void FD3D12CommandContext::UnregisterAccelerationStructuresInternalMGPU(const TArrayView<const FRayTracingGeometryBuildParams> Params, FRHIGPUMask GPUMask)
{
	// We need to unregister rename listeners for all GPUs in a separate pass before running "RHIBuildAccelerationStructures", as the build process
	// may modify the buffer references in the ray tracing geometry.  This leads to an assert where the code attempts to unregister the newer buffer
	// references on the additional GPUs, rather than the original buffer references.  It's OK to unregister redundantly, as a flag is set to track
	// whether a buffer is registered, and additional unregister calls do nothing.
	for (uint32 GPUIndex : GPUMask)
	{
		for (const FRayTracingGeometryBuildParams& P : Params)
		{
			FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(P.Geometry.GetReference());
			Geometry->UnregisterAsRenameListener(GPUIndex);
		}
	}
}
#endif  // WITH_MGPU

void FD3D12CommandContext::RHIBuildAccelerationStructures(const TArrayView<const FRayTracingGeometryBuildParams> Params, const FRHIBufferRange& ScratchBufferRange)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(BuildAccelerationStructure_BottomLevel);
	SCOPE_CYCLE_COUNTER(STAT_D3D12BuildBLAS);
	LLM_SCOPE_BYNAME(TEXT("FD3D12RT/BLAS"));

	checkf(ScratchBufferRange.Buffer != nullptr, TEXT("BuildAccelerationStructures requires valid scratch buffer"));

	// Update geometry vertex buffers
	for (const FRayTracingGeometryBuildParams& P : Params)
	{
		FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(P.Geometry.GetReference());
		Geometry->UnregisterAsRenameListener(GetGPUIndex());

		if (P.Segments.Num())
		{
			checkf(P.Segments.Num() == Geometry->Initializer.Segments.Num(),
				TEXT("If updated segments are provided, they must exactly match existing geometry segments. Only vertex buffer bindings may change."));

			for (int32 i = 0; i < P.Segments.Num(); ++i)
			{
				checkf(P.Segments[i].MaxVertices <= Geometry->Initializer.Segments[i].MaxVertices,
					TEXT("Maximum number of vertices in a segment (%u) must not be larger than what was declared during FRHIRayTracingGeometry creation (%u), as this controls BLAS memory allocation."),
					P.Segments[i].MaxVertices, Geometry->Initializer.Segments[i].MaxVertices
				);

				Geometry->Initializer.Segments[i].VertexBuffer            = P.Segments[i].VertexBuffer;
				Geometry->Initializer.Segments[i].VertexBufferElementType = P.Segments[i].VertexBufferElementType;
				Geometry->Initializer.Segments[i].VertexBufferStride      = P.Segments[i].VertexBufferStride;
				Geometry->Initializer.Segments[i].VertexBufferOffset      = P.Segments[i].VertexBufferOffset;
			}
		}
	}

	// Transition all VBs and IBs to readable state

	for (const FRayTracingGeometryBuildParams& P : Params)
	{
		FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(P.Geometry.GetReference());
		Geometry->TransitionBuffers(*this);
	}

	{
		FD3D12Buffer* ScratchBuffer = FD3D12DynamicRHI::ResourceCast(ScratchBufferRange.Buffer, GetGPUIndex());
		if (ScratchBuffer->GetResource()->RequiresResourceStateTracking())
		{
			TransitionResource(
				ScratchBuffer->GetResource(),
				D3D12_RESOURCE_STATE_TBD,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				0
			);
		}
	}

	FlushResourceBarriers();
		
	const uint32 GPUIndex = GetGPUIndex();

	// Then do all work
	TArray<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC, TInlineAllocator<32>> BuildDescs;
	BuildDescs.Reserve(Params.Num());

	uint32 ScratchBufferSize = ScratchBufferRange.Size ? ScratchBufferRange.Size : ScratchBufferRange.Buffer->GetSize();

	checkf(ScratchBufferSize + ScratchBufferRange.Offset <= ScratchBufferRange.Buffer->GetSize(),
		TEXT("BLAS scratch buffer range size is %lld bytes with offset %lld, but the buffer only has %lld bytes. "),
		ScratchBufferRange.Size, ScratchBufferRange.Offset, ScratchBufferRange.Buffer->GetSize());


	const uint64 ScratchAlignment = GRHIRayTracingAccelerationStructureAlignment;
	FD3D12Buffer* ScratchBuffer = FD3D12DynamicRHI::ResourceCast(ScratchBufferRange.Buffer, GPUIndex);
	uint32 ScratchBufferOffset = ScratchBufferRange.Offset;

	UpdateResidency(ScratchBuffer->GetResource());

	FMemMark Mark(FMemStack::Get());

	for (int32 i = 0; i < Params.Num(); i++)
	{
		const FRayTracingGeometryBuildParams& P = Params[i];

		FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(P.Geometry.GetReference());
		Geometry->SetDirty(GetGPUMask(), true);

		// Register as rename listener to index/vertex buffers
		Geometry->UnregisterAsRenameListener(GPUIndex);
		Geometry->RegisterAsRenameListener(GPUIndex);

		// Recreate the hit group system parameters and use them during setup of the descs
		Geometry->SetupHitGroupSystemParameters(GPUIndex);

		if (Geometry->IsDirty(GPUIndex))
		{
			uint64 ScratchBufferRequiredSize = P.BuildMode == EAccelerationStructureBuildMode::Update ? Geometry->SizeInfo.UpdateScratchSize : Geometry->SizeInfo.BuildScratchSize;
			checkf(ScratchBufferRequiredSize + ScratchBufferOffset <= ScratchBufferSize,
				TEXT("BLAS scratch buffer size is %lld bytes with offset %lld (%lld bytes available), but the build requires %lld bytes. "),
				ScratchBufferSize, ScratchBufferOffset, ScratchBufferSize - ScratchBufferOffset, ScratchBufferRequiredSize);

			D3D12_GPU_VIRTUAL_ADDRESS ScratchBufferAddress = ScratchBuffer->ResourceLocation.GetGPUVirtualAddress() + ScratchBufferOffset;

			ScratchBufferOffset = Align(ScratchBufferOffset + ScratchBufferRequiredSize, ScratchAlignment);

			checkf(ScratchBufferAddress % GRHIRayTracingAccelerationStructureAlignment == 0,
				TEXT("BLAS scratch buffer (plus offset) must be aligned to %lld bytes."),
				GRHIRayTracingAccelerationStructureAlignment);

			// We need to keep D3D12_RAYTRACING_GEOMETRY_DESCs that are used in D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC alive.
			const uint32 NumGeometryDescs = Geometry->GeometryDescs.Num();
			D3D12_RAYTRACING_GEOMETRY_DESC* LocalGeometryDescsMemory = (D3D12_RAYTRACING_GEOMETRY_DESC*)FMemStack::Get().Alloc(NumGeometryDescs * sizeof(D3D12_RAYTRACING_GEOMETRY_DESC), alignof(D3D12_RAYTRACING_GEOMETRY_DESC));
			TArrayView<D3D12_RAYTRACING_GEOMETRY_DESC> LocalGeometryDescs = MakeArrayView(LocalGeometryDescsMemory, NumGeometryDescs);

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& BuildDesc = BuildDescs.AddZeroed_GetRef();			
			Geometry->CreateAccelerationStructureBuildDesc(*this, P.BuildMode, ScratchBufferAddress, BuildDesc, LocalGeometryDescs);

			Geometry->UpdateResidency(*this);

			if (P.BuildMode == EAccelerationStructureBuildMode::Update)
			{
				INC_DWORD_STAT(STAT_D3D12RayTracingUpdatedBLAS);
			}
			else
			{
				INC_DWORD_STAT(STAT_D3D12RayTracingBuiltBLAS);
			}
		}		
	}

	if (ShouldRunRayTracingGPUValidation())
	{
		TRHICommandList_RecursiveHazardous<FD3D12CommandContext> RHICmdList(this);

		for (const FRayTracingGeometryBuildParams& P : Params)
		{
			FRayTracingValidateGeometryBuildParamsCS::Dispatch(RHICmdList, P);
		}
	}

	BuildAccelerationStructuresInternal(BuildDescs);

	for (const FRayTracingGeometryBuildParams& P : Params)
	{
		FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(P.Geometry.GetReference());

		if (Geometry->IsDirty(GPUIndex))
		{
			ERayTracingAccelerationStructureFlags GeometryBuildFlags = GetRayTracingAccelerationStructureBuildFlags(Geometry->Initializer);
			if (ShouldCompactAfterBuild(GeometryBuildFlags))
			{
				GetParentDevice()->GetRayTracingCompactionRequestHandler()->RequestCompact(Geometry);
				Geometry->bHasPendingCompactionRequests[GPUIndex] = true;
			}

			Geometry->SetDirty(GetGPUMask(), false);
		}
	}

	// Add a UAV barrier after each acceleration structure build batch.
	// This is required because there are currently no explicit read/write barriers
	// for acceleration structures, but we need to ensure that all commands
	// are complete before BLAS is used again on the GPU.

	AddUAVBarrier();
}

void FD3D12CommandContext::RHIBuildAccelerationStructure(const FRayTracingSceneBuildParams& SceneBuildParams)
{
	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(SceneBuildParams.Scene);
	FD3D12Buffer* ScratchBuffer = RetrieveObject<FD3D12Buffer>(SceneBuildParams.ScratchBuffer);
	FD3D12Buffer* InstanceBuffer = RetrieveObject<FD3D12Buffer>(SceneBuildParams.InstanceBuffer);
	Scene->BuildAccelerationStructure(
		*this,
		ScratchBuffer, SceneBuildParams.ScratchBufferOffset,
		InstanceBuffer, SceneBuildParams.InstanceBufferOffset
	);
}

void FD3D12CommandContext::RHIBindAccelerationStructureMemory(FRHIRayTracingScene* InScene, FRHIBuffer* InBuffer, uint32 InBufferOffset)
{
	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);
	Scene->BindBuffer(InBuffer, InBufferOffset);
}

void FD3D12CommandContext::RHIClearRayTracingBindings(FRHIRayTracingScene* InScene)
{
	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);
	check(Scene);

	auto& Table = Scene->ShaderTables[GetGPUIndex()];
	for (auto Item : Table)
	{
		delete Item.Value;
	}
	Table.Reset();
}

struct FD3D12RayTracingGlobalResourceBinder
{
	FD3D12RayTracingGlobalResourceBinder(FD3D12CommandContext& InCommandContext, FD3D12ExplicitDescriptorCache& InDescriptorCache)
		: CommandContext(InCommandContext)
		, DescriptorCache(InDescriptorCache)
	{
		check(IsInRHIThread() || !IsRunningRHIInSeparateThread());
	}

	void SetRootCBV(uint32 BaseSlotIndex, uint32 DescriptorIndex, D3D12_GPU_VIRTUAL_ADDRESS Address)
	{
		CommandContext.GraphicsCommandList()->SetComputeRootConstantBufferView(BaseSlotIndex + DescriptorIndex, Address);
	}

	void SetRootSRV(uint32 BaseSlotIndex, uint32 DescriptorIndex, D3D12_GPU_VIRTUAL_ADDRESS Address)
	{
		CommandContext.GraphicsCommandList()->SetComputeRootShaderResourceView(BaseSlotIndex + DescriptorIndex, Address);
	}

	void SetRootDescriptorTable(uint32 SlotIndex, D3D12_GPU_DESCRIPTOR_HANDLE DescriptorTable)
	{
		CommandContext.GraphicsCommandList()->SetComputeRootDescriptorTable(SlotIndex, DescriptorTable);
	}

	FD3D12ConstantBufferView* CreateTransientConstantBuffer(FD3D12ResourceLocation& ResourceLocation, const void* Data, uint32 DataSize)
	{
		checkf(0, TEXT("Loose parameters and transient constant buffers are not implemented for global ray tracing shaders (raygen, miss, callable)"));
		return nullptr;
	}

	void AddResourceReference(FD3D12Resource* D3D12Resource)
	{
		CommandContext.UpdateResidency(D3D12Resource);
	}

	void AddResourceTransition(FD3D12ShaderResourceView* SRV)
	{
		CommandContext.TransitionResource(SRV, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	void AddResourceTransition(FD3D12UnorderedAccessView* UAV)
	{
		CommandContext.TransitionResource(UAV, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	FD3D12Device* GetDevice()
	{
		return CommandContext.GetParentDevice();
	}

#if ENABLE_RHI_VALIDATION
	RHIValidation::FTracker* GetValidationTracker()
	{
		return CommandContext.Tracker;
	}
#endif

	FD3D12CommandContext& CommandContext;
	FD3D12ExplicitDescriptorCache& DescriptorCache;
	static constexpr uint32 WorkerIndex = 0;
};

struct FD3D12RayTracingLocalResourceBinder
{
	FD3D12RayTracingLocalResourceBinder(FD3D12Device& InDevice, FD3D12RayTracingShaderTable& InShaderTable, const FD3D12RootSignature& InRootSignature, uint32 InRecordIndex, uint32 InWorkerIndex, ERayTracingBindingType InBindingType)
		: Device(InDevice)
		, ShaderTable(InShaderTable)
		, DescriptorCache(*InShaderTable.DescriptorCache)
		, RootSignature(InRootSignature)
		, RecordIndex(InRecordIndex)
		, WorkerIndex(InWorkerIndex)
	{
		check(InShaderTable.DescriptorCache != nullptr);
		check(WorkerIndex < InShaderTable.MaxBindingWorkers);
		check(WorkerIndex < uint32(DescriptorCache.WorkerData.Num()));
		check(RecordIndex != ~0u);

		switch (InBindingType)
		{
		case ERayTracingBindingType::CallableShader:
			ShaderTableOffset = InShaderTable.CallableShaderTableOffset;
			break;
		case ERayTracingBindingType::HitGroup:
			ShaderTableOffset = InShaderTable.HitGroupShaderTableOffset;
			break;
		case ERayTracingBindingType::MissShader:
			ShaderTableOffset = InShaderTable.MissShaderTableOffset;
			break;
		default:
			checkNoEntry();
		}
	}

	void SetRootDescriptor(uint32 BaseSlotIndex, uint32 DescriptorIndex, D3D12_GPU_VIRTUAL_ADDRESS Address)
	{
		const uint32 BindOffsetBase = RootSignature.GetBindSlotOffsetInBytes(BaseSlotIndex);
		const uint32 DescriptorSize = uint32(sizeof(D3D12_GPU_VIRTUAL_ADDRESS));
		const uint32 CurrentOffset = BindOffsetBase + DescriptorIndex * DescriptorSize;
		ShaderTable.SetLocalShaderParameters(ShaderTableOffset, RecordIndex, CurrentOffset, Address);
	}

	void SetRootCBV(uint32 BaseSlotIndex, uint32 DescriptorIndex, D3D12_GPU_VIRTUAL_ADDRESS Address)
	{
		SetRootDescriptor(BaseSlotIndex, DescriptorIndex, Address);
	}

	void SetRootSRV(uint32 BaseSlotIndex, uint32 DescriptorIndex, D3D12_GPU_VIRTUAL_ADDRESS Address)
	{
		SetRootDescriptor(BaseSlotIndex, DescriptorIndex, Address);
	}

	void SetRootDescriptorTable(uint32 SlotIndex, D3D12_GPU_DESCRIPTOR_HANDLE DescriptorTable)
	{
		const uint32 BindOffset = RootSignature.GetBindSlotOffsetInBytes(SlotIndex);
		ShaderTable.SetLocalShaderParameters(ShaderTableOffset, RecordIndex, BindOffset, DescriptorTable);
	}

	FD3D12ConstantBufferView* CreateTransientConstantBuffer(FD3D12ResourceLocation& ResourceLocation, const void* Data, uint32 DataSize)
	{
		// If we see a significant number of transient allocations coming through this path, we should consider
		// caching constant buffer blocks inside ShaderTable and linearly sub-allocate from them.
		// If the amount of data is relatively small, it may also be possible to use root constants and avoid extra allocations entirely.

	#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		FD3D12ConstantBufferView* ConstantBufferView = new FD3D12ConstantBufferView(GetDevice());
		ShaderTable.WorkerData[WorkerIndex].TransientCBVs.Add(ConstantBufferView);
	#else // D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		FD3D12ConstantBufferView* ConstantBufferView = nullptr;
	#endif // D3D12RHI_USE_CONSTANT_BUFFER_VIEWS

		FD3D12FastConstantAllocator& Allocator = Device.GetParentAdapter()->GetTransientUniformBufferAllocator();
		void* MappedData = Allocator.Allocate(DataSize, ResourceLocation, ConstantBufferView);

		FMemory::Memcpy(MappedData, Data, DataSize);

		ShaderTable.AddResourceReference(ResourceLocation.GetResource(), WorkerIndex);

		return ConstantBufferView;
	}

	void AddResourceReference(FD3D12Resource* D3D12Resource)
	{
		ShaderTable.AddResourceReference(D3D12Resource, WorkerIndex);
	}

	void AddResourceTransition(FD3D12ShaderResourceView* SRV)
	{
		if (SRV->GetResource()->RequiresResourceStateTracking())
		{
			ShaderTable.AddResourceTransition(SRV, WorkerIndex);
		}
	}

	void AddResourceTransition(FD3D12UnorderedAccessView* UAV)
	{
		if (UAV->GetResource()->RequiresResourceStateTracking())
		{
			ShaderTable.AddResourceTransition(UAV, WorkerIndex);
		}
	}

	FD3D12Device* GetDevice()
	{
		return &Device;
	}

#if ENABLE_RHI_VALIDATION
	RHIValidation::FTracker* GetValidationTracker()
	{
		// We can't validate resource states in RHISetRayTracingBindings because there's no command context at that point, and because the states will
		// change before the raytracing command is dispatched anyway.
		return nullptr;
	}
#endif

	FD3D12Device& Device;
	FD3D12RayTracingShaderTable& ShaderTable;
	FD3D12ExplicitDescriptorCache& DescriptorCache;
	const FD3D12RootSignature& RootSignature;
	uint32 ShaderTableOffset = 0;
	uint32 RecordIndex = ~0u;
	uint32 WorkerIndex = 0;
};

template <typename ResourceBinderType>
static bool SetRayTracingShaderResources(
	const FD3D12RayTracingShader* Shader,
	uint32 InNumTextures, FRHITexture* const* Textures,
	uint32 InNumSRVs, FRHIShaderResourceView* const* SRVs,
	uint32 InNumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
	uint32 InNumSamplers, FRHISamplerState* const* Samplers,
	uint32 InNumUAVs, FRHIUnorderedAccessView* const* UAVs,
	uint32 InLooseParameterDataSize, const void* InLooseParameterData,
	ResourceBinderType& Binder)
{
	const FD3D12RootSignature* RootSignature = Shader->pRootSignature;

	struct FBindings
	{
		ResourceBinderType& Binder;
		uint32 GPUIndex;

#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		D3D12_CPU_DESCRIPTOR_HANDLE LocalCBVs[MAX_CBS];
#endif
		D3D12_GPU_VIRTUAL_ADDRESS RemoteCBVs[MAX_CBS];

		D3D12_CPU_DESCRIPTOR_HANDLE LocalSRVs[MAX_SRVS];
		D3D12_CPU_DESCRIPTOR_HANDLE LocalUAVs[MAX_UAVS];
		D3D12_CPU_DESCRIPTOR_HANDLE LocalSamplers[MAX_SAMPLERS];

#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		uint32 CBVVersions[MAX_CBS];
#endif
		uint32 SRVVersions[MAX_SRVS];
		uint32 UAVVersions[MAX_SRVS];
		uint32 SamplerVersions[MAX_SRVS];

		TArray<FD3D12Resource*, TInlineAllocator<MAX_CBS + MAX_SRVS + MAX_UAVS>> ReferencedResources;

		uint64 BoundSRVMask = 0;
		uint64 BoundCBVMask = 0;
		uint64 BoundUAVMask = 0;
		uint64 BoundSamplerMask = 0;

		void SetUAV(FRHIUnorderedAccessView* RHIUAV, uint8 Index)
		{
			FD3D12UnorderedAccessView* UAV = FD3D12CommandContext::RetrieveObject<FD3D12UnorderedAccessView_RHI>(RHIUAV, GPUIndex);
			check(UAV != nullptr);

			FD3D12OfflineDescriptor Descriptor = UAV->GetOfflineCpuHandle();
			LocalUAVs[Index] = Descriptor;
			UAVVersions[Index] = Descriptor.GetVersion();

			BoundUAVMask |= 1ull << Index;

			ReferencedResources.Add(UAV->GetResource());
			Binder.AddResourceTransition(UAV);
		}

		void SetSRV(FRHIShaderResourceView* RHISRV, uint8 Index)
		{
			FD3D12ShaderResourceView* SRV = FD3D12CommandContext::RetrieveObject<FD3D12ShaderResourceView_RHI>(RHISRV, GPUIndex);
			check(SRV != nullptr);

			FD3D12OfflineDescriptor Descriptor = SRV->GetOfflineCpuHandle();
			LocalSRVs[Index] = Descriptor;
			SRVVersions[Index] = Descriptor.GetVersion();

			BoundSRVMask |= 1ull << Index;

			ReferencedResources.Add(SRV->GetResource());
			Binder.AddResourceTransition(SRV);
		}

		void SetTexture(FRHITexture* RHITexture, uint8 Index)
		{
			FD3D12ShaderResourceView* SRV = FD3D12CommandContext::RetrieveTexture(RHITexture, GPUIndex)->GetShaderResourceView();
			if (!ensure(SRV))
			{
				SRV = FD3D12CommandContext::RetrieveTexture(GBlackTexture->TextureRHI, GPUIndex)->GetShaderResourceView();
			}
			check(SRV != nullptr);

			FD3D12OfflineDescriptor Descriptor = SRV->GetOfflineCpuHandle();
			LocalSRVs[Index] = Descriptor;
			SRVVersions[Index] = Descriptor.GetVersion();

			BoundSRVMask |= 1ull << Index;

			ReferencedResources.Add(SRV->GetResource());
			Binder.AddResourceTransition(SRV);
		}

		void SetSampler(FRHISamplerState* RHISampler, uint8 Index)
		{
			FD3D12SamplerState* Sampler = FD3D12CommandContext::RetrieveObject<FD3D12SamplerState>(RHISampler, GPUIndex);
			check(Sampler != nullptr);

			FD3D12OfflineDescriptor Descriptor = Sampler->OfflineDescriptor;
			LocalSamplers[Index] = Descriptor;
			SamplerVersions[Index] = Descriptor.GetVersion();

			BoundSamplerMask |= 1ull << Index;
		}

	} Bindings { Binder, Binder.GetDevice()->GetGPUIndex() };

	for (uint32 TextureIndex = 0; TextureIndex < InNumTextures; ++TextureIndex)
	{
		FRHITexture* Resource = Textures[TextureIndex];
		if (Resource)
		{
			Bindings.SetTexture(Resource, TextureIndex);
		}
	}

	for (uint32 SRVIndex = 0; SRVIndex < InNumSRVs; ++SRVIndex)
	{
		FRHIShaderResourceView* Resource = SRVs[SRVIndex];
		if (Resource)
		{
			Bindings.SetSRV(Resource, SRVIndex);
		}
	}

	for (uint32 CBVIndex = 0; CBVIndex < InNumUniformBuffers; ++CBVIndex)
	{
		FRHIUniformBuffer* Resource = UniformBuffers[CBVIndex];
		if (Resource)
		{
			FD3D12UniformBuffer* CBV = FD3D12CommandContext::RetrieveObject<FD3D12UniformBuffer>(Resource, Bindings.GPUIndex);
		#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
			FD3D12OfflineDescriptor Descriptor = CBV->View->GetOfflineCpuHandle();
			Bindings.LocalCBVs[CBVIndex] = Descriptor;
			Bindings.CBVVersions[CBVIndex] = Descriptor.GetVersion();
		#endif // D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
			Bindings.RemoteCBVs[CBVIndex] = CBV->ResourceLocation.GetGPUVirtualAddress();
			Bindings.BoundCBVMask |= 1ull << CBVIndex;

			Bindings.ReferencedResources.Add(CBV->ResourceLocation.GetResource());
		}
	}

	for (uint32 SamplerIndex = 0; SamplerIndex < InNumSamplers; ++SamplerIndex)
	{
		FRHISamplerState* Resource = Samplers[SamplerIndex];
		if (Resource)
		{
			Bindings.SetSampler(Resource, SamplerIndex);
		}
	}

	for (uint32 UAVIndex = 0; UAVIndex < InNumUAVs; ++UAVIndex)
	{
		FRHIUnorderedAccessView* Resource = UAVs[UAVIndex];
		if (Resource)
		{
			Bindings.SetUAV(Resource, UAVIndex);
		}
	}

	{
		uint32 DirtyUniformBuffers = ~(0u);
		UE::RHICore::SetResourcesFromTables(
			  Bindings
			, *Shader
			, Shader->ShaderResourceTable
			, DirtyUniformBuffers
			, UniformBuffers
#if ENABLE_RHI_VALIDATION
			, Binder.GetValidationTracker()
#endif
		);
	}

	// Bind loose parameters

	if (Shader->UsesGlobalUniformBuffer())
	{
		checkf(InLooseParameterDataSize && InLooseParameterData, TEXT("Shader uses global uniform buffer, but the required loose parameter data is not provided."));
	}

	if (InLooseParameterData && Shader->UsesGlobalUniformBuffer())
	{
		const uint32 CBVIndex = 0; // Global uniform buffer is always assumed to be in slot 0

		FD3D12ResourceLocation ResourceLocation(Binder.GetDevice());
		FD3D12ConstantBufferView* ConstantBufferView = Binder.CreateTransientConstantBuffer(ResourceLocation, InLooseParameterData, InLooseParameterDataSize);

	#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		Bindings.LocalCBVs[CBVIndex] = ConstantBufferView->GetOfflineCpuHandle();
	#endif // D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		Bindings.RemoteCBVs[CBVIndex] = ResourceLocation.GetGPUVirtualAddress();

		Bindings.BoundCBVMask |= 1ull << CBVIndex;
	}

	// Validate that all resources required by the shader are set

	auto IsCompleteBinding = [](uint32 ExpectedCount, uint64 BoundMask)
	{
		if (ExpectedCount > 64) return false; // Bound resource mask can't be represented by uint64

		// All bits of the mask [0..ExpectedCount) are expected to be set
		uint64 ExpectedMask = ExpectedCount == 64 ? ~0ull : ((1ull << ExpectedCount) - 1);
		return (ExpectedMask & BoundMask) == ExpectedMask;
	};
	check(IsCompleteBinding(Shader->ResourceCounts.NumSRVs    , Bindings.BoundSRVMask));
	check(IsCompleteBinding(Shader->ResourceCounts.NumUAVs    , Bindings.BoundUAVMask));
	check(IsCompleteBinding(Shader->ResourceCounts.NumCBs     , Bindings.BoundCBVMask));
	check(IsCompleteBinding(Shader->ResourceCounts.NumSamplers, Bindings.BoundSamplerMask));

	FD3D12ExplicitDescriptorCache& DescriptorCache = Binder.DescriptorCache;
	const uint32 WorkerIndex = Binder.WorkerIndex;

	const uint32 NumSRVs = Shader->ResourceCounts.NumSRVs;
	if (NumSRVs)
	{
		const int32 DescriptorTableBaseIndex = DescriptorCache.AllocateDeduplicated(Bindings.SRVVersions, Bindings.LocalSRVs, NumSRVs, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, WorkerIndex);
		if (DescriptorTableBaseIndex < 0)
		{
			return false;
		}

		const uint32 BindSlot = RootSignature->SRVRDTBindSlot(SF_Compute);
		check(BindSlot != 0xFF);

		const D3D12_GPU_DESCRIPTOR_HANDLE ResourceDescriptorTableBaseGPU = DescriptorCache.ViewHeap.GetDescriptorGPU(DescriptorTableBaseIndex);
		Binder.SetRootDescriptorTable(BindSlot, ResourceDescriptorTableBaseGPU);
	}

	const uint32 NumUAVs = Shader->ResourceCounts.NumUAVs;
	if (NumUAVs)
	{
		const int32 DescriptorTableBaseIndex = DescriptorCache.AllocateDeduplicated(Bindings.UAVVersions, Bindings.LocalUAVs, NumUAVs, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, WorkerIndex);
		if (DescriptorTableBaseIndex < 0)
		{
			return false;
		}

		const uint32 BindSlot = RootSignature->UAVRDTBindSlot(SF_Compute);
		check(BindSlot != 0xFF);

		const D3D12_GPU_DESCRIPTOR_HANDLE ResourceDescriptorTableBaseGPU = DescriptorCache.ViewHeap.GetDescriptorGPU(DescriptorTableBaseIndex);
		Binder.SetRootDescriptorTable(BindSlot, ResourceDescriptorTableBaseGPU);
	}

	const uint32 NumCBVs = Shader->ResourceCounts.NumCBs;
	if (Shader->ResourceCounts.NumCBs)
	{
	#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		if (!EnumHasAllFlags(Shader->ResourceCounts.UsageFlags, EShaderResourceUsageFlags::BindlessResources))
		{
			const uint32 DescriptorTableBaseIndex = DescriptorCache.AllocateDeduplicated(Bindings.CBVVersions, Bindings.LocalCBVs, NumCBVs, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, WorkerIndex);
			const uint32 BindSlot = RootSignature->CBVRDTBindSlot(SF_Compute);
			check(BindSlot != 0xFF);

			const D3D12_GPU_DESCRIPTOR_HANDLE ResourceDescriptorTableBaseGPU = DescriptorCache.ViewHeap.GetDescriptorGPU(DescriptorTableBaseIndex);
			Binder.SetRootDescriptorTable(BindSlot, ResourceDescriptorTableBaseGPU);
		}
		else
	#endif // D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		{
			checkf(RootSignature->CBVRDTBindSlot(SF_Compute) == 0xFF, TEXT("Root CBV descriptor tables are not implemented for ray tracing shaders."));

			const uint32 BindSlot = RootSignature->CBVRDBaseBindSlot(SF_Compute);
			check(BindSlot != 0xFF);

			for (uint32 i = 0; i < Shader->ResourceCounts.NumCBs; ++i)
			{
				const uint64 SlotMask = (1ull << i);
				D3D12_GPU_VIRTUAL_ADDRESS BufferAddress = (Bindings.BoundCBVMask & SlotMask) ? Bindings.RemoteCBVs[i] : 0;
				Binder.SetRootCBV(BindSlot, i, BufferAddress);
			}
		}
	}

	// Bind samplers

	const uint32 NumSamplers = Shader->ResourceCounts.NumSamplers;
	if (NumSamplers)
	{
		const int32 DescriptorTableBaseIndex = DescriptorCache.AllocateDeduplicated(Bindings.SamplerVersions, Bindings.LocalSamplers, NumSamplers, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, WorkerIndex);
		if (DescriptorTableBaseIndex < 0)
		{
			return false;
		}

		const uint32 BindSlot = RootSignature->SamplerRDTBindSlot(SF_Compute);
		check(BindSlot != 0xFF);

		const D3D12_GPU_DESCRIPTOR_HANDLE ResourceDescriptorTableBaseGPU = DescriptorCache.SamplerHeap.GetDescriptorGPU(DescriptorTableBaseIndex);
		Binder.SetRootDescriptorTable(BindSlot, ResourceDescriptorTableBaseGPU);
	}

	for (FD3D12Resource* Resource : Bindings.ReferencedResources)
	{
		Binder.AddResourceReference(Resource);
	}

	return true;
}

template <typename ResourceBinderType>
static bool SetRayTracingShaderResources(
	const FD3D12RayTracingShader* Shader,
	const FRayTracingShaderBindings& ResourceBindings,
	ResourceBinderType& Binder)
{
	static_assert(
		sizeof(ResourceBindings.SRVs) / sizeof(*ResourceBindings.SRVs) == MAX_SRVS,
		"Ray Tracing Shader Bindings SRV array size must match D3D12 RHI Limit");
	static_assert(
		sizeof(ResourceBindings.UniformBuffers) / sizeof(*ResourceBindings.UniformBuffers) == MAX_CBS,
		"Ray Tracing Shader Bindings Uniform Buffer array size must match D3D12 RHI Limit");
	static_assert(
		sizeof(ResourceBindings.Samplers) / sizeof(*ResourceBindings.Samplers) == MAX_SAMPLERS,
		"Ray Tracing Shader Bindings Sampler array size must match D3D12 RHI Limit");
	static_assert(
		sizeof(ResourceBindings.UAVs) / sizeof(*ResourceBindings.UAVs) == MAX_UAVS,
		"Ray Tracing Shader Bindings UAV array size must match D3D12 RHI Limit");

	return SetRayTracingShaderResources(
		Shader,
		UE_ARRAY_COUNT(ResourceBindings.Textures), ResourceBindings.Textures,
		UE_ARRAY_COUNT(ResourceBindings.SRVs), ResourceBindings.SRVs,
		UE_ARRAY_COUNT(ResourceBindings.UniformBuffers), ResourceBindings.UniformBuffers,
		UE_ARRAY_COUNT(ResourceBindings.Samplers), ResourceBindings.Samplers,
		UE_ARRAY_COUNT(ResourceBindings.UAVs), ResourceBindings.UAVs,
		0, nullptr, // loose parameters
		Binder);
}

static void DispatchRays(FD3D12CommandContext& CommandContext,
	const FRayTracingShaderBindings& GlobalBindings,
	const FD3D12RayTracingPipelineState* Pipeline,
	uint32 RayGenShaderIndex,
	FD3D12RayTracingShaderTable* OptShaderTable,
	const D3D12_DISPATCH_RAYS_DESC& DispatchDesc,
	FD3D12Buffer* ArgumentBuffer = nullptr, uint32 ArgumentOffset = 0)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12DispatchRays);

	FD3D12Device* Device = CommandContext.GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

	FD3D12Buffer* DispatchRaysDescBuffer = nullptr;

	if (ArgumentBuffer)
	{
		// Source indirect argument buffer only contains the dispatch dimensions, however D3D12 requires a full D3D12_DISPATCH_RAYS_DESC structure.
		// We create a new buffer, fill the SBT pointers on CPU and copy the dispatch dimensions into the right place.

		DispatchRaysDescBuffer = Device->GetRayTracingDispatchRaysDescBuffer();
		FD3D12Resource* DispatchRaysDescBufferResource = DispatchRaysDescBuffer->GetResource();

		CommandContext.TransitionResource(DispatchRaysDescBufferResource, D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_COPY_DEST, 0);
		CommandContext.TransitionResource(ArgumentBuffer->GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
		CommandContext.FlushResourceBarriers();

		// Compute the allocation & copy sizes
		uint32 DispatchRayDescSize = sizeof(D3D12_DISPATCH_RAYS_DESC);
		uint32 SBTPartSize = offsetof(D3D12_DISPATCH_RAYS_DESC, Width);
		uint32 IndirectDimensionSize = DispatchRayDescSize - SBTPartSize;
		static_assert((sizeof(D3D12_DISPATCH_RAYS_DESC) - offsetof(D3D12_DISPATCH_RAYS_DESC, Width)) == sizeof(uint32) * 4, "Assume 4 uints at the end of the struct to store the dimension + alignment overhead");

		uint32 BaseRayDescBufferOffset = DispatchRaysDescBuffer->ResourceLocation.GetOffsetFromBaseOfResource();

		// Copy SBT data part of the dispatch desc to upload memory
		FD3D12ResourceLocation UploadResourceLocation(Device);
		void* Data = Device->GetDefaultFastAllocator().Allocate(DispatchRayDescSize, 256, &UploadResourceLocation);
		FMemory::Memcpy(Data, &DispatchDesc, SBTPartSize);

		// Copy SBT data part to resource
		CommandContext.GraphicsCommandList()->CopyBufferRegion(
			DispatchRaysDescBufferResource->GetResource(),
			BaseRayDescBufferOffset,
			UploadResourceLocation.GetResource()->GetResource(),
			UploadResourceLocation.GetOffsetFromBaseOfResource(),
			SBTPartSize
		);

		// Copy GPU computed indirect args to resource
		CommandContext.GraphicsCommandList()->CopyBufferRegion(
			DispatchRaysDescBufferResource->GetResource(),
			BaseRayDescBufferOffset + SBTPartSize,
			ArgumentBuffer->GetResource()->GetResource(),
			ArgumentBuffer->ResourceLocation.GetOffsetFromBaseOfResource() + ArgumentOffset,
			IndirectDimensionSize
		);

		CommandContext.TransitionResource(
			DispatchRaysDescBufferResource,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
			0
		);

		CommandContext.FlushResourceBarriers();
	}

	// Setup state for RT dispatch

	// Invalidate state cache to ensure all root parameters for regular shaders are reset when non-RT work is dispatched later.
	CommandContext.StateCache.TransitionComputeState(ED3D12PipelineType::RayTracing);

	CommandContext.GraphicsCommandList()->SetComputeRootSignature(Pipeline->GlobalRootSignature);

	// Bind diagnostic buffer to allow asserts in ray generation shaders
	{
		const FD3D12RootSignature* RootSignature = Adapter->GetGlobalRayTracingRootSignature();

		const int8 DiagnosticBufferSlot = RootSignature->GetDiagnosticBufferSlot();
		FD3D12Queue& Queue = CommandContext.GetParentDevice()->GetQueue(CommandContext.QueueType);
		D3D12_GPU_VIRTUAL_ADDRESS DiagnosticBufferAddress = Queue.GetDiagnosticBufferGPUAddress();

		if (DiagnosticBufferSlot >= 0 && DiagnosticBufferAddress)
		{
			CommandContext.GraphicsCommandList()->SetComputeRootUnorderedAccessView(DiagnosticBufferSlot, DiagnosticBufferAddress);
		}
	}

	FD3D12RayTracingShader* RayGenShader = Pipeline->RayGenShaders.Shaders[RayGenShaderIndex];

	bool bResourcesBound = false;
	if (OptShaderTable && OptShaderTable->DescriptorCache)
	{
		FD3D12ExplicitDescriptorCache* DescriptorCache = OptShaderTable->DescriptorCache;
		check(DescriptorCache != nullptr);

		DescriptorCache->SetDescriptorHeaps(CommandContext);
		FD3D12RayTracingGlobalResourceBinder ResourceBinder(CommandContext, *DescriptorCache);
		bResourcesBound = SetRayTracingShaderResources(RayGenShader, GlobalBindings, ResourceBinder);

		OptShaderTable->UpdateResidency(CommandContext);
	}
	else
	{
		FD3D12ExplicitDescriptorCache TransientDescriptorCache(CommandContext.GetParentDevice(), FD3D12RayTracingScene::MaxBindingWorkers);
		TransientDescriptorCache.Init(MAX_SRVS + MAX_UAVS, MAX_SAMPLERS, ERHIBindlessConfiguration::RayTracingShaders);
		TransientDescriptorCache.SetDescriptorHeaps(CommandContext);
		FD3D12RayTracingGlobalResourceBinder ResourceBinder(CommandContext, TransientDescriptorCache);
		bResourcesBound = SetRayTracingShaderResources(RayGenShader, GlobalBindings, ResourceBinder);
	}

	if (bResourcesBound)
	{
		if (OptShaderTable)
		{
			OptShaderTable->TransitionResources(CommandContext);
		}

		CommandContext.FlushResourceBarriers();

		ID3D12StateObject* RayTracingStateObject = nullptr;

		// Select a specialized RTPSO, if one is available
		if (GRayTracingAllowSpecializedStateObjects
			&& !Pipeline->SpecializedStateObjects.IsEmpty() 
			&& !Pipeline->SpecializationIndices.IsEmpty())
		{
			int32 SpecializationIndex = Pipeline->SpecializationIndices[RayGenShaderIndex];
			if (SpecializationIndex != INDEX_NONE)
			{
				RayTracingStateObject = Pipeline->SpecializedStateObjects[SpecializationIndex];
			}
		}

		// Fall back to default full RTPSO if specialization is not available
		if (!RayTracingStateObject)
		{
			RayTracingStateObject = Pipeline->StateObject.GetReference();
		}
;
		CommandContext.RayTracingCommandList()->SetPipelineState1(RayTracingStateObject);

		if (DispatchRaysDescBuffer)
		{
			ID3D12CommandSignature* CommandSignature = Adapter->GetDispatchRaysIndirectCommandSignature();
			CommandContext.RayTracingCommandList()->ExecuteIndirect(
				CommandSignature,
				1,
				DispatchRaysDescBuffer->ResourceLocation.GetResource()->GetResource(),
				DispatchRaysDescBuffer->ResourceLocation.GetOffsetFromBaseOfResource(),
				nullptr,
				0
			);
		}
		else
		{
			CommandContext.RayTracingCommandList()->DispatchRays(&DispatchDesc);
		}

		if (CommandContext.IsDefaultContext())
		{
			CommandContext.GetParentDevice()->RegisterGPUWork(1);
		}
	}

	// Restore old global descriptor heaps
	CommandContext.StateCache.GetDescriptorCache()->RestoreAfterExternalHeapsSet();
}

void FD3D12CommandContext::RHIRayTraceDispatch(FRHIRayTracingPipelineState* InRayTracingPipelineState, FRHIRayTracingShader* RayGenShaderRHI,
	FRHIRayTracingScene* InScene,
	const FRayTracingShaderBindings& GlobalResourceBindings,
	uint32 Width, uint32 Height)
{
	const FD3D12RayTracingPipelineState* Pipeline = FD3D12DynamicRHI::ResourceCast(InRayTracingPipelineState);

	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);

	FD3D12RayTracingShaderTable* ShaderTable = Scene->FindOrCreateShaderTable(Pipeline, GetParentDevice());

	if (ShaderTable->bIsDirty)
	{
		ShaderTable->CopyToGPU(*this);
	}

	Scene->UpdateResidency(*this);

	FD3D12RayTracingShader* RayGenShader = FD3D12DynamicRHI::ResourceCast(RayGenShaderRHI);
	const int32 RayGenShaderIndex = Pipeline->RayGenShaders.Find(RayGenShader->GetHash());
	checkf(RayGenShaderIndex != INDEX_NONE,
		TEXT("RayGen shader '%s' is not present in the given ray tracing pipeline. ")
		TEXT("All RayGen shaders must be declared when creating RTPSO."),
			*(RayGenShader->EntryPoint));

	D3D12_DISPATCH_RAYS_DESC DispatchDesc = ShaderTable->GetDispatchRaysDesc(RayGenShaderIndex, Pipeline->bAllowHitGroupIndexing);

	DispatchDesc.Width = Width;
	DispatchDesc.Height = Height;
	DispatchDesc.Depth = 1;

	DispatchRays(*this, GlobalResourceBindings, Pipeline, RayGenShaderIndex, ShaderTable, DispatchDesc);
}

void FD3D12CommandContext::RHIRayTraceDispatchIndirect(FRHIRayTracingPipelineState* InRayTracingPipelineState, FRHIRayTracingShader* RayGenShaderRHI,
	FRHIRayTracingScene* InScene,
	const FRayTracingShaderBindings& GlobalResourceBindings,
	FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset)
{
	checkf(GRHISupportsRayTracingDispatchIndirect, TEXT("RHIRayTraceDispatchIndirect may not be used because DXR 1.1 is not supported on this machine."));

	const FD3D12RayTracingPipelineState* Pipeline = FD3D12DynamicRHI::ResourceCast(InRayTracingPipelineState);

	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);

	FD3D12RayTracingShaderTable* ShaderTable = Scene->FindOrCreateShaderTable(Pipeline, GetParentDevice());

	if (ShaderTable->bIsDirty)
	{
		ShaderTable->CopyToGPU(*this);
	}

	Scene->UpdateResidency(*this);

	FD3D12RayTracingShader* RayGenShader = FD3D12DynamicRHI::ResourceCast(RayGenShaderRHI);
	const int32 RayGenShaderIndex = Pipeline->RayGenShaders.Find(RayGenShader->GetHash());
	checkf(RayGenShaderIndex != INDEX_NONE, TEXT("RayGen shader is not present in the given ray tracing pipeline. All RayGen shaders must be declared when creating RTPSO."));

	D3D12_DISPATCH_RAYS_DESC DispatchDesc = ShaderTable->GetDispatchRaysDesc(RayGenShaderIndex, Pipeline->bAllowHitGroupIndexing);
	DispatchRays(*this, GlobalResourceBindings, Pipeline, RayGenShaderIndex, ShaderTable, DispatchDesc, RetrieveObject<FD3D12Buffer>(ArgumentBuffer), ArgumentOffset);
}

static void SetRayTracingHitGroup(
	FD3D12Device* Device,
	FD3D12RayTracingShaderTable* ShaderTable,
	FD3D12RayTracingScene* Scene,
	FD3D12RayTracingPipelineState* Pipeline,
	uint32 InstanceIndex, uint32 SegmentIndex, uint32 ShaderSlot, uint32 HitGroupIndex,
	uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
	uint32 LooseParameterDataSize, const void* LooseParameterData,
	uint32 UserData,
	uint32 WorkerIndex)
{
	checkf(ShaderSlot < Scene->Initializer.ShaderSlotsPerGeometrySegment, TEXT("Shader slot is invalid. Make sure that ShaderSlotsPerGeometrySegment is correct on FRayTracingSceneInitializer."));

	const uint32 RecordIndex = Scene->GetHitRecordBaseIndex(InstanceIndex, SegmentIndex) + ShaderSlot;
	const uint32 GPUIndex = Device->GetGPUIndex();

#if DO_CHECK
	{
		const uint32 NumSceneInstances = (uint32)Scene->Initializer.PerInstanceGeometries.Num();
		checkf(InstanceIndex < NumSceneInstances, TEXT("Instance index %d is out of range for the scene that contains %d instances"), InstanceIndex, NumSceneInstances);

		const FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(Scene->Initializer.PerInstanceGeometries[InstanceIndex]);
		const uint32 NumGeometrySegments = Geometry->GetNumSegments();
		checkf(SegmentIndex < NumGeometrySegments, TEXT("Segment %d is out of range for ray tracing geometry '%s' that contains %d segments"),
			SegmentIndex, Geometry->DebugName.IsNone() ? TEXT("UNKNOWN") : *Geometry->DebugName.ToString(), NumGeometrySegments);
	}
#endif // DO_CHECK

	FHitGroupSystemParameters SystemParameters;
	if (GPUIndex == 0)
	{
		uint32 PrefixedSegmentIndex = Scene->Initializer.SegmentPrefixSum[InstanceIndex];
		SystemParameters = Scene->HitGroupSystemParametersCache[PrefixedSegmentIndex + SegmentIndex];
	}
	else
	{
		const FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(Scene->Initializer.PerInstanceGeometries[InstanceIndex]);
		const TArray<FHitGroupSystemParameters>& HitGroupSystemParametersForThisGPU = Geometry->HitGroupSystemParameters[GPUIndex];

		SystemParameters = HitGroupSystemParametersForThisGPU[SegmentIndex];
	}

	SystemParameters.RootConstants.BaseInstanceIndex = Scene->Initializer.BaseInstancePrefixSum[InstanceIndex];
	SystemParameters.RootConstants.UserData = UserData;
	ShaderTable->SetHitGroupSystemParameters(RecordIndex, SystemParameters);

	const FD3D12RayTracingShader* Shader = Pipeline->HitGroupShaders.Shaders[HitGroupIndex];

	FD3D12RayTracingShaderTable::FShaderRecordCacheKey CacheKey;

	const bool bCanUseRecordCache = GRayTracingCacheShaderRecords
		&& Scene->Initializer.Lifetime == RTSL_SingleFrame
		&& LooseParameterDataSize == 0 // loose parameters end up in unique constant buffers, so SBT records can't be shared
		&& NumUniformBuffers > 0 // there is no benefit from cache if no resources are being bound
		&& NumUniformBuffers <= CacheKey.MaxUniformBuffers;

	if (bCanUseRecordCache)
	{
		CacheKey = FD3D12RayTracingShaderTable::FShaderRecordCacheKey(NumUniformBuffers, UniformBuffers, HitGroupIndex);

		uint32* ExistingRecordIndex = ShaderTable->WorkerData[WorkerIndex].ShaderRecordCache.Find(CacheKey);
		if (ExistingRecordIndex)
		{
			// Simply copy local shader parameters from existing SBT record and set the shader identifier, skipping resource binding work.
			const uint32 OffsetFromRootSignatureStart = sizeof(FHitGroupSystemParameters);
			ShaderTable->SetHitGroupIdentifier(RecordIndex, Pipeline->HitGroupShaders.Identifiers[HitGroupIndex]);
			ShaderTable->CopyHitGroupParameters(RecordIndex, *ExistingRecordIndex, OffsetFromRootSignatureStart);
			return;
		}
	}

	FD3D12RayTracingLocalResourceBinder ResourceBinder(*Device, *ShaderTable, *(Shader->pRootSignature), RecordIndex, WorkerIndex, ERayTracingBindingType::HitGroup);
	const bool bResourcesBound = SetRayTracingShaderResources(Shader,
		0, nullptr, // Textures
		0, nullptr, // SRVs
		NumUniformBuffers, UniformBuffers,
		0, nullptr, // Samplers
		0, nullptr, // UAVs
		LooseParameterDataSize, LooseParameterData,
		ResourceBinder);

	if (bCanUseRecordCache && bResourcesBound)
	{
		ShaderTable->WorkerData[WorkerIndex].ShaderRecordCache.FindOrAdd(CacheKey, RecordIndex);
	}

	ShaderTable->SetHitGroupIdentifier(RecordIndex,
		bResourcesBound
		? Pipeline->HitGroupShaders.Identifiers[HitGroupIndex]
		: FD3D12ShaderIdentifier::Null);
}

static void SetRayTracingCallableShader(
	FD3D12Device* Device,
	FD3D12RayTracingShaderTable* ShaderTable,
	FD3D12RayTracingScene* Scene,
	FD3D12RayTracingPipelineState* Pipeline,
	uint32 ShaderSlotInScene, uint32 ShaderIndexInPipeline,
	uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
	uint32 LooseParameterDataSize, const void* LooseParameterData,
	uint32 UserData,
	uint32 WorkerIndex)
{
	checkf(ShaderSlotInScene < Scene->Initializer.NumCallableShaderSlots, TEXT("Shader slot is invalid. Make sure that NumCallableShaderSlots is correct on FRayTracingSceneInitializer."));

	const uint32 RecordIndex = ShaderSlotInScene;

	const uint32 UserDataOffset = offsetof(FHitGroupSystemParameters, RootConstants) + offsetof(FHitGroupSystemRootConstants, UserData);
	ShaderTable->SetCallableShaderParameters(RecordIndex, UserDataOffset, UserData);

	const FD3D12ShaderIdentifier* ShaderIdentifier = &FD3D12ShaderIdentifier::Null;

	if (ShaderIndexInPipeline != INDEX_NONE)
	{
		const FD3D12RayTracingShader* Shader = Pipeline->CallableShaders.Shaders[ShaderIndexInPipeline];

		FD3D12RayTracingLocalResourceBinder ResourceBinder(*Device, *ShaderTable, *(Shader->pRootSignature), RecordIndex, WorkerIndex, ERayTracingBindingType::CallableShader);
		const bool bResourcesBound = SetRayTracingShaderResources(Shader,
			0, nullptr, // Textures
			0, nullptr, // SRVs
			NumUniformBuffers, UniformBuffers,
			0, nullptr, // Samplers
			0, nullptr, // UAVs
			LooseParameterDataSize, LooseParameterData, // Loose parameters
			ResourceBinder);

		if (bResourcesBound)
		{
			ShaderIdentifier = &Pipeline->CallableShaders.Identifiers[ShaderIndexInPipeline];
		}
	}

	ShaderTable->SetCallableIdentifier(RecordIndex, *ShaderIdentifier);
}

static void SetRayTracingMissShader(
	FD3D12Device* Device,
	FD3D12RayTracingShaderTable* ShaderTable,
	FD3D12RayTracingScene* Scene,
	FD3D12RayTracingPipelineState* Pipeline,
	uint32 ShaderSlotInScene, uint32 ShaderIndexInPipeline,
	uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
	uint32 LooseParameterDataSize, const void* LooseParameterData,
	uint32 UserData,
	uint32 WorkerIndex)
{
	checkf(ShaderSlotInScene < Scene->Initializer.NumMissShaderSlots, TEXT("Shader slot is invalid. Make sure that NumMissShaderSlots is correct on FRayTracingSceneInitializer."));

	const uint32 RecordIndex = ShaderSlotInScene;

	const uint32 UserDataOffset = offsetof(FHitGroupSystemParameters, RootConstants) + offsetof(FHitGroupSystemRootConstants, UserData);
	ShaderTable->SetMissShaderParameters(RecordIndex, UserDataOffset, UserData);

	const FD3D12RayTracingShader* Shader = Pipeline->MissShaders.Shaders[ShaderIndexInPipeline];

	FD3D12RayTracingLocalResourceBinder ResourceBinder(*Device, *ShaderTable, *(Shader->pRootSignature), RecordIndex, WorkerIndex, ERayTracingBindingType::MissShader);
	const bool bResourcesBound = SetRayTracingShaderResources(Shader,
		0, nullptr, // Textures
		0, nullptr, // SRVs
		NumUniformBuffers, UniformBuffers,
		0, nullptr, // Samplers
		0, nullptr, // UAVs
		LooseParameterDataSize, LooseParameterData, // Loose parameters
		ResourceBinder);

	ShaderTable->SetMissIdentifier(ShaderSlotInScene,
		bResourcesBound
		? Pipeline->MissShaders.Identifiers[ShaderIndexInPipeline]
		: FD3D12ShaderIdentifier::Null);
}

void FD3D12CommandContext::RHISetRayTracingBindings(
	FRHIRayTracingScene* InScene, FRHIRayTracingPipelineState* InPipeline,
	uint32 NumBindings, const FRayTracingLocalShaderBindings* Bindings,
	ERayTracingBindingType BindingType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RHISetRayTracingBindings);
	SCOPE_CYCLE_COUNTER(STAT_D3D12SetRayTracingBindings);

	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);
	FD3D12RayTracingPipelineState* Pipeline = FD3D12DynamicRHI::ResourceCast(InPipeline);

	checkf(Scene->bBuilt, TEXT("Ray tracing scene must be built before any shaders can be bound to it. Make sure that RHIBuildAccelerationStructure() command has been executed."));

	FD3D12RayTracingShaderTable* ShaderTable = Scene->FindOrCreateShaderTable(Pipeline, Device);

	FGraphEventArray TaskList;

	const uint32 NumWorkerThreads = FTaskGraphInterface::Get().GetNumWorkerThreads();
	const uint32 MaxTasks = FApp::ShouldUseThreadingForPerformance() 
		? FMath::Min<uint32>(NumWorkerThreads, FD3D12RayTracingScene::MaxBindingWorkers)
		: 1;

	struct FTaskContext
	{
		uint32 WorkerIndex = 0;
	};

	TArray<FTaskContext, TInlineAllocator<FD3D12RayTracingScene::MaxBindingWorkers>> TaskContexts;
	for (uint32 WorkerIndex = 0; WorkerIndex < MaxTasks; ++WorkerIndex)
	{
		TaskContexts.Add(FTaskContext{WorkerIndex});
	}

	auto BindingTask = [Bindings, Device = Device, ShaderTable, Scene, Pipeline, BindingType](const FTaskContext& Context, int32 CurrentIndex)
	{
		const FRayTracingLocalShaderBindings& Binding = Bindings[CurrentIndex];

		if (BindingType == ERayTracingBindingType::HitGroup)
		{
			SetRayTracingHitGroup(Device, ShaderTable, Scene, Pipeline,
				Binding.InstanceIndex,
				Binding.SegmentIndex,
				Binding.ShaderSlot,
				Binding.ShaderIndexInPipeline,
				Binding.NumUniformBuffers,
				Binding.UniformBuffers,
				Binding.LooseParameterDataSize,
				Binding.LooseParameterData,
				Binding.UserData,
				Context.WorkerIndex);
		}
		else if (BindingType == ERayTracingBindingType::CallableShader)
		{
			SetRayTracingCallableShader(Device, ShaderTable, Scene, Pipeline,
				Binding.ShaderSlot,
				Binding.ShaderIndexInPipeline,
				Binding.NumUniformBuffers,
				Binding.UniformBuffers,
				Binding.LooseParameterDataSize,
				Binding.LooseParameterData,
				Binding.UserData,
				Context.WorkerIndex);
		}
		else if (BindingType == ERayTracingBindingType::MissShader)
		{
			SetRayTracingMissShader(Device, ShaderTable, Scene, Pipeline,
				Binding.ShaderSlot,
				Binding.ShaderIndexInPipeline,
				Binding.NumUniformBuffers,
				Binding.UniformBuffers,
				Binding.LooseParameterDataSize,
				Binding.LooseParameterData,
				Binding.UserData,
				Context.WorkerIndex);
		}
		else
		{
			checkNoEntry();
		}
	};

	// One helper worker task will be created at most per this many work items, plus one worker for current thread (unless running on a task thread),
	// up to a hard maximum of FD3D12RayTracingScene::MaxBindingWorkers.
	// Internally, parallel for tasks still subdivide the work into smaller chunks and perform fine-grained load-balancing.
	const int32 ItemsPerTask = 1024;

	ParallelForWithExistingTaskContext(TEXT("SetRayTracingBindings"), MakeArrayView(TaskContexts), NumBindings, ItemsPerTask, BindingTask);

	ShaderTable->bIsDirty = true;
}

void FD3D12CommandContext::RHISetRayTracingHitGroup(
	FRHIRayTracingScene* InScene, uint32 InstanceIndex, uint32 SegmentIndex, uint32 ShaderSlot,
	FRHIRayTracingPipelineState* InPipeline, uint32 HitGroupIndex,
	uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
	uint32 LooseParameterDataSize, const void* LooseParameterData,
	uint32 UserData)
{
	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);
	FD3D12RayTracingPipelineState* Pipeline = FD3D12DynamicRHI::ResourceCast(InPipeline);

	checkf(ShaderSlot < Scene->Initializer.ShaderSlotsPerGeometrySegment, TEXT("Shader slot is invalid. Make sure that ShaderSlotsPerGeometrySegment is correct on FRayTracingSceneInitializer."));

	FD3D12RayTracingShaderTable* ShaderTable = Scene->FindOrCreateShaderTable(Pipeline, GetParentDevice());

	const uint32 WorkerIndex = 0;
	SetRayTracingHitGroup(GetParentDevice(), ShaderTable, Scene, Pipeline,
		InstanceIndex,
		SegmentIndex,
		ShaderSlot,
		HitGroupIndex,
		NumUniformBuffers,
		UniformBuffers,
		LooseParameterDataSize,
		LooseParameterData,
		UserData,
		WorkerIndex);

	ShaderTable->bIsDirty = true;
}

void FD3D12CommandContext::RHISetRayTracingCallableShader(
	FRHIRayTracingScene* InScene, uint32 ShaderSlotInScene,
	FRHIRayTracingPipelineState* InPipeline, uint32 ShaderIndexInPipeline,
	uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
	uint32 UserData)
{
	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);
	FD3D12RayTracingPipelineState* Pipeline = FD3D12DynamicRHI::ResourceCast(InPipeline);
	FD3D12RayTracingShaderTable* ShaderTable = Scene->FindOrCreateShaderTable(Pipeline, GetParentDevice());
	const uint32 WorkerIndex = 0;

	SetRayTracingCallableShader(GetParentDevice(), ShaderTable, Scene, Pipeline, ShaderSlotInScene, ShaderIndexInPipeline, NumUniformBuffers, UniformBuffers, 0, nullptr, UserData, WorkerIndex);

	ShaderTable->bIsDirty = true;
}

void FD3D12CommandContext::RHISetRayTracingMissShader(
	FRHIRayTracingScene* InScene, uint32 ShaderSlotInScene,
	FRHIRayTracingPipelineState* InPipeline, uint32 ShaderIndexInPipeline,
	uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
	uint32 UserData)
{
	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);
	FD3D12RayTracingPipelineState* Pipeline = FD3D12DynamicRHI::ResourceCast(InPipeline);
	FD3D12RayTracingShaderTable* ShaderTable = Scene->FindOrCreateShaderTable(Pipeline, GetParentDevice());
	const uint32 WorkerIndex = 0;
	SetRayTracingMissShader(GetParentDevice(), ShaderTable, Scene, Pipeline,
		ShaderSlotInScene, ShaderIndexInPipeline,
		NumUniformBuffers, UniformBuffers,
		0, nullptr, // Loose parameters
		UserData,
		WorkerIndex);

	ShaderTable->bIsDirty = true;
}

#endif // D3D12_RHI_RAYTRACING
