// Copyright Epic Games, Inc. All Rights Reserved.

/**
* Here are a number of profiling helper functions so we do not have to duplicate a lot of the glue
* code everywhere.  And we can have consistent naming for all our files.
*
*/

// Core includes.
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "HAL/IConsoleManager.h"

double FScopedLoadTimeAccumulatorTimer::DummyTimer = 0.0;

FScopedLoadTimeAccumulatorTimer::FScopedLoadTimeAccumulatorTimer(const FName& InTimerName, const FName& InInstanceName)
	: FScopedDurationTimer(FLoadTimeTracker::Get().IsAccumulating() ? FLoadTimeTracker::Get().GetScopeTimeAccumulator(InTimerName, InInstanceName) : DummyTimer)
{}

FLoadTimeTracker::FLoadTimeTracker()
{
	ResetRawLoadTimes();
	bAccumulating = false;
}

FLoadTimeTracker& FLoadTimeTracker::Get()
{
	static FLoadTimeTracker Singleton;
	return Singleton;
}

void FLoadTimeTracker::ReportScopeTime(double ScopeTime, const FName ScopeLabel)
{
	check(IsInGameThread());
	TArray<double>& LoadTimes = TimeInfo.FindOrAdd(ScopeLabel);
	LoadTimes.Add(ScopeTime);
}

double& FLoadTimeTracker::GetScopeTimeAccumulator(const FName& ScopeLabel, const FName& ScopeInstance)
{
	check(IsInGameThread());
	FAccumulatorTracker& Tracker = AccumulatedTimeInfo.FindOrAdd(ScopeLabel);
	FTimeAndCount& TimeAndCount = Tracker.TimeInfo.FindOrAdd(ScopeInstance);
	TimeAndCount.Count++;
	return TimeAndCount.Time;
}

void FLoadTimeTracker::DumpHighLevelLoadTimes() const
{
	double TotalTime = 0.0;
	UE_LOG(LogLoad, Log, TEXT("------------- Load times -------------"));
	for(auto Itr = TimeInfo.CreateConstIterator(); Itr; ++Itr)
	{
		const FString KeyName = Itr.Key().ToString();
		const TArray<double>& LoadTimes = Itr.Value();
		if(LoadTimes.Num() == 1)
		{
			TotalTime += Itr.Value()[0];
			UE_LOG(LogLoad, Log, TEXT("%s: %f"), *KeyName, Itr.Value()[0]);
		}
		else
		{
			double InnerTotal = 0.0;
			for(int Index = 0; Index < LoadTimes.Num(); ++Index)
			{
				InnerTotal += Itr.Value()[Index];
				UE_LOG(LogLoad, Log, TEXT("%s[%d]: %f"), *KeyName, Index, LoadTimes[Index]);
			}

			UE_LOG(LogLoad, Log, TEXT("    Sub-Total: %f"), InnerTotal);

			TotalTime += InnerTotal;
		}
		
	}
	UE_LOG(LogLoad, Log, TEXT("------------- ---------- -------------"));
	UE_LOG(LogLoad, Log, TEXT("Total Load times: %f"), TotalTime);
}

void FLoadTimeTracker::ResetHighLevelLoadTimes()
{
	static bool bActuallyReset = !FParse::Param(FCommandLine::Get(), TEXT("NoLoadTrackClear"));
	if(bActuallyReset)
	{
		TimeInfo.Reset();
	}
}

