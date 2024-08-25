// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/AvaClonerActorVis.h"
#include "AvaField.h"
#include "AvaShapeSprites.h"
#include "Cloner/CEClonerActor.h"
#include "Cloner/CEClonerComponent.h"
#include "Cloner/Layouts/CEClonerCircleLayout.h"
#include "Cloner/Layouts/CEClonerCylinderLayout.h"
#include "Cloner/Layouts/CEClonerGridLayout.h"
#include "Cloner/Layouts/CEClonerHoneycombLayout.h"
#include "Cloner/Layouts/CEClonerLineLayout.h"
#include "Cloner/Layouts/CEClonerSphereUniformLayout.h"
#include "EditorViewportClient.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "IAvalancheComponentVisualizersModule.h"
#include "ScopedTransaction.h"
#include "TextureResource.h"

IMPLEMENT_HIT_PROXY(HAvaClonerActorSpacingHitProxy, HAvaHitProxy);

#define LOCTEXT_NAMESPACE "AvaClonerActorVisualizer"

FAvaClonerActorVisualizer::FAvaClonerActorVisualizer()
	: FAvaVisualizerBase()
{
}

void FAvaClonerActorVisualizer::StoreInitialValues()
{
	Super::StoreInitialValues();

	if (GetEditedComponent() == nullptr)
	{
		return;
	}

	const ACEClonerActor* ClonerActor = ClonerActorWeak.Get();

	if (!ClonerActor)
	{
		return;
	}

	if (const UCEClonerGridLayout* GridLayout = ClonerActor->GetActiveLayout<UCEClonerGridLayout>())
	{
		InitialSpacing = FVector(GridLayout->GetSpacingX(), GridLayout->GetSpacingY(), GridLayout->GetSpacingZ());
	}
	else if (const UCEClonerLineLayout* LineLayout = ClonerActor->GetActiveLayout<UCEClonerLineLayout>())
	{
		InitialSpacing = FVector(LineLayout->GetSpacing());
	}
	else if (const UCEClonerHoneycombLayout* HoneycombLayout = ClonerActor->GetActiveLayout<UCEClonerHoneycombLayout>())
	{
		const ECEClonerPlane Plane = HoneycombLayout->GetPlane();

		FVector Spacing;
		if (Plane == ECEClonerPlane::XY)
		{
			Spacing = FVector(HoneycombLayout->GetWidthSpacing(), HoneycombLayout->GetHeightSpacing(), 0);
		}
		else if (Plane == ECEClonerPlane::YZ)
		{
			Spacing = FVector(0, HoneycombLayout->GetWidthSpacing(), HoneycombLayout->GetHeightSpacing());
		}
		else if (Plane == ECEClonerPlane::XZ)
		{
			Spacing = FVector(HoneycombLayout->GetWidthSpacing(), 0, HoneycombLayout->GetHeightSpacing());
		}

		InitialSpacing = Spacing;
	}
	else if (const UCEClonerCircleLayout* CircleLayout = ClonerActor->GetActiveLayout<UCEClonerCircleLayout>())
	{
		InitialSpacing = FVector(CircleLayout->GetRadius());
	}
	else if (const UCEClonerCylinderLayout* CylinderLayout = ClonerActor->GetActiveLayout<UCEClonerCylinderLayout>())
	{
		InitialSpacing = FVector(0, CylinderLayout->GetRadius(), CylinderLayout->GetHeight());
	}
	else if (const UCEClonerSphereUniformLayout* SphereLayout = ClonerActor->GetActiveLayout<UCEClonerSphereUniformLayout>())
	{
		InitialSpacing = FVector(0, SphereLayout->GetRadius(), 0);
	}
}

FBox FAvaClonerActorVisualizer::GetComponentBounds(const UActorComponent* InComponent) const
{
	if (const UCEClonerComponent* ClonerComponent = Cast<UCEClonerComponent>(InComponent))
	{
		if (const ACEClonerActor* ClonerActor = Cast<ACEClonerActor>(ClonerComponent->GetOwner()))
		{
			FVector Origin;
			FVector Extent;
			ClonerActor->GetActorBounds(false, Origin, Extent);
			return FBox(-Extent, Extent);
		}
	}

	return Super::GetComponentBounds(InComponent);
}

