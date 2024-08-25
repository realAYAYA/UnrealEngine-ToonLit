// Copyright Epic Games, Inc. All Rights Reserved.

#include "SVGDynamicMeshVisualizer.h"
#include "EditorViewportClient.h"
#include "HitProxies.h"
#include "ProceduralMeshes/SVGDynamicMeshComponent.h"
#include "SVGActor.h"
#include "SceneManagement.h"

float FSVGDynamicMeshVisualizer::FillsExtrudeMin;
float FSVGDynamicMeshVisualizer::FillsExtrudeMax;
float FSVGDynamicMeshVisualizer::StrokesExtrudeMin;
float FSVGDynamicMeshVisualizer::StrokesExtrudeMax;

struct HSVGActorExtrudeHitProxy : HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HSVGActorExtrudeHitProxy(const UActorComponent* InComponent, ASVGActor* InSVGActor, bool bInIsFirstComponent)
		: HComponentVisProxy(InComponent, HPP_Wireframe)
		, SVGActorWeak(InSVGActor)
		, bIsFirstComponent(bInIsFirstComponent)
	{ }

	TWeakObjectPtr<ASVGActor> SVGActorWeak;
	bool bIsFirstComponent = false;
};

IMPLEMENT_HIT_PROXY(HSVGActorExtrudeHitProxy, HComponentVisProxy)

FSVGDynamicMeshVisualizer::FSVGDynamicMeshVisualizer()
{
	bIsFirstSVGComponent = false;
	bIsExtruding = false;

	UpdateMinMaxExtrudeValues();
}

void FSVGDynamicMeshVisualizer::DrawVisualization(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI)
{
	constexpr static float ExtrudeHandleSize = 10.0f;

	const USVGDynamicMeshComponent* SVGDynamicMeshComponent = Cast<USVGDynamicMeshComponent>(InComponent);
	if (!SVGDynamicMeshComponent)
	{
		InPDI->SetHitProxy(nullptr);
		EndEditing();
		return;
	}

	if (ASVGActor* SVGActor = Cast<ASVGActor>(SVGDynamicMeshComponent->GetOwner()))
	{
		if (SVGActor->ExtrudeType == ESVGExtrudeType::None)
		{
			EndEditing();
		}
		else
		{
			SVGActorWeak = SVGActor;
			const TArray<UDynamicMeshComponent*>& SVGShapeComponents = SVGActor->GetSVGDynamicMeshes();

			if (!SVGShapeComponents.IsEmpty())
			{
				if (SVGDynamicMeshComponent == SVGShapeComponents[0])
				{
					const FVector ProxyHandleLocation = GetExtrudeWidgetLocation();
					const FVector LineStartLocation = GetExtrudeSurfaceLocation();
					InPDI->DrawLine(LineStartLocation, ProxyHandleLocation, FLinearColor::White, SDPG_Foreground);

					InPDI->SetHitProxy(new HSVGActorExtrudeHitProxy(SVGDynamicMeshComponent, SVGActor, true));
					InPDI->DrawPoint(ProxyHandleLocation, FLinearColor::White, ExtrudeHandleSize, SDPG_Foreground);
					InPDI->SetHitProxy(nullptr);
				}
			}
		}
	}

	// Draw box to highlight bevel setting while in interactive mode
	if (SVGDynamicMeshComponent->bIsBevelBeingEdited)
	{
		FVector BoxLocation = SVGDynamicMeshComponent->GetComponentLocation() - FVector(SVGDynamicMeshComponent->Bounds.BoxExtent.X, 0, 0) + FVector(SVGDynamicMeshComponent->Bevel/2.0, 0, 0);

		if (SVGDynamicMeshComponent->ExtrudeType == ESVGExtrudeType::FrontFaceOnly)
		{
			BoxLocation -= FVector(SVGDynamicMeshComponent->Bounds.BoxExtent.X, 0, 0);
		}

		FBox Box;
		Box.Max = BoxLocation + FVector(SVGDynamicMeshComponent->Bevel/2.0f, SVGDynamicMeshComponent->Bounds.BoxExtent.Y, SVGDynamicMeshComponent->Bounds.BoxExtent.Z);
		Box.Min = BoxLocation - FVector(SVGDynamicMeshComponent->Bevel/2.0f, SVGDynamicMeshComponent->Bounds.BoxExtent.Y, SVGDynamicMeshComponent->Bounds.BoxExtent.Z);

		DrawWireBox(InPDI, Box, FColor::Green, SDPG_Foreground);
	}
}

