// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"

#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Containers/IndirectArray.h"
#include "Containers/StringConv.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureLODSettings.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMisc.h"
#include "Interfaces/ITargetPlatform.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Logging/TokenizedMessage.h"
#include "Math/BoxSphereBounds.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectIdentifier.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceGroupProjector.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceModifier.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceSurface.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialBase.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExtendMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialVariation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipWithMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTexture.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureBinarise.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureColourMap.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureFromChannels.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureInterpolate.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureInvert.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureLayer.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureProject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureSwitch.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureToChannels.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureTransform.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureVariation.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "MuR/Mesh.h"
#include "MuR/Ptr.h"
#include "MuR/Skeleton.h"
#include "MuT/Node.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeComponentEdit.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeMeshApplyPose.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodeSurfaceEdit.h"
#include "MuT/NodeSurfaceNew.h"
#include "PerPlatformProperties.h"
#include "PerQualityLevelProperties.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PlatformInfo.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Templates/Casts.h"
#include "Templates/ChooseClass.h"
#include "Templates/SubclassOf.h"
#include "Trace/Detail/Channel.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


// Forward declarations 
bool AffectsCurrentComponent(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext);


uint32 GetTypeHash(const FGraphCycleKey& Key)
{
	return HashCombine(GetTypeHash(&Key.Pin), GetTypeHash(Key.Id));
}


FGraphCycle::FGraphCycle(const FGraphCycleKey&& Key, FMutableGraphGenerationContext& Context) :
	Key(Key),
	Context(Context)
{
}


FGraphCycle::~FGraphCycle()
{
	Context.VisitedPins.Remove(Key);
}


bool FGraphCycle::FoundCycle() const
{
	const UCustomizableObjectNode& Node = *Cast<UCustomizableObjectNode>(Key.Pin.GetOwningNode());

	if (const UCustomizableObject** Result = Context.VisitedPins.Find(Key))
	{
		Context.Compiler->CompilerLog(LOCTEXT("CycleFoundNode", "Cycle detected."), &Node, EMessageSeverity::Error, true);
		Context.CustomizableObjectWithCycle = *Result;
		return true;
	}
	else
	{
		Context.VisitedPins.Add(Key, Node.GetGraph()->GetTypedOuter<UCustomizableObject>());
		return false;	
	}
}


/** Warn if the node has more outputs than it is meant to have. */
void CheckNumOutputs(const UEdGraphPin& Pin, const FMutableGraphGenerationContext& GenerationContext)
{
	if (const UCustomizableObjectNode* Typed = Cast<UCustomizableObjectNode>(Pin.GetOwningNode()))
	{
		if (Typed->IsSingleOutputNode())
		{
			int numOutLinks = 0;
			for (UEdGraphPin* NodePin : Typed->GetAllNonOrphanPins())
			{
				if (NodePin->Direction == EGPD_Output)
				{
					numOutLinks += NodePin->LinkedTo.Num();
				}
			}

			if (numOutLinks > 1)
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("MultipleOutgoing", "The node has several outgoing connections, but it should be limited to 1."), CastChecked<UCustomizableObjectNode>(Pin.GetOwningNode()));
			}
		}
	}
}


FMutableGraphGenerationContext::FMutableGraphGenerationContext(UCustomizableObject* InObject, FCustomizableObjectCompiler* InCompiler, const FCompilationOptions& InOptions)
	: Object(InObject), Compiler(InCompiler), Options(InOptions)
{
	// Default flags for mesh generation nodes.
	MeshGenerationFlags.Push(0);
}


mu::MeshPtr FMutableGraphGenerationContext::FindGeneratedMesh( const FGeneratedMeshData::FKey& Key )
{
	for (const FGeneratedMeshData& d : GeneratedMeshes)
	{
		if (d.Key == Key )
		{
			return d.Generated;
		}
	}

	return nullptr;
}


/** Adds to ParameterNamesMap the node Node to the array of elements with name Name */
void FMutableGraphGenerationContext::AddParameterNameUnique(const UCustomizableObjectNode* Node, FString Name)
{
	if (TArray<const UCustomizableObjectNode*>* ArrayResult = ParameterNamesMap.Find(Name))
	{
		ArrayResult->AddUnique(Node);
	}
	else
	{
		TArray<const UCustomizableObjectNode*> ArrayTemp;
		ArrayTemp.Add(Node);
		ParameterNamesMap.Add(Name, ArrayTemp);
	}
}


const FGuid FMutableGraphGenerationContext::GetNodeIdUnique(const UCustomizableObjectNode* Node)
{
	TArray<const UCustomizableObjectNode*>* ArrayResult = NodeIdsMap.Find(Node->NodeGuid);

	if (ArrayResult == nullptr)
	{
		TArray<const UCustomizableObjectNode*> ArrayTemp;
		ArrayTemp.Add(Node);
		NodeIdsMap.Add(Node->NodeGuid, ArrayTemp);
		return Node->NodeGuid;
	}

	ArrayResult->Add(Node);
	return FGuid::NewGuid();
}


void FMutableGraphGenerationContext::GenerateClippingCOInternalTags()
{
	int32 i = 0;
	int32 j;

	// The following array ArrayCachedTaggedMaterialNodes and related functionality was removed because it prevented two clip nodes from
	// clipping the same material node
	// In the case more than one CO clips a particular CO, the associated mu::NodeSurfaceNewPtr nodes with that particular CO only need to be tagged once
	//TArray<FGuid> ArrayCachedTaggedMaterialNodes;

	for (auto It = MapClipMeshNodeToMutableClipMeshNodeArray.CreateConstIterator(); It; ++It)
	{
		FString TagName = FString::Printf(TEXT("ClippingTag_%d"), i);
		i++;

		for (j = 0; j < It->Value.Num(); ++j)
		{
			It->Value[j]->AddTag(TCHAR_TO_ANSI(*TagName));
		}

		UCustomizableObjectNodeMeshClipWithMesh* CustomizableObjectNodeMeshClipWithMesh = It->Key;

		// Tag also those nodes in the material side of the clipping functionality (since a CO can have several materials with the same LOD,
		// all of them are considered, so CustomizableObjectNodeMeshClipWithMesh->ArrayMaterialNodeToClipWithID contains the Guids of all
		// the material nodes with the same name selected for being affected by the clipping operation
		TArray<FGuid>& ArrayMaterialNodeToClipWithID = CustomizableObjectNodeMeshClipWithMesh->ArrayMaterialNodeToClipWithID;

		for (j = 0; j < ArrayMaterialNodeToClipWithID.Num(); ++j)
		{
			// The following lines were commented for the same reasons as ArrayCachedTaggedMaterialNodes above
			//if (ArrayCachedTaggedMaterialNodes.Find(CustomizableObjectNodeMeshClipWithMesh->ArrayMaterialNodeToClipWithID[j]) != INDEX_NONE)
			//{
			//    continue;
			//}

			bool FoundEntryInMap = false;
			for (auto It2 = MapMaterialNodeToMutableSurfaceNodeArray.CreateConstIterator(); (It2 && !FoundEntryInMap); ++It2)
			{
				if (It2->Key->NodeGuid == ArrayMaterialNodeToClipWithID[j])
				{
					FoundEntryInMap = true;

					for (int32 k = 0; k < It2->Value.Num(); ++k)
					{
						It2->Value[k]->AddTag(TCHAR_TO_ANSI(*TagName));
					}
				}
			}

			// The following lines were commented for the same reasons as ArrayCachedTaggedMaterialNodes above
			//ArrayCachedTaggedMaterialNodes.Add(CustomizableObjectNodeMeshClipWithMesh->ArrayMaterialNodeToClipWithID[j]);
		}
	}
}