bool FAvaClonerActorVisualizer::HandleInputDeltaInternal(FEditorViewportClient* InViewportClient, FViewport* InViewport, const FVector& InAccumulatedTranslation, const FRotator& InAccumulatedRotation, const FVector& InAccumulatedScale)
{
	if (!FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
	{
		return false;
	}

	const ACEClonerActor* ClonerActor = ClonerActorWeak.Get();

	if (!ClonerActor)
	{
		EndEditing();
	}
	else if (GetViewportWidgetMode(InViewportClient) == UE::Widget::WM_Translate)
	{
		const EAxisList::Type AxisList = GetViewportWidgetAxisList(InViewportClient);

		if (UCEClonerGridLayout* GridLayout = ClonerActor->GetActiveLayout<UCEClonerGridLayout>())
		{
			if (AxisList & EAxisList::X)
			{
				const float SpacingX = InitialSpacing.X + InAccumulatedTranslation.X / FMath::Max(1, GridLayout->GetCountX() / 2.f);
				GridLayout->SetSpacingX(SpacingX);
				OnPropertyModified(GridLayout, GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingX));
			}
			else if (AxisList & EAxisList::Y)
			{
				const float SpacingY = InitialSpacing.Y + InAccumulatedTranslation.Y / FMath::Max(1, GridLayout->GetCountY() / 2.f);
				GridLayout->SetSpacingY(SpacingY);
				OnPropertyModified(GridLayout, GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingY));
			}
			else if (AxisList & EAxisList::Z)
			{
				const float SpacingZ = InitialSpacing.Z + InAccumulatedTranslation.Z / FMath::Max(1, GridLayout->GetCountZ() / 2.f);
				GridLayout->SetSpacingZ(SpacingZ);
				OnPropertyModified(GridLayout, GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingZ));
			}

			return true;
		}
		else if (UCEClonerLineLayout* LineLayout = ClonerActor->GetActiveLayout<UCEClonerLineLayout>())
		{
			const int32 Count = LineLayout->GetCount();
			float Spacing = LineLayout->GetSpacing();

			if (AxisList & EAxisList::X)
			{
				Spacing = InitialSpacing.X + InAccumulatedTranslation.X / FMath::Max(1, Count);
			}
			if (AxisList & EAxisList::Y)
			{
				Spacing = InitialSpacing.Y + InAccumulatedTranslation.Y / FMath::Max(1, Count);
			}
			if (AxisList & EAxisList::Z)
			{
				Spacing = InitialSpacing.Z + InAccumulatedTranslation.Z / FMath::Max(1, Count);
			}

			LineLayout->SetSpacing(Spacing);
			OnPropertyModified(LineLayout, GET_MEMBER_NAME_CHECKED(UCEClonerLineLayout, Spacing));

			return true;
		}
		else if (UCEClonerHoneycombLayout* HoneycombLayout = ClonerActor->GetActiveLayout<UCEClonerHoneycombLayout>())
		{
			const ECEClonerPlane Plane = HoneycombLayout->GetPlane();
			const int32 WidthCount = HoneycombLayout->GetWidthCount();
			const int32 HeightCount = HoneycombLayout->GetHeightCount();
			float WidthSpacing = HoneycombLayout->GetWidthSpacing();
			float HeightSpacing = HoneycombLayout->GetHeightSpacing();

			if (Plane == ECEClonerPlane::XY)
			{
				WidthSpacing = InitialSpacing.X + InAccumulatedTranslation.X / FMath::Max(1, WidthCount / 2.f);
				HeightSpacing = InitialSpacing.Y + InAccumulatedTranslation.Y / FMath::Max(1, HeightCount / 2.f);
			}
			else if (Plane == ECEClonerPlane::YZ)
			{
				WidthSpacing = InitialSpacing.Y + InAccumulatedTranslation.Y / FMath::Max(1, WidthCount / 2.f);
				HeightSpacing = InitialSpacing.Z + InAccumulatedTranslation.Z / FMath::Max(1, HeightCount / 2.f);
			}
			else if (Plane == ECEClonerPlane::XZ)
			{
				WidthSpacing = InitialSpacing.X + InAccumulatedTranslation.X / FMath::Max(1, WidthCount / 2.f);
				HeightSpacing = InitialSpacing.Z + InAccumulatedTranslation.Z / FMath::Max(1, HeightCount / 2.f);
			}

			HoneycombLayout->SetWidthSpacing(WidthSpacing);
			HoneycombLayout->SetHeightSpacing(HeightSpacing);

			OnPropertyModified(HoneycombLayout, GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, WidthSpacing));
			OnPropertyModified(HoneycombLayout, GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, HeightSpacing));

			return true;
		}
		else if (UCEClonerCircleLayout* CircleLayout = ClonerActor->GetActiveLayout<UCEClonerCircleLayout>())
		{
			float Radius = CircleLayout->GetRadius();

			if (AxisList & EAxisList::X)
			{
				Radius = InitialSpacing.X + InAccumulatedTranslation.X;
			}
			else if (AxisList & EAxisList::Y)
			{
				Radius = InitialSpacing.Y + InAccumulatedTranslation.Y;
			}
			else if (AxisList & EAxisList::Z)
			{
				Radius = InitialSpacing.Z + InAccumulatedTranslation.Z;
			}

			CircleLayout->SetRadius(Radius);
			OnPropertyModified(CircleLayout, GET_MEMBER_NAME_CHECKED(UCEClonerCircleLayout, Radius));

			return true;
		}
		else if (UCEClonerCylinderLayout* CylinderLayout = ClonerActor->GetActiveLayout<UCEClonerCylinderLayout>())
		{
			const ECEClonerPlane Plane = CylinderLayout->GetPlane();
			float Radius = CylinderLayout->GetRadius();
			float Height = CylinderLayout->GetHeight();

			if (Plane == ECEClonerPlane::XY)
			{
				if (AxisList & EAxisList::Y)
				{
					Radius = InitialSpacing.Y + InAccumulatedTranslation.Y;
				}
				else
				{
					Height = InitialSpacing.Z + InAccumulatedTranslation.Z;
				}
			}
			else if (Plane == ECEClonerPlane::YZ)
			{
				if (AxisList & EAxisList::Y)
				{
					Radius = InitialSpacing.Y + InAccumulatedTranslation.Y;
				}
				else
				{
					Height = InitialSpacing.Z + InAccumulatedTranslation.X;
				}
			}
			else if (Plane == ECEClonerPlane::XZ)
			{
				if (AxisList & EAxisList::Z)
				{
					Radius = InitialSpacing.Y + InAccumulatedTranslation.Z;
				}
				else
				{
					Height = InitialSpacing.Z + InAccumulatedTranslation.Y;
				}
			}

			CylinderLayout->SetRadius(Radius);
			CylinderLayout->SetHeight(Height);

			OnPropertyModified(CylinderLayout, GET_MEMBER_NAME_CHECKED(UCEClonerCylinderLayout, Radius));
			OnPropertyModified(CylinderLayout, GET_MEMBER_NAME_CHECKED(UCEClonerCylinderLayout, Height));

			return true;
		}
		else if (UCEClonerSphereUniformLayout* SphereLayout = ClonerActor->GetActiveLayout<UCEClonerSphereUniformLayout>())
		{
			float Radius = SphereLayout->GetRadius();

			if (AxisList & EAxisList::Y)
			{
				Radius = InitialSpacing.Y + InAccumulatedTranslation.Y;
			}

			SphereLayout->SetRadius(Radius);
			OnPropertyModified(SphereLayout, GET_MEMBER_NAME_CHECKED(UCEClonerSphereUniformLayout, Radius));

			return true;
		}
	}

	return Super::HandleInputDeltaInternal(InViewportClient, InViewport, InAccumulatedTranslation, InAccumulatedRotation, InAccumulatedScale);
}

