// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Fbx/InterchangeFbxTranslator.h"

#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "InterchangeCommonAnimationPayload.h"
#include "InterchangeDispatcher.h"
#include "InterchangeDispatcherTask.h"
#include "InterchangeManager.h"
#include "InterchangeImportLog.h"
#include "Mesh/InterchangeMeshPayload.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/LargeMemoryReader.h"
#include "Texture/InterchangeTexturePayloadData.h"
#include "UObject/GCObjectScopeGuard.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeFbxTranslator)

static bool GInterchangeEnableFBXImport = false;
static FAutoConsoleVariableRef CCvarInterchangeEnableFBXImport(
	TEXT("Interchange.FeatureFlags.Import.FBX"),
	GInterchangeEnableFBXImport,
	TEXT("[Experimental] Whether FBX support is enabled."),
	ECVF_Default);

namespace UE::Interchange::Private
{
	void ApplyTranslatorMessage(const UInterchangeFbxTranslator* Translator, const FString& JsonMessage)
	{
		UInterchangeResult* InterchangeResult = UInterchangeResult::FromJson(JsonMessage);
		if (InterchangeResult)
		{
			//Downgrade warning message to display log when we are in automation
			if (GIsAutomationTesting && InterchangeResult->IsA(UInterchangeResultWarning::StaticClass()))
			{
				UE_LOG(LogInterchangeImport, Display, TEXT("%s"), *InterchangeResult->GetText().ToString());
			}
			else
			{
				Translator->AddMessage(InterchangeResult);
			}
		}
	}
} //ns UE::Interchange::Private

UInterchangeFbxTranslator::UInterchangeFbxTranslator()
{
	Dispatcher = nullptr;
}

EInterchangeTranslatorType UInterchangeFbxTranslator::GetTranslatorType() const
{
	return EInterchangeTranslatorType::Scenes;
}

EInterchangeTranslatorAssetType UInterchangeFbxTranslator::GetSupportedAssetTypes() const
{
	//fbx translator support Meshes, Materials and animation
	return EInterchangeTranslatorAssetType::Materials | EInterchangeTranslatorAssetType::Meshes | EInterchangeTranslatorAssetType::Animations;
}

TArray<FString> UInterchangeFbxTranslator::GetSupportedFormats() const
{
	if (GInterchangeEnableFBXImport || GIsAutomationTesting)
	{
		TArray<FString> Formats{ TEXT("fbx;Filmbox") };
		return Formats;
	}
	else
	{
		return TArray<FString>{};
	}
}

bool UInterchangeFbxTranslator::Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const
{
	FString Filename = GetSourceData()->GetFilename();
	if (!FPaths::FileExists(Filename))
	{
		return false;
	}

	if(!Dispatcher.IsValid())
	{
		//Dispatch an Interchange worker by using the InterchangeDispatcher
		//Build Result folder
		FGuid RandomGuid;
		FPlatformMisc::CreateGuid(RandomGuid);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString ProjectSavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		const FString RamdomGuidDir = RandomGuid.ToString(EGuidFormats::Base36Encoded);
		if (!PlatformFile.DirectoryExists(*ProjectSavedDir))
		{
			PlatformFile.CreateDirectory(*ProjectSavedDir);
		}
		const FString InterchangeDir = FPaths::Combine(ProjectSavedDir, TEXT("Interchange"));
		if (!PlatformFile.DirectoryExists(*InterchangeDir))
		{
			PlatformFile.CreateDirectory(*InterchangeDir);
		}
		const FString ResultFolder = FPaths::Combine(InterchangeDir, RamdomGuidDir);
		if (!PlatformFile.DirectoryExists(*ResultFolder))
		{
			PlatformFile.CreateDirectory(*ResultFolder);
		}

		//Create the dispatcher
		Dispatcher = MakeUnique<UE::Interchange::FInterchangeDispatcher>(ResultFolder);

		if(ensure(Dispatcher.IsValid()))
		{
			Dispatcher->StartProcess();
		}
	}

	if(!Dispatcher.IsValid())
	{
		return false;
	}

	//Create a json command to read the fbx file
	FString JsonCommand = CreateLoadFbxFileCommand(Filename);
	int32 TaskIndex = Dispatcher->AddTask(JsonCommand);

	//Blocking call until all tasks are executed
	Dispatcher->WaitAllTaskToCompleteExecution();
	
	FString WorkerFatalError = Dispatcher->GetInterchangeWorkerFatalError();
	if (!WorkerFatalError.IsEmpty())
	{
		AddMessage(UInterchangeResult::FromJson(WorkerFatalError));
	}

	UE::Interchange::ETaskState TaskState;
	FString JsonResult;
	TArray<FString> JsonMessages;
	Dispatcher->GetTaskState(TaskIndex, TaskState, JsonResult, JsonMessages);

	// Parse the Json messages into UInterchangeResults
	for (const FString& JsonMessage : JsonMessages)
	{
		UE::Interchange::Private::ApplyTranslatorMessage(this, JsonMessage);
	}

	if(TaskState != UE::Interchange::ETaskState::ProcessOk)
	{
		return false;
	}
	//Grab the result file and fill the BaseNodeContainer
	UE::Interchange::FJsonLoadSourceCmd::JsonResultParser ResultParser;
	ResultParser.FromJson(JsonResult);
	FString BaseNodeContainerFilename = ResultParser.GetResultFilename();

	//Parse the filename to fill the container
	BaseNodeContainer.LoadFromFile(BaseNodeContainerFilename);

	return true;
}

