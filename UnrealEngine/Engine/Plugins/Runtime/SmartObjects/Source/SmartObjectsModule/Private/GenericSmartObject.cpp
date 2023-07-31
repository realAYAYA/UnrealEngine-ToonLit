// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericSmartObject.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Texture2D.h"
#include "Components/BillboardComponent.h"
#include "SmartObjectRenderingComponent.h"
#include "SmartObjectComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GenericSmartObject)

#if WITH_EDITOR
#include "ObjectEditorUtils.h"
#endif // WITH_EDITOR


AGenericSmartObject::AGenericSmartObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent = SceneComponent;
	RootComponent->Mobility = EComponentMobility::Static;

	SOComponent = CreateDefaultSubobject<USmartObjectComponent>(TEXT("SmartObjectComp"));
	SOComponent->SetupAttachment(RootComponent);

#if WITH_EDITORONLY_DATA
	UBillboardComponent* SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	RenderingComponent = CreateEditorOnlyDefaultSubobject<USmartObjectRenderingComponent>(TEXT("RenderComp"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> NoteTextureObject;
			FName NotesID;
			FText GenericSOName;
			FConstructorStatics()
				: NoteTextureObject(TEXT("/SmartObjects/S_BrainInBox"))
				, NotesID(TEXT("SmartObject"))
				, GenericSOName(NSLOCTEXT("SpriteCategory", "GenericSO", "GenericSO"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;
		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.NoteTextureObject.Get();
			SpriteComponent->SetRelativeScale3D_Direct(FVector(0.5f, 0.5f, 0.5f));
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.NotesID;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.GenericSOName;
			SpriteComponent->SetupAttachment(RootComponent);
			SpriteComponent->Mobility = EComponentMobility::Static;
		}

		if (RenderingComponent)
		{
			RenderingComponent->SetupAttachment(RootComponent);
		}
	}

#endif // WITH_EDITORONLY_DATA

	SetHidden(true);
	SetCanBeDamaged(false);
}

#if WITH_EDITOR
void AGenericSmartObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName SmartObjectName = FName(TEXT("SmartObject"));

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		if (FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.Property) == SmartObjectName)
		{
			if (RenderingComponent)
			{
				MarkComponentsRenderStateDirty();
			}
		}
	}
}
#endif // WITH_EDITOR

