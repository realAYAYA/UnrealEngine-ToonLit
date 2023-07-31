// Copyright Epic Games, Inc. All Rights Reserved.

#include "UVEditorBackgroundPreview.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Drawing/MeshWireframeComponent.h"
#include "Engine/Texture2D.h"
#include "ToolSetupUtil.h"
#include "Async/Async.h"
#include "UVEditorUXSettings.h"
#include "UDIMUtilities.h"
#include "Materials/MaterialExpressionTextureBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UVEditorBackgroundPreview)

using namespace UE::Geometry;

void UUVEditorBackgroundPreview::OnCreated()
{
	Settings = NewObject<UUVEditorBackgroundPreviewProperties>(this);
	Settings->WatchProperty(Settings->bVisible, [this](bool) { bSettingsModified = true; });
	Settings->WatchProperty(Settings->SourceType, [this](EUVEditorBackgroundSourceType) { bSettingsModified = true; });
	Settings->WatchProperty(Settings->SourceTexture, [this](UTexture2D*) {bSettingsModified = true; });
	Settings->WatchProperty(Settings->SourceMaterial, [this](UMaterial*) {bSettingsModified = true; });
	Settings->WatchProperty(Settings->UDIMBlocks, [this](const TArray<int32>&) {bSettingsModified = true; });

	bSettingsModified = false;

	BackgroundComponent = NewObject<UTriangleSetComponent>(GetActor());
	BackgroundComponent->SetupAttachment(GetActor()->GetRootComponent());
	BackgroundComponent->RegisterComponent();
}

void UUVEditorBackgroundPreview::OnTick(float DeltaTime)
{
	// Check if the CVAR has been updated behind the scenes
	bool bEnableUDIMSupport = (FUVEditorUXSettings::CVarEnablePrototypeUDIMSupport.GetValueOnGameThread() > 0);
	if (Settings->bUDIMsEnabled != bEnableUDIMSupport)
	{
		Settings->bUDIMsEnabled = bEnableUDIMSupport;
		bSettingsModified = true;
	}

	if (bSettingsModified)
	{		
		UpdateBackground();
		UpdateVisibility();
		bSettingsModified = false;
		OnBackgroundMaterialChange.Broadcast(BackgroundMaterial);
	}
}

void UUVEditorBackgroundPreview::UpdateVisibility()
{
	if (Settings->bVisible == false)
	{
		BackgroundComponent->SetVisibility(false);
		return;
	}

	BackgroundComponent->SetVisibility(true);
	BackgroundComponent->MarkRenderStateDirty();
}