void UInterchangeFbxTranslator::ReleaseSource()
{
	if (Dispatcher.IsValid())
	{
		//Do not block the main thread
		Dispatcher->StopProcess(!IsInGameThread());
	}
}

void UInterchangeFbxTranslator::ImportFinish()
{
	if (Dispatcher.IsValid())
	{
		Dispatcher->TerminateProcess();
	}
}


TOptional<UE::Interchange::FImportImage> UInterchangeFbxTranslator::GetTexturePayloadData(const FString& PayLoadKey, TOptional<FString>& AlternateTexturePath) const
{
	UInterchangeSourceData* PayloadSourceData = UInterchangeManager::GetInterchangeManager().CreateSourceData(PayLoadKey);
	FGCObjectScopeGuard ScopedSourceData(PayloadSourceData);
	
	if (!PayloadSourceData)
	{
		return TOptional<UE::Interchange::FImportImage>();
	}
	UInterchangeTranslatorBase* SourceTranslator = UInterchangeManager::GetInterchangeManager().GetTranslatorForSourceData(PayloadSourceData);
	FGCObjectScopeGuard ScopedSourceTranslator(SourceTranslator);
	const IInterchangeTexturePayloadInterface* TextureTranslator = Cast<IInterchangeTexturePayloadInterface>(SourceTranslator);
	if (!ensure(TextureTranslator))
	{
		return TOptional<UE::Interchange::FImportImage>();
	}

	AlternateTexturePath = PayLoadKey;
	SourceTranslator->SetResultsContainer(Results);

	return TextureTranslator->GetTexturePayloadData(PayLoadKey, AlternateTexturePath);
}