void FAvaClonerActorVisualizer::DrawVisualizationEditing(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex)
{
	Super::DrawVisualizationEditing(InComponent, InView, InPDI, InOutIconIndex);

	const UCEClonerComponent* ClonerComponent = Cast<UCEClonerComponent>(InComponent);

	if (!ClonerComponent)
	{
		return;
	}

	const ACEClonerActor* ClonerActor = Cast<ACEClonerActor>(ClonerComponent->GetOwner());

	if (!ClonerActor || !ClonerActor->GetEnabled() || ClonerActor->GetMeshCount() == 0)
	{
		return;
	}

	if (const UCEClonerGridLayout* GridLayout = ClonerActor->GetActiveLayout<UCEClonerGridLayout>())
	{
		if (GridLayout->GetCountX() > 0)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::X, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}

		if (GridLayout->GetCountY() > 0)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}

		if (GridLayout->GetCountZ() > 0)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
	}
	else if (const UCEClonerLineLayout* LineLayout = ClonerActor->GetActiveLayout<UCEClonerLineLayout>())
	{
		if (LineLayout->GetCount() > 0)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, LineLayout->GetAxis(), FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
	}
	else if (const UCEClonerHoneycombLayout* HoneycombLayout = ClonerActor->GetActiveLayout<UCEClonerHoneycombLayout>())
	{
		const ECEClonerPlane Plane = HoneycombLayout->GetPlane();

		if (Plane == ECEClonerPlane::XY)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::X, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::YZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::XZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::X, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
	}
	else if (const UCEClonerCircleLayout* CircleLayout = ClonerActor->GetActiveLayout<UCEClonerCircleLayout>())
	{
		const ECEClonerPlane Plane = CircleLayout->GetPlane();

		if (Plane == ECEClonerPlane::XY)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::YZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::XZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
	}
	else if (const UCEClonerCylinderLayout* CylinderLayout = ClonerActor->GetActiveLayout<UCEClonerCylinderLayout>())
	{
		const ECEClonerPlane Plane = CylinderLayout->GetPlane();

		if (Plane == ECEClonerPlane::XY)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::YZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::X, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::XZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
	}
	else if (ClonerActor->IsActiveLayout<UCEClonerSphereUniformLayout>())
	{
		DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
		InOutIconIndex++;
	}
}