//void FMutableGraphGenerationContext::CheckPhysicsAssetInSkeletalMesh(const USkeletalMesh* SkeletalMesh)
//{
//	if (SkeletalMesh && SkeletalMesh->GetPhysicsAsset() && !DiscartedPhysicsAssetMap.Find(SkeletalMesh->GetPhysicsAsset()))
//	{
//		for (TObjectPtr<USkeletalBodySetup> Bodies : SkeletalMesh->GetPhysicsAsset()->SkeletalBodySetups)
//		{
//			if (Bodies->BoneName != NAME_None && SkeletalMesh->GetRefSkeleton().FindBoneIndex(Bodies->BoneName) == INDEX_NONE)
//			{
//				DiscartedPhysicsAssetMap.Add(SkeletalMesh->GetPhysicsAsset(), DiscartedPhysicsAssetMap.Num());
//				break;
//			}
//		}
//	}
//}


FMutableComponentInfo& FMutableGraphGenerationContext::GetCurrentComponentInfo()
{
	check(ComponentInfos.IsValidIndex(CurrentMeshComponent));
	return ComponentInfos[CurrentMeshComponent];
}


FGeneratedKey::FGeneratedKey(void* InFunctionAddress, const UEdGraphPin& InPin, const UCustomizableObjectNode& Node, FMutableGraphGenerationContext& GenerationContext, const bool UseMesh)
{
	FunctionAddress = InFunctionAddress;
	Pin = &InPin;
	LOD = Node.IsAffectedByLOD() ? GenerationContext.CurrentLOD : 0;

	if (UseMesh)
	{
		Flags = GenerationContext.MeshGenerationFlags.Last();
		MeshMorphStack = GenerationContext.MeshMorphStack;
	}
}


bool FGeneratedKey::operator==(const FGeneratedKey& Other) const
{
	return FunctionAddress == Other.FunctionAddress &&
		Pin == Other.Pin &&
		LOD == Other.LOD &&
		Flags == Other.Flags &&
		MeshMorphStack == Other.MeshMorphStack;
}


uint32 GetTypeHash(const FGeneratedKey& Key)
{
	uint32 Hash = GetTypeHash(Key.FunctionAddress);
	Hash = HashCombine(Hash, GetTypeHash(Key.Pin));
	Hash = HashCombine(Hash, GetTypeHash(Key.LOD));
	Hash = HashCombine(Hash, GetTypeHash(Key.Flags));
	//Hash = HashCombine(Hash, GetTypeHash(Key.MeshMorphStack)); // Does not support array
	
	return Hash;
}


void FPinDataValue::Append(const FPinDataValue& From)
{
	MeshesData.Append(From.MeshesData);
}


FPinDataValue* FPinData::Find(const UEdGraphPin* Pin)
{
	return Data.Find(Pin);
}


FPinDataValue& FPinData::GetCurrent()
{
	return Data[PinStack.Last()];
}


void FPinData::Pop()
{
	const uint32 Num = PinStack.Num();

	check(Num >= 1); // Pop called without a previous push

	if (Num >= 2)
	{
		Data[PinStack[Num - 2]].Append(Data[PinStack[Num - 1]]);
	}

	PinStack.Pop();
}


void FPinData::Push(const UEdGraphPin* Pin)
{
	PinStack.Push(Pin);
	Data.FindOrAdd(Pin);
}


FGraphCycleKey::FGraphCycleKey(const UEdGraphPin& Pin, uint32 Id) :
	Pin(Pin),
	Id(Id)
{
}


bool FGraphCycleKey::operator==(const FGraphCycleKey& Other) const
{
	return &Pin == &Other.Pin && Id == Other.Id;
}


FScopedPinData::FScopedPinData(FMutableGraphGenerationContext& Context, const UEdGraphPin* Pin)
	: Context(Context)
{
	Context.PinData.Push(Pin);
}


FScopedPinData::~FScopedPinData()
{
	Context.PinData.Pop();
}


