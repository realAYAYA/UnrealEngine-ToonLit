// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneValidator.h"

#include "DatasmithUtils.h"
#include "DatasmithMeshUObject.h"
#include "Serialization/MemoryReader.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "DatasmithAnimationElements.h"
#include "DatasmithVariantElements.h"
#include "Modules/ModuleInterface.h"

DATASMITHVALIDATOR_API DECLARE_LOG_CATEGORY_EXTERN(LogDatasmithValidator, Log, All);

DEFINE_LOG_CATEGORY(LogDatasmithValidator)

class FDatasmithValidatorModule : public IModuleInterface
{
};

//IMPLEMENT_MODULE(FDatasmithValidatorModule, DatasmithValidator);

namespace Validator {

FSceneValidator::FSceneValidator(const TSharedRef< IDatasmithScene >& InScene)
	: Scene(InScene)
{
}

FString FSceneValidator::GetElementTypes(const IDatasmithElement& InElement)
{
	const int32 ReserveForFastCatenation = 100;
	FString		TypesString(TEXT(""), ReserveForFastCatenation);
	if (InElement.IsA(EDatasmithElementType::StaticMesh))
	{
		TypesString += TEXT(", StaticMesh");
	}
	if (InElement.IsA(EDatasmithElementType::Actor))
	{
		TypesString += TEXT(", Actor");
	}
	if (InElement.IsA(EDatasmithElementType::StaticMeshActor))
	{
		TypesString += TEXT(", StaticMeshActor");
	}
	if (InElement.IsA(EDatasmithElementType::Light))
	{
		TypesString += TEXT(", Light");
	}
	if (InElement.IsA(EDatasmithElementType::PointLight))
	{
		TypesString += TEXT(", PointLight");
	}
	if (InElement.IsA(EDatasmithElementType::SpotLight))
	{
		TypesString += TEXT(", SpotLight");
	}
	if (InElement.IsA(EDatasmithElementType::DirectionalLight))
	{
		TypesString += TEXT(", DirectionalLight");
	}
	if (InElement.IsA(EDatasmithElementType::AreaLight))
	{
		TypesString += TEXT(", AreaLight");
	}
	if (InElement.IsA(EDatasmithElementType::LightmassPortal))
	{
		TypesString += TEXT(", LightmassPortal");
	}
	if (InElement.IsA(EDatasmithElementType::EnvironmentLight))
	{
		TypesString += TEXT(", EnvironmentLight");
	}
	if (InElement.IsA(EDatasmithElementType::Camera))
	{
		TypesString += TEXT(", Camera");
	}
	if (InElement.IsA(EDatasmithElementType::Shader))
	{
		TypesString += TEXT(", Shader");
	}
	if (InElement.IsA(EDatasmithElementType::BaseMaterial))
	{
		TypesString += TEXT(", BaseMaterial");
	}
	if (InElement.IsA(EDatasmithElementType::MaterialInstance))
	{
		TypesString += TEXT(", MaterialInstance");
	}
	if (InElement.IsA(EDatasmithElementType::KeyValueProperty))
	{
		TypesString += TEXT(", KeyValueProperty");
	}
	if (InElement.IsA(EDatasmithElementType::Texture))
	{
		TypesString += TEXT(", Texture");
	}
	if (InElement.IsA(EDatasmithElementType::MaterialId))
	{
		TypesString += TEXT(", MaterialId");
	}
	if (InElement.IsA(EDatasmithElementType::Scene))
	{
		TypesString += TEXT(", Scene");
	}
	if (InElement.IsA(EDatasmithElementType::MetaData))
	{
		TypesString += TEXT(", MetaData");
	}
	if (InElement.IsA(EDatasmithElementType::CustomActor))
	{
		TypesString += TEXT(", CustomActor");
	}
	if (InElement.IsA(EDatasmithElementType::Material))
	{
		TypesString += TEXT(", Material");
	}
	if (InElement.IsA(EDatasmithElementType::Landscape))
	{
		TypesString += TEXT(", Landscape");
	}
	if (InElement.IsA(EDatasmithElementType::UEPbrMaterial))
	{
		TypesString += TEXT(", UEPbrMaterial");
	}
	if (InElement.IsA(EDatasmithElementType::PostProcessVolume))
	{
		TypesString += TEXT(", PostProcessVolume");
	}
	if (InElement.IsA(EDatasmithElementType::LevelSequence))
	{
		TypesString += TEXT(", LevelSequence");
	}
	if (InElement.IsA(EDatasmithElementType::Animation))
	{
		TypesString += TEXT(", Animation");
	}
	if (InElement.IsA(EDatasmithElementType::HierarchicalInstanceStaticMesh))
	{
		TypesString += TEXT(", HierarchicalInstanceStaticMesh");
	}
	if (InElement.IsA(EDatasmithElementType::Variant))
	{
		TypesString += TEXT(", Variant");
	}
	if (InElement.IsA(EDatasmithElementType::Variant))
	{
		TypesString += TEXT(", Variant");
	}
	if (InElement.IsA(EDatasmithElementType::Decal))
	{
		TypesString += TEXT(", Decal");
	}
	if (InElement.IsA(EDatasmithElementType::DecalMaterial))
	{
		TypesString += TEXT(", DecalMaterial");
	}
	if (InElement.IsA(EDatasmithElementType::MaterialExpression))
	{
		TypesString += TEXT(", MaterialExpression");
	}
	if (InElement.IsA(EDatasmithElementType::MaterialExpressionInput))
	{
		TypesString += TEXT(", MaterialExpressionInput");
	}
	if (InElement.IsA(EDatasmithElementType::MaterialExpressionOutput))
	{
		TypesString += TEXT(", MaterialExpressionOutput");
	}

	if (TypesString.Len() < 2)
	{
		AddMessage(kBug, TEXT("FSceneValidator::GetElementTypes - Unknown element types \"%s\""), InElement.GetName());
		TypesString = TEXT("Unknown type");
	}
	else
	{
		TypesString.RightChopInline(2);
	}

	return TypesString;
}

FString FSceneValidator::GetElementsDescription(const IDatasmithElement& InElement)
{
	FString Types = GetElementTypes(InElement);
	return FString::Printf(TEXT("Types(%s), Name=\"%s\", Label=\"%s\""), *Types, InElement.GetName(),
						   InElement.GetLabel());
}

void FSceneValidator::AddElements(const IDatasmithElement& InElement, FMapNameToUsage* IOMap)
{
	FNamePtr ElementName(InElement.GetName());

	if (IOMap != nullptr)
	{
		FUsage* Usage = IOMap->Find(ElementName);
		if (Usage != nullptr)
		{
			if (Usage->bExist == true)
			{
				AddMessage(kBug, TEXT("Element duplicated %s"), *GetElementsDescription(InElement));
			}
		}
		else
		{
			IOMap->Add(ElementName, {true, false});
		}
	}

	if (NameToElementMap.Contains(ElementName))
	{
		AddMessage(kError, TEXT("Elements with same\n\tNew Element %s\n\tOld Element %s"),
				   *GetElementsDescription(InElement), *GetElementsDescription(*NameToElementMap[ElementName]));
	}
	else
	{
		NameToElementMap.Add(ElementName, &InElement);
		if (FCString::Strcmp(InElement.GetName(), *FDatasmithUtils::SanitizeObjectName(InElement.GetName())) != 0)
		{
			AddMessage(kError, TEXT("Elements name isn't Sanitized %s"), *GetElementsDescription(InElement));
		}
	}
}

void FSceneValidator::AddMessageImpl(TInfoLevel Level, const FString& Message)
{
	check(Level >= kBug && Level < kInfoLevelMax);
	++MessagesCounts[Level];
	Messages.Add(FMessage(Level, Message));
}

const TCHAR* FSceneValidator::LevelName(TInfoLevel Level)
{
	return Level == kBug	   ? TEXT("Bug")
		   : Level == kError   ? TEXT("Error")
		   : Level == kWarning ? TEXT("Warning")
		   : Level == kVerbose ? TEXT("Verbose")
							   : TEXT("???????");
}

FString FSceneValidator::GetReports(TInfoLevel InLevel)
{
    FString ReportString;

	// Reports counts for each level
	const TInfoLevel Levels[] = {TInfoLevel::kBug, TInfoLevel::kError, TInfoLevel::kWarning, TInfoLevel::kVerbose};
	for (const auto Level : Levels)
	{
		if (Level <= InLevel && MessagesCounts[Level] != 0)
		{
            ReportString += FString::Printf(TEXT("%d %ss collected\n"), MessagesCounts[Level], LevelName(Level));
		}
	}

	// Reports messages
	for (const FMessage& Message : Messages)
	{
		if (Message.Level <= InLevel)
		{
            ReportString += FString::Printf(TEXT("%-7s:%s\n"), LevelName(Message.Level), *Message.Message);
		}
	}
    return ReportString;
}

void FSceneValidator::CheckTexturesFiles()
{
    const IDatasmithScene& MyScene = *Scene;

    FString RessourcePath(MyScene.GetResourcePath());
    if (!RessourcePath.IsEmpty())
    {
        if (!IFileManager::Get().DirectoryExists(*RessourcePath))
        {
            AddMessage(kBug, TEXT("ResourcePath \"%s\" doesn't exist"), *RessourcePath);
        }
    }

    int32 Count = Scene->GetTexturesCount();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const TSharedPtr< IDatasmithTextureElement >& Texture = MyScene.GetTexture(Index);
        if (Texture.IsValid())
        {
            bool Reported = false;
            FString TextureFile = Texture->GetFile();
            if (FPaths::IsRelative(TextureFile))
            {
                if (!RessourcePath.IsEmpty())
                {
                    TextureFile = FPaths::ConvertRelativePathToFull(RessourcePath, TextureFile);
                }
                else
                {
                    AddMessage(kBug, TEXT("Texture Name=\"%s\", Label=\"%s\", Path=\"%s\" - Relative path whitout reference path"), Texture->GetName(), Texture->GetLabel(), *TextureFile);
                    Reported = true;
                }
            }
            if (!Reported && !FPaths::FileExists(TextureFile))
            {
                AddMessage(kBug, TEXT("Texture Name=\"%s\", Label=\"%s\", Path=\"%s\" - File not found"), *TextureFile);
            }
        }
    }
}

