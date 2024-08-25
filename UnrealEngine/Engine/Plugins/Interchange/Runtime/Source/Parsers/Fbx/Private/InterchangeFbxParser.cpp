// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeFbxParser.h"

#include "CoreMinimal.h"
#include "FbxAPI.h"
#include "HAL/CriticalSection.h"
#include "InterchangeTextureNode.h"
#include "Misc/ScopeLock.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "InterchangeFbxParser"

namespace UE::Interchange
{
	namespace Private
	{
		FString HashString(const FString& String)
		{
			TArray<uint8> TempBytes;
			TempBytes.Reserve(64);
			//The archive is flagged as persistent so that machines of different endianness produce identical binary results.
			FMemoryWriter Ar(TempBytes, /*bIsPersistent=*/ true);
			//Hack because serialization do not support const
			FString& NonConst = *const_cast<FString*>(&String);
			Ar << NonConst;
			FSHA1 Sha;
			Sha.Update(TempBytes.GetData(), TempBytes.Num() * TempBytes.GetTypeSize());
			Sha.Final();
			// Retrieve the hash and use it to construct a pseudo-GUID.
			uint32 Hash[5];
			Sha.GetHash((uint8*)Hash);
			FGuid Guid = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);
			return Guid.ToString(EGuidFormats::Base36Encoded);
		}
	}

	FInterchangeFbxParser::FInterchangeFbxParser()
	{
		ResultsContainer.Reset(NewObject<UInterchangeResultsContainer>(GetTransientPackage()));
		FbxParserPrivate = MakeUnique<Private::FFbxParser>(ResultsContainer.Get());
	}
	FInterchangeFbxParser::~FInterchangeFbxParser()
	{
		ReleaseResources();
	}

	void FInterchangeFbxParser::ReleaseResources()
	{
		ResultsContainer = nullptr;
		FbxParserPrivate = nullptr;
	}

	void FInterchangeFbxParser::Reset()
	{
		ResultPayloads.Reset();
		FbxParserPrivate->Reset();
	}

	void FInterchangeFbxParser::SetResultContainer(UInterchangeResultsContainer* Result)
	{
		InternalResultsContainer = Result;
		FbxParserPrivate->SetResultContainer(Result);
	}

	void FInterchangeFbxParser::SetConvertSettings(const bool InbConvertScene, const bool InbForceFrontXAxis, const bool InbConvertSceneUnit)
	{
		FbxParserPrivate->SetConvertSettings(InbConvertScene, InbForceFrontXAxis, InbConvertSceneUnit);
	}

	void FInterchangeFbxParser::LoadFbxFile(const FString& Filename, const FString& ResultFolder)
	{
		check(FbxParserPrivate.IsValid());
		SourceFilename = Filename;
		ResultsContainer->Empty();

		if (!FbxParserPrivate->LoadFbxFile(Filename))
		{
			if (UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>())
			{
				Error->SourceAssetName = SourceFilename;
				Error->Text = LOCTEXT("CantLoadFbxFile", "Cannot load the FBX file.");
			}
			return;
		}

		ResultFilepath = ResultFolder + TEXT("/SceneDescription.itc");
		//Since we are not in main thread we cannot use TStrongPtr, so we will add the object to the root and remove it when we are done
		UInterchangeBaseNodeContainer* Container = NewObject<UInterchangeBaseNodeContainer>(GetTransientPackage(), NAME_None);
		if (!ensure(Container != nullptr))
		{
			if (UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>())
			{
				Error->SourceAssetName = SourceFilename;
				Error->Text = LOCTEXT("CantAllocate", "Cannot allocate base node container to add FBX scene data.");
			}
			return;
		}

		Container->AddToRoot();
		FbxParserPrivate->FillContainerWithFbxScene(*Container);
		Container->SaveToFile(ResultFilepath);
		Container->RemoveFromRoot();
	}

	void FInterchangeFbxParser::LoadFbxFile(const FString& Filename, UInterchangeBaseNodeContainer& BaseNodecontainer)
	{
		SourceFilename = Filename;
		if (!ensure(FbxParserPrivate.IsValid()))
		{
			if (UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>())
			{
				Error->SourceAssetName = SourceFilename;
				Error->Text = LOCTEXT("CantLoadFbxFile_ParserInvalid", "FInterchangeFbxParser::LoadFbxFile: Cannot load the FBX file. The internal fbx parser is invalid.");
			}
			return;
		}
		
		if (!FbxParserPrivate->LoadFbxFile(Filename))
		{
			if (UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>())
			{
				Error->SourceAssetName = SourceFilename;
				Error->Text = LOCTEXT("CantLoadFbxFile_ParserError", "FInterchangeFbxParser::LoadFbxFile: Cannot load the FBX file. There was an error when parsing the file.");
			}
			return;
		}
		FbxParserPrivate->FillContainerWithFbxScene(BaseNodecontainer);
	}

	void FInterchangeFbxParser::FetchPayload(const FString& PayloadKey, const FString& ResultFolder)
	{
		ResultsContainer->Empty();
		if (!ensure(FbxParserPrivate.IsValid()))
		{
			if (UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>())
			{
				Error->SourceAssetName = PayloadKey;
				Error->Text = LOCTEXT("CantFetchPayload_ParserInvalid", "FInterchangeFbxParser::FetchPayload: Cannot fetch the payload. The internal fbx parser is invalid.");
			}
			return;
		}
		
		FString PayloadFilepathCopy;
		{
			FScopeLock Lock(&ResultPayloadsCriticalSection);
			FString& PayloadFilepath = ResultPayloads.FindOrAdd(PayloadKey);
			//To avoid file path with too many character, we hash the payloadKey so we have a deterministic length for the file path.
			FString PayloadKeyHash = Private::HashString(PayloadKey);
			PayloadFilepath = ResultFolder + TEXT("/") + PayloadKeyHash + FString::FromInt(UniqueIdCounter.IncrementExchange()) + TEXT(".payload");

			//Copy the map filename key because we are multithreaded and the TMap can be reallocated
			PayloadFilepathCopy = PayloadFilepath;
		}
		if (!FbxParserPrivate->FetchPayloadData(PayloadKey, PayloadFilepathCopy))
		{
			if (UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>())
			{
				Error->SourceAssetName = SourceFilename;
				Error->Text = LOCTEXT("CantFetchPayload", "Cannot fetch FBX payload data.");
			}
			return;
		}
	}

	FString FInterchangeFbxParser::FetchMeshPayload(const FString& PayloadKey, const FTransform& MeshGlobalTransform, const FString& ResultFolder)
	{
		ResultsContainer->Empty();
		FString PayloadFilepathCopy;
		FString ResultPayloadUniqueId = PayloadKey + MeshGlobalTransform.ToString();
		if (!ensure(FbxParserPrivate.IsValid()))
		{
			if (UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>())
			{
				Error->SourceAssetName = PayloadKey;
				Error->Text = LOCTEXT("CantFetchMeshPayload_ParserInvalid", "FInterchangeFbxParser::FetchMeshPayload: Cannot fetch the mesh payload. The internal fbx parser is invalid.");
			}
			return ResultPayloadUniqueId;
		}
		
		//If we already have extract this mesh, no need to extract again
		if (ResultPayloads.Contains(ResultPayloadUniqueId))
		{
			return ResultPayloadUniqueId;
		}

		{
			FScopeLock Lock(&ResultPayloadsCriticalSection);
			FString& PayloadFilepath = ResultPayloads.FindOrAdd(ResultPayloadUniqueId);
			//To avoid file path with too many character, we hash the payloadKey so we have a deterministic length for the file path.
			FString PayloadKeyHash = Private::HashString(PayloadKey);
			PayloadFilepath = ResultFolder + TEXT("/") + PayloadKeyHash + FString::FromInt(UniqueIdCounter.IncrementExchange()) + TEXT(".payload");

			//Copy the map filename key because we are multithreaded and the TMap can be reallocated
			PayloadFilepathCopy = PayloadFilepath;
		}
		if (!FbxParserPrivate->FetchMeshPayloadData(PayloadKey, MeshGlobalTransform, PayloadFilepathCopy))
		{
			if (UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>())
			{
				Error->SourceAssetName = SourceFilename;
				Error->Text = LOCTEXT("CantFetchPayload", "Cannot fetch FBX payload data.");
			}
		}
		return ResultPayloadUniqueId;
	}

