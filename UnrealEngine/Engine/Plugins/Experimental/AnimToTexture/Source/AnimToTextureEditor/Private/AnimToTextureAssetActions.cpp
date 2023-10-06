// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimToTextureAssetActions.h"
#include "AnimToTextureDataAsset.h"
#include "AnimToTextureBPLibrary.h"

#include "Materials/MaterialInstanceConstant.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"

FText FAnimToTextureAssetActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AnimToTextureAssetActions", "AnimToTexture");
}

FColor FAnimToTextureAssetActions::GetTypeColor() const
{
	return FColor::Blue;
}

TSharedPtr<SWidget> FAnimToTextureAssetActions::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	const FSlateBrush* Icon = FSlateIconFinder::FindIconBrushForClass(UAnimToTextureDataAsset::StaticClass());;

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetNoBrush())
		.Visibility(EVisibility::HitTestInvisible)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 3.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		[
			SNew(SImage)
			.Image(Icon)
		];
}

UClass* FAnimToTextureAssetActions::GetSupportedClass() const
{
	return UAnimToTextureDataAsset::StaticClass();
}

uint32 FAnimToTextureAssetActions::GetCategories()
{
	return EAssetTypeCategories::Animation;
}

const TArray<FText>& FAnimToTextureAssetActions::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		NSLOCTEXT("AnimToTextureAssetActions", "AnimDeformersSubMenu", "Deformers")
	};
	return SubMenus;
}

void FAnimToTextureAssetActions::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	TArray<TWeakObjectPtr<UAnimToTextureDataAsset>> Objects = GetTypedWeakObjectPtrs<UAnimToTextureDataAsset>(InObjects);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AnimToTextureAssetActions","AnimToTexture_Run", "Run Animation To Texture"),
		NSLOCTEXT("AnimToTextureAssetActions", "AnimToTexture_RunTooltip", "Creates Vertex Animation Textures (VAT)"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAnimToTextureAssetActions::RunAnimToTexture, Objects ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuSeparator();
}

void FAnimToTextureAssetActions::RunAnimToTexture(TArray<TWeakObjectPtr<UAnimToTextureDataAsset>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UAnimToTextureDataAsset* DataAsset = (*ObjIt).Get())
		{
			// Create UVs and Textures
			if (UAnimToTextureBPLibrary::AnimationToTexture(DataAsset))
			{
				// Update Material Instances (if Possible)
				if (UStaticMesh* StaticMesh = DataAsset->GetStaticMesh())
				{
					for (FStaticMaterial& StaticMaterial: StaticMesh->GetStaticMaterials())
					{
						if (UMaterialInstanceConstant* MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(StaticMaterial.MaterialInterface))
						{
							UAnimToTextureBPLibrary::UpdateMaterialInstanceFromDataAsset(DataAsset, MaterialInstanceConstant, EMaterialParameterAssociation::LayerParameter);
						}
					}
				}
			}
		}
	}
}