TArray< UDatasmithMesh* > GetDatasmithMeshFromMeshElement( const TSharedRef< IDatasmithMeshElement > MeshElement )
{
    TArray< UDatasmithMesh* > Result;
    
    TUniquePtr<FArchive> Archive( IFileManager::Get().CreateFileReader(MeshElement->GetFile()) );
    if ( !Archive.IsValid() )
    {
        return Result;
    }
    
    int32 NumMeshes = 0;
    *Archive << NumMeshes;
    
    // Currently we only have 1 mesh per file. If there's a second mesh, it will be a CollisionMesh
    while (NumMeshes-- > 0)
    {
        TArray< uint8 > Bytes;
        *Archive << Bytes;
        
        UDatasmithMesh* DatasmithMesh = NewObject< UDatasmithMesh >();
        
        FMemoryReader MemoryReader( Bytes, true );
        MemoryReader.ArIgnoreClassRef = false;
        MemoryReader.ArIgnoreArchetypeRef = false;
        MemoryReader.SetWantBinaryPropertySerialization(true);
        DatasmithMesh->Serialize( MemoryReader );
        
        Result.Emplace( DatasmithMesh );
    }
    
    return Result;
}

void FSceneValidator::CheckMeshFiles()
{
    const IDatasmithScene& MyScene = *Scene;
    int32 Count = MyScene.GetMeshesCount();
    for (int32 IndexMeshElement = 0; IndexMeshElement < Count; ++IndexMeshElement)
    {
        const TSharedPtr< IDatasmithMeshElement >& Mesh = MyScene.GetMesh(IndexMeshElement);
        if (Mesh.IsValid())
        {
            TArray< UDatasmithMesh* > DatasmithMeshArray = GetDatasmithMeshFromMeshElement(Mesh.ToSharedRef());
            int32 NumMesh = DatasmithMeshArray.Num();
            if (NumMesh != 0)
            {
                for (int32 IndexMesh = 0; IndexMesh < NumMesh; ++IndexMesh)
                {
                    const auto& DatasmithMeshSourceModel = DatasmithMeshArray[IndexMesh]->SourceModels;
                    int32 NumSourceModel = DatasmithMeshSourceModel.Num();
                    for (int32 IndexSourceModel = 0; IndexSourceModel < NumSourceModel; ++IndexSourceModel)
                    {
                        FRawMesh    RawMesh;
                        // const_cast because LoadRawMesh isn't const
                        const_cast<FDatasmithMeshSourceModel&>(DatasmithMeshSourceModel[IndexSourceModel]).RawMeshBulkData.LoadRawMesh(RawMesh);
                        if (RawMesh.IsValid())
                        {
                            TArray<bool>    MaterialUsed = {};
                            int32 IndiceMax = 0;
                            for (int32 Indice : RawMesh.FaceMaterialIndices)
                            {
                                if (Indice >= IndiceMax)
                                {
                                    MaterialUsed.AddZeroed(Indice +1 - IndiceMax);
                                    IndiceMax = Indice + 1;
                                }
                                MaterialUsed[Indice] = true;
                            }
                            int32 Index = 0;
                            for (Index = 0; Index < IndiceMax && MaterialUsed[Index]; ++Index)
                            {
                                if (!MaterialUsed[Index])
                                {
                                    AddMessage(kWarning, TEXT("Raw mesh [%d][%d] unused material %d Name=\"%s\" Label=\"%s\" File=\"%s\""), IndexMesh, IndexSourceModel, Index, Mesh->GetName(), Mesh->GetLabel(), Mesh->GetFile());
                                    break;
                                }
                            }
                            if (RawMesh.MaterialIndexToImportIndex.Num() != 0 && RawMesh.MaterialIndexToImportIndex.Num() != IndiceMax)
                            {
                                AddMessage(kError, TEXT("Raw mesh [%d][%d] invalid material invariant Name=\"%s\" Label=\"%s\" File=\"%s\""), IndexMesh, IndexSourceModel, Mesh->GetName(), Mesh->GetLabel(), Mesh->GetFile());
                            }
                            if (Mesh->GetMaterialSlotCount() != IndiceMax)
                            {
                                AddMessage(kWarning, TEXT("Raw mesh [%d][%d] MaterialSlotCount != #Materials Name=\"%s\" Label=\"%s\" File=\"%s\""), IndexMesh, IndexSourceModel, Mesh->GetName(), Mesh->GetLabel(), Mesh->GetFile());
                            }
                        }
                        else
                        {
                            AddMessage(kError, TEXT("Raw mesh [%d][%d] is invalid Name=\"%s\" Label=\"%s\" File=\"%s\""), IndexMesh, IndexSourceModel, Mesh->GetName(), Mesh->GetLabel(), Mesh->GetFile());
                        }
                    }
                }
            }
            else
            {
                AddMessage(kWarning, TEXT("Mesh file is empty Name=\"%s\" Label=\"%s\" File=\"%s\""), Mesh->GetName(), Mesh->GetLabel(), Mesh->GetFile());
            }
        }
    }
}