#if WITH_ENGINE
	void FInterchangeFbxParser::FetchMeshPayload(const FString& PayloadKey, const FTransform& MeshGlobalTransform, FMeshPayloadData& OutMeshPayloadData)
	{
		if (!FbxParserPrivate->FetchMeshPayloadData(PayloadKey, MeshGlobalTransform, OutMeshPayloadData))
		{
			if (UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>())
			{
				Error->SourceAssetName = SourceFilename;
				Error->Text = LOCTEXT("CantFetchPayload", "Cannot fetch FBX payload data.");
			}
		}
	}
#endif

	FString FInterchangeFbxParser::FetchAnimationBakeTransformPayload(const FString& PayloadKey, const double BakeFrequency, const double RangeStartTime, const double RangeEndTime, const FString& ResultFolder)
	{
		ResultsContainer->Empty();
		FString PayloadFilepathCopy;
		FString ResultPayloadUniqueId = PayloadKey + FString::FromInt(static_cast<int32>(BakeFrequency * 1000.0)) + FString::FromInt(static_cast<int32>(RangeStartTime * 1000.0)) + FString::FromInt(static_cast<int32>(RangeEndTime * 1000.0));

		if (!ensure(FbxParserPrivate.IsValid()))
		{
			if (UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>())
			{
				Error->SourceAssetName = PayloadKey;
				Error->Text = LOCTEXT("CantFetchAnimationBakeTransformPayload_ParserInvalid", "FInterchangeFbxParser::FetchAnimationBakeTransformPayload: Cannot fetch the animation bake transform payload. The internal fbx parser is invalid.");
			}
			return ResultPayloadUniqueId;
		}
		
		//If we already have extract this mesh, no need to extract again
		if (ResultPayloads.Contains(ResultPayloadUniqueId))
		{
			return ResultPayloadUniqueId;
		}

		{
			FScopeLock Lock(&ResultPayloadsCriticalSection);
			FString& PayloadFilepath = ResultPayloads.FindOrAdd(ResultPayloadUniqueId);
			//To avoid file path with too many character, we hash the payloadKey so we have a deterministic length for the file path.
			FString PayloadKeyHash = Private::HashString(PayloadKey);
			PayloadFilepath = ResultFolder + TEXT("/") + PayloadKeyHash + FString::FromInt(UniqueIdCounter.IncrementExchange()) + TEXT(".payload");

			//Copy the map filename key because we are multithreaded and the TMap can be reallocated
			PayloadFilepathCopy = PayloadFilepath;
		}
		if (!FbxParserPrivate->FetchAnimationBakeTransformPayload(PayloadKey, BakeFrequency, RangeStartTime, RangeEndTime, PayloadFilepathCopy))
		{
			if (UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>())
			{
				Error->SourceAssetName = SourceFilename;
				Error->Text = LOCTEXT("CantFetchPayload", "Cannot fetch FBX payload data.");
			}
		}
		return ResultPayloadUniqueId;
	}

	TArray<FString> FInterchangeFbxParser::GetJsonLoadMessages() const
	{
		TArray<FString> JsonResults;
		for (UInterchangeResult* Result : GetResultContainer()->GetResults())
		{
			JsonResults.Add(Result->ToJson());
		}

		return JsonResults;
	}

	UInterchangeResultsContainer* FInterchangeFbxParser::GetResultContainer() const
	{
		if (InternalResultsContainer)
		{
			return InternalResultsContainer;
		}
		ensure(ResultsContainer);
		return ResultsContainer.Get();
	}

}//ns UE::Interchange

#undef LOCTEXT_NAMESPACE
