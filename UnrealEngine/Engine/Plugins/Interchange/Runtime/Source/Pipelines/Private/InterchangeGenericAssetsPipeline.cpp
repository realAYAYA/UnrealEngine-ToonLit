// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeGenericAssetsPipeline.h"

#include "Animation/Skeleton.h"
#include "CoreMinimal.h"
#include "InterchangeAnimSequenceFactoryNode.h"
#include "InterchangeCommonPipelineDataFactoryNode.h"
#include "InterchangeGenericAnimationPipeline.h"
#include "InterchangeGenericMaterialPipeline.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeGenericTexturePipeline.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangePipelineLog.h"
#include "InterchangePipelineMeshesUtilities.h"
#include "InterchangeSceneNode.h"
#include "InterchangeSkeletalMeshFactoryNode.h"
#include "InterchangeSkeletalMeshLodDataNode.h"
#include "InterchangeSkeletonFactoryNode.h"
#include "InterchangeSourceData.h"
#include "InterchangeStaticMeshFactoryNode.h"
#include "InterchangeStaticMeshLodDataNode.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/MetaData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeGenericAssetsPipeline)

UInterchangeGenericAssetsPipeline::UInterchangeGenericAssetsPipeline()
{
	MaterialPipeline = CreateDefaultSubobject<UInterchangeGenericMaterialPipeline>("MaterialPipeline");
	CommonMeshesProperties = CreateDefaultSubobject<UInterchangeGenericCommonMeshesProperties>("CommonMeshesProperties");
	CommonSkeletalMeshesAndAnimationsProperties = CreateDefaultSubobject<UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties>("CommonSkeletalMeshesAndAnimationsProperties");
	MeshPipeline = CreateDefaultSubobject<UInterchangeGenericMeshPipeline>("MeshPipeline");
	MeshPipeline->CommonMeshesProperties = CommonMeshesProperties;
	MeshPipeline->CommonSkeletalMeshesAndAnimationsProperties = CommonSkeletalMeshesAndAnimationsProperties;
	AnimationPipeline = CreateDefaultSubobject<UInterchangeGenericAnimationPipeline>("AnimationPipeline");
	AnimationPipeline->CommonSkeletalMeshesAndAnimationsProperties = CommonSkeletalMeshesAndAnimationsProperties;
}

void UInterchangeGenericAssetsPipeline::PreDialogCleanup(const FName PipelineStackName)
{
	check(CommonSkeletalMeshesAndAnimationsProperties)
	//We always clean the pipeline skeleton when showing the dialog
	CommonSkeletalMeshesAndAnimationsProperties->Skeleton = nullptr;

	if (MaterialPipeline)
	{
		MaterialPipeline->PreDialogCleanup(PipelineStackName);
	}
	
	if (MeshPipeline)
	{
		MeshPipeline->PreDialogCleanup(PipelineStackName);
	}

	if (AnimationPipeline)
	{
		AnimationPipeline->PreDialogCleanup(PipelineStackName);
	}
	
	SaveSettings(PipelineStackName);
}

bool UInterchangeGenericAssetsPipeline::IsSettingsAreValid(TOptional<FText>& OutInvalidReason) const
{
	if (MaterialPipeline && !MaterialPipeline->IsSettingsAreValid(OutInvalidReason))
	{
		return false;
	}

	if (CommonMeshesProperties && !CommonMeshesProperties->IsSettingsAreValid(OutInvalidReason))
	{
		return false;
	}

	if (CommonSkeletalMeshesAndAnimationsProperties && !CommonSkeletalMeshesAndAnimationsProperties->IsSettingsAreValid(OutInvalidReason))
	{
		return false;
	}

	if (MeshPipeline && !MeshPipeline->IsSettingsAreValid(OutInvalidReason))
	{
		return false;
	}

	if (AnimationPipeline && !AnimationPipeline->IsSettingsAreValid(OutInvalidReason))
	{
		return false;
	}

	return Super::IsSettingsAreValid(OutInvalidReason);
}

