// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithFBXImporter.h"
#include "DatasmithFBXImporterLog.h"
#include "DatasmithFBXImportOptions.h"
#include "DatasmithFBXScene.h"
#include "DatasmithFBXSceneProcessor.h"
#include "DatasmithImportedSequencesActor.h"
#include "DatasmithScene.h"
#include "DatasmithSceneActor.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithUtils.h"
#include "IDatasmithSceneElements.h"
#include "ObjectTemplates/DatasmithStaticMeshTemplate.h"
#include "Utility/DatasmithMeshHelper.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "FbxImporter.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

DEFINE_LOG_CATEGORY(LogDatasmithFBXImport);

namespace DatasmithFBXImporterImpl
{
	void BuildActorMapRecursive(TSharedPtr<IDatasmithActorElement> ActorElement, FActorMap& ActorMap)
	{
		if (!ActorElement.IsValid())
		{
			return;
		}

		FName OriginalName = ActorElement->GetTag(0);
		ActorMap.FindOrAdd(OriginalName).Add(ActorElement);

		for (int32 ChildIndex = 0; ChildIndex < ActorElement->GetChildrenCount(); ++ChildIndex)
		{
			BuildActorMapRecursive(ActorElement->GetChild(ChildIndex), ActorMap);
		}
	}
};

FDatasmithFBXImporter::FDatasmithFBXImporter()
{
	IntermediateScene = MakeUnique<FDatasmithFBXScene>();
}

FDatasmithFBXImporter::~FDatasmithFBXImporter()
{
}

void FDatasmithFBXImporter::GetGeometriesForMeshElementAndRelease(const TSharedRef<IDatasmithMeshElement> MeshElement, TArray<FMeshDescription>& OutMeshDescriptions)
{
	TSharedPtr<FDatasmithFBXSceneMesh>* FoundMesh = MeshNameToFBXMesh.Find(MeshElement->GetName());
	if (FoundMesh && (*FoundMesh).IsValid())
	{
		OutMeshDescriptions.Add(MoveTemp((*FoundMesh)->MeshDescription));
	}
	else
	{
		UE_LOG(LogDatasmithFBXImport, Error, TEXT("Failed to return FMeshDescription object for requested mesh element '%s'"), MeshElement->GetName());
	}
}

void FDatasmithFBXImporter::BuildAssetMaps(TSharedRef<IDatasmithScene> Scene, FActorMap& ActorsByOriginalName, FMaterialMap& MaterialsByName)
{
	ActorsByOriginalName.Reset();
	for (int32 RootActorIndex = 0; RootActorIndex < Scene->GetActorsCount(); ++RootActorIndex)
	{
		DatasmithFBXImporterImpl::BuildActorMapRecursive(Scene->GetActor(RootActorIndex), ActorsByOriginalName);
	}

	MaterialsByName.Reset();
	for (const auto& MatEntry : ImportedMaterials)
	{
		const TSharedPtr<FDatasmithFBXSceneMaterial>& FBXMaterial = MatEntry.Key;
		TSharedPtr<IDatasmithBaseMaterialElement> DatasmithMaterial = MatEntry.Value;

		MaterialsByName.Add(FName(*FBXMaterial->Name), DatasmithMaterial);
	}
}

