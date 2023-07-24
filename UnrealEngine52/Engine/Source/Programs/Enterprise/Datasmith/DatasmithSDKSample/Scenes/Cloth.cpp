// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"

#include "shared.h"
#include "ScenesManager.h"
#include "MeshUtils.h"

#include "DatasmithSceneExporter.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithMesh.h"
#include "DatasmithCloth.h"
#include "DatasmithMeshExporter.h"
#include "DatasmithExportOptions.h"
#include "GenericPlatform/GenericPlatformFile.h"


struct FClothScene : public ISampleScene
{
	virtual FString GetName() const override { return TEXT("Cloth");}
	virtual FString GetDescription() const override { return TEXT("A sample of cloth description in datasmith."); }
	virtual TArray<FString> GetTags() const override { return {"cloth"}; }
	virtual TSharedPtr<IDatasmithScene> Export() override;
};
REGISTER_SCENE(FClothScene)


TSharedPtr<IDatasmithScene> FClothScene::Export()
{
	FDatasmithSceneExporter DatasmithSceneExporter;

	DatasmithSceneExporter.SetName(*GetName());
	DatasmithSceneExporter.SetOutputPath(*GetExportPath());

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.CreateDirectoryTree(DatasmithSceneExporter.GetOutputPath()))
	{
		return nullptr;
	}

	TSharedRef<IDatasmithScene> DatasmithScene = FDatasmithSceneFactory::CreateScene(*GetName());
	SetupSharedSceneProperties(DatasmithScene);

	// Creates a mesh asset
	{
		FDatasmithMesh BaseMesh;
		CreateSimpleBox(BaseMesh);
		// no path info

		FDatasmithMeshExporter MeshExporter;

		TSharedPtr<IDatasmithMeshElement> MeshElement = MeshExporter.ExportToUObject(DatasmithSceneExporter.GetAssetsOutputPath(), TEXT("BoxMesh"), BaseMesh, nullptr, EDSExportLightmapUV::Always /** Not used */);

		// Add the mesh to the DatasmithScene
		DatasmithScene->AddMesh(MeshElement);
	}

	// Creates a cloth asset and actor
	{
		TSharedRef<IDatasmithClothElement> ClothElement = FDatasmithSceneFactory::CreateCloth(TEXT("my cloth asset"));
		DatasmithScene->AddCloth(ClothElement);

		TSharedRef<IDatasmithClothActorElement> ClothActorElement = FDatasmithSceneFactory::CreateClothActor(TEXT("my cloth actor"));
		ClothActorElement->SetCloth(ClothElement->GetName());
		DatasmithScene->AddActor(ClothActorElement);

		FDatasmithCloth Cloth;

		// Make Rectangle
		auto MakeRectangle = [](FDatasmithCloth& Cloth, FVector3f Offset)
		{
			FDatasmithClothPattern& Pattern = Cloth.Patterns.AddDefaulted_GetRef();
			int32 h = 7;
			int32 w = 7;
			for (int32 a = 0; a < h; ++a)
			{
				for (int32 b = 0; b < w; ++b)
				{
					Pattern.SimPosition.Add(FVector2f(100.f*a, 100.f*b));
					Pattern.SimRestPosition.Add(Offset + FVector3f(100.f*a, 100.f*b, 0.f));

					if (a < h-1 && b < w-1)
					{
						// A---B
						// | / |
						// C---D
						Pattern.SimTriangleIndices.Add(a*w+b); // A
						Pattern.SimTriangleIndices.Add(a*w+b+1); // B
						Pattern.SimTriangleIndices.Add((a+1)*w+b); // C

						Pattern.SimTriangleIndices.Add(a*w+b+1); // B
						Pattern.SimTriangleIndices.Add((a+1)*w+b+1); // D
						Pattern.SimTriangleIndices.Add((a+1)*w+b); // C
					}
				}
			}

// 			// #ue_ds_sdk_todo Proper data API
// 			// eg. SetVertices, SetVertexParameter...
// 			FParameterData& Param = Pattern.Parameters.AddDefaulted_GetRef();
// // 			Param.Name = TEXT("Test");
// 			Param.Data.Set<TArray<double>>({1,2,3});
		};

// 		MakeRectangle(Cloth, FVector3f{});
		MakeRectangle(Cloth, FVector3f{-30.f, 0.f, 90.f});
		MakeRectangle(Cloth, FVector3f{-30.f, 0.f, 190.f});
		MakeRectangle(Cloth, FVector3f{-30.f, 0.f, 290.f});

		FDatasmithMeshExporter MeshExporter;
		FString ClothFilePath = FString(DatasmithSceneExporter.GetAssetsOutputPath()) / ClothElement->GetName();

		TSharedPtr<IDatasmithClothElement> ClothElementPtr = ClothElement;
		MeshExporter.ExportCloth(Cloth, ClothElementPtr, *ClothFilePath, DatasmithSceneExporter.GetOutputPath());
	}

	// Export
	DatasmithSceneExporter.Export(DatasmithScene, false);

	return DatasmithScene;
}