void UInterchangeGenericAssetsPipeline::AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset)
{
	Super::AdjustSettingsForContext(ImportType, ReimportAsset);

	if (MaterialPipeline)
	{
		MaterialPipeline->AdjustSettingsForContext(ImportType, ReimportAsset);
	}

	if (MeshPipeline)
	{
		MeshPipeline->AdjustSettingsForContext(ImportType, ReimportAsset);
	}

	if (AnimationPipeline)
	{
		AnimationPipeline->AdjustSettingsForContext(ImportType, ReimportAsset);
	}
}

void UInterchangeGenericAssetsPipeline::ExecutePreImportPipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas)
{
	check(CommonSkeletalMeshesAndAnimationsProperties);

	if (!InBaseNodeContainer)
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericAssetsPipeline: Cannot execute pre-import pipeline because InBaseNodeContrainer is null"));
		return;
	}

	//Set the result container to allow error message
	//The parent Results container should be set at this point
	ensure(Results);
	{
		if (MaterialPipeline)
		{
			MaterialPipeline->SetResultsContainer(Results);
		}
		if (MeshPipeline)
		{
			MeshPipeline->SetResultsContainer(Results);
		}
		if (AnimationPipeline)
		{
			AnimationPipeline->SetResultsContainer(Results);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//Make sure all options go together
	
	//When we import only animation we need to prevent material and physic asset to be created
	if (CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations)
	{
		MaterialPipeline->bImportMaterials = false;
		MeshPipeline->bImportStaticMeshes = false;
		MeshPipeline->bCreatePhysicsAsset = false;
		MeshPipeline->PhysicsAsset = nullptr;
		MaterialPipeline->TexturePipeline->bImportTextures = false;
	}

	//////////////////////////////////////////////////////////////////////////


	BaseNodeContainer = InBaseNodeContainer;

	//Setup the Global import offset
	{
		//Make sure the scale value is greater than zero, warn the user in this case and set the scale to the default value 1.0f
		if (ImportOffsetUniformScale < UE_SMALL_NUMBER)
		{
			FNumberFormattingOptions FormatingOptions;
			FormatingOptions.SetMaximumFractionalDigits(6);
			FormatingOptions.SetMinimumFractionalDigits(1);
			float DefaultScaleValue = 1.0f;
			UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
			Message->Text = FText::Format(NSLOCTEXT("UInterchangeGenericAssetsPipeline", "BadImportOffsetUniformScale", "Value [{0}] for ImportOffsetUniformScale setting is too small, we will use the default value [{1}]."),
				FText::AsNumber(ImportOffsetUniformScale, &FormatingOptions),
				FText::AsNumber(DefaultScaleValue, &FormatingOptions));
			ImportOffsetUniformScale = DefaultScaleValue;
		}

		FTransform ImportOffsetTransform;
		ImportOffsetTransform.SetTranslation(ImportOffsetTranslation);
		ImportOffsetTransform.SetRotation(FQuat(ImportOffsetRotation));
		ImportOffsetTransform.SetScale3D(FVector(ImportOffsetUniformScale));

		UInterchangeCommonPipelineDataFactoryNode* CommonPipelineDataFactoryNode = UInterchangeCommonPipelineDataFactoryNode::FindOrCreateUniqueInstance(BaseNodeContainer);
		CommonPipelineDataFactoryNode->SetCustomGlobalOffsetTransform(BaseNodeContainer, ImportOffsetTransform);

		// In case all mesh types are forced to Static/Skeletal we bake the scene instance hierarchy transforms
		CommonPipelineDataFactoryNode->SetBakeMeshes(BaseNodeContainer, CommonMeshesProperties->ForceAllMeshAsType != EInterchangeForceMeshType::IFMT_None || CommonMeshesProperties->bBakeMeshes);
	}

	SourceDatas.Empty(InSourceDatas.Num());
	for (const UInterchangeSourceData* SourceData : InSourceDatas)
	{
		SourceDatas.Add(SourceData);
	}

	if (MaterialPipeline)
	{
		MaterialPipeline->ScriptedExecutePreImportPipeline(InBaseNodeContainer, InSourceDatas);
	}
	if (MeshPipeline)
	{
		MeshPipeline->ScriptedExecutePreImportPipeline(InBaseNodeContainer, InSourceDatas);
	}
	if (AnimationPipeline)
	{
		AnimationPipeline->ScriptedExecutePreImportPipeline(InBaseNodeContainer, InSourceDatas);
	}

	ImplementUseSourceNameForAssetOption();
	//Make sure all factory nodes have the specified strategy
	BaseNodeContainer->IterateNodesOfType<UInterchangeFactoryBaseNode>([ReimportStrategyClosure = ReimportStrategy](const FString& NodeUid, UInterchangeFactoryBaseNode* FactoryNode)
		{
			FactoryNode->SetReimportStrategyFlags(ReimportStrategyClosure);
		});
}

void UInterchangeGenericAssetsPipeline::ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* InBaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport)
{
	if (MaterialPipeline)
	{
		MaterialPipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}
	if (MeshPipeline)
	{
		MeshPipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}
	if (AnimationPipeline)
	{
		AnimationPipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}

#if WITH_EDITORONLY_DATA
	AddPackageMetaData(CreatedAsset, InBaseNodeContainer->GetNode(NodeKey));
#endif
}

