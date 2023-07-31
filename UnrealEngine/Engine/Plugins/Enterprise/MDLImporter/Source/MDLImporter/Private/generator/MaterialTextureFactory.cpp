// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialTextureFactory.h"

#include "EditorFramework/AssetImportData.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "ObjectTools.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace Generator
{
	FMaterialTextureFactory::FMaterialTextureFactory()
	    : Factory(nullptr)
	{
	}

	UTexture2D* FMaterialTextureFactory::CreateTexture(UObject* ParentPackage, const Common::FTextureProperty& Property, FTextureSourcePtr& Source,
	                                                   EObjectFlags Flags, TArray<MDLImporterLogging::FLogMessage>* InLogMessages /*= nullptr*/)
	{
		const FString TextureName = AssetPrefix + TEXT("_") + ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(Property.Path));
		if (TextureName.IsEmpty())
		{
			return nullptr;
		}

		// save texture settings if texture exists
		Factory->SuppressImportOverwriteDialog();
		UpdateTextureFactorySettings(Factory, Property);

		UTexture2D* Texture = nullptr;
		const FString Extension = FPaths::GetExtension(Property.Path);
		if (Extension == TEXT("png") || Extension == TEXT("jpg") || Extension == TEXT("jpeg"))
		{
			// check for asset collision
			UObject* Asset = LoadObject<UObject>(ParentPackage, *TextureName, nullptr, LOAD_NoWarn);
			if (Asset)
			{
				check(Asset->IsA<UTexture2D>());
				return Cast<UTexture2D>(Asset);  // already loaded
			}

			check(Source == nullptr);
			bool bOperationCanceled = false;
			Texture = static_cast<UTexture2D*>(Factory->FactoryCreateFile(UTexture2D::StaticClass(), ParentPackage, *TextureName, Flags,
			                                                              Property.Path, TEXT("MDL"), nullptr, bOperationCanceled));
			Texture->AssetImportData->Update(Property.Path);
			FAssetRegistryModule::AssetCreated(Texture);
			Texture->MarkPackageDirty();
		}
		else
		{
			if (Source != nullptr)
			{
				check(Source->GetWidth() > 4 && Source->GetHeight() > 4);
				Texture = Factory->CreateTexture2D(ParentPackage, *TextureName, Flags);
				Texture->Source.Init(*Source);
				Texture->Source.Compress();

				delete Source;
				Source = nullptr;
			}
			else
			{
				if (InLogMessages)
				{
					InLogMessages->Emplace(MDLImporterLogging::EMessageSeverity::Warning, TEXT("Not supported texture format '") + Extension + "' for " + Property.Path);
				}
			}
		}

		if (Texture)
		{
			UpdateTextureSettings(Texture, Property, InLogMessages);
		}

		return Texture;
	}

	void FMaterialTextureFactory::UpdateTextureFactorySettings(UTextureFactory* TextureFactory, const Common::FTextureProperty& Property)
	{
		// Set the settings on the factory so that the texture gets built with the correct settings when the factory builds it
		TextureFactory->bUseHashAsGuid = true; // Use texture source data hash as DDC guid
		TextureFactory->MipGenSettings = Property.MipGenSettings;
		TextureFactory->NoAlpha = Property.bCompressionNoAlpha;
		TextureFactory->CompressionSettings = Property.CompressionSettings;
		TextureFactory->LODGroup = Property.LODGroup;
		TextureFactory->bFlipNormalMapGreenChannel = Property.bFlipGreenChannel;
		TextureFactory->ColorSpaceMode = Property.bIsSRGB ? ETextureSourceColorSpace::SRGB : ETextureSourceColorSpace::Linear;
	}

	void FMaterialTextureFactory::UpdateTextureSettings(UTexture2D* Texture, const Common::FTextureProperty& Property, TArray<MDLImporterLogging::FLogMessage>* LogMessages /*= nullptr*/)
	{
		TextureMipGenSettings MipGenSettings = Property.MipGenSettings;

		if (!Texture->Source.IsPowerOfTwo())
		{
			MipGenSettings = TMGS_NoMipmaps;

			if (LogMessages)
			{
				LogMessages->Emplace(
					MDLImporterLogging::EMessageSeverity::Warning,
					FString::Printf(TEXT("Texture %s does not have power of two dimensions and therefore no mipmaps will be generated"),
						*Texture->GetName()));
			}
		}

		const bool bIsSRGB = Texture->IsNormalMap() ? false : Property.bIsSRGB;

		const bool bSettingsChanged =
			(Texture->MipGenSettings != MipGenSettings || Texture->CompressionNoAlpha != Property.bCompressionNoAlpha ||
				Texture->CompressionSettings != Property.CompressionSettings || Texture->Filter != Property.Filter ||
				Texture->AddressY != Property.Address || Texture->AddressX != Property.Address ||
				Texture->SRGB != bIsSRGB || Texture->bFlipGreenChannel != Property.bFlipGreenChannel);

		if (bSettingsChanged)
		{
			Texture->MipGenSettings = MipGenSettings;
			Texture->CompressionNoAlpha = Property.bCompressionNoAlpha;
			Texture->CompressionSettings = Property.CompressionSettings;
			Texture->Filter = Property.Filter;
			Texture->AddressY = Property.Address;
			Texture->AddressX = Property.Address;
			Texture->LODGroup = Property.LODGroup;
			Texture->SRGB = bIsSRGB;
			Texture->bFlipGreenChannel = Property.bFlipGreenChannel;
			Texture->UpdateResource();
			Texture->PostEditChange();
		}
	}

}  // namespace Generator