void UUVEditorBackgroundPreview::UpdateBackground()
{
	const FVector Normal(0, 0, 1);
	const FColor BackgroundColor = FColor::Blue;

	UMaterial* RegularMaterial = LoadObject<UMaterial>(nullptr, TEXT("/UVEditor/Materials/UVEditorBackground"));
	UMaterial* VirtualTextureMaterial = LoadObject<UMaterial>(nullptr, TEXT("/UVEditor/Materials/UVEditorVTBackground"));

	check(RegularMaterial);
	check(VirtualTextureMaterial);

	BackgroundMaterial = UMaterialInstanceDynamic::Create(RegularMaterial, this);
	switch (Settings->SourceType)
	{
		case EUVEditorBackgroundSourceType::Checkerboard:
		{
			// Do nothing, since the default material is already set up for a checkerboard.
		}
		break;

		case EUVEditorBackgroundSourceType::Material:
		{
			if (Settings->SourceMaterial)
			{
				BackgroundMaterial = UMaterialInstanceDynamic::Create(Settings->SourceMaterial.Get(), this);
			}
		}
		break;

		case EUVEditorBackgroundSourceType::Texture:
		{
			if (Settings->SourceTexture)
			{
				if (Settings->SourceTexture->IsCurrentlyVirtualTextured())
				{
					BackgroundMaterial = UMaterialInstanceDynamic::Create(VirtualTextureMaterial, this);
				}

				EMaterialSamplerType SamplerRequiredForTexture = UMaterialExpressionTextureBase::GetSamplerTypeForTexture(Settings->SourceTexture.Get());

				switch (SamplerRequiredForTexture)
				{
				case EMaterialSamplerType::SAMPLERTYPE_Color:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_Color"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("TextureSampler"), 0);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_Grayscale:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_Grayscale"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("TextureSampler"), 1);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_Alpha:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_Alpha"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("TextureSampler"), 2);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_Normal:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_Normal"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("TextureSampler"), 3);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_Masks:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_Masks"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("TextureSampler"), 4);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_DistanceFieldFont:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_DistanceFeildFont"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("TextureSampler"), 5);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_LinearColor:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_LinearColor"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("TextureSampler"), 6);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_LinearGrayscale:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_LinearGrayscale"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("TextureSampler"), 7);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_Data:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_Data"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("TextureSampler"), 8);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_External:
					// TODO: Enable support for External textures at a future date as needed.
					//BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_Data"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("TextureSampler"), 9);
					break;

				case EMaterialSamplerType::SAMPLERTYPE_VirtualColor:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_VTColor"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("VirtualTextureSampler"), 0);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_VirtualGrayscale:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_VTGrayscale"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("VirtualTextureSampler"), 1);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_VirtualAlpha:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_VTAlpha"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("VirtualTextureSampler"), 2);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_VirtualNormal:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_VTNormal"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("VirtualTextureSampler"), 3);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_VirtualMasks:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_VTMasks"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("VirtualTextureSampler"), 4);
					break;						
				case EMaterialSamplerType::SAMPLERTYPE_VirtualLinearColor:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_VTLinearColor"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("VirtualTextureSampler"), 6);
					break;
				case EMaterialSamplerType::SAMPLERTYPE_VirtualLinearGrayscale:
					BackgroundMaterial->SetTextureParameterValue(TEXT("BackgroundBaseMap_VTLinearGrayscale"), Settings->SourceTexture);
					BackgroundMaterial->SetScalarParameterValue(TEXT("VirtualTextureSampler"), 7);
					break;
				}
			}
		}
		break;

		default:
			ensure(false);
	}

	BackgroundMaterial->SetScalarParameterValue(TEXT("BackgroundPixelDepthOffset"), FUVEditorUXSettings::BackgroundQuadDepthOffset);	
	BackgroundComponent->Clear();

	TArray<FVector2f> UDimBlocksToRender;
	if (Settings->bUDIMsEnabled)
	{
		// TODO: Find a way to access the list of UDIMs from the context object instead? 
		for (int32 BlockIndex = 0; BlockIndex < Settings->UDIMBlocks.Num(); ++BlockIndex)
		{
			FVector2f Block;
			int32 UCoord, VCoord;
			UE::TextureUtilitiesCommon::ExtractUDIMCoordinates(Settings->UDIMBlocks[BlockIndex], UCoord, VCoord);
			Block.X = static_cast<float>(UCoord);
			Block.Y = static_cast<float>(VCoord);
			UDimBlocksToRender.Push(Block);
		}
	}
	if (UDimBlocksToRender.Num() == 0)
	{
		UDimBlocksToRender.Push(FVector2f(0, 0));
	}

	for (const FVector2f& GridStep : UDimBlocksToRender)
	{
		FVector2f UV00 = { (GridStep.X + 0.0f) , (GridStep.Y + 0.0f) };
		FVector2f UV01 = { (GridStep.X + 1.0f) , (GridStep.Y + 0.0f) };		
		FVector2f UV10 = { (GridStep.X + 0.0f) , (GridStep.Y + 1.0f) };
		FVector2f UV11 = { (GridStep.X + 1.0f) , (GridStep.Y + 1.0f) };

		UV00 = FUVEditorUXSettings::ExternalUVToInternalUV(UV00);
		UV01 = FUVEditorUXSettings::ExternalUVToInternalUV(UV01);
		UV10 = FUVEditorUXSettings::ExternalUVToInternalUV(UV10);
		UV11 = FUVEditorUXSettings::ExternalUVToInternalUV(UV11);

		FRenderableTriangleVertex A((FVector)FUVEditorUXSettings::UVToVertPosition(UV00), (FVector2D)UV00, Normal, BackgroundColor);
		FRenderableTriangleVertex B((FVector)FUVEditorUXSettings::UVToVertPosition(UV10), (FVector2D)UV10, Normal, BackgroundColor);
		FRenderableTriangleVertex C((FVector)FUVEditorUXSettings::UVToVertPosition(UV01), (FVector2D)UV01, Normal, BackgroundColor);
		FRenderableTriangleVertex D((FVector)FUVEditorUXSettings::UVToVertPosition(UV11), (FVector2D)UV11, Normal, BackgroundColor);

		FRenderableTriangle Lower(BackgroundMaterial, A, D, B);
		FRenderableTriangle Upper(BackgroundMaterial, A, C, D);

		BackgroundComponent->AddTriangle(Lower);
		BackgroundComponent->AddTriangle(Upper);
	}
}