void FAvaClonerActorVisualizer::DrawVisualizationNotEditing(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex)
{
	Super::DrawVisualizationNotEditing(InComponent, InView, InPDI, InOutIconIndex);

	const UCEClonerComponent* ClonerComponent = Cast<UCEClonerComponent>(InComponent);

	if (!ClonerComponent)
	{
		return;
	}

	const ACEClonerActor* ClonerActor = Cast<ACEClonerActor>(ClonerComponent->GetOwner());

	if (!ClonerActor || !ClonerActor->GetEnabled() || ClonerActor->GetMeshCount() == 0)
	{
		return;
	}

	if (const UCEClonerGridLayout* GridLayout = ClonerActor->GetActiveLayout<UCEClonerGridLayout>())
	{
		if (GridLayout->GetCountX() > 0)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::X, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}

		if (GridLayout->GetCountY() > 0)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}

		if (GridLayout->GetCountZ() > 0)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
	}
	else if (const UCEClonerLineLayout* LineLayout = ClonerActor->GetActiveLayout<UCEClonerLineLayout>())
	{
		if (LineLayout->GetCount() > 0)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, LineLayout->GetAxis(), FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
	}
	else if (const UCEClonerHoneycombLayout* HoneycombLayout = ClonerActor->GetActiveLayout<UCEClonerHoneycombLayout>())
	{
		const ECEClonerPlane Plane = HoneycombLayout->GetPlane();

		if (Plane == ECEClonerPlane::XY)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::X, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::YZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::XZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::X, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
	}
	else if (const UCEClonerCircleLayout* CircleLayout = ClonerActor->GetActiveLayout<UCEClonerCircleLayout>())
	{
		const ECEClonerPlane Plane = CircleLayout->GetPlane();

		if (Plane == ECEClonerPlane::XY)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::YZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::XZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Custom, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
	}
	else if (const UCEClonerCylinderLayout* CylinderLayout = ClonerActor->GetActiveLayout<UCEClonerCylinderLayout>())
	{
		const ECEClonerPlane Plane = CylinderLayout->GetPlane();

		if (Plane == ECEClonerPlane::XY)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::YZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::X, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
		else if (Plane == ECEClonerPlane::XZ)
		{
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Z, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
			DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
	}
	else if (ClonerActor->IsActiveLayout<UCEClonerSphereUniformLayout>())
	{
		DrawSpacingButton(ClonerActor, InView, InPDI, InOutIconIndex, ECEClonerAxis::Y, FAvaVisualizerBase::Inactive);
		InOutIconIndex++;
	}
}

