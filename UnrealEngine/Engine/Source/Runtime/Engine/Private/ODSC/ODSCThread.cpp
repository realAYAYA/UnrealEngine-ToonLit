// Copyright Epic Games, Inc. All Rights Reserved.

#include "ODSCThread.h"
#include "CookOnTheFly.h"
#include "ODSCLog.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"

FODSCRequestPayload::FODSCRequestPayload(
	EShaderPlatform InShaderPlatform,
	ERHIFeatureLevel::Type InFeatureLevel,
	EMaterialQualityLevel::Type InQualityLevel,
	const FString& InMaterialName,
	const FString& InVertexFactoryName,
	const FString& InPipelineName,
	const TArray<FString>& InShaderTypeNames,
	int32 InPermutationId,
	const FString& InRequestHash
)
: ShaderPlatform(InShaderPlatform)
, FeatureLevel(InFeatureLevel)
, QualityLevel(InQualityLevel)
, MaterialName(InMaterialName)
, VertexFactoryName(InVertexFactoryName)
, PipelineName(InPipelineName)
, ShaderTypeNames(std::move(InShaderTypeNames))
, PermutationId(InPermutationId)
, RequestHash(InRequestHash)
{

}

FODSCMessageHandler::FODSCMessageHandler(EShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type InQualityLevel, ODSCRecompileCommand InRecompileCommandType)
:	ShaderPlatform(InShaderPlatform),
	FeatureLevel(InFeatureLevel),
	QualityLevel(InQualityLevel),
	RecompileCommandType(InRecompileCommandType)
{
}

FODSCMessageHandler::FODSCMessageHandler(const TArray<FString>& InMaterials, const FString& ShaderTypesToLoad, EShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type InQualityLevel, ODSCRecompileCommand InRecompileCommandType) :
	MaterialsToLoad(std::move(InMaterials)),
	ShaderTypesToLoad(ShaderTypesToLoad),
	ShaderPlatform(InShaderPlatform),
	FeatureLevel(InFeatureLevel),
	QualityLevel(InQualityLevel),
	RecompileCommandType(InRecompileCommandType)
{
}

void FODSCMessageHandler::FillPayload(FArchive& Payload)
{
	// When did we start this request?
	RequestStartTime = FPlatformTime::Seconds();

	int32 ConvertedShaderPlatform = static_cast<int32>(ShaderPlatform);
	int32 ConvertedFeatureLevel = static_cast<int32>(FeatureLevel);
	int32 ConvertedQualityLevel = static_cast<int32>(QualityLevel);

	Payload << MaterialsToLoad;
	Payload << ShaderTypesToLoad;
	Payload << ConvertedShaderPlatform;
	Payload << ConvertedFeatureLevel;
	Payload << ConvertedQualityLevel;
	Payload << RecompileCommandType;
	Payload << RequestBatch;
}

void FODSCMessageHandler::ProcessResponse(FArchive& Response)
{
	UE_LOG(LogODSC, Display, TEXT("Received response in %lf seconds."), FPlatformTime::Seconds() - RequestStartTime);

	// pull back the compiled mesh material data (if any)
	Response << OutMeshMaterialMaps;
	Response << OutGlobalShaderMap;
}

void FODSCMessageHandler::AddPayload(const FODSCRequestPayload& Payload)
{
	RequestBatch.Add(Payload);
}

const TArray<FString>& FODSCMessageHandler::GetMaterialsToLoad() const
{
	return MaterialsToLoad;
}

const TArray<uint8>& FODSCMessageHandler::GetMeshMaterialMaps() const
{
	return OutMeshMaterialMaps;
}

const TArray<uint8>& FODSCMessageHandler::GetGlobalShaderMap() const
{
	return OutGlobalShaderMap;
}

bool FODSCMessageHandler::ReloadGlobalShaders() const
{
	return RecompileCommandType == ODSCRecompileCommand::Global;
}

FODSCThread::FODSCThread(const FString& HostIP)
	: Thread(nullptr),
	  WakeupEvent(FPlatformProcess::GetSynchEventFromPool(true))
{
	UE_LOG(LogODSC, Log, TEXT("ODSC Thread active."));

	// Attempt to get a default connection to the COTF server (which cooks assets).
	UE::Cook::ICookOnTheFlyModule& CookOnTheFlyModule = FModuleManager::LoadModuleChecked<UE::Cook::ICookOnTheFlyModule>(TEXT("CookOnTheFly"));
	if (!CookOnTheFlyModule.GetDefaultServerConnection())
	{
		// If we don't have a default connection make a specific connection to the HostIP provided.
		UE::Cook::FCookOnTheFlyHostOptions CookOnTheFlyHostOptions;
		CookOnTheFlyHostOptions.Hosts.Add(HostIP);
		CookOnTheFlyServerConnection = CookOnTheFlyModule.ConnectToServer(CookOnTheFlyHostOptions);
		if (!CookOnTheFlyServerConnection)
		{
			UE_LOG(LogODSC, Warning, TEXT("Failed to connect to cook on the fly server."));
		}
	}
}

FODSCThread::~FODSCThread()
{
	StopThread();

	FPlatformProcess::ReturnSynchEventToPool(WakeupEvent);
	WakeupEvent = nullptr;
}

void FODSCThread::StartThread()
{
	Thread = FRunnableThread::Create(this, TEXT("ODSCThread"), 128 * 1024, TPri_Normal);
}

void FODSCThread::StopThread()
{
	if (Thread != nullptr)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}
}

void FODSCThread::Tick()
{
	Process();
}

