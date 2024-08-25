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
			struct FFbxHelper;
		}
#if WITH_ENGINE
		struct FMeshPayloadData;
#endif
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

				void Reset();

				void SetResultContainer(UInterchangeResultsContainer* Result)
				{
					ResultsContainer = Result;
				}

				void SetConvertSettings(const bool InbConvertScene, const bool InbForceFrontXAxis, const bool InbConvertSceneUnit)
				{
					bConvertScene = InbConvertScene;
					bForceFrontXAxis = InbForceFrontXAxis;
					bConvertSceneUnit = InbConvertSceneUnit;
				}

				//return the fbx helper for this parser
				const TSharedPtr<FFbxHelper> GetFbxHelper();

				/* Load an fbx file into the fbx sdk, return false if the file could not be load. */
				bool LoadFbxFile(const FString& Filename);

				/* Extract the fbx data from the sdk into our node container */
				void FillContainerWithFbxScene(UInterchangeBaseNodeContainer& NodeContainer);

				/* Extract the fbx data from the sdk into our node container */
				bool FetchPayloadData(const FString& PayloadKey, const FString& PayloadFilepath);

				/* Extract the fbx mesh data from the sdk into our node container */
				bool FetchMeshPayloadData(const FString& PayloadKey, const FTransform& MeshGlobalTransform, const FString& PayloadFilepath);
#if WITH_ENGINE
				bool FetchMeshPayloadData(const FString& PayloadKey, const FTransform& MeshGlobalTransform, FMeshPayloadData& OutMeshPayloadData);
#endif

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

				FbxScene* GetSDKScene() { return SDKScene; }

				double GetFrameRate() { return FrameRate; }

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
				FbxIOSettings* SDKIoSettings = nullptr;
				FString SourceFilename;
				TMap<FString, TSharedPtr<FPayloadContextBase>> PayloadContexts;
				TSharedPtr<FFbxHelper> FbxHelper;

				//For PivotReset and Animation Conversion:
				double FrameRate = 30.0;

				//Convert settings
				bool bConvertScene = true;
				bool bForceFrontXAxis = false;
				bool bConvertSceneUnit = true;
			};
		}//ns Private
	}//ns Interchange
}//ns UE
