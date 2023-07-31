// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_TextureRenderTarget.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Engine/VolumeTexture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTarget2DArray.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_TextureRenderTarget::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	FAssetTypeActions_Texture::GetActions(InObjects, Section);

	auto RenderTargets = GetTypedWeakObjectPtrs<UTextureRenderTarget>(InObjects);

	Section.AddMenuEntry(
		"TextureRenderTarget_CreateStatic",
		LOCTEXT("TextureRenderTarget_CreateStatic", "Create Static Texture"),
		LOCTEXT("TextureRenderTarget_CreateStaticTooltip", "Creates a static texture from the selected render targets."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_TextureRenderTarget::ExecuteCreateStatic, RenderTargets ),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_TextureRenderTarget::ExecuteCreateStatic(TArray<TWeakObjectPtr<UTextureRenderTarget>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			FString Name;
			FString PackageName;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), TEXT("_Tex"), PackageName, Name);

			UObject* NewObj = NULL;
			UTextureRenderTarget2D* TexRT = Cast<UTextureRenderTarget2D>(Object);
			UTextureRenderTarget2DArray* TexRT2DArray = Cast<UTextureRenderTarget2DArray>(Object);
			UTextureRenderTargetCube* TexRTCube = Cast<UTextureRenderTargetCube>(Object);
			UTextureRenderTargetVolume* TexRTVolume = Cast<UTextureRenderTargetVolume>(Object);
			if( TexRTCube )
			{
				// create a static cube texture as well as its 6 faces
				NewObj = TexRTCube->ConstructTextureCube( CreatePackage(*PackageName), Name, Object->GetMaskedFlags() );
			}
			else if (TexRTVolume)
			{
				NewObj = TexRTVolume->ConstructTextureVolume(CreatePackage( *PackageName), Name, Object->GetMaskedFlags());
			}
			else if (TexRT2DArray)
			{
				NewObj = TexRT2DArray->ConstructTexture2DArray(CreatePackage(*PackageName), Name, Object->GetMaskedFlags());
			}
			else if( TexRT )
			{
				// create a static 2d texture
				NewObj = TexRT->ConstructTexture2D( CreatePackage(*PackageName), Name, Object->GetMaskedFlags(), CTF_Default, NULL );
			}

			if( NewObj )
			{
				// package needs saving
				NewObj->MarkPackageDirty();

				// Notify the asset registry
				FAssetRegistryModule::AssetCreated(NewObj);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
