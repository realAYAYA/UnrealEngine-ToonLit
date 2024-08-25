// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mask2D/AvaMask2DWriteModifier.h"

#include "AvaMaskUtilities.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Framework/AvaGizmoComponent.h"
#include "GeometryMaskCanvas.h"
#include "GeometryMaskWriteComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Modifiers/ActorModifierCoreStack.h"

#define LOCTEXT_NAMESPACE "AvaMask2DModifier"

#if WITH_EDITOR
const TAvaPropertyChangeDispatcher<UAvaMask2DWriteModifier> UAvaMask2DWriteModifier::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UAvaMask2DWriteModifier, WriteOperation), &UAvaMask2DWriteModifier::OnWriteOperationChanged }
};
#endif

#if WITH_EDITOR
void UAvaMask2DWriteModifier::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	UAvaMask2DWriteModifier::PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UAvaMask2DWriteModifier::Apply()
{
	Super::Apply();
	
	if (GetCurrentCanvas())
	{
		for (TPair<TWeakObjectPtr<AActor>, FAvaMask2DActorData>& ActorDataPair : ActorData)
		{
			if (AActor* Actor = ActorDataPair.Key.Get())
			{
				if (!ApplyWrite(Actor, ActorDataPair.Value))
				{
					return;
				}
			}
		}
	}

	Next();
}

bool UAvaMask2DWriteModifier::ApplyWrite(AActor* InActor, FAvaMask2DActorData& InActorData)
{
	// Only add read/write component to actors with primitives
	if (ActorSupportsMaskReadWrite(InActor))
	{
		if (const UGeometryMaskCanvas* GeometryCanvas = GetCurrentCanvas())
		{
			const UTexture* CanvasTexture = TryResolveCanvasTexture(InActor, InActorData);
			if (!CanvasTexture)
			{
				return true;
			}
		
			if (UGeometryMaskWriteMeshComponent* WriteComponent = FindOrAddMaskComponent<UGeometryMaskWriteMeshComponent>(InActor))
			{
				FGeometryMaskWriteParameters Parameters = WriteComponent->GetParameters();
				Parameters.CanvasName = Channel;
				Parameters.ColorChannel = GeometryCanvas->GetColorChannel();
				Parameters.OperationType = WriteOperation;

				WriteComponent->SetParameters(Parameters);
			}
		}
	}

	if (UAvaGizmoComponent* VisualizationComponent = UE::AvaMask::Internal::FindOrAddComponent<UAvaGizmoComponent>(InActor))
	{
		VisualizationComponent->SetVisibleInEditor(true);
		VisualizationComponent->SetHiddenInGame(false);
		VisualizationComponent->SetShowWireframe(false);
		VisualizationComponent->SetWireframeColor(FLinearColor::Red);
		VisualizationComponent->SetsStencil(true);
		VisualizationComponent->SetStencilId(150);
		VisualizationComponent->SetRenderInMainPass(false);
		VisualizationComponent->SetRenderDepth(false);
		VisualizationComponent->SetMaterialToDefault();
		VisualizationComponent->ApplyGizmoValues();
	}

	return true;
}

void UAvaMask2DWriteModifier::SetWriteOperation(const EGeometryMaskCompositeOperation InWriteOperation)
{
	if (WriteOperation != InWriteOperation)
	{
		WriteOperation = InWriteOperation;
		OnWriteOperationChanged();
	}
}

void UAvaMask2DWriteModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("MaskWrite"));
	InMetadata.SetCategory(TEXT("Rendering"));
	InMetadata.DisallowAfter(TEXT("MaskRead"));
	InMetadata.DisallowBefore(TEXT("MaskRead"));
#if WITH_EDITOR
	InMetadata.SetDisplayName(FText::FromString(TEXT("Mask (Set)")));
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Allows to use a custom mask texture on attached actors materials"));
#endif
}

void UAvaMask2DWriteModifier::SetupMaskComponent(UActorComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}
	
	if (IGeometryMaskWriteInterface* MaskWriter = Cast<IGeometryMaskWriteInterface>(InComponent))
	{
		SetupMaskWriteComponent(MaskWriter);
	}

	Super::SetupMaskComponent(InComponent);
}

void UAvaMask2DWriteModifier::SetupMaskWriteComponent(IGeometryMaskWriteInterface* InMaskWriter)
{
	if (const UGeometryMaskCanvas* Canvas = GetCurrentCanvas())
	{
		// Set canvas name to write to
		FGeometryMaskWriteParameters WriteParameters = InMaskWriter->GetParameters();
		WriteParameters.CanvasName = Channel;
		WriteParameters.ColorChannel = Canvas->GetColorChannel();

		InMaskWriter->SetParameters(WriteParameters);
	}
}

void UAvaMask2DWriteModifier::OnWriteOperationChanged()
{
	MarkModifierDirty();
}

#undef LOCTEXT_NAMESPACE
