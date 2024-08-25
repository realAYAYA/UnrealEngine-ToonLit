// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditor.h"

#include "Animation/Skeleton.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEditorUtil.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowEditor)

DEFINE_LOG_CATEGORY(LogDataflowEditor);

UDataflowEditor::UDataflowEditor() : Super()
{}

TSharedPtr<FBaseAssetToolkit> UDataflowEditor::CreateToolkit()
{
	TSharedPtr<FDataflowEditorToolkit> DataflowToolkit = MakeShared<FDataflowEditorToolkit>(this);
	return DataflowToolkit;
}

void UDataflowEditor::Initialize(const TArray<TObjectPtr<UObject>>& InObjects)
{
	if(!InObjects.IsEmpty())
	{
		InitializeContent(nullptr, InObjects[0]);
	}
}

void UDataflowEditor::InitializeContent(TObjectPtr<UDataflowBaseContent> BaseContent, const TObjectPtr<UObject>& ContentOwner)
{
	DataflowContent = BaseContent;
	if(!DataflowContent)
	{
		if(UDataflow* DataflowAsset = Cast<UDataflow>(ContentOwner))
		{
			DataflowContent = NewObject<UDataflowBaseContent>();
			
			DataflowContent->SetDataflowAsset(DataflowAsset);
			DataflowContent->SetDataflowTerminal(FString());
		}
		else
		{
			if(Private::HasDataflowAsset(ContentOwner))
			{
				if(Private::HasSkeletalMesh(ContentOwner))
				{
					DataflowContent = NewObject<UDataflowSkeletalContent>();
					const TObjectPtr<UDataflowSkeletalContent> SkeletalContent = Cast<UDataflowSkeletalContent>(DataflowContent);
					
					SkeletalContent->SetSkeletalMesh(Private::GetSkeletalMeshFrom(ContentOwner));
                    SkeletalContent->SetSkeleton(Private::GetSkeletonFrom(ContentOwner));
                    SkeletalContent->SetAnimationAsset(Private::GetAnimationAssetFrom(ContentOwner));
				}
				else
				{
					DataflowContent = NewObject<UDataflowBaseContent>();
				}
				DataflowContent->SetDataflowAsset(Private::GetDataflowAssetFrom(ContentOwner));
				DataflowContent->SetDataflowTerminal(Private::GetDataflowTerminalFrom(ContentOwner));
			}
		}
	}

	if(const TObjectPtr<UDataflowSkeletalContent> SkeletalContent = Cast<UDataflowSkeletalContent>(DataflowContent))
	{
		if(!SkeletalContent->GetSkeletalMesh())
		{
			const FName SkeletalMeshName = MakeUniqueObjectName(SkeletalContent->GetDataflowAsset(), UDataflow::StaticClass(), FName("USkeletalMesh"));
			USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>(SkeletalContent->GetDataflowAsset(), SkeletalMeshName);

			USkeleton* Skeleton = SkeletalContent->GetSkeleton();
			if(!Skeleton)
			{
				const FName SkeletonName = MakeUniqueObjectName(SkeletalContent->GetDataflowAsset(), UDataflow::StaticClass(), FName("USkeleton"));
                Skeleton = NewObject<USkeleton>(SkeletalContent->GetDataflowAsset(), SkeletonName);
			}
			SkeletalMesh->SetSkeleton(Skeleton);
			SkeletalContent->SetSkeletalMesh(SkeletalMesh);
		}
		else if(!SkeletalContent->GetSkeleton())
		{
			SkeletalContent->SetSkeleton(SkeletalContent->GetSkeletalMesh()->GetSkeleton());
		}
	}

	if(DataflowContent && DataflowContent->GetDataflowAsset())
	{
		DataflowContent->GetDataflowAsset()->Schema = UDataflowSchema::StaticClass();
		DataflowContent->BuildBaseContent(ContentOwner);
		
		// the owner is either a base content derived class or the dataflow itself
		DataflowContent->SetDataflowOwner(ContentOwner);
	}
	
	// Potentially we could add additional objects to edit here (fields, meshes....)
	// If these objects have a matching factory we would be able to use geometry tools
	const TArray<TObjectPtr<UObject>> ArrayObjects = {ContentOwner};
	UBaseCharacterFXEditor::Initialize(ArrayObjects);
}