#if WITH_EDITORONLY_DATA
void UInterchangeGenericAssetsPipeline::AddPackageMetaData(UObject* CreatedAsset, const UInterchangeBaseNode* Node)
{
	if (!CreatedAsset || !Node)
	{
		return;
	}

	const FString InterchangeMetaDataPrefix = TEXT("INTERCHANGE.");

	//Add UObject package meta data
	if (UMetaData* MetaData = CreatedAsset->GetOutermost()->GetMetaData())
	{
		//Cleanup existing INTERCHANGE_ prefix metadata name for this object (in case we re-import)
		{
			TArray<FName> InterchangeMetaDataKeys;
			if(TMap<FName, FString>* MetaDataMapPtr = MetaData->GetMapForObject(CreatedAsset))
			{
				for (const TPair<FName, FString>& ObjectMetadata : *MetaDataMapPtr)
				{
					if (ObjectMetadata.Key.ToString().StartsWith(InterchangeMetaDataPrefix))
					{
						InterchangeMetaDataKeys.Add(ObjectMetadata.Key);
					}
				}
				for (const FName& MetaDataKey : InterchangeMetaDataKeys)
				{
					MetaData->RemoveValue(CreatedAsset, MetaDataKey);
				}
			}
		}
		TArray<FInterchangeUserDefinedAttributeInfo> UserAttributeInfos;
		UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttributeInfos(Node, UserAttributeInfos);
		//We must convert all different type to String since meta data only support string
		for (const FInterchangeUserDefinedAttributeInfo& UserAttributeInfo : UserAttributeInfos)
		{
			if (UserAttributeInfo.PayloadKey.IsSet())
			{
				//Skip animated attributes
				continue;
			}
			TOptional<FString> MetaDataValue;
			TOptional<FString> PayloadKey;
			switch (UserAttributeInfo.Type)
			{
				case UE::Interchange::EAttributeTypes::Bool:
				{
					bool Value = false;
					if(UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Int8:
				{
					int8 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Int16:
				{
					int16 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Int32:
				{
					int32 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Int64:
				{
					int64 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::UInt8:
				{
					uint8 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::UInt16:
				{
					uint16 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::UInt32:
				{
					uint32 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::UInt64:
				{
					uint64 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Float:
				{
					float Value = 0.0f;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Float16:
				{
					FFloat16 Value = 0.0f;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector2f:
				{
					FVector2f Value(0.0f);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector3f:
				{
					FVector3f Value(0.0f);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector4f:
				{
					FVector4f Value(0.0f);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Double:
				{
					double Value = 0.0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector2d:
				{
					FVector2D Value(0.0);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector3d:
				{
					FVector3d Value(0.0);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector4d:
				{
					FVector4d Value(0.0);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::String:
				{
					FString Value;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
			}
			if (MetaDataValue.IsSet())
			{
				const FString& MetaDataStringValue = MetaDataValue.GetValue();
				const FName& MetaDataKey = FName(InterchangeMetaDataPrefix + UserAttributeInfo.Name);
				//SetValue either add the key or set the new value
				MetaData->SetValue(CreatedAsset, MetaDataKey, *MetaDataStringValue);
			}
		}
	}
}
#endif // WITH_EDITORONLY_DATA

void UInterchangeGenericAssetsPipeline::SetReimportSourceIndex(UClass* ReimportObjectClass, const int32 SourceFileIndex)
{
	if (MaterialPipeline)
	{
		MaterialPipeline->ScriptedSetReimportSourceIndex(ReimportObjectClass, SourceFileIndex);
	}

	if (MeshPipeline)
	{
		MeshPipeline->ScriptedSetReimportSourceIndex(ReimportObjectClass, SourceFileIndex);
	}

	if (AnimationPipeline)
	{
		AnimationPipeline->ScriptedSetReimportSourceIndex(ReimportObjectClass, SourceFileIndex);
	}
}

void UInterchangeGenericAssetsPipeline::ImplementUseSourceNameForAssetOption()
{
	if (bUseSourceNameForAsset)
	{
		const UClass* SkeletalMeshFactoryNodeClass = UInterchangeSkeletalMeshFactoryNode::StaticClass();
		TArray<FString> SkeletalMeshNodeUids;
		BaseNodeContainer->GetNodes(SkeletalMeshFactoryNodeClass, SkeletalMeshNodeUids);

		const UClass* StaticMeshFactoryNodeClass = UInterchangeStaticMeshFactoryNode::StaticClass();
		TArray<FString> StaticMeshNodeUids;
		BaseNodeContainer->GetNodes(StaticMeshFactoryNodeClass, StaticMeshNodeUids);

		const UClass* AnimSequenceFactoryNodeClass = UInterchangeAnimSequenceFactoryNode::StaticClass();
		TArray<FString> AnimSequenceNodeUids;
		BaseNodeContainer->GetNodes(AnimSequenceFactoryNodeClass, AnimSequenceNodeUids);

		//If we import only one mesh, we want to rename the mesh using the file name.
		const int32 MeshesImportedNodeCount = SkeletalMeshNodeUids.Num() + StaticMeshNodeUids.Num();

		//SkeletalMesh
		MeshPipeline->ImplementUseSourceNameForAssetOptionSkeletalMesh(MeshesImportedNodeCount, bUseSourceNameForAsset);

		//StaticMesh
		if (MeshesImportedNodeCount == 1 && StaticMeshNodeUids.Num() > 0)
		{
			UInterchangeStaticMeshFactoryNode* StaticMeshNode = Cast<UInterchangeStaticMeshFactoryNode>(BaseNodeContainer->GetFactoryNode(StaticMeshNodeUids[0]));
			const FString DisplayLabelName = FPaths::GetBaseFilename(SourceDatas[0]->GetFilename());
			StaticMeshNode->SetDisplayLabel(DisplayLabelName);
		}

		//Animation, simply look if we import only 1 animation before applying the option to animation
		if (AnimSequenceNodeUids.Num() == 1)
		{
			UInterchangeAnimSequenceFactoryNode* AnimSequenceNode = Cast<UInterchangeAnimSequenceFactoryNode>(BaseNodeContainer->GetFactoryNode(AnimSequenceNodeUids[0]));
			const FString DisplayLabelName = FPaths::GetBaseFilename(SourceDatas[0]->GetFilename()) + TEXT("_Anim");
			AnimSequenceNode->SetDisplayLabel(DisplayLabelName);
		}
	}

}