UTexture2D* FindReferenceImage(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);
	
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	const UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	UTexture2D* Result = nullptr;

	if (const UCustomizableObjectNodeTexture* TypedNodeTex = Cast<UCustomizableObjectNodeTexture>(Node))
	{
		Result = TypedNodeTex->Texture;
	}

	else if (const UCustomizableObjectNodeTextureParameter* ParamNodeTex = Cast<UCustomizableObjectNodeTextureParameter>(Node))
	{
		Result = ParamNodeTex->DefaultValue;
	}

	else if (const UCustomizableObjectNodeMesh* TypedNodeMesh = Cast<UCustomizableObjectNodeMesh>(Node))
	{
		Result = TypedNodeMesh->FindTextureForPin(Pin);
	}

	else if (const UCustomizableObjectNodeTextureInterpolate* TypedNodeInterp = Cast<UCustomizableObjectNodeTextureInterpolate>(Node))
	{
		for (int LayerIndex = 0; !Result && LayerIndex < TypedNodeInterp->GetNumTargets(); ++LayerIndex)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeInterp->Targets(LayerIndex)))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureLayer* TypedNodeLayer = Cast<UCustomizableObjectNodeTextureLayer>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeLayer->BasePin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}

		for (int LayerIndex = 0; !Result && LayerIndex < TypedNodeLayer->GetNumLayers(); ++LayerIndex)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeLayer->LayerPin(LayerIndex)))
			{
				if (ConnectedPin->PinType.PinCategory == Helper_GetPinCategory(Schema->PC_Image))
				{
					Result = FindReferenceImage(ConnectedPin, GenerationContext);
				}
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureSwitch* TypedNodeSwitch = Cast<UCustomizableObjectNodeTextureSwitch>(Node))
	{
		for (int SelectorIndex = 0; !Result && SelectorIndex < TypedNodeSwitch->GetNumElements(); ++SelectorIndex)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeSwitch->GetElementPin(SelectorIndex)))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureVariation* TypedNodeVariation = Cast<UCustomizableObjectNodeTextureVariation>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeVariation->DefaultPin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}

		for (int SelectorIndex = 0; !Result && SelectorIndex < TypedNodeVariation->GetNumVariations(); ++SelectorIndex)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeVariation->VariationPin(SelectorIndex)))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureFromChannels* TypedNodeFrom = Cast<UCustomizableObjectNodeTextureFromChannels>(Node))
	{
		mu::NodeImagePtr RNode, GNode, BNode, ANode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->RPin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}
		if (!Result)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->GPin()))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
		if (!Result)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->BPin()))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
			
		}
		if (!Result)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->APin()))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureToChannels* TypedNodeTo = Cast<UCustomizableObjectNodeTextureToChannels>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTo->InputPin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTextureProject* TypedNodeProj = Cast<UCustomizableObjectNodeTextureProject>(Node))
	{
		int TexIndex = -1;// TypedNodeProj->OutputPins.Find((UEdGraphPin*)Pin);
		for (int32 i = 0; i < TypedNodeProj->GetNumOutputs(); ++i)
		{
			if (TypedNodeProj->OutputPins(i) == Pin)
			{
				TexIndex = i;
			}
		}

		check(TexIndex >= 0 && TexIndex < TypedNodeProj->GetNumTextures());

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProj->TexturePins(TexIndex)))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTextureBinarise* TypedNodeBin = Cast<UCustomizableObjectNodeTextureBinarise>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeBin->GetBaseImagePin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTextureInvert* TypedNodeInv = Cast<UCustomizableObjectNodeTextureInvert>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeInv->GetBaseImagePin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTextureColourMap* TypedNodeColourMap = Cast<UCustomizableObjectNodeTextureColourMap>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeColourMap->GetBasePin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTextureTransform* TypedNodeTransform = Cast<UCustomizableObjectNodeTextureTransform>(Node))
	{
		if ( UEdGraphPin* BaseImagePin = FollowInputPin(*TypedNodeTransform->GetBaseImagePin()) )
		{
			Result = FindReferenceImage(BaseImagePin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(Node))
	{
		if (Pin->PinType.PinCategory == Helper_GetPinCategory(Schema->PC_MaterialAsset))
		{
			Result = TypedNodeTable->FindReferenceTextureParameter(Pin, GenerationContext.CurrentMaterialTableParameter);
		}
		else
		{
			Result = TypedNodeTable->GetColumnDefaultAssetByType<UTexture2D>(Pin);
		}
	}
	
	return Result;
}


mu::NodeMeshApplyPosePtr CreateNodeMeshApplyPose(mu::NodeMeshPtr InputMeshNode, UCustomizableObject * CustomizableObject, TArray<FString> ArrayBoneName, TArray<FTransform> ArrayTransform)
{
	check(ArrayBoneName.Num() == ArrayTransform.Num());

	mu::MeshPtr MutableMesh = new mu::Mesh();
	mu::NodeMeshConstantPtr PoseNodeMesh = new mu::NodeMeshConstant;
	PoseNodeMesh->SetValue(MutableMesh);

	mu::SkeletonPtr MutableSkeleton = new mu::Skeleton;
	MutableMesh->SetSkeleton(MutableSkeleton);
	MutableMesh->SetBonePoseCount(ArrayBoneName.Num());
	MutableSkeleton->SetBoneCount(ArrayBoneName.Num());

	for (int32 i = 0; i < ArrayBoneName.Num(); ++i)
	{
		MutableSkeleton->SetBoneName(i, TCHAR_TO_ANSI(*ArrayBoneName[i]));
		MutableMesh->SetBonePose(i, TCHAR_TO_ANSI(*ArrayBoneName[i]), (FTransform3f)ArrayTransform[i], true);
	}

	mu::NodeMeshApplyPosePtr NodeMeshApplyPose = new mu::NodeMeshApplyPose;
	NodeMeshApplyPose->SetBase(InputMeshNode);
	NodeMeshApplyPose->SetPose(PoseNodeMesh);

	return NodeMeshApplyPose;
}


// Convert a CustomizableObject Source Graph into a mutable source graph  
mu::NodeObjectPtr GenerateMutableSource(const UEdGraphPin * Pin, FMutableGraphGenerationContext & GenerationContext, bool bPartialCompilation)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);
	
	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());
	
	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSource), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeObject*>(Generated->Node.get());
	}

	mu::NodeObjectPtr Result;
	
	if (const UCustomizableObjectNodeObject* TypedNodeObj = Cast<UCustomizableObjectNodeObject>(Node))
	{
		// Add it to the current processing hierarchy if we have none
		// \TODO: or special flag starting a separate component in the future.
		bool bStartedComponent = false;
		if (!GenerationContext.ComponentNewNode.Num())
		{
			GenerationContext.ComponentNewNode.Push(FMutableGraphGenerationContext::ObjectParent());
			bStartedComponent = true;
		}

		mu::NodeObjectNewPtr ObjectNode = new mu::NodeObjectNew();
		Result = ObjectNode;

		ObjectNode->SetName(TCHAR_TO_ANSI(*TypedNodeObj->ObjectName));
		ObjectNode->SetUid(TCHAR_TO_ANSI(*TypedNodeObj->NodeGuid.ToString()));


		// LOD
		int NumLODs = TypedNodeObj->GetNumLODPins();

		// Fill the basic LOD Settings
		if (!GenerationContext.NumLODsInRoot)
		{
			check(!GenerationContext.ComponentInfos.IsEmpty());

			// NumLODsInRoot
			int32 MaxRefMeshLODs = 1;
			for (int32 MeshIndex = 0; MeshIndex < GenerationContext.ComponentInfos.Num(); ++MeshIndex)
			{
				const USkeletalMesh* RefSkeletalMesh = GenerationContext.ComponentInfos[MeshIndex].RefSkeletalMesh;
				if (RefSkeletalMesh && RefSkeletalMesh->GetLODNum() > MaxRefMeshLODs)
				{
					MaxRefMeshLODs = RefSkeletalMesh->GetLODNum();
				}
			}

			if (MaxRefMeshLODs < NumLODs)
			{
				FString Msg = FString::Printf(TEXT("The object has %d LODs but the reference mesh only %d. Resulting objects will have %d LODs."),
					NumLODs, MaxRefMeshLODs, MaxRefMeshLODs);
				GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node, EMessageSeverity::Warning);
				GenerationContext.NumLODsInRoot = MaxRefMeshLODs;
			}
			else
			{
				GenerationContext.NumLODsInRoot = NumLODs;
			}
		
			USkeletalMesh* RefSkeletalMesh = GenerationContext.ComponentInfos[0].RefSkeletalMesh;
			check(RefSkeletalMesh);

			const FMutableLODSettings& LODSettings = GenerationContext.Object->LODSettings;

			// Find the MinLOD available for the target platform
			if (RefSkeletalMesh->IsMinLodQualityLevelEnable())
			{
				GenerationContext.FirstLODAvailable = LODSettings.MinQualityLevelLOD.GetValueForPlatform(GenerationContext.Options.TargetPlatform);
			}
			else
			{
				GenerationContext.FirstLODAvailable = LODSettings.MinLOD.GetValueForPlatform(*GenerationContext.Options.TargetPlatform->IniPlatformName());
			}

			GenerationContext.FirstLODAvailable = FMath::Clamp(GenerationContext.FirstLODAvailable, 0, GenerationContext.NumLODsInRoot - 1);

			// Find the streaming settings for the target platform
			if (LODSettings.bOverrideLODStreamingSettings)
			{
				GenerationContext.bEnableLODStreaming = LODSettings.bEnableLODStreaming.GetValueForPlatform(*GenerationContext.Options.TargetPlatform->IniPlatformName());
				GenerationContext.NumMaxLODsToStream = LODSettings.NumMaxStreamedLODs.GetValueForPlatform(*GenerationContext.Options.TargetPlatform->IniPlatformName());
			}
			else
			{
				for (int32 MeshIndex = 0; MeshIndex < GenerationContext.ComponentInfos.Num(); ++MeshIndex)
				{
					RefSkeletalMesh = GenerationContext.ComponentInfos[MeshIndex].RefSkeletalMesh;
					check(RefSkeletalMesh);

					GenerationContext.bEnableLODStreaming = GenerationContext.bEnableLODStreaming &&
						RefSkeletalMesh->GetEnableLODStreaming(GenerationContext.Options.TargetPlatform);

					GenerationContext.NumMaxLODsToStream = FMath::Min(GenerationContext.NumMaxLODsToStream,
						RefSkeletalMesh->GetMaxNumStreamedLODs(GenerationContext.Options.TargetPlatform));
				}
			}
		}


		// States
		int NumStates = TypedNodeObj->States.Num();
		ObjectNode->SetStateCount(NumStates);

		// In a partial compilation we will filter the states of the root object
		bool bFilterStates = true;

		if (bPartialCompilation)
		{
			if (!TypedNodeObj->ParentObject)
			{
				bFilterStates = false;
			}
		}

		for (int StateIndex = 0; StateIndex < NumStates && bFilterStates; ++StateIndex)
		{
			const FCustomizableObjectState& State = TypedNodeObj->States[StateIndex];
			ObjectNode->SetStateName(StateIndex, TCHAR_TO_ANSI(*State.Name));
			for (int ParamIndex = 0; ParamIndex < State.RuntimeParameters.Num(); ++ParamIndex)
			{
				ObjectNode->AddStateParam(StateIndex, TCHAR_TO_ANSI(*State.RuntimeParameters[ParamIndex]));
			}
			ObjectNode->SetStateProperties(StateIndex, State.bDontCompressRuntimeTextures, State.bBuildOnlyFirstLOD, GenerationContext.FirstLODAvailable);

			// UI Data
			FParameterUIData ParameterUIData(State.Name, State.StateUIMetadata, EMutableParameterType::None);
			ParameterUIData.bDontCompressRuntimeTextures = State.bDontCompressRuntimeTextures;
			ParameterUIData.ForcedParameterValues = State.ForcedParameterValues;

			GenerationContext.StateUIDataMap.Add(State.Name, ParameterUIData);
		}

		// Update the current automatic LOD policy
		ECustomizableObjectAutomaticLODStrategy LastLODStrategy = GenerationContext.CurrentAutoLODStrategy;
		if (TypedNodeObj->AutoLODStrategy != ECustomizableObjectAutomaticLODStrategy::Inherited)
		{
			GenerationContext.CurrentAutoLODStrategy = TypedNodeObj->AutoLODStrategy;
		}

		// UI Data
		GenerationContext.ParameterUIDataMap.Add(TypedNodeObj->ObjectName, FParameterUIData(
			TypedNodeObj->ObjectName,
			TypedNodeObj->ParamUIMetadata,
			EMutableParameterType::Int));

		// Mesh components per instance
		if (!GenerationContext.NumMeshComponentsInRoot)
		{
			GenerationContext.NumMeshComponentsInRoot = TypedNodeObj->NumMeshComponents;
		}

		while (GenerationContext.ComponentNewNode.Last().ComponentsPerLOD.Num() < GenerationContext.NumLODsInRoot)
		{
			TArray<mu::NodeComponentNewPtr> AuxArray;
			AuxArray.AddZeroed(GenerationContext.NumMeshComponentsInRoot);
			GenerationContext.ComponentNewNode.Last().ComponentsPerLOD.Add(AuxArray);
		}

		int32 LastDefinedLOD = -1;
		ObjectNode->SetLODCount(GenerationContext.NumLODsInRoot);
		for (int32 LODIndex = 0; LODIndex < GenerationContext.NumLODsInRoot; ++LODIndex)
		{
			GenerationContext.CurrentLOD = LODIndex;

			mu::NodeLODPtr LODNode = new mu::NodeLOD();
			ObjectNode->SetLOD(LODIndex, LODNode);
			LODNode->SetMessageContext(Node);
			LODNode->SetComponentCount(GenerationContext.NumMeshComponentsInRoot);

			for (int32 MeshComponentIndex = 0; MeshComponentIndex < GenerationContext.NumMeshComponentsInRoot; ++MeshComponentIndex)
			{
				GenerationContext.CurrentMeshComponent = MeshComponentIndex;
				mu::NodeComponentPtr ComponentNode;

				if (!GenerationContext.ComponentNewNode.Last().ComponentsPerLOD[LODIndex][MeshComponentIndex])
				{
					mu::NodeComponentNewPtr ComponentNewNode = new mu::NodeComponentNew();
					ComponentNewNode->SetMessageContext(Node);
					ComponentNewNode->SetId(MeshComponentIndex);
					GenerationContext.ComponentNewNode.Last().ComponentsPerLOD[LODIndex][MeshComponentIndex] = ComponentNewNode;
					ComponentNode = ComponentNewNode;
				}
				else
				{
					mu::NodeComponentEditPtr ComponentEditNode = new mu::NodeComponentEdit();
					ComponentEditNode->SetMessageContext(Node);
					ComponentEditNode->SetParent(GenerationContext.ComponentNewNode.Last().ComponentsPerLOD[LODIndex][MeshComponentIndex].get());
					ComponentNode = ComponentEditNode;
				}

				LODNode->SetComponent(MeshComponentIndex, ComponentNode);

				// Is the LOD defined in this object?
				int32 LODToGenerate = -1;
				if (LODIndex < NumLODs)
				{
					// If it is manual LODs, we don't want to show anything for this node in this LOD
					if (GenerationContext.CurrentAutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh
						&& LastDefinedLOD != INDEX_NONE)
					{
						LODToGenerate = LastDefinedLOD;
					}
					else
					{
						// In automatic LODs, we follow the last LOD
						LastDefinedLOD = LODIndex;
						LODToGenerate = LODIndex;
					}
				}
				else
				{
					// This LOD is not defined in this object, so we assume the last LOD has to be used instead
					LODToGenerate = LastDefinedLOD;
				}

				if (GenerationContext.CurrentLOD < GenerationContext.FirstLODAvailable)
				{
					continue;
				}

				if (LODToGenerate >= 0)
				{
					TArray<UEdGraphPin*> ConnectedLODPins = FollowInputPinArray(*TypedNodeObj->LODPin(LODToGenerate));
					int32 LODConnections = ConnectedLODPins.Num();
					int32 NumMaterials = 0;
					int32 NumModifiers = 0;
					ComponentNode->SetSurfaceCount(NumMaterials);
					for (int32 MatIndex = 0; MatIndex < LODConnections; ++MatIndex)
					{
						const UEdGraphPin* ChildNodePin = ConnectedLODPins[MatIndex];
						if (!AffectsCurrentComponent(ChildNodePin, GenerationContext))
						{
							continue;
						}

						if (Cast<UCustomizableObjectNodeModifierBase>(ChildNodePin->GetOwningNode()))
						{
							mu::NodeModifierPtr MatNode = GenerateMutableSourceModifier(ChildNodePin, GenerationContext);
							LODNode->SetModifierCount(NumModifiers + 1);
							LODNode->SetModifier(NumModifiers, MatNode.get());
							LODNode->SetMessageContext(Node);
							++NumModifiers;
						}
						else
						{
							FMutableGraphSurfaceGenerationData DummySurfaceData;
							mu::NodeSurfacePtr MatNode = GenerateMutableSourceSurface(ChildNodePin, GenerationContext, DummySurfaceData);
							ComponentNode->SetSurfaceCount(NumMaterials + 1);
							ComponentNode->SetSurface(NumMaterials, MatNode.get());
							ComponentNode->SetMessageContext(Node);
							++NumMaterials;
						}
					}
				}
			}
		}

		// Children
		TArray<UEdGraphPin*> ConnectedChildrenPins = FollowInputPinArray(*TypedNodeObj->ChildrenPin());
		ObjectNode->SetChildCount(ConnectedChildrenPins.Num());
		for (int ChildIndex = 0; ChildIndex < ConnectedChildrenPins.Num(); ++ChildIndex)
		{
			mu::NodeObjectPtr ChildNode = GenerateMutableSource(ConnectedChildrenPins[ChildIndex], GenerationContext, bPartialCompilation);
			ObjectNode->SetChild(ChildIndex, ChildNode.get());
		}

		// Remove from the current processing hierarchy
		if (bStartedComponent)
		{
			GenerationContext.ComponentNewNode.Pop();
		}

		GenerationContext.CurrentAutoLODStrategy = LastLODStrategy;
	}

	else if (const UCustomizableObjectNodeObjectGroup* TypedNodeGroup = Cast<UCustomizableObjectNodeObjectGroup>(Node))
	{
		mu::NodeObjectGroupPtr GroupNode = new mu::NodeObjectGroup();
		Result = GroupNode;

		GenerationContext.AddParameterNameUnique(TypedNodeGroup, TypedNodeGroup->GroupName);
		GroupNode->SetName(TCHAR_TO_ANSI(*TypedNodeGroup->GroupName));
		GroupNode->SetUid(TCHAR_TO_ANSI(*TypedNodeGroup->NodeGuid.ToString()));

		// Get all group projectors and put them in the generation context so that they are available to the child material nodes of this group node
		uint32 NumProjectorCountBeforeNode = GenerationContext.ProjectorGroupMap.Num();
		UEdGraphPin* ProjectorsPin = TypedNodeGroup->GroupProjectorsPin();
		
		if (ProjectorsPin && TypedNodeGroup)
		{
			for (const UEdGraphPin* GroupProjectorNodePin : FollowInputPinArray(*ProjectorsPin))
			{
				GenerateMutableSourceGroupProjector(GroupProjectorNodePin, GenerationContext, TypedNodeGroup);
			}
		}

		mu::NodeObjectGroup::CHILD_SELECTION Type = mu::NodeObjectGroup::CS_ALWAYS_ALL;
		switch (TypedNodeGroup->GroupType)
		{
		case ECustomizableObjectGroupType::COGT_ALL: Type = mu::NodeObjectGroup::CS_ALWAYS_ALL; break;
		case ECustomizableObjectGroupType::COGT_TOGGLE: Type = mu::NodeObjectGroup::CS_TOGGLE_EACH; break;
		case ECustomizableObjectGroupType::COGT_ONE: Type = mu::NodeObjectGroup::CS_ALWAYS_ONE; break;
		case ECustomizableObjectGroupType::COGT_ONE_OR_NONE: Type = mu::NodeObjectGroup::CS_ONE_OR_NONE; break;
		default:
			GenerationContext.Compiler->CompilerLog(LOCTEXT("UnsupportedGroupType", "Object Group Type not supported. Setting to 'ALL'."), Node);
			break;
		}
		GroupNode->SetSelectionType(Type);

		// External children
		TArray<UCustomizableObjectNodeObject*> ExternalChildNodes;
		GenerationContext.GroupIdToExternalNodeMap.MultiFind(TypedNodeGroup->NodeGuid, ExternalChildNodes);
		GenerationContext.GuidToParamNameMap.Add(TypedNodeGroup->NodeGuid, TypedNodeGroup->GroupName);

		// Children
		const TArray<UEdGraphPin*> ConnectedChildrenPins = FollowInputPinArray(*TypedNodeGroup->ObjectsPin());
		const int32 NumChildren = ConnectedChildrenPins.Num();
		const int32 TotalNumChildren = NumChildren + ExternalChildNodes.Num();

		GroupNode->SetChildCount(TotalNumChildren);
		int32 ChildIndex = 0;

		// UI data
		FParameterUIData ParameterUIData(
			TypedNodeGroup->GroupName,
			TypedNodeGroup->ParamUIMetadata,
			EMutableParameterType::Int);

		ParameterUIData.IntegerParameterGroupType = TypedNodeGroup->GroupType;

		// In the case of a partial compilation, make sure at least one child is connected so that the param is no optimized
		bool bAtLeastOneConnected = false;

		for (; ChildIndex < NumChildren; ++ChildIndex)
		{
			bool bLastChildNode = (ChildIndex == NumChildren - 1) && (ExternalChildNodes.Num() == 0);
			bool bConnectAtLeastTheLastChild = bLastChildNode && !bAtLeastOneConnected;

			UCustomizableObjectNodeObject* CustomizableObjectNodeObject = Cast<UCustomizableObjectNodeObject>(ConnectedChildrenPins[ChildIndex]->GetOwningNode());

			FString* SelectedOptionName = GenerationContext.ParamNamesToSelectedOptions.Find(TypedNodeGroup->GroupName); // If the param is in the map restrict to only the selected option

			if (bConnectAtLeastTheLastChild || !SelectedOptionName || *SelectedOptionName == CustomizableObjectNodeObject->ObjectName)
			{
				bAtLeastOneConnected = true;

				mu::NodeObjectPtr ChildNode = GenerateMutableSource(ConnectedChildrenPins[ChildIndex], GenerationContext, bPartialCompilation);
				GroupNode->SetChild(ChildIndex, ChildNode.get());

				if (CustomizableObjectNodeObject)
				{
					FString LeftSplit = CustomizableObjectNodeObject->GetPathName();
					LeftSplit.Split(".", &LeftSplit, nullptr);
					GenerationContext.CustomizableObjectPathMap.Add(CustomizableObjectNodeObject->Identifier.ToString(), LeftSplit);
					GenerationContext.GroupNodeMap.Add(CustomizableObjectNodeObject->Identifier.ToString(), FCustomizableObjectIdPair(TypedNodeGroup->GroupName, ChildNode->GetName()));
					ParameterUIData.ArrayIntegerParameterOption.Add(FIntegerParameterUIData(
						CustomizableObjectNodeObject->ObjectName,
						CustomizableObjectNodeObject->ParamUIMetadata));

					if (TypedNodeGroup->GroupType == ECustomizableObjectGroupType::COGT_TOGGLE)
					{
						GenerationContext.AddParameterNameUnique(CustomizableObjectNodeObject, CustomizableObjectNodeObject->ObjectName);
					}
				}
			}
			else
			{
				mu::NodeObjectPtr ChildNode = new mu::NodeObjectNew;
				ChildNode->SetName(TCHAR_TO_ANSI(*CustomizableObjectNodeObject->ObjectName));
				GroupNode->SetChild(ChildIndex, ChildNode.get());
			}
		}

		const bool bCollapseUnderParent = TypedNodeGroup->ParamUIMetadata.ExtraInformation.Find(FString("CollapseUnderParent")) != nullptr;
		constexpr bool bHideWhenNotSelected = true; //TypedNodeGroup->ParamUIMetadata.ExtraInformation.Find(FString("HideWhenNotSelected"));
		
		if (bCollapseUnderParent || bHideWhenNotSelected)
		{
			if (const UEdGraphPin* ConnectedPin = FollowOutputPin(*Pin))
			{
				UCustomizableObjectNodeObject* NodeObject = Cast<UCustomizableObjectNodeObject>(ConnectedPin->GetOwningNode());

				if (NodeObject && NodeObject->bIsBase)
				{
					const FGuid* ParentId = GenerationContext.GroupIdToExternalNodeMap.FindKey(NodeObject);

					if (ParentId)
					{
						FString* ParentParamName = GenerationContext.GuidToParamNameMap.Find(*ParentId);

						if (ParentParamName)
						{
							ParameterUIData.ParamUIMetadata.ExtraInformation.Add(FString("__ParentParamName"), *ParentParamName);

							if (bHideWhenNotSelected)
							{
								ParameterUIData.ParamUIMetadata.ExtraInformation.Add(FString("__DisplayWhenParentValueEquals"), NodeObject->ObjectName);
							}

							if (bCollapseUnderParent)
							{
								ParameterUIData.ParamUIMetadata.ExtraInformation.Add(FString("CollapseUnderParent"));

								FParameterUIData ParentParameterUIData;
								ParentParameterUIData.ParamUIMetadata.ExtraInformation.Add(FString("__HasCollapsibleChildren"));
								GenerationContext.ParameterUIDataMap.Add(*ParentParamName, ParentParameterUIData);
							}
						}
					}
				}
			}
		}

		// Build external objects that reference this object as parent
		const int32 NumExternalChildren = FMath::Max(0, TotalNumChildren - NumChildren);
		for (int32 ExternalChildIndex = 0; ExternalChildIndex < NumExternalChildren; ++ExternalChildIndex)
		{
			const UCustomizableObjectNodeObject* ExternalChildNode = ExternalChildNodes[ExternalChildIndex];
			bool bLastExternalChildNode = ExternalChildIndex == ExternalChildNodes.Num() - 1;
			bool bConnectAtLeastTheLastChild = bLastExternalChildNode && !bAtLeastOneConnected;

			UCustomizableObjectNodeObject* CustomizableObjectNodeObject = Cast<UCustomizableObjectNodeObject>(ExternalChildNode->OutputPin()->GetOwningNode());

			FString* SelectedOptionName = GenerationContext.ParamNamesToSelectedOptions.Find(TypedNodeGroup->GroupName); // If the param is in the map restrict to only the selected option

			if (bConnectAtLeastTheLastChild || !SelectedOptionName || *SelectedOptionName == CustomizableObjectNodeObject->ObjectName)
			{
				bAtLeastOneConnected = true;

				mu::NodeObjectPtr ChildNode = GenerateMutableSource(ExternalChildNode->OutputPin(), GenerationContext, bPartialCompilation);
				GroupNode->SetChild(ChildIndex, ChildNode.get());

				if (CustomizableObjectNodeObject)
				{
					FString LeftSplit = ExternalChildNode->GetPathName();
					LeftSplit.Split(".", &LeftSplit, nullptr);
					GenerationContext.CustomizableObjectPathMap.Add(CustomizableObjectNodeObject->Identifier.ToString(), LeftSplit);
					GenerationContext.GroupNodeMap.Add(CustomizableObjectNodeObject->Identifier.ToString(), FCustomizableObjectIdPair(TypedNodeGroup->GroupName, ChildNode->GetName()));
					ParameterUIData.ArrayIntegerParameterOption.Add(FIntegerParameterUIData(
						CustomizableObjectNodeObject->ObjectName,
						CustomizableObjectNodeObject->ParamUIMetadata));

					if (CustomizableObjectNodeObject->ObjectName.IsEmpty())
					{
						GenerationContext.NoNameNodeObjectArray.AddUnique(CustomizableObjectNodeObject);
					}

					if (TypedNodeGroup->GroupType == ECustomizableObjectGroupType::COGT_TOGGLE)
					{
						GenerationContext.AddParameterNameUnique(CustomizableObjectNodeObject, CustomizableObjectNodeObject->ObjectName);
					}
				}
			}
			else
			{
				mu::NodeObjectPtr ChildNode = new mu::NodeObjectNew;
				ChildNode->SetName(TCHAR_TO_ANSI(*CustomizableObjectNodeObject->ObjectName));
				GroupNode->SetChild(ChildIndex, ChildNode.get());
			}

			ChildIndex++;
		}

		const FParameterUIData* ChildFilledUIData = GenerationContext.ParameterUIDataMap.Find(TypedNodeGroup->GroupName);
		if (ChildFilledUIData && ChildFilledUIData->ParamUIMetadata.ExtraInformation.Find(FString("__HasCollapsibleChildren")))
		{
			// Some child param filled the HasCollapsibleChildren UI info, refill it so it's not lost
			ParameterUIData.ParamUIMetadata.ExtraInformation.Add(FString("__HasCollapsibleChildren"));
		}

		if (TypedNodeGroup->GroupType == ECustomizableObjectGroupType::COGT_TOGGLE)
		{
			for (const FIntegerParameterUIData& BooleanParam : ParameterUIData.ArrayIntegerParameterOption)
			{
				FParameterUIData ParameterUIDataBoolean(
					BooleanParam.Name,
					BooleanParam.ParamUIMetadata,
					EMutableParameterType::Bool);

				GenerationContext.ParameterUIDataMap.Add(BooleanParam.Name, ParameterUIDataBoolean);
			}
		}
		else
		{
			GenerationContext.ParameterUIDataMap.Add(TypedNodeGroup->GroupName, ParameterUIData);
		}

		// Remove the projectors from this node
		GenerationContext.ProjectorGroupMap.Remove(TypedNodeGroup);

		check(NumProjectorCountBeforeNode == GenerationContext.ProjectorGroupMap.Num());
	}
	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	GenerationContext.GeneratedNodes.Add(Node);

	return Result;
}