TFuture<TOptional<UE::Interchange::FMeshPayloadData>> UInterchangeFbxTranslator::GetMeshPayloadData(const FInterchangeMeshPayLoadKey& PayLoadKey, const FTransform& MeshGlobalTransform) const
{
	TSharedPtr<TPromise<TOptional<UE::Interchange::FMeshPayloadData>>> Promise = MakeShared<TPromise<TOptional<UE::Interchange::FMeshPayloadData>>>();

	if (!Dispatcher.IsValid())
	{
		Promise->SetValue(TOptional<UE::Interchange::FMeshPayloadData>());
		return Promise->GetFuture();
	}

	// Create a json command to read the fbx file
	FString JsonCommand = CreateFetchMeshPayloadFbxCommand(PayLoadKey.UniqueId, MeshGlobalTransform);
	const int32 CreatedTaskIndex = Dispatcher->AddTask(JsonCommand, FInterchangeDispatcherTaskCompleted::CreateLambda([this, Promise, PayLoadKey](const int32 TaskIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE("UInterchangeFbxTranslator::GetStaticMeshPayloadData::Dispatcher->AddTaskDone")
		UE::Interchange::ETaskState TaskState;
		FString JsonResult;
		TArray<FString> JsonMessages;
		Dispatcher->GetTaskState(TaskIndex, TaskState, JsonResult, JsonMessages);

		// Parse the Json messages into UInterchangeResults
		for (const FString& JsonMessage : JsonMessages)
		{
			UE::Interchange::Private::ApplyTranslatorMessage(this, JsonMessage);
		}

		if (TaskState != UE::Interchange::ETaskState::ProcessOk)
		{
			Promise->SetValue(TOptional<UE::Interchange::FMeshPayloadData>());
			return;
		}

		// Grab the result file and fill the BaseNodeContainer
		UE::Interchange::FJsonFetchMeshPayloadCmd::JsonResultParser ResultParser;
		ResultParser.FromJson(JsonResult);
		FString StaticMeshPayloadFilename = ResultParser.GetResultFilename();

		//Mesh payload file generation can fail due to invalid Mesh (for ep No Polygons/Only Degenerate Polygons)
		if (!FPaths::FileExists(StaticMeshPayloadFilename))
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Expected mesh payload file does not exist for PayloadKey: %s"), *PayLoadKey.UniqueId);

			Promise->SetValue(TOptional<UE::Interchange::FMeshPayloadData>());
			return;
		}

		UE::Interchange::FMeshPayloadData MeshPayloadData;
		MeshPayloadData.MeshDescription.Empty();

		// All sub object should be gone with the reset
		TArray64<uint8> Buffer;
		FFileHelper::LoadFileToArray(Buffer, *StaticMeshPayloadFilename);
		uint8* FileData = Buffer.GetData();
		int64 FileDataSize = Buffer.Num();
		if (FileDataSize < 1)
		{
			// Nothing to load from this file
			Promise->SetValue(TOptional<UE::Interchange::FMeshPayloadData>());
			return;
		}

		switch (PayLoadKey.Type)
		{
		case EInterchangeMeshPayLoadType::STATIC:
		case EInterchangeMeshPayLoadType::SKELETAL:
			{
				// Buffer keeps the ownership of the data, the large memory reader is use to serialize the TMap
				FLargeMemoryReader Ar(FileData, FileDataSize);
				MeshPayloadData.MeshDescription.Serialize(Ar);

				// This is a static mesh payload can contain skinned data if we need to convert skeletalmesh to staticmesh
				bool bFetchSkinnedData = false;
				Ar << bFetchSkinnedData;
				if (bFetchSkinnedData)
				{
					Ar << MeshPayloadData.JointNames;
				}
			}
			break;
		case EInterchangeMeshPayLoadType::MORPHTARGET:
			{
				//Buffer keep the ownership of the data, the large memory reader is use to serialize the TMap
				FLargeMemoryReader Ar(FileData, FileDataSize);
				MeshPayloadData.MeshDescription.Serialize(Ar);
			}
			break;
		case EInterchangeMeshPayLoadType::NONE:
		default:
			break;
		}

		
		Promise->SetValue(MoveTemp(MeshPayloadData));
	}));

	// The task was not added to the dispatcher
	if (CreatedTaskIndex == INDEX_NONE)
	{
		Promise->SetValue(TOptional<UE::Interchange::FMeshPayloadData>{});
	}

	return Promise->GetFuture();
}