void FLoadTimeTracker::DumpRawLoadTimes() const
{
#if ENABLE_LOADTIME_RAW_TIMINGS
	UE_LOG(LogStreaming, Display, TEXT("-------------------------------------------------"));
	UE_LOG(LogStreaming, Display, TEXT("Async Loading Stats"));
	UE_LOG(LogStreaming, Display, TEXT("-------------------------------------------------"));
	UE_LOG(LogStreaming, Display, TEXT("AsyncLoadingTime: %f"), AsyncLoadingTime);
	UE_LOG(LogStreaming, Display, TEXT("CreateAsyncPackagesFromQueueTime: %f"), CreateAsyncPackagesFromQueueTime);
	UE_LOG(LogStreaming, Display, TEXT("ProcessAsyncLoadingTime: %f"), ProcessAsyncLoadingTime);
	UE_LOG(LogStreaming, Display, TEXT("ProcessLoadedPackagesTime: %f"), ProcessLoadedPackagesTime);
	//UE_LOG(LogStreaming, Display, TEXT("SerializeTaggedPropertiesTime: %f"), SerializeTaggedPropertiesTime);
	UE_LOG(LogStreaming, Display, TEXT("CreateLinkerTime: %f"), CreateLinkerTime);
	UE_LOG(LogStreaming, Display, TEXT("FinishLinkerTime: %f"), FinishLinkerTime);
	UE_LOG(LogStreaming, Display, TEXT("CreateImportsTime: %f"), CreateImportsTime);
	UE_LOG(LogStreaming, Display, TEXT("CreateExportsTime: %f"), CreateExportsTime);
	UE_LOG(LogStreaming, Display, TEXT("PreLoadObjectsTime: %f"), PreLoadObjectsTime);
	UE_LOG(LogStreaming, Display, TEXT("PostLoadObjectsTime: %f"), PostLoadObjectsTime);
	UE_LOG(LogStreaming, Display, TEXT("PostLoadDeferredObjectsTime: %f"), PostLoadDeferredObjectsTime);
	UE_LOG(LogStreaming, Display, TEXT("FinishObjectsTime: %f"), FinishObjectsTime);
	UE_LOG(LogStreaming, Display, TEXT("MaterialPostLoad: %f"), MaterialPostLoad);
	UE_LOG(LogStreaming, Display, TEXT("MaterialInstancePostLoad: %f"), MaterialInstancePostLoad);
	UE_LOG(LogStreaming, Display, TEXT("SerializeInlineShaderMaps: %f"), SerializeInlineShaderMaps);
	UE_LOG(LogStreaming, Display, TEXT("MaterialSerializeTime: %f"), MaterialSerializeTime);
	UE_LOG(LogStreaming, Display, TEXT("MaterialInstanceSerializeTime: %f"), MaterialInstanceSerializeTime);
	UE_LOG(LogStreaming, Display, TEXT(""));
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_CreateLoader: %f"), LinkerLoad_CreateLoader);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializePackageFileSummary: %f"), LinkerLoad_SerializePackageFileSummary);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializeNameMap: %f"), LinkerLoad_SerializeNameMap);
	UE_LOG(LogStreaming, Display, TEXT("\tProcessingEntries: %f"), LinkerLoad_SerializeNameMap_ProcessingEntries);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializeGatherableTextDataMap: %f"), LinkerLoad_SerializeGatherableTextDataMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializeImportMap: %f"), LinkerLoad_SerializeImportMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializeExportMap: %f"), LinkerLoad_SerializeExportMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_FixupImportMap: %f"), LinkerLoad_FixupImportMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_FixupExportMap: %f"), LinkerLoad_FixupExportMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializeDependsMap: %f"), LinkerLoad_SerializeDependsMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializePreloadDependencies: %f"), LinkerLoad_SerializePreloadDependencies);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_CreateExportHash: %f"), LinkerLoad_CreateExportHash);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_FindExistingExports: %f"), LinkerLoad_FindExistingExports);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_FinalizeCreation: %f"), LinkerLoad_FinalizeCreation);

	UE_LOG(LogStreaming, Display, TEXT("Package_FinishLinker: %f"), Package_FinishLinker);
	UE_LOG(LogStreaming, Display, TEXT("Package_LoadImports: %f"), Package_LoadImports);
	UE_LOG(LogStreaming, Display, TEXT("Package_CreateImports: %f"), Package_CreateImports);
	UE_LOG(LogStreaming, Display, TEXT("Package_CreateLinker: %f"), Package_CreateLinker);
	UE_LOG(LogStreaming, Display, TEXT("Package_CreateExports: %f"), Package_CreateExports);
	UE_LOG(LogStreaming, Display, TEXT("Package_PreLoadObjects: %f"), Package_PreLoadObjects);
	UE_LOG(LogStreaming, Display, TEXT("Package_ExternalReadDependencies: %f"), Package_ExternalReadDependencies);
	UE_LOG(LogStreaming, Display, TEXT("Package_PostLoadObjects: %f"), Package_PostLoadObjects);
	UE_LOG(LogStreaming, Display, TEXT("Package_Tick: %f"), Package_Tick);
	UE_LOG(LogStreaming, Display, TEXT("Package_CreateAsyncPackagesFromQueue: %f"), Package_CreateAsyncPackagesFromQueue);
	UE_LOG(LogStreaming, Display, TEXT("Package_EventIOWait: %f"), Package_EventIOWait);

	UE_LOG(LogStreaming, Display, TEXT("TickAsyncLoading_ProcessLoadedPackages: %f"), TickAsyncLoading_ProcessLoadedPackages);

	UE_LOG(LogStreaming, Display, TEXT("Package_Temp1: %f"), Package_Temp1);
	UE_LOG(LogStreaming, Display, TEXT("Package_Temp2: %f"), Package_Temp2);
	UE_LOG(LogStreaming, Display, TEXT("Package_Temp3: %f"), Package_Temp3);
	UE_LOG(LogStreaming, Display, TEXT("Package_Temp4: %f"), Package_Temp4);

	UE_LOG(LogStreaming, Display, TEXT("Graph_AddNode: %f     %u"), Graph_AddNode, Graph_AddNodeCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_AddArc: %f     %u"), Graph_AddArc, Graph_AddArcCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_RemoveNode: %f     %u"), Graph_RemoveNode, Graph_RemoveNodeCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_RemoveNodeFire: %f     %u"), Graph_RemoveNodeFire, Graph_RemoveNodeFireCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_DoneAddingPrerequistesFireIfNone: %f     %u"), Graph_DoneAddingPrerequistesFireIfNone, Graph_DoneAddingPrerequistesFireIfNoneCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_DoneAddingPrerequistesFireIfNoneFire: %f     %u"), Graph_DoneAddingPrerequistesFireIfNoneFire, Graph_DoneAddingPrerequistesFireIfNoneFireCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_Misc: %f     %u"), Graph_Misc, Graph_MiscCnt);


	UE_LOG(LogStreaming, Display, TEXT("TickAsyncLoading_ProcessLoadedPackages: %f"), TickAsyncLoading_ProcessLoadedPackages);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializeNameMap_ProcessingEntries: %f"), LinkerLoad_SerializeNameMap_ProcessingEntries);
	UE_LOG(LogStreaming, Display, TEXT("FFileCacheHandle_AcquireSlotAndReadLine: %f"), FFileCacheHandle_AcquireSlotAndReadLine);
	UE_LOG(LogStreaming, Display, TEXT("FFileCacheHandle_PreloadData: %f"), FFileCacheHandle_PreloadData);
	UE_LOG(LogStreaming, Display, TEXT("FFileCacheHandle_ReadData: %f"), FFileCacheHandle_ReadData);
	UE_LOG(LogStreaming, Display, TEXT("FTypeLayoutDesc_Find: %f"), FTypeLayoutDesc_Find);
	UE_LOG(LogStreaming, Display, TEXT("FMemoryImageResult_ApplyPatchesFromArchive: %f"), FMemoryImageResult_ApplyPatchesFromArchive);
	UE_LOG(LogStreaming, Display, TEXT("LoadImports_Event: %f"), LoadImports_Event);
	UE_LOG(LogStreaming, Display, TEXT("StartPrecacheRequests: %f"), StartPrecacheRequests);
	UE_LOG(LogStreaming, Display, TEXT("MakeNextPrecacheRequestCurrent: %f"), MakeNextPrecacheRequestCurrent);
	UE_LOG(LogStreaming, Display, TEXT("FlushPrecacheBuffer: %f"), FlushPrecacheBuffer);
	UE_LOG(LogStreaming, Display, TEXT("ProcessImportsAndExports_Event: %f"), ProcessImportsAndExports_Event);
	UE_LOG(LogStreaming, Display, TEXT("CreateLinker_CreatePackage: %f"), CreateLinker_CreatePackage);
	UE_LOG(LogStreaming, Display, TEXT("CreateLinker_SetFlags: %f"), CreateLinker_SetFlags);
	UE_LOG(LogStreaming, Display, TEXT("CreateLinker_FindLinker: %f"), CreateLinker_FindLinker);
	UE_LOG(LogStreaming, Display, TEXT("CreateLinker_GetRedirectedName: %f"), CreateLinker_GetRedirectedName);
	UE_LOG(LogStreaming, Display, TEXT("CreateLinker_MassagePath: %f"), CreateLinker_MassagePath);
	UE_LOG(LogStreaming, Display, TEXT("CreateLinker_DoesExist: %f"), CreateLinker_DoesExist);
	UE_LOG(LogStreaming, Display, TEXT("CreateLinker_MissingPackage: %f"), CreateLinker_MissingPackage);
	UE_LOG(LogStreaming, Display, TEXT("CreateLinker_CreateLinkerAsync: %f"), CreateLinker_CreateLinkerAsync);
	UE_LOG(LogStreaming, Display, TEXT("FPackageName_DoesPackageExist: %f"), FPackageName_DoesPackageExist);
	UE_LOG(LogStreaming, Display, TEXT("PreLoadAndSerialize: %f"), PreLoadAndSerialize);
	UE_LOG(LogStreaming, Display, TEXT("PostLoad: %f"), PostLoad);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_ReconstructImportAndExportMap: %f"), LinkerLoad_ReconstructImportAndExportMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_PopulateInstancingContext: %f"), LinkerLoad_PopulateInstancingContext);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_VerifyImportInner: %f"), LinkerLoad_VerifyImportInner);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_LoadAllObjects: %f"), LinkerLoad_LoadAllObjects);
	UE_LOG(LogStreaming, Display, TEXT("UObject_Serialize: %f"), UObject_Serialize);
	UE_LOG(LogStreaming, Display, TEXT("BulkData_Serialize: %f"), BulkData_Serialize);
	UE_LOG(LogStreaming, Display, TEXT("BulkData_SerializeBulkData: %f"), BulkData_SerializeBulkData);
	UE_LOG(LogStreaming, Display, TEXT("EndLoad: %f"), EndLoad);
	UE_LOG(LogStreaming, Display, TEXT("FTextureReference_InitRHI: %f"), FTextureReference_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FShaderMapPointerTable_LoadFromArchive: %f"), FShaderMapPointerTable_LoadFromArchive);
	UE_LOG(LogStreaming, Display, TEXT("FShaderLibraryInstance_PreloadShaderMap: %f"), FShaderLibraryInstance_PreloadShaderMap);
	UE_LOG(LogStreaming, Display, TEXT("LoadShaderResource_Internal: %f"), LoadShaderResource_Internal);
	UE_LOG(LogStreaming, Display, TEXT("LoadShaderResource_AddOrDeleteResource: %f"), LoadShaderResource_AddOrDeleteResource);
	UE_LOG(LogStreaming, Display, TEXT("FShaderCodeLibrary_LoadResource: %f"), FShaderCodeLibrary_LoadResource);
	UE_LOG(LogStreaming, Display, TEXT("FMaterialShaderMapId_Serialize: %f"), FMaterialShaderMapId_Serialize);
	UE_LOG(LogStreaming, Display, TEXT("FMaterialShaderMapLayoutCache_CreateLayout: %f"), FMaterialShaderMapLayoutCache_CreateLayout);
	UE_LOG(LogStreaming, Display, TEXT("FMaterialShaderMap_IsComplete: %f"), FMaterialShaderMap_IsComplete);
	UE_LOG(LogStreaming, Display, TEXT("FMaterialShaderMap_Serialize: %f"), FMaterialShaderMap_Serialize);
	UE_LOG(LogStreaming, Display, TEXT("FMaterialResourceProxyReader_Initialize: %f"), FMaterialResourceProxyReader_Initialize);
	UE_LOG(LogStreaming, Display, TEXT("FSkeletalMeshVertexClothBuffer_InitRHI: %f"), FSkeletalMeshVertexClothBuffer_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FSkinWeightVertexBuffer_InitRHI: %f"), FSkinWeightVertexBuffer_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FStaticMeshVertexBuffer_InitRHI: %f"), FStaticMeshVertexBuffer_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FStreamableTextureResource_InitRHI: %f"), FStreamableTextureResource_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FShaderLibraryInstance_PreloadShader: %f"), FShaderLibraryInstance_PreloadShader);
	UE_LOG(LogStreaming, Display, TEXT("FShaderMapResource_SharedCode_InitRHI: %f"), FShaderMapResource_SharedCode_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FStaticMeshInstanceBuffer_InitRHI: %f"), FStaticMeshInstanceBuffer_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FInstancedStaticMeshVertexFactory_InitRHI: %f"), FInstancedStaticMeshVertexFactory_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FLocalVertexFactory_InitRHI: %f"), FLocalVertexFactory_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FLocalVertexFactory_InitRHI_CreateLocalVFUniformBuffer: %f"), FLocalVertexFactory_InitRHI_CreateLocalVFUniformBuffer);
	UE_LOG(LogStreaming, Display, TEXT("FSinglePrimitiveStructuredBuffer_InitRHI: %f"), FSinglePrimitiveStructuredBuffer_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FColorVertexBuffer_InitRHI: %f"), FColorVertexBuffer_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FFMorphTargetVertexInfoBuffers_InitRHI: %f"), FFMorphTargetVertexInfoBuffers_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("FSlateTexture2DRHIRef_InitDynamicRHI: %f"), FSlateTexture2DRHIRef_InitDynamicRHI);
	UE_LOG(LogStreaming, Display, TEXT("FLightmapResourceCluster_InitRHI: %f"), FLightmapResourceCluster_InitRHI);
	UE_LOG(LogStreaming, Display, TEXT("UMaterialExpression_Serialize: %f"), UMaterialExpression_Serialize);
	UE_LOG(LogStreaming, Display, TEXT("UMaterialExpression_PostLoad: %f"), UMaterialExpression_PostLoad);
	UE_LOG(LogStreaming, Display, TEXT("FSlateTextureRenderTarget2DResource_InitDynamicRHI: %f"), FSlateTextureRenderTarget2DResource_InitDynamicRHI);
	UE_LOG(LogStreaming, Display, TEXT("VerifyGlobalShaders: %f"), VerifyGlobalShaders);
	UE_LOG(LogStreaming, Display, TEXT("FLandscapeVertexBuffer_InitRHI: %f"), FLandscapeVertexBuffer_InitRHI);


	UE_LOG(LogStreaming, Display, TEXT("-------------------------------------------------"));

