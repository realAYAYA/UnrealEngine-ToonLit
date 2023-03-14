// Copyright Epic Games, Inc. All Rights Reserved.
#include "AssetsImportController.h"
#include "Utilities/MiscUtils.h"
#include "AssetImporters/UAssetNormalImport.h"
#include "AssetImporters/DHIImport.h"
#include "AssetImporters/ProgressiveImport3D.h"
#include "AssetImporters/ProgressiveImportSurfaces.h"
#include "MSSettings.h"

#include "AssetImporters/ImportFactory.h"

#include "Internationalization/Text.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/MessageDialog.h"
#include "GenericPlatform/GenericPlatformMisc.h"




TSharedPtr<FAssetsImportController> FAssetsImportController::AssetsImportController;


// Get instance of the class
TSharedPtr<FAssetsImportController> FAssetsImportController::Get()
{
	if (!AssetsImportController.IsValid())
	{
		AssetsImportController = MakeShareable(new FAssetsImportController);	
	}
	return AssetsImportController;
}

void FAssetsImportController::DataReceived(const FString DataFromBridge)
{
	
	float LocationOffset = 0.0f;

	
	FString StartString = TEXT("{\"QuixelAssets\":");
	FString EndString = TEXT("}");
	FString FinalString = StartString + DataFromBridge + EndString;
	TSharedPtr<FJsonObject> ImportDataObject = DeserializeJson(FinalString);
	

	TArray<TSharedPtr<FJsonValue> > AssetsImportDataArray = ImportDataObject->GetArrayField(TEXT("QuixelAssets"));



	
	for (TSharedPtr<FJsonValue> AssetJson : AssetsImportDataArray)
	{
		
		EAssetImportType ImportType = JsonUtils::GetImportType(AssetJson->AsObject());
		FString AssetType = AssetJson->AsObject()->GetStringField("assetType");
		
		
		if (ImportType == EAssetImportType::MEGASCANS_UASSET)
		{
			CopyMSPresets();
			//if (!SupportedAssetTypes.Contains(AssetType)) continue;

			FString ExportMode = AssetJson->AsObject()->GetStringField(TEXT("exportMode"));
			if (ExportMode == TEXT("normal"))
			{
				FImportUAssetNormal::Get()->ImportAsset(AssetJson->AsObject());
			}
			else if (ExportMode == TEXT("progressive") || ExportMode == TEXT("normal_drag"))
			{
				bool bIsNormal = (ExportMode == TEXT("normal_drag")) ? true : false;				

				if (AssetType == TEXT("3d") || AssetType == TEXT("3dplant"))
				{
					FImportProgressive3D::Get()->ImportAsset(AssetJson->AsObject(), LocationOffset, bIsNormal);
					
				}

				else if (AssetType == TEXT("surface") || AssetType == TEXT("atlas"))
				{
					FImportProgressiveSurfaces::Get()->ImportAsset(AssetJson->AsObject(), LocationOffset, bIsNormal);
				}

				if (AssetJson->AsObject()->GetIntegerField(TEXT("progressiveStage")) == 1 || bIsNormal)
				{
					LocationOffset += 200;
				}
			}			

			

		}	
		else if (ImportType == EAssetImportType::DHI_CHARACTER)
		{
			FImportDHI::Get()->ImportAsset(AssetJson->AsObject());
		}
	
	}	

}