void FSceneValidator::CheckElementsName()
{
	const IDatasmithScene& MyScene = *Scene;

	int32 Index;
	int32 Count;

	Count = MyScene.GetTexturesCount();
	for (Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr< IDatasmithTextureElement >& Texture = MyScene.GetTexture(Index);
		if (Texture.IsValid())
		{
			AddElements(*Texture, &TexturesUsages);
		}
		else
		{
			AddMessage(kBug, TEXT("Texture %d is invalid"), Index);
		}
	}

	Count = MyScene.GetMaterialsCount();
	for (Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr< IDatasmithBaseMaterialElement >& Material = MyScene.GetMaterial(Index);
		if (Material.IsValid())
		{
			AddElements(*Material, &MaterialsUsages);
		}
		else
		{
			AddMessage(kBug, TEXT("Material %d is invalid"), Index);
		}
	}

	Count = MyScene.GetMeshesCount();
	for (Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr< IDatasmithMeshElement >& Mesh = MyScene.GetMesh(Index);
		if (Mesh.IsValid())
		{
			AddElements(*Mesh, &MeshesUsages);
		}
		else
		{
			AddMessage(kBug, TEXT("Mesh %d is invalid"), Index);
		}
	}

	Count = MyScene.GetActorsCount();
	for (Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr< IDatasmithActorElement >& Actor = MyScene.GetActor(Index);
		if (Actor.IsValid())
		{
			CheckActorsName(*Actor);
		}
		else
		{
			AddMessage(kBug, TEXT("Actor %d is invalid"), Index);
		}
	}

	if (MyScene.GetPostProcess().IsValid())
	{
		AddElements(*MyScene.GetPostProcess());
	}

	if (MyScene.GetPostProcess().IsValid())
	{
		AddElements(*MyScene.GetPostProcess());
	}

	Count = MyScene.GetMetaDataCount();
	for (Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr< IDatasmithMetaDataElement >& MetaData = MyScene.GetMetaData(Index);
		if (MetaData.IsValid())
		{
			AddElements(*MetaData);
		}
		else
		{
			AddMessage(kBug, TEXT("MetaData %d is invalid"), Index);
		}
	}

	Count = MyScene.GetLevelSequencesCount();
	for (Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr< IDatasmithLevelSequenceElement >& LevelSequence =
			Scene->GetLevelSequence(Index); // No const getter
		if (LevelSequence.IsValid())
		{
			AddElements(*LevelSequence, &LevelSequencesUsages);
		}
		else
		{
			AddMessage(kBug, TEXT("LevelSequence %d is invalid"), Index);
		}
	}

	Count = MyScene.GetLevelVariantSetsCount();
	for (Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr< IDatasmithLevelVariantSetsElement >& LevelVariant =
			Scene->GetLevelVariantSets(Index); // No const getter
		if (LevelVariant.IsValid())
		{
			AddElements(*LevelVariant);
		}
		else
		{
			AddMessage(kBug, TEXT("LevelVariant %d is invalid"), Index);
		}
	}
}