#endif

}

void FLoadTimeTracker::ResetRawLoadTimes()
{
#if ENABLE_LOADTIME_RAW_TIMINGS
	CreateAsyncPackagesFromQueueTime = 0.0;
	ProcessAsyncLoadingTime = 0.0;
	ProcessLoadedPackagesTime = 0.0;
	SerializeTaggedPropertiesTime = 0.0;
	CreateLinkerTime = 0.0;
	FinishLinkerTime = 0.0;
	CreateImportsTime = 0.0;
	CreateExportsTime = 0.0;
	PreLoadObjectsTime = 0.0;
	PostLoadObjectsTime = 0.0;
	PostLoadDeferredObjectsTime = 0.0;
	FinishObjectsTime = 0.0;
	MaterialPostLoad = 0.0;
	MaterialInstancePostLoad = 0.0;
	SerializeInlineShaderMaps = 0.0;
	MaterialSerializeTime = 0.0;
	MaterialInstanceSerializeTime = 0.0;
	AsyncLoadingTime = 0.0;
	CreateMetaDataTime = 0.0;

	LinkerLoad_CreateLoader = 0.0;
	LinkerLoad_SerializePackageFileSummary = 0.0;
	LinkerLoad_SerializeNameMap = 0.0;
	LinkerLoad_SerializeGatherableTextDataMap = 0.0;
	LinkerLoad_SerializeImportMap = 0.0;
	LinkerLoad_SerializeExportMap = 0.0;
	LinkerLoad_FixupImportMap = 0.0;
	LinkerLoad_FixupExportMap = 0.0;
	LinkerLoad_SerializeDependsMap = 0.0;
	LinkerLoad_SerializePreloadDependencies = 0.0;
	LinkerLoad_CreateExportHash = 0.0;
	LinkerLoad_FindExistingExports = 0.0;
	LinkerLoad_FinalizeCreation = 0.0;

	Package_FinishLinker = 0.0;
	Package_LoadImports = 0.0;
	Package_CreateImports = 0.0;
	Package_CreateLinker = 0.0;
	Package_CreateExports = 0.0;
	Package_PreLoadObjects = 0.0;
	Package_ExternalReadDependencies = 0.0;
	Package_PostLoadObjects = 0.0;
	Package_Tick = 0.0;
	Package_CreateAsyncPackagesFromQueue = 0.0;
	Package_CreateMetaData = 0.0;
	Package_EventIOWait = 0.0;

	Package_Temp1 = 0.0;
	Package_Temp2 = 0.0;
	Package_Temp3 = 0.0;
	Package_Temp4 = 0.0;

	Graph_AddNode = 0.0;
	Graph_AddNodeCnt = 0;

	Graph_AddArc = 0.0;
	Graph_AddArcCnt = 0;

	Graph_RemoveNode = 0.0;
	Graph_RemoveNodeCnt = 0;

	Graph_RemoveNodeFire = 0.0;
	Graph_RemoveNodeFireCnt = 0;

	Graph_DoneAddingPrerequistesFireIfNone = 0.0;
	Graph_DoneAddingPrerequistesFireIfNoneCnt = 0;

	Graph_DoneAddingPrerequistesFireIfNoneFire = 0.0;
	Graph_DoneAddingPrerequistesFireIfNoneFireCnt = 0;

	Graph_Misc = 0.0;
	Graph_MiscCnt = 0;

	TickAsyncLoading_ProcessLoadedPackages = 0.0;

	LinkerLoad_SerializeNameMap_ProcessingEntries = 0.0;

	FFileCacheHandle_AcquireSlotAndReadLine = 0.0;
	FFileCacheHandle_PreloadData = 0.0;
	FFileCacheHandle_ReadData = 0.0;

	FTypeLayoutDesc_Find = 0.0;

	FMemoryImageResult_ApplyPatchesFromArchive = 0.0;
	LoadImports_Event = 0.0;
	StartPrecacheRequests = 0.0;
	MakeNextPrecacheRequestCurrent = 0.0;
	FlushPrecacheBuffer = 0.0;
	ProcessImportsAndExports_Event = 0.0;
	CreateLinker_CreatePackage = 0.0;
	CreateLinker_SetFlags = 0.0;
	CreateLinker_FindLinker = 0.0;
	CreateLinker_GetRedirectedName = 0.0;
	CreateLinker_MassagePath = 0.0;
	CreateLinker_DoesExist = 0.0;
	CreateLinker_MissingPackage = 0.0;
	CreateLinker_CreateLinkerAsync = 0.0;
	FPackageName_DoesPackageExist = 0.0;
	PreLoadAndSerialize = 0.0;
	PostLoad = 0.0;
	LinkerLoad_ReconstructImportAndExportMap = 0.0;
	LinkerLoad_PopulateInstancingContext = 0.0;
	LinkerLoad_VerifyImportInner = 0.0;
	LinkerLoad_LoadAllObjects = 0.0;
	UObject_Serialize = 0.0;
	BulkData_Serialize = 0.0;
	BulkData_SerializeBulkData = 0.0;
	EndLoad = 0.0;
	FTextureReference_InitRHI = 0.0;
	FShaderMapPointerTable_LoadFromArchive = 0.0;
	FShaderLibraryInstance_PreloadShaderMap = 0.0;
	LoadShaderResource_Internal = 0.0;
	LoadShaderResource_AddOrDeleteResource = 0.0;
	FShaderCodeLibrary_LoadResource = 0.0;
	FMaterialShaderMapId_Serialize = 0.0;
	FMaterialShaderMapLayoutCache_CreateLayout = 0.0;
	FMaterialShaderMap_IsComplete = 0.0;
	FMaterialShaderMap_Serialize = 0.0;
	FMaterialResourceProxyReader_Initialize = 0.0;
	FSkeletalMeshVertexClothBuffer_InitRHI = 0.0;
	FSkinWeightVertexBuffer_InitRHI = 0.0;
	FStaticMeshVertexBuffer_InitRHI = 0.0;
	FStreamableTextureResource_InitRHI = 0.0;
	FShaderLibraryInstance_PreloadShader = 0.0;
	FShaderMapResource_SharedCode_InitRHI = 0.0;
	FStaticMeshInstanceBuffer_InitRHI = 0.0;
	FInstancedStaticMeshVertexFactory_InitRHI = 0.0;
	FLocalVertexFactory_InitRHI = 0.0;
	FLocalVertexFactory_InitRHI_CreateLocalVFUniformBuffer = 0.0;
	FSinglePrimitiveStructuredBuffer_InitRHI = 0.0;
	FColorVertexBuffer_InitRHI = 0.0;
	FFMorphTargetVertexInfoBuffers_InitRHI = 0.0;
	FSlateTexture2DRHIRef_InitDynamicRHI = 0.0;
	FLightmapResourceCluster_InitRHI = 0.0;
	UMaterialExpression_Serialize = 0.0;
	UMaterialExpression_PostLoad = 0.0;
	FSlateTextureRenderTarget2DResource_InitDynamicRHI = 0.0;
	VerifyGlobalShaders = 0.0;
	FLandscapeVertexBuffer_InitRHI = 0.0;

#endif

}