FVector FAvaClonerActorVisualizer::GetHandleSpacingLocation(const ACEClonerActor* InClonerActor, ECEClonerAxis InAxis) const
{
	const FRotator ClonerRotation = InClonerActor->GetActorRotation();
	const FVector ClonerScale = InClonerActor->GetActorScale();
	FVector OutLocation = FVector::ZeroVector;
	FVector Axis = InAxis == ECEClonerAxis::X ? FVector::XAxisVector : (InAxis == ECEClonerAxis::Z ? FVector::ZAxisVector : FVector::YAxisVector);

	if (const UCEClonerGridLayout* GridLayout = InClonerActor->GetActiveLayout<UCEClonerGridLayout>())
	{
		const FVector Spacing(GridLayout->GetSpacingX(), GridLayout->GetSpacingY(), GridLayout->GetSpacingZ());
		const FVector Count(GridLayout->GetCountX(), GridLayout->GetCountY(), GridLayout->GetCountZ());

		OutLocation = ClonerRotation.RotateVector(Axis * Spacing * Count / 2);
	}
	else if (const UCEClonerLineLayout* LineLayout = InClonerActor->GetActiveLayout<UCEClonerLineLayout>())
	{
		if (InAxis == ECEClonerAxis::Custom)
		{
			Axis = LineLayout->GetDirection().GetSafeNormal();
		}

		OutLocation = ClonerRotation.RotateVector(Axis) * LineLayout->GetSpacing() * FVector(LineLayout->GetCount());
	}
	else if (const UCEClonerHoneycombLayout* HoneycombLayout = InClonerActor->GetActiveLayout<UCEClonerHoneycombLayout>())
	{
		const ECEClonerPlane Plane = HoneycombLayout->GetPlane();
		const float WidthSpacing = HoneycombLayout->GetWidthSpacing();
		const float HeightSpacing = HoneycombLayout->GetHeightSpacing();
		const int32 WidthCount = HoneycombLayout->GetWidthCount();
		const int32 HeightCount = HoneycombLayout->GetHeightCount();

		FVector Spacing;
		FVector Count;
		if (Plane == ECEClonerPlane::XY)
		{
			Spacing = FVector(WidthSpacing, HeightSpacing, 0);
			Count = FVector(WidthCount, HeightCount, 0);
		}
		else if (Plane == ECEClonerPlane::YZ)
		{
			Spacing = FVector(0, WidthSpacing, HeightSpacing);
			Count = FVector(0, WidthCount, HeightCount);
		}
		else if (Plane == ECEClonerPlane::XZ)
		{
			Spacing = FVector(WidthSpacing, 0, HeightSpacing);
			Count = FVector(WidthCount, 0, HeightCount);
		}

		OutLocation = ClonerRotation.RotateVector(Axis * Spacing * Count/2);
	}
	else if (const UCEClonerCircleLayout* CircleLayout = InClonerActor->GetActiveLayout<UCEClonerCircleLayout>())
	{
		FRotator Rotation = CircleLayout->GetRotation();
		const FVector Scale = CircleLayout->GetScale();
		const ECEClonerPlane Plane = CircleLayout->GetPlane();
		const float Radius = CircleLayout->GetRadius();

		if (Plane == ECEClonerPlane::XY)
		{
			Rotation = FRotator::ZeroRotator;
		}
		else if (Plane == ECEClonerPlane::YZ)
		{
			Rotation = FRotator(90, 0, 0);
		}
		else if (Plane == ECEClonerPlane::XZ)
		{
			Rotation = FRotator(0, 90, 0);
		}

		OutLocation = ClonerRotation.RotateVector(Rotation.RotateVector(Axis * Scale)) * Radius;
	}
	else if (const UCEClonerCylinderLayout* CylinderLayout = InClonerActor->GetActiveLayout<UCEClonerCylinderLayout>())
	{
		FRotator Rotation = CylinderLayout->GetRotation();
		const FVector Scale = CylinderLayout->GetScale();
		const ECEClonerPlane Plane = CylinderLayout->GetPlane();
		const float Radius = CylinderLayout->GetRadius();
		const float Height = CylinderLayout->GetHeight();
		float Dim = 0.f;

		if (Plane == ECEClonerPlane::XY)
		{
			Rotation = FRotator::ZeroRotator;
			if (InAxis == ECEClonerAxis::Y)
			{
				Dim = Radius;
			}
			else
			{
				Dim = Height / 2;
			}
		}
		else if (Plane == ECEClonerPlane::YZ)
		{
			if (InAxis == ECEClonerAxis::Y)
			{
				Rotation = FRotator(90, 0, 0);
				Dim = Radius;
			}
			else
			{
				Rotation = FRotator(0, 0, 90);
				Dim = Height / 2;
			}
		}
		else if (Plane == ECEClonerPlane::XZ)
		{
			if (InAxis == ECEClonerAxis::Z)
			{
				Rotation = FRotator(0, 90, 0);
				Dim = Radius;
			}
			else
			{
				Rotation = FRotator(90, 0, 0);
				Dim = Height / 2;
			}
		}

		OutLocation = ClonerRotation.RotateVector(Rotation.RotateVector(Axis * Scale)) * Dim;
	}
	else if (const UCEClonerSphereUniformLayout* SphereLayout = InClonerActor->GetActiveLayout<UCEClonerSphereUniformLayout>())
	{
		const float Radius = SphereLayout->GetRadius();
		const FVector Scale = SphereLayout->GetScale();

		OutLocation = ClonerRotation.RotateVector(Axis) * Scale * Radius;
	}

	return InClonerActor->GetActorLocation() + OutLocation * ClonerScale;
}