bool AffectsCurrentComponent(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext)
{
	check(Pin);
	RETURN_ON_CYCLE(*Pin, GenerationContext)
	
	int32 ComponentIndex = INDEX_NONE;

	const UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());
	if (const UCustomizableObjectNodeMaterialVariation* TypedNodeVar = Cast<UCustomizableObjectNodeMaterialVariation>(Node))
	{
		bool bAffectsCurrentComponent = false;

		for (const UEdGraphPin* ConnectedPin : FollowInputPinArray(*TypedNodeVar->DefaultPin()))
		{
			if (!AffectsCurrentComponent(ConnectedPin, GenerationContext))
			{
				if (bAffectsCurrentComponent)
				{
					FString Msg = FString::Printf(TEXT("Error! One or more materials nodes linked to a material variation node have different component index"));
					GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node, EMessageSeverity::Error);
				}

				return false;
			}
		
			bAffectsCurrentComponent = true;
		}

		for (int VariationIndex = 0; VariationIndex < TypedNodeVar->Variations.Num(); ++VariationIndex)
		{
			if (UEdGraphPin* VariationPin = TypedNodeVar->VariationPin(VariationIndex))
			{
				for (const UEdGraphPin* ConnectedPin : FollowInputPinArray(*VariationPin))
				{
					if (!AffectsCurrentComponent(ConnectedPin, GenerationContext))
					{
						if (bAffectsCurrentComponent)
						{
							FString Msg = FString::Printf(TEXT("Error! One or more materials nodes linked to a material variation node have different component index"));
							GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node, EMessageSeverity::Error);
						}

						return false;
					}

					bAffectsCurrentComponent = true;
				}
			}
		}

		return bAffectsCurrentComponent;
	}
	else if (const UCustomizableObjectNodeMaterial* TypedNodeMat = Cast<UCustomizableObjectNodeMaterial>(Node))
	{
		ComponentIndex = TypedNodeMat->MeshComponentIndex;
	}
	else if (const UCustomizableObjectNodeExtendMaterial* TypedNodeExt = Cast<UCustomizableObjectNodeExtendMaterial>(Node))
	{
		UCustomizableObjectNodeMaterial* OriginalParentMaterialNode = TypedNodeExt->GetParentMaterialNode();
		ComponentIndex = OriginalParentMaterialNode ? OriginalParentMaterialNode->MeshComponentIndex : GenerationContext.CurrentMeshComponent;
	}
	else if (const UCustomizableObjectNodeEditMaterialBase* TypedNodeEdit = Cast<UCustomizableObjectNodeEditMaterialBase>(Node))
	{
		UCustomizableObjectNodeMaterial* OriginalParentMaterialNode = TypedNodeEdit->GetParentMaterialNode();
		ComponentIndex = OriginalParentMaterialNode ? OriginalParentMaterialNode->MeshComponentIndex : GenerationContext.CurrentMeshComponent;
	}
	else if (const UCustomizableObjectNodeModifierBase* TypedNodeModifier = Cast<UCustomizableObjectNodeModifierBase>(Node))
	{
		// Modifiers affect all components with the exception of the UCustomizableObjectNodeMeshClipMorph
		if (const UCustomizableObjectNodeMeshClipMorph* TypedNodeMeshClipMorph = Cast<UCustomizableObjectNodeMeshClipMorph>(Node))
		{
			ComponentIndex = TypedNodeMeshClipMorph->ReferenceSkeletonIndex;
		}
		else
		{
			return true;
		}
	}
	else
	{
		unimplemented()
	}

	return ComponentIndex == GenerationContext.CurrentMeshComponent;
}


