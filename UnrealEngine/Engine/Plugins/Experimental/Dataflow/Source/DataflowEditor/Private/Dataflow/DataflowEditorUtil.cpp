// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorUtil.h"

#include "Animation/Skeleton.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowObject.h"
#include "DynamicMesh/MeshNormals.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Materials/Material.h"


using namespace UE::Geometry;

namespace Private
{
	bool HasSkeletalMesh(UObject* InObject)
	{
		if (UClass* Class = InObject->GetClass())
		{
			return Class->FindPropertyByName(FName("SkeletalMesh")) &&
				   Class->FindPropertyByName(FName("Skeleton"));
		}
		return false;
	}
	
	bool HasDataflowAsset(UObject* InObject)
	{
		if (UClass* Class = InObject->GetClass())
		{
			return Class->FindPropertyByName(FName("DataflowAsset")) &&
				   Class->FindPropertyByName(FName("DataflowTerminal"));
		}
		return false;
	}
	
	UDataflow* GetDataflowAssetFrom(UObject* InObject)
	{
		if (UClass* Class = InObject->GetClass())
		{
			if (FProperty* Property = Class->FindPropertyByName(FName("DataflowAsset")))
			{
				return *Property->ContainerPtrToValuePtr<UDataflow*>(InObject);
			}
		}
		return nullptr;
	}

	USkeletalMesh* GetSkeletalMeshFrom(UObject* InObject)
	{
		if (UClass* Class = InObject->GetClass())
		{
			if (FProperty* Property = Class->FindPropertyByName(FName("SkeletalMesh")))
			{
				return *Property->ContainerPtrToValuePtr<USkeletalMesh*>(InObject);
			}
		}
		return nullptr;
	}

	USkeleton* GetSkeletonFrom(UObject* InObject)
	{
		if (UClass* Class = InObject->GetClass())
		{
			if (FProperty* Property = Class->FindPropertyByName(FName("Skeleton")))
			{
				return *Property->ContainerPtrToValuePtr<USkeleton*>(InObject);
			}
		}
		return nullptr;
	}
	
	UAnimationAsset* GetAnimationAssetFrom(UObject* InObject)
	{
		if (UClass* Class = InObject->GetClass())
		{
			if (FProperty* Property = Class->FindPropertyByName(FName("AnimationAsset")))
			{
				return *Property->ContainerPtrToValuePtr<UAnimationAsset*>(InObject);
			}
		}
		return nullptr;
	}

	FString GetDataflowTerminalFrom(UObject* InObject)
	{
		if (UClass* Class = InObject->GetClass())
		{
			if (FProperty* Property = Class->FindPropertyByName(FName("DataflowTerminal")))
			{
				return *Property->ContainerPtrToValuePtr<FString>(InObject);
			}
		}
		return FString();
	}
};


namespace UE
{
	namespace Material
	{
		UMaterial* LoadMaterialFromPath(const FName& InPath, UObject* Outer)
		{
			if (InPath == NAME_None) return nullptr;

			return Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), Outer, *InPath.ToString()));
		}
	}

}

namespace Dataflow
{
	TSharedPtr<FEngineContext> GetContext(TObjectPtr<UDataflowBaseContent> Content)
	{
		if (Content)
		{
			if (!Content->GetDataflowContext())
			{
				Content->SetDataflowContext(MakeShared<FEngineContext>(Content->GetDataflowOwner(), Content->GetDataflowAsset(), FTimestamp::Invalid));
			}
			return Content->GetDataflowContext();
		}

		ensure(false);
		return MakeShared<FEngineContext>(nullptr, nullptr, FTimestamp::Invalid);
	}
}