void FODSCThread::AddRequest(const TArray<FString>& MaterialsToCompile, const FString& ShaderTypesToLoad, EShaderPlatform ShaderPlatform, ERHIFeatureLevel::Type FeatureLevel, EMaterialQualityLevel::Type QualityLevel, ODSCRecompileCommand RecompileCommandType)
{
	PendingMaterialThreadedRequests.Enqueue(new FODSCMessageHandler(MaterialsToCompile, ShaderTypesToLoad, ShaderPlatform, FeatureLevel, QualityLevel, RecompileCommandType));
}

void FODSCThread::AddShaderPipelineRequest(
	EShaderPlatform ShaderPlatform,
	ERHIFeatureLevel::Type FeatureLevel,
	EMaterialQualityLevel::Type QualityLevel,
	const FString& MaterialName,
	const FString& VertexFactoryName,
	const FString& PipelineName,
	const TArray<FString>& ShaderTypeNames,
	int32 PermutationId
)
{
	// TODO: Requests for individual permutations come in here, but a single coalesced payload is submitted to the server since 
	// we compile all material shader permutations encountered for the moment. Consider batching up requested permutations and 
	// have the server skip compiling those not in the list. Ensure that DDC key and shader map assumptions are correct!

	FString RequestString = (MaterialName + VertexFactoryName + PipelineName);
	for (const auto& ShaderTypeName : ShaderTypeNames)
	{
		RequestString += ShaderTypeName;
	}
	const FString RequestHash = FMD5::HashAnsiString(*RequestString);

	FScopeLock Lock(&RequestHashCriticalSection);
	if (!RequestHashes.Contains(RequestHash))
	{
		PendingMeshMaterialThreadedRequests.Enqueue(FODSCRequestPayload(ShaderPlatform, FeatureLevel, QualityLevel, MaterialName, VertexFactoryName, PipelineName, ShaderTypeNames, PermutationId, RequestHash));
		RequestHashes.Add(RequestHash);
	}
}

void FODSCThread::GetCompletedRequests(TArray<FODSCMessageHandler*>& OutCompletedRequests)
{
	check(IsInGameThread());
	FODSCMessageHandler* Request = nullptr;
	while (CompletedThreadedRequests.Dequeue(Request))
	{
		OutCompletedRequests.Add(Request);
	}
}

void FODSCThread::Wakeup()
{
	WakeupEvent->Trigger();
}

bool FODSCThread::Init()
{
	return true;
}

uint32 FODSCThread::Run()
{
	while (!ExitRequest.GetValue())
	{
		if (WakeupEvent->Wait())
		{
			Process();
		}
	}
	return 0;
}

void FODSCThread::Stop()
{
	ExitRequest.Set(true);
	WakeupEvent->Trigger();
}

void FODSCThread::Exit()
{

}

void FODSCThread::Process()
{
	// cache all pending requests.
	FODSCRequestPayload Payload;
	TArray<FODSCRequestPayload> PayloadsToAggregate;
	{
		FScopeLock Lock(&RequestHashCriticalSection);
		while (PendingMeshMaterialThreadedRequests.Dequeue(Payload))
		{
			PayloadsToAggregate.Add(Payload);
			int FoundIndex = INDEX_NONE;
			if (RequestHashes.Find(Payload.RequestHash, FoundIndex))
			{
				RequestHashes.RemoveAt(FoundIndex);
			}
		}
	}

	// cache material requests.
	FODSCMessageHandler* Request = nullptr;
	TArray<FODSCMessageHandler*> RequestsToStart;
	while (PendingMaterialThreadedRequests.Dequeue(Request))
	{
		RequestsToStart.Add(Request);
	}

	// process any material or recompile change shader requests or global shader compile requests.
	for (FODSCMessageHandler* NextRequest : RequestsToStart)
	{
		// send the info, the handler will process the response (and update shaders, etc)
		SendMessageToServer(NextRequest);

		CompletedThreadedRequests.Enqueue(NextRequest);
	}

	// process any specific mesh material shader requests.
	if (PayloadsToAggregate.Num())
	{
		FODSCMessageHandler* RequestHandler = new FODSCMessageHandler(PayloadsToAggregate[0].ShaderPlatform, PayloadsToAggregate[0].FeatureLevel, PayloadsToAggregate[0].QualityLevel, ODSCRecompileCommand::Material);
		for (const FODSCRequestPayload& payload : PayloadsToAggregate)
		{
			RequestHandler->AddPayload(payload);
		}

		// send the info, the handler will process the response (and update shaders, etc)
		SendMessageToServer(RequestHandler);

		CompletedThreadedRequests.Enqueue(RequestHandler);
	}

	WakeupEvent->Reset();
}

void FODSCThread::SendMessageToServer(IPlatformFile::IFileServerMessageHandler* Handler)
{
	// If we have a default connection that already exists, send directly to that.
	if ((CookOnTheFlyServerConnection == nullptr) || (!CookOnTheFlyServerConnection->IsConnected()))
	{
		IFileManager::Get().SendMessageToServer(TEXT("RecompileShaders"), Handler);
		return;
	}

	// We don't have a default COTF connection so use our specific connection to send our command.
	UE::Cook::FCookOnTheFlyRequest Request(UE::Cook::ECookOnTheFlyMessage::RecompileShaders);
	{
		TUniquePtr<FArchive> Ar = Request.WriteBody();
		Handler->FillPayload(*Ar);
	}

	UE::Cook::FCookOnTheFlyResponse Response = CookOnTheFlyServerConnection->SendRequest(Request).Get();
	if (Response.IsOk())
	{
		TUniquePtr<FArchive> Ar = Response.ReadBody();
		Handler->ProcessResponse(*Ar);
	}

	check(Response.IsOk());
}
