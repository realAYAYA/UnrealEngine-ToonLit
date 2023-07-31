// Copyright Epic Games, Inc. All Rights Reserved.
#include "InterchangeImportCommon.h"

#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "InterchangeAssetImportData.h"
#include "InterchangePipelineBase.h"
#include "InterchangePythonPipelineBase.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "UObject/Object.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"

namespace UE
{
	namespace Interchange
	{
		namespace Private::ImportCommon
		{
			UInterchangeAssetImportData* BeginSetupAssetData(FFactoryCommon::FUpdateImportAssetDataParameters& Parameters)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::Private::ImportCommon::BeginSetupAssetData")
				if (!ensure(IsInGameThread()))
				{
					return nullptr;
				}
				if (!ensure(Parameters.SourceData && Parameters.AssetImportDataOuter))
				{
					return nullptr;
				}

				UInterchangeAssetImportData* AssetImportData = Cast<UInterchangeAssetImportData>(Parameters.AssetImportData);

				if (!AssetImportData)
				{
					AssetImportData = NewObject<UInterchangeAssetImportData>(Parameters.AssetImportDataOuter, NAME_None);
				}

				ensure(AssetImportData);

				return AssetImportData;
			}

			void RecursivelyDuplicateFactoryNodeDependencies(UInterchangeBaseNodeContainer* SourceContainer, UInterchangeBaseNodeContainer* DestinationContainer, TArray<FString>& FactoryDependencies)
			{
				for (const FString& DependencyUid : FactoryDependencies)
				{
					if (UInterchangeFactoryBaseNode* FactoryNode = Cast<UInterchangeFactoryBaseNode>(SourceContainer->GetFactoryNode(DependencyUid)))
					{
						FObjectDuplicationParameters DupParam(FactoryNode, DestinationContainer);
						DestinationContainer->AddNode(CastChecked<UInterchangeBaseNode>(StaticDuplicateObjectEx(DupParam)));

						TArray<FString> ChildFactoryDependencies;
						FactoryNode->GetFactoryDependencies(ChildFactoryDependencies);
						RecursivelyDuplicateFactoryNodeDependencies(SourceContainer, DestinationContainer, ChildFactoryDependencies);
					}
				}
			}

			void EndSetupAssetData(FFactoryCommon::FUpdateImportAssetDataParameters& Parameters, UInterchangeAssetImportData* AssetImportData)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::Private::ImportCommon::EndSetupAssetData")
				AssetImportData->NodeContainer = NewObject<UInterchangeBaseNodeContainer>(AssetImportData);
				//We copy only the factory node dependencies, we use this only 
				if (UInterchangeFactoryBaseNode* FactoryNode = Cast<UInterchangeFactoryBaseNode>(Parameters.NodeContainer->GetFactoryNode(Parameters.NodeUniqueID)))
				{
					//Set the interchange node graph data
					AssetImportData->NodeUniqueID = Parameters.NodeUniqueID;
					TArray<FString> FactoryDependencies;
					FactoryDependencies.Add(AssetImportData->NodeUniqueID);
					RecursivelyDuplicateFactoryNodeDependencies(Parameters.NodeContainer, AssetImportData->NodeContainer, FactoryDependencies);
				}
				AssetImportData->Pipelines.Reset();
				for (const UObject* Pipeline : Parameters.Pipelines)
				{
					UObject* DupPipeline = Cast<UObject>(StaticDuplicateObject(Pipeline, AssetImportData));
					if (DupPipeline)
					{
						AssetImportData->Pipelines.Add(DupPipeline);
					}
				}
			}
		}

		FFactoryCommon::FUpdateImportAssetDataParameters::FUpdateImportAssetDataParameters(UObject* InAssetImportDataOuter
																							, UAssetImportData* InAssetImportData
																							, const UInterchangeSourceData* InSourceData
																							, FString InNodeUniqueID
																							, UInterchangeBaseNodeContainer* InNodeContainer
																						   , const TArray<UObject*>& InPipelines)
			: AssetImportDataOuter(InAssetImportDataOuter)
			, AssetImportData(InAssetImportData)
			, SourceData(InSourceData)
			, NodeUniqueID(InNodeUniqueID)
			, NodeContainer(InNodeContainer)
			, Pipelines(InPipelines)
		{
			ensure(AssetImportDataOuter);
			ensure(SourceData);
			ensure(!NodeUniqueID.IsEmpty());
			ensure(NodeContainer);
		}

		UAssetImportData* FFactoryCommon::UpdateImportAssetData(FUpdateImportAssetDataParameters& Parameters)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FFactoryCommon::UpdateImportAssetData")
			return UpdateImportAssetData(Parameters, [&Parameters](UInterchangeAssetImportData* AssetImportData)
				{
#if WITH_EDITORONLY_DATA
					//Set the asset import data file source to allow reimport. TODO: manage MD5 Hash properly
					TOptional<FMD5Hash> FileContentHash = Parameters.SourceData->GetFileContentHash();

					//Update the first filename, TODO for asset using multiple source file we have to update the correct index
					AssetImportData->Update(Parameters.SourceData->GetFilename(), FileContentHash.IsSet() ? &FileContentHash.GetValue() : nullptr);
#endif //WITH_EDITORONLY_DATA
				});

		}

		UAssetImportData* FFactoryCommon::UpdateImportAssetData(FUpdateImportAssetDataParameters& Parameters, TFunctionRef<void(UInterchangeAssetImportData*)> CustomFileSourceUpdate)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FFactoryCommon::UpdateImportAssetData2")
