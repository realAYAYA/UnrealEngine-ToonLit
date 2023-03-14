// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Thumbnails/CustomizableObjectInstanceThumbnailRenderer.h"

#include "CanvasTypes.h"
#include "Engine/Texture2D.h"
#include "Math/Color.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "SceneView.h"
#include "ShowFlags.h"
#include "Templates/Casts.h"
#include "TextureResource.h"
#include "ThumbnailHelpers.h"
#include "ThumbnailRendering/ThumbnailRenderer.h"
#include "UObject/Class.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Object.h"

class FRenderTarget;
class USkeletalMesh;

#define LOCTEXT_NAMESPACE "CustomizableObjectInstanceThumbnailRenderer"

UCustomizableObjectInstanceThumbnailRenderer::UCustomizableObjectInstanceThumbnailRenderer()
	: Super()
{
	ThumbnailScene = nullptr;
	//CustomizableObjectInstance = ConstructObject<UCustomizableObjectInstance>(UCustomizableObjectInstance::StaticClass(), GetTransientPackage(), NAME_None, RF_Transient);

	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> PSysThumbnail_NoImage;
		ConstructorHelpers::FObjectFinder<UTexture2D> PSysThumbnail_OOD;
		FConstructorStatics()
			: PSysThumbnail_NoImage(TEXT("/Engine/EditorMaterials/ParticleSystems/PSysThumbnail_NoImage"))
			, PSysThumbnail_OOD(TEXT("/Engine/EditorMaterials/ParticleSystems/PSysThumbnail_OOD"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	NoImage = ConstructorStatics.PSysThumbnail_NoImage.Object;
}

void UCustomizableObjectInstanceThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UCustomizableObjectInstance* CustomizableObjectInstance = Cast<UCustomizableObjectInstance>(Object);
	
	if (CustomizableObjectInstance)
	{
		UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject(); 

		if ( ThumbnailScene == nullptr )
		{
			ThumbnailScene = new FSkeletalMeshThumbnailScene();
		}

		//CustomizableObjectInstance = ConstructObject<UCustomizableObjectInstance>(UCustomizableObjectInstance::StaticClass(), GetTransientPackage(), NAME_None, RF_Transient);
		//CustomizableObjectInstance->SetObject(CustomizableObject);
		//CustomizableObjectInstance->bShowOnlyRuntimeParameters = false;

		if (!CustomizableObject->IsCompiled())
		{
			// Use default compilation options?
			FCompilationOptions Options;

			FCustomizableObjectCompiler Compiler;
			// Compiler.Compile(CustomizableObjectInstance->CustomizableObject, Options, false); // TODO Can not be sync!
		}

		// The compilation might fail because the graph is not valid or incomplete, so it has to be checked again
		if (CustomizableObject->GetRefSkeletalMesh() && CustomizableObject->IsCompiled()) // TODO MultiComp
		{
			CustomizableObjectInstance->SetObject(CustomizableObject);
			
			// \todo removed
			//CustomizableObjectInstance->GetPrivate()->ReloadParametersFromObject();
			
			UCustomizableSkeletalComponent* PreviewCustomizableSkeletalComponent = NewObject<UCustomizableSkeletalComponent>(UCustomizableSkeletalComponent::StaticClass());
			PreviewCustomizableSkeletalComponent->CustomizableObjectInstance = CustomizableObjectInstance;

			PreviewCustomizableSkeletalComponent->UpdateSkeletalMeshAsync();
			USkeletalMesh* SkeletalMesh = PreviewCustomizableSkeletalComponent->GetSkeletalMesh();

			if (SkeletalMesh)
			{
				ThumbnailScene->SetSkeletalMesh(SkeletalMesh);

				FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
					// \todo This differs from engine version to engine version. While thumbnails are not enable, we can disable this code.
					//.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime)
					.SetTime(UThumbnailRenderer::GetTime())
					.SetAdditionalViewFamily(bAdditionalViewFamily));

				ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
				ViewFamily.EngineShowFlags.MotionBlur = 0;
				ViewFamily.EngineShowFlags.LOD = 0;

				RenderViewFamily(Canvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height));
				ThumbnailScene->SetSkeletalMesh(nullptr);
			}
			else
			{
				// Use the texture interface to draw
				Canvas->DrawTile(X, Y, Width, Height, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, NoImage->GetResource(), false);
			}

			PreviewCustomizableSkeletalComponent->DestroyComponent();
		}
		else
		{
			// Use the texture interface to draw
			Canvas->DrawTile(X, Y, Width, Height, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, NoImage->GetResource(), false);
		}
	}
}

void UCustomizableObjectInstanceThumbnailRenderer::BeginDestroy()
{
	if ( ThumbnailScene != nullptr )
	{
		delete ThumbnailScene;
		ThumbnailScene = nullptr;
	}

	Super::BeginDestroy();
}


#undef LOCTEXT_NAMESPACE

