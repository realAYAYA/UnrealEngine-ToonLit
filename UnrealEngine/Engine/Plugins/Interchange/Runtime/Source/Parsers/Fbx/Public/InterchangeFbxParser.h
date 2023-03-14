// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
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

			/**
			 * Parse a file support by the fbx sdk. It just extract all the fbx node and create a FBaseNodeContainer and dump it in a json file inside the ResultFolder
			 * @param - Filename is the file that the fbx sdk will read (.fbx or .obj)
			 * @param - ResultFolder is the folder where we must put any result file
			 */
			void LoadFbxFile(const FString& Filename, const FString& ResultFolder);

			/**
			 * Extract payload data from the fbx, the key tell the translator what payload the client ask
			 * @param - PayloadKey is the key that describe the payload data to extract from the fbx file
			 * @param - ResultFolder is the folder where we must put any result file
			 */
			void FetchPayload(const FString& PayloadKey, const FString& ResultFolder);

			/**
			 * Extract bake transform animation payload data from the fbx, the key tell the translator what payload the client ask
			 * @param - PayloadKey is the key that describe the payload data to extract from the fbx file
			 * @param - BakeFrequency is the Hz at which we should bake the transform
			 * @param - RangeStartTime is the start time of the bake
			 * @param - RangeEndTime is the end time of the bake
			 * @param - ResultFolder is the folder where we must put any result file
			 */
			void FetchAnimationBakeTransformPayload(const FString& PayloadKey, const double BakeFrequency, const double RangeStartTime, const double RangeEndTime, const FString& ResultFolder);

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
				return ResultsContainer->Add<T>();
			}


		private:
			TStrongObjectPtr<UInterchangeResultsContainer> ResultsContainer = nullptr;
			FString SourceFilename;
			FString ResultFilepath;
			mutable FCriticalSection ResultPayloadsCriticalSection;
			TMap<FString, FString> ResultPayloads;
			TUniquePtr<UE::Interchange::Private::FFbxParser> FbxParserPrivate;
		};
	} // ns Interchange
}//ns UE