void FAvaClonerActorVisualizer::DrawSpacingButton(const ACEClonerActor* InClonerActor, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32 InIconIndex, ECEClonerAxis InAxis, FLinearColor InColor) const
{
	UTexture2D* SpacingSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::BevelSprite);

	if (!SpacingSprite || !SpacingSprite->GetResource() || !InClonerActor->GetEnabled() || InClonerActor->GetMeshCount() == 0)
	{
		return;
	}

	FVector IconLocation;
	float IconSize;
	GetIconMetrics(InView, InIconIndex, IconLocation, IconSize);

	IconLocation = GetHandleSpacingLocation(InClonerActor, InAxis);

	InPDI->SetHitProxy(new HAvaClonerActorSpacingHitProxy(InClonerActor->GetRootComponent(), InAxis));
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, SpacingSprite->GetResource(), InColor, SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

void FAvaClonerActorVisualizer::OnPropertyModified(UObject* InPropertyObject, FName InPropertyName, EPropertyChangeType::Type InType)
{
	if (!InPropertyObject)
	{
		return;
	}

	InPropertyObject->Modify();
	bHasBeenModified = true;
	FProperty* PropertyModified = UE::AvaCore::GetProperty<UObject>(InPropertyName);
	NotifyPropertyModified(InPropertyObject, PropertyModified, InType);
}

UActorComponent* FAvaClonerActorVisualizer::GetEditedComponent() const
{
	return ClonerActorWeak.IsValid() ? ClonerActorWeak->GetRootComponent() : nullptr;
}

