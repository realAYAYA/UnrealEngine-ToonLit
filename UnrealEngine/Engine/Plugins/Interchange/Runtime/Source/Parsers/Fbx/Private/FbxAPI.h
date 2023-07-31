// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FbxInclude.h"
#include "HAL/CriticalSection.h"
#include "InterchangeResultsContainer.h"


#define FBX_METADATA_PREFIX TEXT("FBX.")
#define INVALID_UNIQUE_ID 0xFFFFFFFFFFFFFFFF

namespace UE
{
	namespace Interchange
	{
		namespace Private
		{
			class FPayloadContextBase;
		}
	}
}
class UInterchangeBaseNodeContainer;

namespace UE
{
	namespace Interchange
	{
		namespace Private
		{
			class FFbxParser
			{
			public:
				explicit FFbxParser(TWeakObjectPtr<UInterchangeResultsContainer> InResultsContainer)
					: ResultsContainer(InResultsContainer)
				{}

				~FFbxParser();

				/* Load an fbx file into the fbx sdk, return false if the file could not be load. */
				bool LoadFbxFile(const FString& Filename);

				/* Extract the fbx data from the sdk into our node container */
				void FillContainerWithFbxScene(UInterchangeBaseNodeContainer& NodeContainer);

				/* Extract the fbx data from the sdk into our node container */
				bool FetchPayloadData(const FString& PayloadKey, const FString& PayloadFilepath);

				/* Extract the fbx data from the sdk into our node container */
				bool FetchAnimationBakeTransformPayload(const FString& PayloadKey, const double BakeFrequency, const double RangeStartTime, const double RangeEndTime, const FString& PayloadFilepath);
				/**
				 * This function is used to add the given message object directly into the results for this operation.
				 */
				template <typename T>
				T* AddMessage() const
				{
					check(ResultsContainer.IsValid());
					T* Item = ResultsContainer->Add<T>();
					Item->SourceAssetName = SourceFilename;
					return Item;
				}


				void AddMessage(UInterchangeResult* Item) const
				{
					check(ResultsContainer.IsValid());
					ResultsContainer->Add(Item);
					Item->SourceAssetName = SourceFilename;
				}

				/**
				 * Critical section to avoid getting multiple payload in same time.
				 * The FBX evaluator use a cache mechanism for evaluating global transform that is not thread safe.
				 * There si other stuff in the sdk which are not thread safe, so all fbx payload should be fetch one by one
				 */
				FCriticalSection PayloadCriticalSection;
			private:

				void CleanupFbxData();

				TWeakObjectPtr<UInterchangeResultsContainer> ResultsContainer;
				FbxManager* SDKManager = nullptr;
				FbxScene* SDKScene = nullptr;
				FbxImporter* SDKImporter = nullptr;
				FbxGeometryConverter* SDKGeometryConverter = nullptr;
				FString SourceFilename;
				TMap<FString, TSharedPtr<FPayloadContextBase>> PayloadContexts;
			};
		}//ns Private
	}//ns Interchange
}//ns UE