#if WITH_EDITORONLY_DATA
			if (!ensure(IsInGameThread()))
			{
				return nullptr;
			}
			if (!ensure(Parameters.SourceData && Parameters.AssetImportDataOuter))
			{
				return nullptr;
			}

			UInterchangeAssetImportData* AssetImportData = Private::ImportCommon::BeginSetupAssetData(Parameters);

			if (Parameters.AssetImportData) //-V1051
			{
				if (!Parameters.AssetImportData->IsA<UInterchangeAssetImportData>())
				{
					// Migrate the old source data
					TArray<FAssetImportInfo::FSourceFile> OldSourceFiles = Parameters.AssetImportData->SourceData.SourceFiles;
					AssetImportData->SetSourceFiles(MoveTemp(OldSourceFiles));
				}
			}

			CustomFileSourceUpdate(AssetImportData);


			Private::ImportCommon::EndSetupAssetData(Parameters, AssetImportData);

			// Return the asset import data so it can be set on the imported asset.
			return AssetImportData;
#endif //#if WITH_EDITORONLY_DATA
			return nullptr;
		}

#if WITH_EDITORONLY_DATA
		FFactoryCommon::FSetImportAssetDataParameters::FSetImportAssetDataParameters(UObject* InAssetImportDataOuter
																					, UAssetImportData* InAssetImportData
																					, const UInterchangeSourceData* InSourceData
																					, FString InNodeUniqueID
																					, UInterchangeBaseNodeContainer* InNodeContainer
																					, const TArray<UObject*>& InPipelines)
			: FUpdateImportAssetDataParameters(InAssetImportDataOuter
				, InAssetImportData
				, InSourceData
				, InNodeUniqueID
				, InNodeContainer
				, InPipelines
				)
			, SourceFiles()
		{
		}

		UAssetImportData* FFactoryCommon::SetImportAssetData(FSetImportAssetDataParameters& Parameters)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FFactoryCommon::SetImportAssetData")
			UInterchangeAssetImportData* AssetImportData = Private::ImportCommon::BeginSetupAssetData(Parameters);

			// Update the source files
			{
				if (Parameters.SourceFiles.IsEmpty())
				{
					TOptional<FMD5Hash> FileContentHash = Parameters.SourceData->GetFileContentHash();

					// Todo add display label?
					Parameters.SourceFiles.Emplace(
						AssetImportData->SanitizeImportFilename(Parameters.SourceData->GetFilename()),
						IFileManager::Get().GetTimeStamp(*Parameters.SourceData->GetFilename()),
						FileContentHash.IsSet() ? FileContentHash.GetValue() : FMD5Hash()
						);
				}
				else
				{
					for (FAssetImportInfo::FSourceFile& Source : Parameters.SourceFiles)
					{
						// This is done here since this is not thread safe
						Source.RelativeFilename = AssetImportData->SanitizeImportFilename(Source.RelativeFilename);
					}
				}

				AssetImportData->SetSourceFiles(MoveTemp(Parameters.SourceFiles));
			}

			Private::ImportCommon::EndSetupAssetData(Parameters, AssetImportData);

			// Return the asset import data so it can be set on the imported asset.
			return AssetImportData;
		}

		bool FFactoryCommon::GetSourceFilenames(const UAssetImportData* AssetImportData, TArray<FString>& OutSourceFilenames)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FFactoryCommon::GetSourceFilenames")
			if (Cast<UInterchangeAssetImportData>(AssetImportData) != nullptr)
			{
				AssetImportData->ExtractFilenames(OutSourceFilenames);
				return true;
			}
			return false;
		}

		bool FFactoryCommon::SetSourceFilename(UAssetImportData* AssetImportData, const FString& SourceFilename, int32 SourceIndex, const FString& SourceLabel)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FFactoryCommon::SetSourceFilename")
			if (AssetImportData)
			{
				const int32 SafeSourceIndex = SourceIndex == INDEX_NONE ? 0 : SourceIndex;				
				if (SafeSourceIndex < AssetImportData->GetSourceFileCount())
				{
					AssetImportData->UpdateFilenameOnly(SourceFilename, SafeSourceIndex);
				}
				else
				{
					//Create a source file entry, this case happen when user import a specific content for the first time
					AssetImportData->AddFileName(SourceFilename, SafeSourceIndex, SourceLabel);
				}
				return true;
			}

			return false;
		}
		
		bool FFactoryCommon::SetReimportSourceIndex(const UObject* Object, UAssetImportData* AssetImportData, int32 SourceIndex)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FFactoryCommon::SetReimportSourceIndex")
			UInterchangeAssetImportData* InterchangeAssetImportData = Cast<UInterchangeAssetImportData>(AssetImportData);
			if (!InterchangeAssetImportData)
			{
				return false;
			}

			for (UObject* PipelineObject : InterchangeAssetImportData->Pipelines)
			{
				if (UInterchangePythonPipelineAsset* PythonPipelineAsset = Cast<UInterchangePythonPipelineAsset>(PipelineObject))
				{
					if (PythonPipelineAsset->GeneratedPipeline)
					{
						PythonPipelineAsset->GeneratedPipeline->ScriptedSetReimportSourceIndex(Object->GetClass(), SourceIndex);
						PythonPipelineAsset->SetupFromPipeline(PythonPipelineAsset->GeneratedPipeline);
					}
				}
				else if (UInterchangePipelineBase* PipelineBase = Cast<UInterchangePipelineBase>(PipelineObject))
				{
					PipelineBase->ScriptedSetReimportSourceIndex(Object->GetClass(), SourceIndex);
				}
			}
			return true;
		}