TFuture<TOptional<UE::Interchange::FAnimationPayloadData>> UInterchangeFbxTranslator::GetAnimationPayloadData(const FInterchangeAnimationPayLoadKey& PayLoadKey, const double BakeFrequency, const double RangeStartSecond, const double RangeStopSecond) const
{
	TSharedPtr<TPromise<TOptional<UE::Interchange::FAnimationPayloadData>>> Promise = MakeShared<TPromise<TOptional<UE::Interchange::FAnimationPayloadData>>>();

	if (!Dispatcher.IsValid())
	{
		Promise->SetValue(TOptional<UE::Interchange::FAnimationPayloadData>());
		return Promise->GetFuture();
	}

	// Create a json command to read the fbx file
	FString JsonCommand = 
		(PayLoadKey.Type == EInterchangeAnimationPayLoadType::BAKED)
		? CreateFetchAnimationBakeTransformPayloadFbxCommand(PayLoadKey.UniqueId, BakeFrequency, RangeStartSecond, RangeStopSecond)
		: CreateFetchPayloadFbxCommand(PayLoadKey.UniqueId);

	const int32 CreatedTaskIndex = Dispatcher->AddTask(JsonCommand, FInterchangeDispatcherTaskCompleted::CreateLambda([this, Promise, PayLoadKey](const int32 TaskIndex)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE("UInterchangeFbxTranslator::GetAnimationCurvePayloadData::Dispatcher->AddTaskDone")
			UE::Interchange::ETaskState TaskState;
			FString JsonResult;
			TArray<FString> JsonMessages;
			Dispatcher->GetTaskState(TaskIndex, TaskState, JsonResult, JsonMessages);

			// Parse the Json messages into UInterchangeResults
			for (const FString& JsonMessage : JsonMessages)
			{
				UE::Interchange::Private::ApplyTranslatorMessage(this, JsonMessage);
			}

			if (TaskState != UE::Interchange::ETaskState::ProcessOk)
			{
				Promise->SetValue(TOptional<UE::Interchange::FAnimationPayloadData>());
				return;
			}

			// Grab the result file and fill the BaseNodeContainer
			UE::Interchange::FJsonFetchPayloadCmd::JsonResultParser ResultParser;
			ResultParser.FromJson(JsonResult);
			FString AnimationTransformPayloadFilename = ResultParser.GetResultFilename();

			if (!ensure(FPaths::FileExists(AnimationTransformPayloadFilename)))
			{
				// TODO log an error saying the payload file does not exist even if the get payload command succeeded
				Promise->SetValue(TOptional<UE::Interchange::FAnimationPayloadData>());
				return;
			}

			UE::Interchange::FAnimationPayloadData AnimationTransformPayload(PayLoadKey.Type);

			// All sub object should be gone with the reset
			TArray64<uint8> Buffer;
			FFileHelper::LoadFileToArray(Buffer, *AnimationTransformPayloadFilename);
			uint8* FileData = Buffer.GetData();
			int64 FileDataSize = Buffer.Num();
			if (FileDataSize < 1)
			{
				// Nothing to load from this file
				Promise->SetValue(TOptional<UE::Interchange::FAnimationPayloadData>());
				return;
			}

			// Buffer keeps the ownership of the data, the large memory reader is use to serialize the TMap
			FLargeMemoryReader Ar(FileData, FileDataSize);

			switch (PayLoadKey.Type)
			{
			case EInterchangeAnimationPayLoadType::CURVE:
			case EInterchangeAnimationPayLoadType::MORPHTARGETCURVE:
				{
					TArray<FInterchangeCurve> InterchangeCurves;
					Ar << InterchangeCurves;
					AnimationTransformPayload.Curves.AddDefaulted(InterchangeCurves.Num());
					for (int32 CurveIndex = 0; CurveIndex < InterchangeCurves.Num(); ++CurveIndex)
					{
						const FInterchangeCurve& InterchangeCurve = InterchangeCurves[CurveIndex];
						InterchangeCurve.ToRichCurve(AnimationTransformPayload.Curves[CurveIndex]);
					}
				}
				break;
			case EInterchangeAnimationPayLoadType::STEPCURVE:
				{
					Ar << AnimationTransformPayload.StepCurves;
				}
				break;
			case EInterchangeAnimationPayLoadType::BAKED:
				AnimationTransformPayload.SerializeBaked(Ar);
				break;
			case EInterchangeAnimationPayLoadType::NONE:
			default:
				break;
			}

			Promise->SetValue(MoveTemp(AnimationTransformPayload));
		}));

	// The task was not added to the dispatcher
	if (CreatedTaskIndex == INDEX_NONE)
	{
		Promise->SetValue(TOptional<UE::Interchange::FAnimationPayloadData>{});
	}

	return Promise->GetFuture();
}

FString UInterchangeFbxTranslator::CreateLoadFbxFileCommand(const FString& FbxFilePath) const
{
	UE::Interchange::FJsonLoadSourceCmd LoadSourceCommand(TEXT("FBX"), FbxFilePath);
	return LoadSourceCommand.ToJson();
}

FString UInterchangeFbxTranslator::CreateFetchPayloadFbxCommand(const FString& FbxPayloadKey) const
{
	UE::Interchange::FJsonFetchPayloadCmd PayloadCommand(TEXT("FBX"), FbxPayloadKey);
	return PayloadCommand.ToJson();
}

FString UInterchangeFbxTranslator::CreateFetchMeshPayloadFbxCommand(const FString& FbxPayloadKey, const FTransform& MeshGlobalTransform) const
{
	UE::Interchange::FJsonFetchMeshPayloadCmd PayloadCommand(TEXT("FBX"), FbxPayloadKey, MeshGlobalTransform);
	return PayloadCommand.ToJson();
}

FString UInterchangeFbxTranslator::CreateFetchAnimationBakeTransformPayloadFbxCommand(const FString& FbxPayloadKey, const double BakeFrequency, const double RangeStartTime, const double RangeEndTime) const
{
	UE::Interchange::FJsonFetchAnimationBakeTransformPayloadCmd PayloadCommand(TEXT("FBX"), FbxPayloadKey, BakeFrequency, RangeStartTime, RangeEndTime);
	return PayloadCommand.ToJson();
}