bool FSVGDynamicMeshVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	EndEditing();

	if (Click.GetKey() == EKeys::LeftMouseButton && VisProxy)
	{
		if (VisProxy->IsA(HSVGActorExtrudeHitProxy::StaticGetType()))
		{
			if (const HSVGActorExtrudeHitProxy* const ExtrudeProxy = static_cast<HSVGActorExtrudeHitProxy*>(VisProxy))
			{
				if (ASVGActor* SVGActor = ExtrudeProxy->SVGActorWeak.Get())
				{
					if (ExtrudeProxy->bIsFirstComponent)
					{
						GEditor->SelectActor(SVGActor, true, false);
						bIsExtruding = true;
						bIsFirstSVGComponent = ExtrudeProxy->bIsFirstComponent;
						SVGActorWeak = SVGActor;
						SVGMeshComponent = Cast<USVGDynamicMeshComponent>(const_cast<UActorComponent*>(ExtrudeProxy->Component.Get()));
						InViewportClient->SetWidgetMode(UE::Widget::WM_None);
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool FSVGDynamicMeshVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	if (bIsFirstSVGComponent && bIsExtruding)
	{
		if (SVGActorWeak.IsValid())
		{
			OutLocation = GetExtrudeWidgetLocation();
			return true;
		}
	}

	return false;
}

bool FSVGDynamicMeshVisualizer::HandleInputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDeltaTranslate, FRotator& InDeltaRotate, FVector& InDeltaScale)
{
	if (!GetEditedComponent() || !GetEditedComponent()->GetOwner())
	{
		EndEditing();
		return false;
	}

	if (!InDeltaTranslate.IsZero())
	{
		if (bIsFirstSVGComponent)
		{
			if (bIsExtruding)
			{
				if (ASVGActor* SVGActor = SVGActorWeak.Get())
				{
					SVGActor->Modify();

					float FillsExtrudeDepth = SVGActor->GetFillsExtrude() - InDeltaTranslate.X;
					FillsExtrudeDepth = FMath::Clamp(FillsExtrudeDepth, FillsExtrudeMin, FillsExtrudeMax);
					SVGActor->SetFillsExtrudeInteractive(FillsExtrudeDepth);

					float StrokesExtrudeDepth = SVGActor->GetStrokesExtrude() - InDeltaTranslate.X;
					StrokesExtrudeDepth = FMath::Clamp(StrokesExtrudeDepth, StrokesExtrudeMin, StrokesExtrudeMax);
					SVGActor->SetStrokesExtrudeInteractive(StrokesExtrudeDepth);

					return true;
				}
			}
		}
	}

	EndEditing();
	return false;
}

void FSVGDynamicMeshVisualizer::EndEditing()
{
	bIsFirstSVGComponent = false;
	bIsExtruding = false;
}

void FSVGDynamicMeshVisualizer::TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove)
{
	if (ASVGActor* SVGActor = SVGActorWeak.Get())
	{
		SVGActor->Modify();

		const float ExtrudeDepth = FMath::Clamp(SVGActor->GetFillsExtrude(), FillsExtrudeMin, FillsExtrudeMax);
		SVGActor->SetFillsExtrude(ExtrudeDepth);

		const float StrokesExtrudeDepth = FMath::Clamp(SVGActor->GetStrokesExtrude(), StrokesExtrudeMin, StrokesExtrudeMax);
		SVGActor->SetStrokesExtrude(StrokesExtrudeDepth);
	}

	FComponentVisualizer::TrackingStopped(InViewportClient, bInDidMove);
}

UActorComponent* FSVGDynamicMeshVisualizer::GetEditedComponent() const
{
	return SVGMeshComponent.Get();
}

FVector FSVGDynamicMeshVisualizer::GetExtrudeSurfaceLocation() const
{
	if (const ASVGActor* SVGActor = SVGActorWeak.Get())
	{
		float ExtrudeDepth = FMath::Max(SVGActor->GetFillsExtrude(), SVGActor->GetStrokesExtrude());

		if (SVGActor->ExtrudeType == ESVGExtrudeType::FrontBackMirror)
		{
			ExtrudeDepth *= 0.5f;
		}

		const FVector SurfaceOffset = -SVGActor->GetActorForwardVector() * ExtrudeDepth;

		return SVGActor->GetActorLocation() + SurfaceOffset;
	}

	return {};
}

FVector FSVGDynamicMeshVisualizer::GetExtrudeWidgetLocation() const
{
	constexpr static float ExtrudeHandleOffset = 10.0f;
	const FVector SurfaceLocation = GetExtrudeSurfaceLocation();

	if (const ASVGActor* SVGActor = SVGActorWeak.Get())
	{
		const float ExtrudeDepth = FMath::Max(SVGActor->GetFillsExtrude(), SVGActor->GetStrokesExtrude());
		FVector HandleOffset = SVGActor->GetActorForwardVector() * ExtrudeHandleOffset;

		if (ExtrudeDepth >= 0)
		{
			HandleOffset = -HandleOffset;
		}

		return SurfaceLocation + HandleOffset;
	}

	return {};
}

void FSVGDynamicMeshVisualizer::UpdateMinMaxExtrudeValues()
{
	static float DefaultExtrudeMin = 0.01f;
	static float DefaultExtrudeMax = 20.0f;

	// Get Min and Max Extrude values from Properties Metadata, to match what the UI allows

	if (const FProperty* FillsExtrudeProperty = ASVGActor::StaticClass()->FindPropertyByName(TEXT("FillsExtrude")))
	{
		if (FillsExtrudeProperty->HasMetaData("UIMin"))
		{
			FillsExtrudeMin = FCString::Atof(*FillsExtrudeProperty->GetMetaData("UIMin"));
		}
		else
		{
			FillsExtrudeMin = DefaultExtrudeMin;
		}

		if (FillsExtrudeProperty->HasMetaData("UIMax"))
		{
			FillsExtrudeMax = FCString::Atof(*FillsExtrudeProperty->GetMetaData("UIMax"));
		}
		else
		{
			FillsExtrudeMax = DefaultExtrudeMax;
		}
	}
	else
	{
		FillsExtrudeMin = DefaultExtrudeMin;
		FillsExtrudeMax = DefaultExtrudeMax;
	}

	if (const FProperty* StrokesExtrudeProperty = ASVGActor::StaticClass()->FindPropertyByName(TEXT("StrokesExtrude")))
	{
		if (StrokesExtrudeProperty->HasMetaData("UIMin"))
		{
			StrokesExtrudeMin = FCString::Atof(*StrokesExtrudeProperty->GetMetaData("UIMin"));
		}
		else
		{
			StrokesExtrudeMin = DefaultExtrudeMin;
		}

		if (StrokesExtrudeProperty->HasMetaData("UIMax"))
		{
			StrokesExtrudeMax = FCString::Atof(*StrokesExtrudeProperty->GetMetaData("UIMax"));
		}
		else
		{
			StrokesExtrudeMax = DefaultExtrudeMax;
		}
	}
	else
	{
		StrokesExtrudeMin = DefaultExtrudeMin;
		StrokesExtrudeMax = DefaultExtrudeMax;
	}
}
