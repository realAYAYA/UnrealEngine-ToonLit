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
		FbxParserPrivate = nullptr;
	}

	void FInterchangeFbxParser::LoadFbxFile(const FString& Filename, const FString& ResultFolder)
	{
		check(FbxParserPrivate.IsValid());
		SourceFilename = Filename;
		ResultsContainer->Empty();

		if (!FbxParserPrivate->LoadFbxFile(Filename))
		{
			UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>();
			Error->SourceAssetName = SourceFilename;
			Error->Text = LOCTEXT("CantLoadFbxFile", "Cannot load the FBX file.");
			return;
		}

		ResultFilepath = ResultFolder + TEXT("/SceneDescription.itc");
		//Since we are not in main thread we cannot use TStrongPtr, so we will add the object to the root and remove it when we are done
		UInterchangeBaseNodeContainer* Container = NewObject<UInterchangeBaseNodeContainer>(GetTransientPackage(), NAME_None);
		if (!ensure(Container != nullptr))
		{
			UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>();
			Error->SourceAssetName = SourceFilename;
			Error->Text = LOCTEXT("CantAllocate", "Cannot allocate base node container to add FBX scene data.");
			return;
		}

		Container->AddToRoot();
		FbxParserPrivate->FillContainerWithFbxScene(*Container);
		Container->SaveToFile(ResultFilepath);
		Container->RemoveFromRoot();
	}

	void FInterchangeFbxParser::FetchPayload(const FString& PayloadKey, const FString& ResultFolder)
	{
		check(FbxParserPrivate.IsValid());
		ResultsContainer->Empty();
		FString PayloadFilepathCopy;
		{
			FScopeLock Lock(&ResultPayloadsCriticalSection);
			FString& PayloadFilepath = ResultPayloads.FindOrAdd(PayloadKey);
			//To avoid file path with too many character, we hash the payloadKey so we have a deterministic length for the file path.
			FString PayloadKeyHash = Private::HashString(PayloadKey);
			PayloadFilepath = ResultFolder + TEXT("/") + PayloadKeyHash + TEXT(".payload");

			//Copy the map filename key because we are multithreaded and the TMap can be reallocated
			PayloadFilepathCopy = PayloadFilepath;
		}
		if (!FbxParserPrivate->FetchPayloadData(PayloadKey, PayloadFilepathCopy))
		{
			UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>();
			Error->SourceAssetName = SourceFilename;
			Error->Text = LOCTEXT("CantFetchPayload", "Cannot fetch FBX payload data.");
			return;
		}
	}

	void FInterchangeFbxParser::FetchAnimationBakeTransformPayload(const FString& PayloadKey, const double BakeFrequency, const double RangeStartTime, const double RangeEndTime, const FString& ResultFolder)
	{
		check(FbxParserPrivate.IsValid());
		ResultsContainer->Empty();
		FString PayloadFilepathCopy;
		{
			FScopeLock Lock(&ResultPayloadsCriticalSection);
			FString& PayloadFilepath = ResultPayloads.FindOrAdd(PayloadKey);
			//To avoid file path with too many character, we hash the payloadKey so we have a deterministic length for the file path.
			FString PayloadKeyHash = Private::HashString(PayloadKey);
			PayloadFilepath = ResultFolder + TEXT("/") + PayloadKeyHash + TEXT(".payload");

			//Copy the map filename key because we are multithreaded and the TMap can be reallocated
			PayloadFilepathCopy = PayloadFilepath;
		}
		if (!FbxParserPrivate->FetchAnimationBakeTransformPayload(PayloadKey, BakeFrequency, RangeStartTime, RangeEndTime, PayloadFilepathCopy))
		{
			UInterchangeResultError_Generic* Error = AddMessage<UInterchangeResultError_Generic>();
			Error->SourceAssetName = SourceFilename;
			Error->Text = LOCTEXT("CantFetchPayload", "Cannot fetch FBX payload data.");
			return;
		}
	}

	TArray<FString> FInterchangeFbxParser::GetJsonLoadMessages() const
	{
		TArray<FString> JsonResults;
		for (UInterchangeResult* Result : ResultsContainer->GetResults())
		{
			JsonResults.Add(Result->ToJson());
		}

		return JsonResults;
	}

}//ns UE::Interchange

#undef LOCTEXT_NAMESPACE