TMap<UObject*, TArray<FProperty*>> FAvaClonerActorVisualizer::GatherEditableProperties(UObject* InObject) const
{
	if (UCEClonerComponent* ClonerComponent = Cast<UCEClonerComponent>(InObject))
	{
		if (UCEClonerLayoutBase* Layout = ClonerComponent->GetClonerActiveLayout())
		{
			TArray<FName> PropertyNames;

			if (UCEClonerGridLayout* GridLayout = Cast<UCEClonerGridLayout>(Layout))
			{
				PropertyNames = {
					GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingX),
					GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingY),
					GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingZ)
				};
			}
			else if (UCEClonerLineLayout* LineLayout = Cast<UCEClonerLineLayout>(Layout))
			{
				PropertyNames = {GET_MEMBER_NAME_CHECKED(UCEClonerLineLayout, Spacing)};
			}
			else if (UCEClonerHoneycombLayout* HoneycombLayout = Cast<UCEClonerHoneycombLayout>(Layout))
			{
				PropertyNames = {
					GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, WidthSpacing),
					GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, HeightSpacing)
				};
			}
			else if (UCEClonerCircleLayout* CircleLayout = Cast<UCEClonerCircleLayout>(Layout))
			{
				PropertyNames = {GET_MEMBER_NAME_CHECKED(UCEClonerCircleLayout, Radius)};
			}
			else if (UCEClonerCylinderLayout* CylinderLayout = Cast<UCEClonerCylinderLayout>(Layout))
			{
				PropertyNames = {
					GET_MEMBER_NAME_CHECKED(UCEClonerCylinderLayout, Radius),
					GET_MEMBER_NAME_CHECKED(UCEClonerCylinderLayout, Height)
				};
			}

			TArray<FProperty*> Properties;
			Properties.Reserve(PropertyNames.Num());

			UClass* LayoutClass = Layout->GetClass();

			for (FName PropertyName : PropertyNames)
			{
				if (FProperty* Property = LayoutClass->FindPropertyByName(PropertyName))
				{
					Properties.Add(Property);
				}
			}

			return {{Layout, Properties}};
		}
	}

	return {};
}

bool FAvaClonerActorVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* InVisProxy, const FViewportClick& InClick)
{
	if (InClick.GetKey() != EKeys::LeftMouseButton)
	{
		EndEditing();
		return Super::VisProxyHandleClick(InViewportClient, InVisProxy, InClick);
	}

	MeshType* DynMesh = Cast<MeshType>(const_cast<UActorComponent*>(InVisProxy->Component.Get()));

	if (DynMesh == nullptr)
	{
		return Super::VisProxyHandleClick(InViewportClient, InVisProxy, InClick);
	}

	if (InVisProxy->IsA(HAvaClonerActorSpacingHitProxy::StaticGetType()))
	{
		EndEditing();
		ClonerActorWeak = Cast<ACEClonerActor>(DynMesh->GetOwner());
		bEditingSpacing = true;
		EditingAxis = static_cast<HAvaClonerActorSpacingHitProxy*>(InVisProxy)->Axis;
		StartEditing(InViewportClient, DynMesh);

		return true;
	}

	return Super::VisProxyHandleClick(InViewportClient, InVisProxy, InClick);
}

bool FAvaClonerActorVisualizer::GetWidgetLocation(const FEditorViewportClient* InViewportClient, FVector& OutLocation) const
{
	if (const ACEClonerActor* ClonerActor = ClonerActorWeak.Get())
	{
		if (bEditingSpacing)
		{
			OutLocation = GetHandleSpacingLocation(ClonerActor, EditingAxis);
			return true;
		}
	}

	return Super::GetWidgetLocation(InViewportClient, OutLocation);
}

bool FAvaClonerActorVisualizer::GetWidgetMode(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode& OutMode) const
{
	if (bEditingSpacing)
	{
		OutMode = UE::Widget::EWidgetMode::WM_Translate;
		return true;
	}

	return Super::GetWidgetMode(InViewportClient, OutMode);
}

