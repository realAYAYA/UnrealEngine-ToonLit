// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/LocalHeightFogComponent.h"
#include "Engine/LocalHeightFog.h"
#include "LocalHeightFogSceneProxy.h"
#include "Components/ArrowComponent.h"
#include "Components/DrawSphereComponent.h"
#include "Components/BillboardComponent.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LocalHeightFogComponent)


ULocalHeightFogComponent::ULocalHeightFogComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LocalHeightFogSceneProxy(nullptr)
{
	Mobility = EComponentMobility::Movable;

	// Set a size by default to be visible when droped in the scene
	const float Size = 500.0f;
	SetRelativeScale3D(FVector(Size, Size, Size));
}

ULocalHeightFogComponent::~ULocalHeightFogComponent()
{
}

void ULocalHeightFogComponent::SendRenderTransformCommand()
{
	if (LocalHeightFogSceneProxy)
	{
		FTransform ComponentTransform = GetComponentTransform();
		float HeightOffset = FogHeightOffset;
		FLocalHeightFogSceneProxy* SceneProxy = LocalHeightFogSceneProxy;
		ENQUEUE_RENDER_COMMAND(FUpdateLocalHeightFogSceneProxyTransformCommand)(
			[SceneProxy, ComponentTransform, HeightOffset](FRHICommandList& RHICmdList)
			{
				// Nothing else is needed so that command could actually go.
				SceneProxy->FogTransform = ComponentTransform;
			});
	}
}

void ULocalHeightFogComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);	
	
	bool bHidden = false;
#if WITH_EDITORONLY_DATA
	bHidden = GetOwner() ? GetOwner()->bHiddenEdLevel : false;
#endif // WITH_EDITORONLY_DATA
	if (!ShouldComponentAddToScene())
	{
		bHidden = true;
	}

	if (GetVisibleFlag() && !bHidden && ShouldComponentAddToScene() && ShouldRender() && IsRegistered() && (GetOuter() == NULL || !GetOuter()->HasAnyFlags(RF_ClassDefaultObject)))
	{
		LocalHeightFogSceneProxy = CreateSceneProxy();
		GetWorld()->Scene->AddLocalHeightFog(LocalHeightFogSceneProxy);
	}
}

void ULocalHeightFogComponent::SendRenderTransform_Concurrent()
{
	Super::SendRenderTransform_Concurrent();
	SendRenderTransformCommand();
}

void ULocalHeightFogComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();

	if (LocalHeightFogSceneProxy)
	{
		GetWorld()->Scene->RemoveLocalHeightFog(LocalHeightFogSceneProxy);

		FLocalHeightFogSceneProxy* SceneProxy = LocalHeightFogSceneProxy;
		ENQUEUE_RENDER_COMMAND(FDestroyLocalHeightFogSceneProxyCommand)(
			[SceneProxy](FRHICommandList& RHICmdList)
			{
				delete SceneProxy;
			});

		LocalHeightFogSceneProxy = nullptr;
	}
}

#if WITH_EDITOR

bool ULocalHeightFogComponent::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (FogMode != ELocalFogMode::LocalHeightFog)
		{
			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULocalHeightFogComponent, FogHeightFalloff)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULocalHeightFogComponent, FogHeightOffset))
			{
				return false;
			}
		}
	}

	return true;
}

void ULocalHeightFogComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FProperty* Property = PropertyChangedEvent.Property;
	const FName PropertyName = Property ? Property->GetFName() : NAME_None;

	if (PropertyName == TEXT("RelativeLocation") ||
		PropertyName == TEXT("RelativeRotation") ||
		PropertyName == TEXT("RelativeScale3D"))
	{
		SendRenderTransformCommand();
	}
	else
	{
		MarkRenderStateDirty();
	}
}

#endif // WITH_EDITOR

FLocalHeightFogSceneProxy* ULocalHeightFogComponent::CreateSceneProxy()
{
	return new FLocalHeightFogSceneProxy(this);
}

#if WITH_EDITOR
#include "ObjectEditorUtils.h"
#endif

ALocalHeightFog::ALocalHeightFog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<ULocalHeightFogComponent>(TEXT("NewLocalHeightFogComponent")))
{
	LocalHeightFogVolume = CreateDefaultSubobject<ULocalHeightFogComponent>(TEXT("LocalHeightFogComponent"));
	RootComponent = LocalHeightFogVolume;

#if WITH_EDITORONLY_DATA

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> LocalHeightFogTextureObject;
			FName ID_LocalHeightFog;
			FText NAME_LocalHeightFog;
			FConstructorStatics()
				: LocalHeightFogTextureObject(TEXT("/Engine/EditorResources/S_SkyAtmosphere"))	// TODO
				, ID_LocalHeightFog(TEXT("Fog"))
				, NAME_LocalHeightFog(NSLOCTEXT("SpriteCategory", "Fog", "Fog"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (GetSpriteComponent())
		{
			GetSpriteComponent()->Sprite = ConstructorStatics.LocalHeightFogTextureObject.Get();
			GetSpriteComponent()->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
			GetSpriteComponent()->SpriteInfo.Category = ConstructorStatics.ID_LocalHeightFog;
			GetSpriteComponent()->SpriteInfo.DisplayName = ConstructorStatics.NAME_LocalHeightFog;
			GetSpriteComponent()->SetupAttachment(LocalHeightFogVolume);
		}
	}
#endif // WITH_EDITORONLY_DATA

	PrimaryActorTick.bCanEverTick = true;
	SetHidden(false);
}
