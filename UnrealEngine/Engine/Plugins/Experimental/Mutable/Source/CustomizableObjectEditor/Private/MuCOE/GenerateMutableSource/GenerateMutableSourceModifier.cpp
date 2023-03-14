// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceModifier.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Math/MathFwd.h"
#include "Math/Matrix.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/MutableUtils.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipDeform.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipWithMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticMesh.h"
#include "MuR/Mesh.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMeshTransform.h"
#include "MuT/NodeModifierMeshClipDeform.h"
#include "MuT/NodeModifierMeshClipMorphPlane.h"
#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "Templates/Casts.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"

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

	if (const UCustomizableObjectNodeMeshClipMorph* TypedNodeClip = Cast<UCustomizableObjectNodeMeshClipMorph>(Node))
	{
		mu::NodeModifierMeshClipMorphPlanePtr ClipNode = new mu::NodeModifierMeshClipMorphPlane();
		Result = ClipNode;

		const FVector Origin = TypedNodeClip->GetOriginWithOffset();
		const FVector& Normal = TypedNodeClip->Normal;

		ClipNode->SetPlane(Origin.X, Origin.Y, Origin.Z, Normal.X, Normal.Y, Normal.Z);
		ClipNode->SetParams(TypedNodeClip->B, TypedNodeClip->Exponent);
		ClipNode->SetMorphEllipse(TypedNodeClip->Radius, TypedNodeClip->Radius2, TypedNodeClip->RotationAngle);

		ClipNode->SetVertexSelectionBone(TCHAR_TO_ANSI(*TypedNodeClip->BoneName.ToString()), TypedNodeClip->MaxEffectRadius);

		for (const FString& Tag : TypedNodeClip->Tags)
		{
			 ClipNode->AddTag(TCHAR_TO_ANSI(*Tag));
		}
	}

	else if (const UCustomizableObjectNodeMeshClipDeform* TypedNodeClipDeform = Cast<UCustomizableObjectNodeMeshClipDeform>(Node))
	{
		mu::Ptr<mu::NodeModifierMeshClipDeform> ClipNode = new mu::NodeModifierMeshClipDeform();
		Result = ClipNode;
	
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeClipDeform->ClipShapePin()))
		{
			FMutableGraphMeshGenerationData DummyMeshData;
			mu::NodeMeshPtr ClipMesh = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, DummyMeshData);

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
	
		for (const FString& Tag : TypedNodeClipDeform->Tags)
		{
			ClipNode->AddTag(TCHAR_TO_ANSI(*Tag));
		}		
	}

	else if (const UCustomizableObjectNodeMeshClipWithMesh* TypedNodeClipMesh = Cast<UCustomizableObjectNodeMeshClipWithMesh>(Node))
	{
		// MeshClipWithMesh can be connected to multiple objects, so the compiled NodeModifierMeshClipWithMesh
		// needs to be different for each object. If it were added to the Generated cache, all the objects would get the same.
		bDoNotAddToGeneratedCache = true;

		mu::NodeModifierMeshClipWithMeshPtr ClipNode = new mu::NodeModifierMeshClipWithMesh();
		Result = ClipNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeClipMesh->ClipMeshPin()))
		{
			FMutableGraphMeshGenerationData DummyMeshData;
			mu::NodeMeshPtr ClipMesh = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, DummyMeshData);

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
					GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), MeshData.Node);
				}

				if (!bClosed)
				{
					GenerationContext.Compiler->CompilerLog(LOCTEXT("Clipping mesh", "Clipping mesh not closed (i.e., it does not enclose a volume)."), MeshData.Node, EMessageSeverity::Warning);
				}
			}

			if (Cast<UCustomizableObjectNodeStaticMesh>(ConnectedPin->GetOwningNode()))
			{
				mu::NodeMeshTransformPtr TransformMesh = new mu::NodeMeshTransform();
				TransformMesh->SetSource(ClipMesh.get());

				FMatrix Matrix = TypedNodeClipMesh->Transform.ToMatrixWithScale();
				const float M[16] = {
				  	float(Matrix.M[0][0]),float(Matrix.M[1][0]),float(Matrix.M[2][0]),float(Matrix.M[3][0]),
					float(Matrix.M[0][1]),float(Matrix.M[1][1]),float(Matrix.M[2][1]),float(Matrix.M[3][1]),
					float(Matrix.M[0][2]),float(Matrix.M[1][2]),float(Matrix.M[2][2]),float(Matrix.M[3][2]),
					float(Matrix.M[0][3]),float(Matrix.M[1][3]),float(Matrix.M[2][3]),float(Matrix.M[3][3])
				};

				TransformMesh->SetTransform(M);
				ClipMesh = TransformMesh;
			}

			ClipNode->SetClipMesh(ClipMesh.get());
		}

		for (const FString& Tag : TypedNodeClipMesh->Tags)
		{
			 ClipNode->AddTag(TCHAR_TO_ANSI(*Tag));
		}

		if (TypedNodeClipMesh->CustomizableObjectToClipWith != nullptr)
		{
			TArray<mu::NodeModifierMeshClipWithMeshPtr>* ArrayDataPtr = GenerationContext.MapClipMeshNodeToMutableClipMeshNodeArray.Find(Cast<UCustomizableObjectNodeMeshClipWithMesh>(Pin->GetOwningNode()));

			if (ArrayDataPtr == nullptr)
			{
				TArray<mu::NodeModifierMeshClipWithMeshPtr> ArrayData;
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

	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	if (!bDoNotAddToGeneratedCache)
	{
		GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	}
	GenerationContext.GeneratedNodes.Add(Node);
	
	return Result;
}

#undef LOCTEXT_NAMESPACE