bool FAvaClonerActorVisualizer::GetWidgetAxisList(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const
{
	if (bEditingSpacing)
	{
		if (EditingAxis == ECEClonerAxis::X)
		{
			OutAxisList = EAxisList::Type::X;
		}
		else if (EditingAxis == ECEClonerAxis::Y)
		{
			OutAxisList = EAxisList::Type::Y;
		}
		else if (EditingAxis == ECEClonerAxis::Z)
		{
			OutAxisList = EAxisList::Type::Z;
		}
		else if (EditingAxis == ECEClonerAxis::Custom)
		{
			OutAxisList = EAxisList::Type::XYZ;
		}
		return true;
	}

	return Super::GetWidgetAxisList(InViewportClient, InWidgetMode, OutAxisList);
}

bool FAvaClonerActorVisualizer::GetWidgetAxisListDragOverride(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const
{
	return Super::GetWidgetAxisListDragOverride(InViewportClient, InWidgetMode, OutAxisList);
}

bool FAvaClonerActorVisualizer::ResetValue(FEditorViewportClient* InViewportClient, HHitProxy* InHitProxy)
{
	if (!InHitProxy->IsA(HAvaClonerActorSpacingHitProxy::StaticGetType()))
	{
		return Super::ResetValue(InViewportClient, InHitProxy);
	}

	const HAvaClonerActorSpacingHitProxy* ComponentHitProxy = static_cast<HAvaClonerActorSpacingHitProxy*>(InHitProxy);

	if (!ComponentHitProxy->Component.IsValid() || !ComponentHitProxy->Component->IsA<UCEClonerComponent>())
	{
		return Super::ResetValue(InViewportClient, InHitProxy);
	}

	if (const ACEClonerActor* ClonerActor = Cast<ACEClonerActor>(ComponentHitProxy->Component->GetOwner()))
	{
		if (UCEClonerGridLayout* GridLayout = ClonerActor->GetActiveLayout<UCEClonerGridLayout>())
		{
			FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
			GridLayout->SetFlags(RF_Transactional);

			GridLayout->SetSpacingX(100.f);
			GridLayout->SetSpacingY(100.f);
			GridLayout->SetSpacingZ(100.f);

			OnPropertyModified(GridLayout, GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingX), EPropertyChangeType::ValueSet);
			OnPropertyModified(GridLayout, GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingY), EPropertyChangeType::ValueSet);
			OnPropertyModified(GridLayout, GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingZ), EPropertyChangeType::ValueSet);
		}
		else if (UCEClonerLineLayout* LineLayout = ClonerActor->GetActiveLayout<UCEClonerLineLayout>())
		{
			FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
			LineLayout->SetFlags(RF_Transactional);

			LineLayout->SetSpacing(500.f);

			OnPropertyModified(LineLayout, GET_MEMBER_NAME_CHECKED(UCEClonerLineLayout, Spacing), EPropertyChangeType::ValueSet);
		}
		else if (UCEClonerHoneycombLayout* HoneycombLayout = ClonerActor->GetActiveLayout<UCEClonerHoneycombLayout>())
		{
			FScopedTransaction Transaction(LOCTEXT( "VisualizerResetValue", "Visualizer Reset Value"));
			HoneycombLayout->SetFlags(RF_Transactional);

			HoneycombLayout->SetWidthSpacing(100.f);
			HoneycombLayout->SetHeightSpacing(100.f);

			OnPropertyModified(HoneycombLayout, GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, WidthSpacing), EPropertyChangeType::ValueSet);
			OnPropertyModified(HoneycombLayout, GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, HeightSpacing), EPropertyChangeType::ValueSet);
		}
		else if (UCEClonerCircleLayout* CircleLayout = ClonerActor->GetActiveLayout<UCEClonerCircleLayout>())
		{
			FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
			CircleLayout->SetFlags(RF_Transactional);

			CircleLayout->SetRadius(500.f);

			OnPropertyModified(CircleLayout, GET_MEMBER_NAME_CHECKED(UCEClonerCircleLayout, Radius), EPropertyChangeType::ValueSet);
		}
		else if (UCEClonerCylinderLayout* CylinderLayout = ClonerActor->GetActiveLayout<UCEClonerCylinderLayout>())
		{
			FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
			CylinderLayout->SetFlags(RF_Transactional);

			CylinderLayout->SetRadius(500.f);
			CylinderLayout->SetHeight(1000.f);

			OnPropertyModified(CylinderLayout, GET_MEMBER_NAME_CHECKED(UCEClonerCylinderLayout, Radius), EPropertyChangeType::ValueSet);
			OnPropertyModified(CylinderLayout, GET_MEMBER_NAME_CHECKED(UCEClonerCylinderLayout, Height), EPropertyChangeType::ValueSet);
		}
	}

	return true;
}

bool FAvaClonerActorVisualizer::IsEditing() const
{
	if (bEditingSpacing)
	{
		return true;
	}

	return Super::IsEditing();
}

void FAvaClonerActorVisualizer::EndEditing()
{
	Super::EndEditing();

	ClonerActorWeak.Reset();
	InitialSpacing = FVector::ZeroVector;
	bEditingSpacing = false;
}

#undef LOCTEXT_NAMESPACE