#endif //#if WITH_EDITORONLY_DATA


		void FFactoryCommon::ApplyReimportStrategyToAsset(UObject* Asset
										  , UInterchangeFactoryBaseNode* PreviousAssetNode
										  , UInterchangeFactoryBaseNode* CurrentAssetNode
										  , UInterchangeFactoryBaseNode* PipelineAssetNode)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FFactoryCommon::ApplyReimportStrategyToAsset")
			if (!ensure(PreviousAssetNode) || !ensure(PipelineAssetNode) || !ensure(CurrentAssetNode))
			{
				return;
			}
			EReimportStrategyFlags ReimportStrategyFlags = PipelineAssetNode->GetReimportStrategyFlags();
			switch (ReimportStrategyFlags)
			{
				case EReimportStrategyFlags::ApplyNoProperties:
				{
					//We want to have no effect
					break;
				}
					
				case EReimportStrategyFlags::ApplyPipelineProperties:
				{
					//Directly apply pipeline node attribute to the asset
					PipelineAssetNode->ApplyAllCustomAttributeToObject(Asset);
					break;
				}
				
				case EReimportStrategyFlags::ApplyEditorChangedProperties:
				{
					TArray<FAttributeKey> RemovedAttributes;
					TArray<FAttributeKey> AddedAttributes;
					TArray<FAttributeKey> ModifiedAttributes;
					UInterchangeBaseNode::CompareNodeStorage(PreviousAssetNode, CurrentAssetNode, RemovedAttributes, AddedAttributes, ModifiedAttributes);

					//set all ModifedAttributes from the CurrentAssetNode to the pipeline node. This will put back all user changes
					UInterchangeBaseNode::CopyStorageAttributes(CurrentAssetNode, PipelineAssetNode, ModifiedAttributes);
					//Now apply the pipeline node attribute to the asset
					PipelineAssetNode->ApplyAllCustomAttributeToObject(Asset);
					break;
				}
			}
		}
	}
}
