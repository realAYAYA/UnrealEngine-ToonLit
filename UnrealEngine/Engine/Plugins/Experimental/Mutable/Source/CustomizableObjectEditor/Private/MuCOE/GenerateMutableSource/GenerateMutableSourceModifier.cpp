// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceModifier.h"

#include "Engine/StaticMesh.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceImage.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/MutableUtils.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipDeform.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipWithMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierClipWithUVMask.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticMesh.h"
#include "MuR/Mesh.h"
#include "MuT/NodeMeshTransform.h"
#include "MuT/NodeModifierMeshClipDeform.h"
#include "MuT/NodeModifierMeshClipMorphPlane.h"
#include "MuT/NodeModifierMeshClipWithUVMask.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


mu::NodeModifierPtr GenerateMutableSourceModifier(const UEdGraphPin * Pin, FMutableGraphGenerationContext & GenerationContext)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);
	
	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceModifier), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeModifier*>(Generated->Node.get());
	}
	
	mu::NodeModifierPtr Result;

	bool bDoNotAddToGeneratedCache = false; // TODO Remove on MTBL-829 

	
	// We don't need all the data for the modifiers meshes
	const EMutableMeshConversionFlags ModifiersMeshFlags = 
			EMutableMeshConversionFlags::IgnoreSkinning |
			EMutableMeshConversionFlags::IgnorePhysics;

	GenerationContext.MeshGenerationFlags.Push(ModifiersMeshFlags);

	if (const UCustomizableObjectNodeMeshClipMorph* TypedNodeClip = Cast<UCustomizableObjectNodeMeshClipMorph>(Node))
	{
		mu::NodeModifierMeshClipMorphPlanePtr ClipNode = new mu::NodeModifierMeshClipMorphPlane();
		Result = ClipNode;

		const FVector Origin = TypedNodeClip->GetOriginWithOffset();
		const FVector& Normal = TypedNodeClip->Normal;

		ClipNode->SetPlane(Origin.X, Origin.Y, Origin.Z, Normal.X, Normal.Y, Normal.Z);
		ClipNode->SetParams(TypedNodeClip->B, TypedNodeClip->Exponent);
		ClipNode->SetMorphEllipse(TypedNodeClip->Radius, TypedNodeClip->Radius2, TypedNodeClip->RotationAngle);

		ClipNode->SetVertexSelectionBone(GenerationContext.BoneNames.AddUnique(TypedNodeClip->BoneName), TypedNodeClip->MaxEffectRadius);

		ClipNode->SetMultipleTagPolicy( TypedNodeClip->MultipleTagPolicy );
		for (const FString& Tag : TypedNodeClip->Tags)
		{
			 ClipNode->AddTag(Tag);
		}
	}

	else if (const UCustomizableObjectNodeMeshClipDeform* TypedNodeClipDeform = Cast<UCustomizableObjectNodeMeshClipDeform>(Node))
	{
		mu::Ptr<mu::NodeModifierMeshClipDeform> ClipNode = new mu::NodeModifierMeshClipDeform();
		Result = ClipNode;
	
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeClipDeform->ClipShapePin()))
		{
			FMutableGraphMeshGenerationData DummyMeshData;
			mu::NodeMeshPtr ClipMesh = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, DummyMeshData, false, true);

			ClipNode->SetClipMesh(ClipMesh.get());

			mu::EShapeBindingMethod BindingMethod = mu::EShapeBindingMethod::ClipDeformClosestProject;
			switch(TypedNodeClipDeform->BindingMethod)
			{
				case EShapeBindingMethod::ClosestProject:
					BindingMethod = mu::EShapeBindingMethod::ClipDeformClosestProject;
					break;
				case EShapeBindingMethod::NormalProject:
					BindingMethod = mu::EShapeBindingMethod::ClipDeformNormalProject;
					break;
				case EShapeBindingMethod::ClosestToSurface:
					BindingMethod = mu::EShapeBindingMethod::ClipDeformClosestToSurface;
					break;
				default:
					check(false);
					break;
			}

			ClipNode->SetBindingMethod(BindingMethod);
		}
		else
		{
			FText ErrorMsg = LOCTEXT("ClipDeform mesh", "The clip deform node requires an input clip shape.");
			GenerationContext.Compiler->CompilerLog(ErrorMsg, TypedNodeClipDeform, EMessageSeverity::Error);
			Result = nullptr;
		}
	
		ClipNode->SetMultipleTagPolicy(TypedNodeClipDeform->MultipleTagPolicy);
		for (const FString& Tag : TypedNodeClipDeform->Tags)
		{
			ClipNode->AddTag(Tag);
		}		
	}

	else if (const UCustomizableObjectNodeMeshClipWithMesh* TypedNodeClipMesh = Cast<UCustomizableObjectNodeMeshClipWithMesh>(Node))
	{
		// MeshClipWithMesh can be connected to multiple objects, so the compiled NodeModifierMeshClipWithMesh
		// needs to be different for each object. If it were added to the Generated cache, all the objects would get the same.
		bDoNotAddToGeneratedCache = true;

		mu::Ptr<mu::NodeModifierMeshClipWithMesh> ClipNode = new mu::NodeModifierMeshClipWithMesh();
		Result = ClipNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeClipMesh->ClipMeshPin()))
		{
			FMutableGraphMeshGenerationData DummyMeshData;

			mu::NodeMeshPtr ClipMesh = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, DummyMeshData, false, true);

			FPinDataValue* PinData = GenerationContext.PinData.Find(ConnectedPin);
			for (const FMeshData& MeshData : PinData->MeshesData)
			{
				bool bClosed = true;
				if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshData.Mesh))
				{
					bClosed = IsMeshClosed(SkeletalMesh, MeshData.LOD, MeshData.MaterialIndex);
				}
				else if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshData.Mesh))
				{
					bClosed = IsMeshClosed(StaticMesh, MeshData.LOD, MeshData.MaterialIndex);
				}
				else
				{
					// TODO: We support the clip mesh not being constant. This message is not precise enough. It should say that it hasn't been 
					// possible to check if the mesh is closed or not.
					GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), MeshData.Node);
				}

				if (!bClosed)
				{
					FText ErrorMsg = FText::Format(LOCTEXT("Clipping mesh", "Clipping mesh [{0}] not closed (i.e., it does not enclose a volume)."), FText::FromName(MeshData.Mesh->GetFName()));
					GenerationContext.Compiler->CompilerLog(ErrorMsg, MeshData.Node, EMessageSeverity::Warning);
				}
			}

			if (Cast<UCustomizableObjectNodeStaticMesh>(ConnectedPin->GetOwningNode()))
			{
				mu::NodeMeshTransformPtr TransformMesh = new mu::NodeMeshTransform();
				TransformMesh->SetSource(ClipMesh.get());

				FMatrix Matrix = TypedNodeClipMesh->Transform.ToMatrixWithScale();
				TransformMesh->SetTransform(FMatrix44f(Matrix));
				ClipMesh = TransformMesh;
			}

			ClipNode->SetClipMesh(ClipMesh.get());
		}
		else
		{
			FText ErrorMsg = LOCTEXT("Clipping mesh missing", "The clip mesh with mesh node requires an input clip mesh.");
			GenerationContext.Compiler->CompilerLog(ErrorMsg, TypedNodeClipMesh, EMessageSeverity::Error);
			Result = nullptr;
		}

		ClipNode->SetMultipleTagPolicy(TypedNodeClipMesh->MultipleTagPolicy);
		for (const FString& Tag : TypedNodeClipMesh->Tags)
		{
			 ClipNode->AddTag(Tag);
		}

		if (TypedNodeClipMesh->CustomizableObjectToClipWith != nullptr)
		{
			TArray<mu::Ptr<mu::NodeModifierMeshClipWithMesh>>* ArrayDataPtr = GenerationContext.MapClipMeshNodeToMutableClipMeshNodeArray.Find(Cast<UCustomizableObjectNodeMeshClipWithMesh>(Pin->GetOwningNode()));

			if (ArrayDataPtr == nullptr)
			{
				TArray<mu::Ptr<mu::NodeModifierMeshClipWithMesh>> ArrayData;
				ArrayData.Add(ClipNode);
				UCustomizableObjectNodeMeshClipWithMesh* CastedNode = Cast<UCustomizableObjectNodeMeshClipWithMesh>(Pin->GetOwningNode());
				GenerationContext.MapClipMeshNodeToMutableClipMeshNodeArray.Add(CastedNode, ArrayData);
			}
			else
			{
				ArrayDataPtr->AddUnique(ClipNode);
			}
		}
	}

	else if (const UCustomizableObjectNodeModifierClipWithUVMask* TypedNodeClipUVMask = Cast<UCustomizableObjectNodeModifierClipWithUVMask>(Node))
	{
		// This modifier can be connected to multiple objects, so the compiled node
		// needs to be different for each object. If it were added to the Generated cache, all the objects would get the same.
		bDoNotAddToGeneratedCache = true;

		mu::Ptr<mu::NodeModifierMeshClipWithUVMask> ClipNode = new mu::NodeModifierMeshClipWithUVMask();
		Result = ClipNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeClipUVMask->ClipMaskPin()))
		{
			FMutableGraphMeshGenerationData DummyMeshData;

			mu::Ptr<mu::NodeImage> ClipMask = GenerateMutableSourceImage(ConnectedPin, GenerationContext, 0);

			ClipNode->SetClipMask(ClipMask.get());
		}
		else
		{
			FText ErrorMsg = LOCTEXT("ClipUVMask mesh", "The clip mesh with UV Mask node requires an input texture mask.");
			GenerationContext.Compiler->CompilerLog(ErrorMsg, TypedNodeClipUVMask, EMessageSeverity::Error);
			Result = nullptr;
		}

		ClipNode->SetLayoutIndex(TypedNodeClipUVMask->UVChannelForMask);

		ClipNode->SetMultipleTagPolicy(TypedNodeClipUVMask->MultipleTagPolicy);
		for (const FString& Tag : TypedNodeClipUVMask->Tags)
		{
			ClipNode->AddTag(Tag);
		}
	}

	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	GenerationContext.MeshGenerationFlags.Pop();

	if (!bDoNotAddToGeneratedCache)
	{
		GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	}
	GenerationContext.GeneratedNodes.Add(Node);
	

	return Result;
}

#undef LOCTEXT_NAMESPACE

