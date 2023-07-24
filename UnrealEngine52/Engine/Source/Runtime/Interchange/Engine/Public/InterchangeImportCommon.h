// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "EditorFramework/AssetImportData.h"

class UAssetImportData;
class UInterchangeAssetImportData;
class UInterchangeBaseNode;
class UInterchangeBaseNodeContainer;
class UInterchangeFactoryBaseNode;
class UInterchangePipelineBase;
class UInterchangeSourceData;
class UObject;
template <typename FuncType> class TFunctionRef;

namespace UE
{
	namespace Interchange
	{
		/**
		 * All the code we cannot put in the base factory class because of dependencies (like Engine dep)
		 * Will be available here.
		 */
		class INTERCHANGEENGINE_API FFactoryCommon
		{
		public:

			struct INTERCHANGEENGINE_API FUpdateImportAssetDataParameters
			{
				UObject* AssetImportDataOuter = nullptr;
				UAssetImportData* AssetImportData = nullptr;
				const UInterchangeSourceData* SourceData = nullptr;
				FString NodeUniqueID;
				UInterchangeBaseNodeContainer* NodeContainer;
				const TArray<UObject*> Pipelines;

				FUpdateImportAssetDataParameters(UObject* InAssetImportDataOuter
																	, UAssetImportData* InAssetImportData
																	, const UInterchangeSourceData* InSourceData
																	, FString InNodeUniqueID
																	, UInterchangeBaseNodeContainer* InNodeContainer
																	, const TArray<UObject*>& InPipelines);
			};

			/**
			 * Update the AssetImportData source file of the specified asset in the parameters. Also update the node container and the node unique id.
			 * If the AssetImportData is null it will create one. If the AssetImportData is not an UInterchangeAssetImportData it will create a new one.
			 * @return The source data that should be stored on the asset or nullptr if a parameter is invalid
			 */
			static UAssetImportData* UpdateImportAssetData(FUpdateImportAssetDataParameters& Parameters);

			/**
			 * Update the AssetImportData of the specified asset in the parameters. Also update the node container and the node unique id.
			 * If the AssetImportData is null it will create one. If the AssetImportData is not an UInterchangeAssetImportData it will create a new one.
			 * The file source update is done by calling the function parameter CustomFileSourceUpdate, so its the client responsability to properly update the file source.
			 * @return The source data that should be stored on the asset or nullptr if a parameter is invalid
			 */
			static UAssetImportData* UpdateImportAssetData(FUpdateImportAssetDataParameters& Parameters, TFunctionRef<void(UInterchangeAssetImportData*)> CustomFileSourceUpdate);

#if WITH_EDITORONLY_DATA
			struct INTERCHANGEENGINE_API FSetImportAssetDataParameters : public FUpdateImportAssetDataParameters
			{
				// Allow the factory to provide is own list of source files.
				TArray<FAssetImportInfo::FSourceFile> SourceFiles;

				FSetImportAssetDataParameters(UObject* InAssetImportDataOuter
					, UAssetImportData* InAssetImportData
					, const UInterchangeSourceData* InSourceData
					, FString InNodeUniqueID
					, UInterchangeBaseNodeContainer* InNodeContainer
					, const TArray<UObject*>& InPipelines);
			};

			/**
			 * Set the AssetImportData source file of the specified asset in the parameters. Also update the node container and the node unique id.
			 * If the AssetImportData is null it will create one. If the AssetImportData is not an UInterchangeAssetImportData it will create a new one.
			 * @return The source data that should be stored on the asset or nullptr if a parameter is invalid
			 */
			static UAssetImportData* SetImportAssetData(FSetImportAssetDataParameters& Parameters);

			/**
			 * Fills the OutSourceFilenames array with the list of source files contained in the asset source data.
			 * Returns true if the operation was successful.
			 */
			static bool GetSourceFilenames(const UAssetImportData* AssetImportData, TArray<FString>& OutSourceFilenames);

			/**
			 * Sets the SourceFileName value at the specified index.
			 */
			static bool SetSourceFilename(UAssetImportData* AssetImportData, const FString& SourceFilename, int32 SourceIndex, const FString& SourceLabel = FString());

			/**
			 * Set the object's reimport source at the specified index value.
			 */
			static bool SetReimportSourceIndex(const UObject* Object, UAssetImportData* AssetImportData, int32 SourceIndex);

#endif // WITH_EDITORONLY_DATA

			/**
			 * Apply the current strategy to the PipelineAssetNode
			 */
			static void ApplyReimportStrategyToAsset(UObject* Asset
											  , UInterchangeFactoryBaseNode* PreviousAssetNode
											  , UInterchangeFactoryBaseNode* CurrentAssetNode
											  , UInterchangeFactoryBaseNode* PipelineAssetNode);
		};
	}
}
