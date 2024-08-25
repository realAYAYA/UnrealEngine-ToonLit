// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#if WITH_ENGINE
#include "Mesh/InterchangeMeshPayload.h"
#endif
#include "Misc/ScopeLock.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "InterchangeResultsContainer.h"
#include "UObject/StrongObjectPtr.h"

namespace UE
{
	namespace Interchange
	{
		namespace Private
		{
			class FFbxParser;
		}

		class INTERCHANGEFBXPARSER_API FInterchangeFbxParser
		{
		public:
			FInterchangeFbxParser();
			~FInterchangeFbxParser();
			
			void ReleaseResources();

			void Reset();

			void SetResultContainer(UInterchangeResultsContainer* Result);

			void SetConvertSettings(const bool InbConvertScene, const bool InbForceFrontXAxis, const bool InbConvertSceneUnit);
			/**
			 * Parse a file support by the fbx sdk. It just extract all the fbx node and create a FBaseNodeContainer and dump it in a json file inside the ResultFolder
			 * @param - Filename is the file that the fbx sdk will read (.fbx or .obj)
			 * @param - ResultFolder is the folder where we must put any result file
			 */
			void LoadFbxFile(const FString& Filename, const FString& ResultFolder);

			/**
			 * Parse a file support by the fbx sdk. It just extract all the fbx node and create a FBaseNodeContainer and dump it in a json file inside the ResultFolder
			 * @param - Filename is the file that the fbx sdk will read (.fbx or .obj)
			 * @param - BaseNodecontainer is the container of the scene graph
			 */
			void LoadFbxFile(const FString& Filename, UInterchangeBaseNodeContainer& BaseNodecontainer);

			/**
			 * Extract payload data from the fbx, the key tell the translator what payload the client ask
			 * @param - PayloadKey is the key that describe the payload data to extract from the fbx file
			 * @param - ResultFolder is the folder where we must put any result file
			 */
			void FetchPayload(const FString& PayloadKey, const FString& ResultFolder);

			/**
			 * Extract mesh payload data from the fbx, the key tell the translator what payload the client ask
			 * @param - PayloadKey is the key that describe the payload data to extract from the fbx file
			 * @param - MeshGlobalTransform is the transform we want to apply to the mesh vertex
			 * @param - ResultFolder is the folder where we must put any result file
			 * @return - Return the 'ResultPayloads' key unique id. We cannot use only the payload key because the mesh global transform can be different.
			 */
			FString FetchMeshPayload(const FString& PayloadKey, const FTransform& MeshGlobalTransform, const FString& ResultFolder);

#if WITH_ENGINE
			/**
			 * Extract mesh payload data from the fbx, the key tell the translator what payload the client ask
			 * @param - PayloadKey is the key that describe the payload data to extract from the fbx file
			 * @param - MeshGlobalTransform is the transform we want to apply to the mesh vertex
			 * @param - OutMeshPayloadData structure receiving the data
			 */
			void FetchMeshPayload(const FString& PayloadKey, const FTransform& MeshGlobalTransform, FMeshPayloadData& OutMeshPayloadData);
#endif
			
			/**
			 * Extract bake transform animation payload data from the fbx, the key tell the translator what payload the client ask
			 * @param - PayloadKey is the key that describe the payload data to extract from the fbx file
			 * @param - BakeFrequency is the Hz at which we should bake the transform
			 * @param - RangeStartTime is the start time of the bake
			 * @param - RangeEndTime is the end time of the bake
			 * @param - ResultFolder is the folder where we must put any result file
			 * @return - Return the 'ResultPayloads' key unique id. We cannot use only the payload key because the bake parameter can be different.
			 */
			FString FetchAnimationBakeTransformPayload(const FString& PayloadKey, const double BakeFrequency, const double RangeStartTime, const double RangeEndTime, const FString& ResultFolder);

			FString GetResultFilepath() const { return ResultFilepath; }
			FString GetResultPayloadFilepath(const FString& PayloadKey) const
			{
				FScopeLock Lock(&ResultPayloadsCriticalSection);
				if (const FString* PayloadPtr = ResultPayloads.Find(PayloadKey))
				{
					return *PayloadPtr;
				}
				return FString();
			}

			/**
			 * Transform the results container into an array of Json strings
			 */
			TArray<FString> GetJsonLoadMessages() const;

			template <typename T>
			T* AddMessage()
			{
				if (UInterchangeResultsContainer* ResultContainer = GetResultContainer())
				{
					return ResultContainer->Add<T>();
				}
				return nullptr;
			}

		private:
			UInterchangeResultsContainer* GetResultContainer() const;

			TObjectPtr<UInterchangeResultsContainer> InternalResultsContainer = nullptr;
			TStrongObjectPtr<UInterchangeResultsContainer> ResultsContainer = nullptr;
			FString SourceFilename;
			FString ResultFilepath;
			mutable FCriticalSection ResultPayloadsCriticalSection;
			TMap<FString, FString> ResultPayloads;
			TUniquePtr<UE::Interchange::Private::FFbxParser> FbxParserPrivate;
			TAtomic<int64> UniqueIdCounter = 0;
		};
	} // ns Interchange
}//ns UE