void FSceneValidator::CheckActorsName(const IDatasmithActorElement& InActor)
{
	AddElements(InActor, &ActorsUsages);
	int32 Count = InActor.GetChildrenCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr< IDatasmithActorElement >& Child = InActor.GetChild(Index);
		if (Child.IsValid())
		{
			CheckActorsName(*Child);
		}
		else
		{
			AddMessage(kBug, TEXT("Child Actor %d is invalid. Parent is %s"), Index, *GetElementsDescription(InActor));
		}
	}
}

void FSceneValidator::CheckDependances()
{
	const IDatasmithScene& MyScene = *Scene;
	int32				   Index;
	int32				   Count;

	Count = MyScene.GetMetaDataCount();
	for (Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr< IDatasmithMetaDataElement >& MetaData = MyScene.GetMetaData(Index);
		if (MetaData.IsValid())
		{
			if (MetaData->GetAssociatedElement().IsValid())
			{
				const IDatasmithElement& associated = *MetaData->GetAssociatedElement();
				if (associated.IsA(EDatasmithElementType::Actor))
				{
					ActorsUsages[associated.GetName()].bIsRefered = true;
				}
				else if (associated.IsA(EDatasmithElementType::Texture))
				{
					TexturesUsages[associated.GetName()].bIsRefered = true;
				}
				else if (associated.IsA(EDatasmithElementType::BaseMaterial))
				{
					MaterialsUsages[associated.GetName()].bIsRefered = true;
				}
				else if (associated.IsA(EDatasmithElementType::StaticMesh))
				{
					MeshesUsages[associated.GetName()].bIsRefered = true;
				}
				else
				{
					AddMessage(kError, TEXT("Metadata %d %s associated to an unexpected element %s"), Index,
							   *GetElementsDescription(*MetaData), *GetElementsDescription(associated));
				}
			}
			else
			{
				AddMessage(kError, TEXT("Metadata without actor %d %s"), Index, *GetElementsDescription(*MetaData));
			}
		}
	}

	Count = MyScene.GetActorsCount();
	for (Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr< IDatasmithActorElement >& Actor = MyScene.GetActor(Index);
		if (Actor.IsValid())
		{
			CheckActorsDependances(*Actor);
		}
	}
}