void FLoadTimeTracker::StartAccumulatedLoadTimes()
{
	bAccumulating = true;
	AccumulatedTimeInfo.Empty();
}

void FLoadTimeTracker::StopAccumulatedLoadTimes()
{
	bAccumulating = false;

	UE_LOG(LogLoad, Log, TEXT("------------- Accumulated Load times -------------"));
	
	for(auto Itr0 = AccumulatedTimeInfo.CreateConstIterator(); Itr0; ++Itr0)
	{
		double TotalTime = 0.0;
		uint64 TotalCount = 0;

		const FString KeyName = Itr0.Key().ToString();
		UE_LOG(LogLoad, Log, TEXT("------------- %s Times ------------"), *KeyName);
		UE_LOG(LogLoad, Log, TEXT("Name Time Count"));

		for(auto Itr1 = Itr0.Value().TimeInfo.CreateConstIterator(); Itr1; ++Itr1)
		{
			const FString InstanceName = Itr1.Key().ToString();
			const double& LoadTime = Itr1.Value().Time;
			const uint64& Count = Itr1.Value().Count;

			TotalTime += LoadTime;
			TotalCount += Count;
			UE_LOG(LogLoad, Log, TEXT("%s %f %llu"), *InstanceName, LoadTime, Count);
		}

		UE_LOG(LogLoad, Log, TEXT("Total%s %f %llu"), *KeyName, TotalTime, TotalCount);
		UE_LOG(LogLoad, Log, TEXT("------------------------------------"));
	}
}