FString GenerateAnimationInstanceTag(const FString& AnimInstance, int32 SlotIndex)
{
	return FString("__AnimBP:") + FString::Printf(TEXT("%d_Slot_"), SlotIndex) + AnimInstance;
}


FString GenerateGameplayTag(const FString& GameplayTag)
{
	return FString("__AnimBPTag:") + GameplayTag;
}


void PopulateReferenceSkeletalMeshesData(FMutableGraphGenerationContext& GenerationContext)
{
	const FString PlatformName = GenerationContext.Options.TargetPlatform->IniPlatformName();
	
	const uint32 LODCount = GenerationContext.NumLODsInRoot;
	const uint32 ComponentCount = GenerationContext.NumMeshComponentsInRoot;

	GenerationContext.ReferenceSkeletalMeshesData.AddDefaulted(ComponentCount);
	for(uint32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
	{
		const USkeletalMesh* RefSkeletalMesh = GenerationContext.ComponentInfos[ComponentIndex].RefSkeletalMesh;
		check(RefSkeletalMesh);

		// Gather LODData, this may include per LOD settings such as render data config or LODDataInfoArray
		FMutableRefSkeletalMeshData& Data = GenerationContext.ReferenceSkeletalMeshesData[ComponentIndex];
		Data.LODData.AddDefaulted(GenerationContext.NumLODsInRoot);
		
		const uint32 RefSkeletalMeshLODCount = RefSkeletalMesh->GetLODNum();
		const TArray<FSkeletalMeshLODInfo>& LODInfos = RefSkeletalMesh->GetLODInfoArray();

		for(uint32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
		{
			FMutableRefLODData& LODData = Data.LODData[LODIndex];
			if(LODIndex < RefSkeletalMeshLODCount)
			{
				// Copy LOD info data from the reference skeletal mesh
				const FSkeletalMeshLODInfo& LODInfo = LODInfos[LODIndex];
				LODData.LODInfo.ScreenSize = LODInfo.ScreenSize.GetValueForPlatform(*PlatformName);
				LODData.LODInfo.LODHysteresis = LODInfo.LODHysteresis;
				LODData.LODInfo.bSupportUniformlyDistributedSampling = LODInfo.bSupportUniformlyDistributedSampling;
				LODData.LODInfo.bAllowCPUAccess = LODInfo.bAllowCPUAccess;

				// Copy Render data settings from the reference skeletal mesh
				const FSkeletalMeshLODRenderData& ReferenceLODModel = Helper_GetLODData(RefSkeletalMesh)[LODIndex];
				LODData.RenderData.bIsLODOptional = ReferenceLODModel.bIsLODOptional;
				LODData.RenderData.bStreamedDataInlined = ReferenceLODModel.bStreamedDataInlined;
			}
			else
			{
				LODData.LODInfo.ScreenSize = 0.3f / (LODIndex + 1);
				LODData.LODInfo.LODHysteresis = 0.02f;				
			}
		}

		// Gather SkeletalMesh Sockets;
		const TArray<USkeletalMeshSocket*>& RefSkeletonSockets = RefSkeletalMesh->GetMeshOnlySocketList();
		const uint32 SocketCount = RefSkeletonSockets.Num();

		Data.Sockets.AddDefaulted(SocketCount);
		for(uint32 SocketIndex = 0 ; SocketIndex < SocketCount; ++SocketIndex)
		{
			const USkeletalMeshSocket* RefSocket = RefSkeletonSockets[SocketIndex];
			check(RefSocket);

			FMutableRefSocket& Socket = Data.Sockets[SocketIndex];
			Socket.SocketName = RefSocket->SocketName;
			Socket.BoneName = RefSocket->BoneName;
			Socket.RelativeLocation = RefSocket->RelativeLocation;
			Socket.RelativeRotation = RefSocket->RelativeRotation;
			Socket.RelativeScale = RefSocket->RelativeScale;
		}

		// TODO: Generate Bounds?
		// Gather Bounds
		Data.Bounds = RefSkeletalMesh->GetBounds();
		
		// Additional Settings
		Data.Settings.bEnablePerPolyCollision = RefSkeletalMesh->GetEnablePerPolyCollision();

		// Skeleton
		if(const USkeleton* Skeleton = RefSkeletalMesh->GetSkeleton())
		{
			Data.Skeleton = RefSkeletalMesh->GetSkeleton();
			GenerationContext.ReferencedSkeletons.AddUnique(Skeleton);
		}
		
		// Physics Asset
		if (const UPhysicsAsset* PhysicsAsset = RefSkeletalMesh->GetPhysicsAsset())
		{
			Data.PhysicsAsset = PhysicsAsset;
			GenerationContext.PhysicsAssetMap.FindOrAdd(Data.PhysicsAsset.ToString(), Data.PhysicsAsset);
		}

		// Post ProcessAnimInstance
		if(const TSubclassOf<UAnimInstance> PostProcessAnimInstance = RefSkeletalMesh->GetPostProcessAnimBlueprint())
		{
			Data.PostProcessAnimInst = PostProcessAnimInstance;
			GenerationContext.AnimBPAssetsMap.FindOrAdd(Data.PostProcessAnimInst.ToString(), Data.PostProcessAnimInst);
		}
		
		// Shadow Physics Asset
		if (const UPhysicsAsset* PhysicsAsset = RefSkeletalMesh->GetPhysicsAsset())
		{
			Data.PhysicsAsset = PhysicsAsset;
			GenerationContext.PhysicsAssetMap.FindOrAdd(Data.PhysicsAsset.ToString(), Data.PhysicsAsset);
		}
	}
}


int32 ComputeLODBias(const FMutableGraphGenerationContext& GenerationContext, const UTexture2D* ReferenceTexture, int32 MaxTextureSize,
	const UCustomizableObjectNodeMaterial* MaterialNode, const int32 ImageIndex)
{
	int32 LODBias = 0;

	// We used to calculate the lod bias directly from the group like this:
	//int LODBias = 0;
	//if (LODSettings.TextureLODGroups.IsValidIndex(ReferenceTexture->LODGroup))
	//{
	//	const FTextureLODGroup& Group = LODSettings.GetTextureLODGroup(ReferenceTexture->LODGroup);
	//	LODBias = Group.LODBias;
	//}

	// ...but now it seems to be more complicated and we may need this:
	// This is not 100% correct either because it makes assumptions about the final texture size
	// that may not be correct, but if the reference texture is really representative of the average 
	// case, then it is as good as we can do.
	if (ReferenceTexture)
	{
		const UTextureLODSettings& LODSettings = GenerationContext.Options.TargetPlatform->GetTextureLODSettings();

		LODBias = LODSettings.CalculateLODBias(ReferenceTexture->Source.GetSizeX(), ReferenceTexture->Source.GetSizeY(), MaxTextureSize,
			ReferenceTexture->LODGroup, ReferenceTexture->LODBias, false, ReferenceTexture->MipGenSettings, ReferenceTexture->IsCurrentlyVirtualTextured());
	}

	// Reduce data size and compilation time for server platforms.
	// \todo: review this hack
	bool bIsServer = GenerationContext.Options.TargetPlatform->GetTargetPlatformInfo().PlatformType == EBuildTargetType::Server;
	if (bIsServer)
	{
		LODBias += 4;
	}

	// Increment the LOD bias per each LOD if we are using automatic LODs
	if (GenerationContext.CurrentLOD > 0
		&&
		GenerationContext.CurrentAutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh)
	{
		// Only if the texture actually uses a layout. Otherwise it could be a special texture we shouldn't scale.
		if (MaterialNode->GetImageUVLayout(ImageIndex) >= 0)
		{
			// \todo: make it an object property to be tweaked
			int MipsToSkipPerLOD = 1;
			LODBias += MipsToSkipPerLOD * GenerationContext.CurrentLOD;
		}
	}

	if (ReferenceTexture)
	{
		UE_LOG(LogMutable, Verbose, TEXT("Compiling texture with reference [%s] will have LOD Bias %d."), *ReferenceTexture->GetName(), LODBias);
	}
	else
	{
		UE_LOG(LogMutable, Verbose, TEXT("Compiling texture without reference will have LOD Bias %d."), LODBias);
	}

	return LODBias;
}


int32 GetMaxTextureSize(const UTexture2D* ReferenceTexture, const FMutableGraphGenerationContext& GenerationContext)
{
	if (ReferenceTexture)
	{
		// Setting the maximum texture size
		const UTextureLODSettings& LODSettings = GenerationContext.Options.TargetPlatform->GetTextureLODSettings();
		FTextureLODGroup TextureGroupSettings = LODSettings.GetTextureLODGroup(ReferenceTexture->LODGroup);

		if (TextureGroupSettings.MaxLODSize > 0)
		{
			return ReferenceTexture->MaxTextureSize == 0 ? TextureGroupSettings.MaxLODSize : FMath::Min(TextureGroupSettings.MaxLODSize, ReferenceTexture->MaxTextureSize);
		}
		else
		{
			return ReferenceTexture->MaxTextureSize;
		}
	}

	return 0;
}

#undef LOCTEXT_NAMESPACE