void FSceneValidator::CheckActorsDependances(const IDatasmithActorElement& InActor)
{
	int32 Count;
	int32 Index;

	{
		TSet< FNamePtr > Tags;
		Count = InActor.GetTagsCount();
		for (Index = 0; Index < Count; ++Index)
		{
			const TCHAR* Tag = InActor.GetTag(Index);
			bool		 bTagIsAlreadyInSet = false;
			Tags.Add(Tag, &bTagIsAlreadyInSet);
			if (bTagIsAlreadyInSet)
			{
				AddMessage(kError, TEXT("Tag \"%s\" present twice for actor %s"), Tag,
						   *GetElementsDescription(InActor));
			}
		}
	}

	if (InActor.IsA(EDatasmithElementType::StaticMeshActor))
	{
		const IDatasmithMeshActorElement& MeshActor = static_cast< const IDatasmithMeshActorElement& >(InActor);
		Count = MeshActor.GetMaterialOverridesCount();

		// Validate the refered mesh
		const IDatasmithMeshElement* MeshPtr = nullptr;
		const TCHAR*				 MeshName = MeshActor.GetStaticMeshPathName();
		if (*MeshName)
		{
			FUsage& MeshUsage = MeshesUsages[MeshName];
			if (!MeshUsage.bIsRefered)
			{
				MeshUsage.bIsRefered = true;
				if (!MeshUsage.bExist)
				{
					AddMessage(kError, TEXT("Unknown mesh \"%s\" for actor %s"), MeshName,
							   *GetElementsDescription(InActor));
				}
			}
			if (MeshUsage.bExist && Count != 0)
			{
				const IDatasmithElement** elementFound = NameToElementMap.Find(MeshName);
				if (elementFound != nullptr && (*elementFound)->IsA(EDatasmithElementType::StaticMesh))
				{
					MeshPtr = static_cast< const IDatasmithMeshElement* >(*elementFound);
				}
			}
		}
		else
		{
			AddMessage(kWarning, TEXT("Mesh actor without mesh %s"), *GetElementsDescription(MeshActor));
		}

		// Validate overload id
		TSet< int32 > MaterialIds;
		for (Index = 0; Index < Count; ++Index)
		{
			TSharedPtr< const IDatasmithMaterialIDElement > MaterialOverride = MeshActor.GetMaterialOverride(Index);
			if (MaterialOverride.IsValid())
			{
				int32 MaterialId = MaterialOverride->GetId();
				bool  bOverloadIsAlreadyInSet = false;
				MaterialIds.Add(MaterialId, &bOverloadIsAlreadyInSet);
				if (bOverloadIsAlreadyInSet)
				{
					AddMessage(kError, TEXT("Multiple overload for same id (%d) for actor %d %s"), MaterialId, Index,
							   *GetElementsDescription(MeshActor));
				}
				else
				{
					if (MeshPtr != nullptr)
					{
						// We have a mesh, so we can validate the override id...
					}
				}
				FUsage& MaterialUsage = MaterialsUsages[MaterialOverride->GetName()];
				if (!MaterialUsage.bIsRefered)
				{
					MaterialUsage.bIsRefered = true;
					if (!MaterialUsage.bExist)
					{
						AddMessage(kError, TEXT("Unknown material \"%s\" for material overloaded for actor %d %s"),
								   MaterialOverride->GetName(), Index, *GetElementsDescription(MeshActor));
					}
				}
			}
			else
			{
				AddMessage(kError, TEXT("Invalid material override %d %s"), Index, *GetElementsDescription(MeshActor));
			}
		}
	}

	// Validate childrens
	Count = InActor.GetChildrenCount();
	for (Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr< IDatasmithActorElement >& Child = InActor.GetChild(Index);
		if (Child.IsValid())
		{
			CheckActorsDependances(*Child);
		}
	}
}

TSharedRef< ISceneValidator > ISceneValidator::CreateForScene(const TSharedRef< IDatasmithScene >& InScene)
{
    return MakeShared<FSceneValidator>(InScene);
}

} // namespace Validator