static FAutoConsoleCommand LoadTimerDumpCmd(
	TEXT("LoadTimes.DumpTracking"),
	TEXT("Dump high level load times being tracked"),
	FConsoleCommandDelegate::CreateStatic(&FLoadTimeTracker::DumpHighLevelLoadTimesStatic)
	);
static FAutoConsoleCommand LoadTimerDumpLowCmd(
	TEXT("LoadTimes.DumpTrackingLow"),
	TEXT("Dump low level load times being tracked"),
	FConsoleCommandDelegate::CreateStatic(&FLoadTimeTracker::DumpRawLoadTimesStatic)
	);

static FAutoConsoleCommand LoadTimerResetCmd(
	TEXT("LoadTimes.ResetTracking"),
	TEXT("Reset load time tracking"),
	FConsoleCommandDelegate::CreateStatic(&FLoadTimeTracker::ResetRawLoadTimesStatic)
	);

static FAutoConsoleCommand AccumulatorTimerStartCmd(
	TEXT("LoadTimes.StartAccumulating"),
	TEXT("Starts capturing fine-grained accumulated load time data"),
	FConsoleCommandDelegate::CreateStatic(&FLoadTimeTracker::StartAccumulatedLoadTimesStatic)
	);

static FAutoConsoleCommand AccumulatorTimerStopCmd(
	TEXT("LoadTimes.StopAccumulating"),
	TEXT("Stops capturing fine-grained accumulated load time data and dump the results"),
	FConsoleCommandDelegate::CreateStatic(&FLoadTimeTracker::StopAccumulatedLoadTimesStatic)
	);
