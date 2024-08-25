// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/AvaEffectorActorVis.h"
#include "AvaField.h"
#include "AvaShapeSprites.h"
#include "AvaVisBase.h"
#include "EditorViewportClient.h"
#include "Effector/CEEffectorComponent.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "IAvalancheComponentVisualizersModule.h"
#include "ScopedTransaction.h"
#include "TextureResource.h"

IMPLEMENT_HIT_PROXY(HAvaEffectorActorZoneHitProxy, HAvaHitProxy);

#define LOCTEXT_NAMESPACE "AvaEffectorActorVisualizer"

FAvaEffectorActorVisualizer::FAvaEffectorActorVisualizer()
	: FAvaVisualizerBase()
{
	using namespace UE::AvaCore;
	InnerRadiusProperty  = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, InnerRadius));
	OuterRadiusProperty  = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OuterRadius));
	InnerExtentProperty  = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, InnerExtent));
	OuterExtentProperty  = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OuterExtent));
	PlaneSpacingProperty = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, PlaneSpacing));
}

void FAvaEffectorActorVisualizer::StoreInitialValues()
{
	Super::StoreInitialValues();
	
	if (GetEditedComponent() == nullptr)
	{
		return;
	}

	InitialInnerRadius = EffectorActorWeak->GetInnerRadius();
	InitialOuterRadius = EffectorActorWeak->GetOuterRadius();
	InitialInnerExtent = EffectorActorWeak->GetInnerExtent();
	InitialOuterExtent = EffectorActorWeak->GetOuterExtent();
	InitialPlaneSpacing = EffectorActorWeak->GetPlaneSpacing();
}

FBox FAvaEffectorActorVisualizer::GetComponentBounds(const UActorComponent* InComponent) const
{
	if (const UCEEffectorComponent* EffectorComponent = Cast<UCEEffectorComponent>(InComponent))
	{
		if (const ACEEffectorActor* EffectorActor = Cast<ACEEffectorActor>(EffectorComponent->GetOwner()))
		{
			if (EffectorActor->GetType() == ECEClonerEffectorType::Box)
			{
				return FBox(-EffectorActor->GetOuterExtent(), EffectorActor->GetOuterExtent());
			}
			if (EffectorActor->GetType() == ECEClonerEffectorType::Sphere)
			{
				return FBox(-FVector(EffectorActor->GetOuterRadius() / 2), FVector(EffectorActor->GetOuterRadius() / 2));
			}
			if (EffectorActor->GetType() == ECEClonerEffectorType::Plane)
			{
				return FBox(-FVector(EffectorActor->GetPlaneSpacing() / 2), FVector(EffectorActor->GetPlaneSpacing() / 2));
			}
		}
	}
	
	return Super::GetComponentBounds(InComponent);
}

bool FAvaEffectorActorVisualizer::HandleInputDeltaInternal(FEditorViewportClient* InViewportClient, FViewport* InViewport, const FVector& InAccumulatedTranslation, const FRotator& InAccumulatedRotation, const FVector& InAccumulatedScale)
{
	if (!FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
	{
		return false;
	}

	if (ACEEffectorActor* EffectorActor = EffectorActorWeak.Get())
	{
		if (GetViewportWidgetMode(InViewportClient) == UE::Widget::WM_Translate)
		{
			if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::XYZ)
			{
				if (EffectorActor->GetType() == ECEClonerEffectorType::Box)
				{
					if (bEditingInnerZone)
					{
						EffectorActor->SetInnerExtent(InitialInnerExtent + InAccumulatedTranslation);
						EffectorActor->Modify();
						bHasBeenModified = true;
						NotifyPropertyModified(EffectorActor, InnerExtentProperty, EPropertyChangeType::Interactive);
					}
					else if (bEditingOuterZone)
					{
						EffectorActor->SetOuterExtent(InitialOuterExtent + InAccumulatedTranslation);
						EffectorActor->Modify();
						bHasBeenModified = true;
						NotifyPropertyModified(EffectorActor, OuterExtentProperty, EPropertyChangeType::Interactive);
					}
					
					return true;
				}
			}
			if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::Y)
			{
				if (EffectorActor->GetType() == ECEClonerEffectorType::Plane)
				{
					if (bEditingOuterZone || bEditingInnerZone)
					{
						EffectorActor->SetPlaneSpacing(InitialPlaneSpacing + InAccumulatedTranslation.Y);
						EffectorActor->Modify();
						bHasBeenModified = true;
						NotifyPropertyModified(EffectorActor, PlaneSpacingProperty, EPropertyChangeType::Interactive);
					}
					
					return true;
				}
				if (EffectorActor->GetType() == ECEClonerEffectorType::Sphere)
				{
					if (bEditingInnerZone)
					{
						EffectorActor->SetInnerRadius(InitialInnerRadius + InAccumulatedTranslation.Y);
						EffectorActor->Modify();
						bHasBeenModified = true;
						NotifyPropertyModified(EffectorActor, InnerRadiusProperty, EPropertyChangeType::Interactive);
					}
					else if (bEditingOuterZone)
					{
						EffectorActor->SetOuterRadius(InitialOuterRadius + InAccumulatedTranslation.Y);
						EffectorActor->Modify();
						bHasBeenModified = true;
						NotifyPropertyModified(EffectorActor, OuterRadiusProperty, EPropertyChangeType::Interactive);
					}
					
					return true;
				}
			}
		}
	}
	else
	{
		EndEditing();
	}
	
	return Super::HandleInputDeltaInternal(InViewportClient, InViewport, InAccumulatedTranslation, InAccumulatedRotation, InAccumulatedScale);
}

void FAvaEffectorActorVisualizer::DrawVisualizationEditing(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex)
{
	Super::DrawVisualizationEditing(InComponent, InView, InPDI, InOutIconIndex);

	const UCEEffectorComponent* EffectorComponent = Cast<UCEEffectorComponent>(InComponent);

	if (!EffectorComponent)
	{
		return;
	}

	const ACEEffectorActor* EffectorActor = Cast<ACEEffectorActor>(EffectorComponent->GetOwner());

	if (!EffectorActor)
	{
		return;
	}

	if (EffectorActor->GetType() != ECEClonerEffectorType::Plane)
	{
		DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, true, FAvaVisualizerBase::Inactive);
		InOutIconIndex++;
	}

	DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, false, bEditingOuterZone ? FAvaVisualizerBase::Active : FAvaVisualizerBase::Inactive);
	InOutIconIndex++;
}

void FAvaEffectorActorVisualizer::DrawVisualizationNotEditing(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex)
{
	Super::DrawVisualizationNotEditing(InComponent, InView, InPDI, InOutIconIndex);

	const UCEEffectorComponent* EffectorComponent = Cast<UCEEffectorComponent>(InComponent);

	if (!EffectorComponent)
	{
		return;
	}

	const ACEEffectorActor* EffectorActor = Cast<ACEEffectorActor>(EffectorComponent->GetOwner());

	if (!EffectorActor)
	{
		return;
	}

	if (EffectorActor->GetType() != ECEClonerEffectorType::Plane)
	{
		DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, true, FAvaVisualizerBase::Inactive);
		InOutIconIndex++;
	}
	
	DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, false, FAvaVisualizerBase::Inactive);
	InOutIconIndex++;
}

FVector FAvaEffectorActorVisualizer::GetHandleZoneLocation(const ACEEffectorActor* InEffectorActor, bool bInInnerSize) const
{
	const FVector EffectorScale = InEffectorActor->GetActorScale();
	const FRotator EffectorRotation = InEffectorActor->GetActorRotation();
	FVector OutLocation = InEffectorActor->GetActorLocation();

	if (InEffectorActor->GetType() == ECEClonerEffectorType::Box)
	{
		if (bInInnerSize)
		{
			OutLocation += EffectorRotation.RotateVector(InEffectorActor->GetInnerExtent()) * EffectorScale;
		}
		else
			{
			OutLocation += EffectorRotation.RotateVector(InEffectorActor->GetOuterExtent()) * EffectorScale;
		}
	}
	else if (InEffectorActor->GetType() == ECEClonerEffectorType::Plane)
	{
		const float ComponentScale = (EffectorRotation.RotateVector(-FVector::YAxisVector) * EffectorScale).Length();
		OutLocation += EffectorRotation.RotateVector(FVector::YAxisVector) * (InEffectorActor->GetPlaneSpacing() / 2) * ComponentScale;
	}
	else if (InEffectorActor->GetType() == ECEClonerEffectorType::Sphere)
	{
		const float MinComponentScale = FMath::Min<float>(FMath::Min<float>(EffectorScale.X, EffectorScale.Y), EffectorScale.Z);
		if (bInInnerSize)
		{
			OutLocation += EffectorRotation.RotateVector(FVector::YAxisVector) * InEffectorActor->GetInnerRadius() * MinComponentScale;
		}
		else
		{
			OutLocation += EffectorRotation.RotateVector(FVector::YAxisVector) * InEffectorActor->GetOuterRadius() * MinComponentScale;
		}
	}
	
	return OutLocation;
}

void FAvaEffectorActorVisualizer::DrawZoneButton(const ACEEffectorActor* InEffectorActor, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32 InIconIndex, bool bInInnerZone, FLinearColor InColor) const
{
	UTexture2D* ZoneSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::BevelSprite);

	if (!ZoneSprite || !ZoneSprite->GetResource())
	{
		return;
	}

	FVector IconLocation;
	float IconSize;
	GetIconMetrics(InView, InIconIndex, IconLocation, IconSize);

	IconLocation = GetHandleZoneLocation(InEffectorActor, bInInnerZone);

	InPDI->SetHitProxy(new HAvaEffectorActorZoneHitProxy(InEffectorActor->GetRootComponent(), bInInnerZone));
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, ZoneSprite->GetResource(), InColor, SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

UActorComponent* FAvaEffectorActorVisualizer::GetEditedComponent() const
{
	return EffectorActorWeak.IsValid() ? EffectorActorWeak->GetRootComponent() : nullptr;
}

TMap<UObject*, TArray<FProperty*>> FAvaEffectorActorVisualizer::GatherEditableProperties(UObject* InObject) const
{
	if (UCEEffectorComponent* EffectorComponent = Cast<UCEEffectorComponent>(InObject))
	{
		if (ACEEffectorActor* EffectorActor = EffectorComponent->GetOuterACEEffectorActor())
		{
			switch (EffectorActor->GetType())
			{
				case ECEClonerEffectorType::Plane:
					return {{EffectorActor, {PlaneSpacingProperty}}};

				case ECEClonerEffectorType::Box:
					return {{EffectorActor, {InnerExtentProperty, OuterExtentProperty}}};

				case ECEClonerEffectorType::Sphere:
					return {{EffectorActor, {InnerRadiusProperty, OuterRadiusProperty}}};
			}
		}
	}

	return {};
}

bool FAvaEffectorActorVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* InVisProxy, const FViewportClick& InClick)
{
	if (InClick.GetKey() != EKeys::LeftMouseButton)
	{
		EndEditing();
		return Super::VisProxyHandleClick(InViewportClient, InVisProxy, InClick);
	}

	UActorComponent* Component = const_cast<UActorComponent*>(InVisProxy->Component.Get());

	if (!Component || !Component->GetOwner()->IsA<ACEEffectorActor>())
	{
		return Super::VisProxyHandleClick(InViewportClient, InVisProxy, InClick);
	}

	if (InVisProxy->IsA(HAvaEffectorActorZoneHitProxy::StaticGetType()))
	{
		EndEditing();
		EffectorActorWeak = Cast<ACEEffectorActor>(InVisProxy->Component->GetOwner());
		bEditingInnerZone = static_cast<HAvaEffectorActorZoneHitProxy*>(InVisProxy)->bInnerZone;
		bEditingOuterZone = !bEditingInnerZone;
		StartEditing(InViewportClient, Component);
		
		return true;
	}

	return Super::VisProxyHandleClick(InViewportClient, InVisProxy, InClick);
}

bool FAvaEffectorActorVisualizer::GetWidgetLocation(const FEditorViewportClient* InViewportClient, FVector& OutLocation) const
{
	if (EffectorActorWeak.IsValid())
	{
		OutLocation = GetHandleZoneLocation(EffectorActorWeak.Get(), bEditingInnerZone);
		return true;
	}
	
	return Super::GetWidgetLocation(InViewportClient, OutLocation);
}

bool FAvaEffectorActorVisualizer::GetWidgetMode(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode& OutMode) const
{
	if (bEditingOuterZone || bEditingInnerZone)
	{
		OutMode = UE::Widget::EWidgetMode::WM_Translate;
		return true;
	}
	
	return Super::GetWidgetMode(InViewportClient, OutMode);
}

bool FAvaEffectorActorVisualizer::GetWidgetAxisList(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const
{
	if (bEditingOuterZone || bEditingInnerZone)
	{
		if (EffectorActorWeak->GetType() != ECEClonerEffectorType::Box)
		{
			OutAxisList = EAxisList::Type::Y;
		}
		else
		{
			OutAxisList = EAxisList::Type::XYZ;
		}
		
		return true;
	}
	
	return Super::GetWidgetAxisList(InViewportClient, InWidgetMode, OutAxisList);
}

bool FAvaEffectorActorVisualizer::GetWidgetAxisListDragOverride(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const
{
	if (bEditingOuterZone || bEditingInnerZone)
	{
		if (EffectorActorWeak->GetType() != ECEClonerEffectorType::Box)
		{
			OutAxisList = EAxisList::Type::Y;
			return true;
		}
	}
	
	return Super::GetWidgetAxisListDragOverride(InViewportClient, InWidgetMode, OutAxisList);
}

bool FAvaEffectorActorVisualizer::ResetValue(FEditorViewportClient* InViewportClient, HHitProxy* InHitProxy)
{
	if (!InHitProxy->IsA(HAvaEffectorActorZoneHitProxy::StaticGetType()))
	{
		return Super::ResetValue(InViewportClient, InHitProxy);
	}
	
	const HAvaEffectorActorZoneHitProxy* ComponentHitProxy = static_cast<HAvaEffectorActorZoneHitProxy*>(InHitProxy);
	
	if (!ComponentHitProxy->Component.IsValid() || !ComponentHitProxy->Component->IsA<UCEEffectorComponent>())
	{
		return Super::ResetValue(InViewportClient, InHitProxy);
	}

	if (ACEEffectorActor* EffectorActor = Cast<ACEEffectorActor>(ComponentHitProxy->Component->GetOwner()))
	{
		if (EffectorActor->GetType() == ECEClonerEffectorType::Box)
		{
			if (ComponentHitProxy->bInnerZone)
			{
				FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
				EffectorActor->SetFlags(RF_Transactional);
				EffectorActor->SetInnerExtent(FVector(50.f));
				EffectorActor->Modify();
				NotifyPropertyModified(EffectorActor, InnerExtentProperty, EPropertyChangeType::ValueSet);
			}
			else
			{
				FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
				EffectorActor->SetFlags(RF_Transactional);
				EffectorActor->SetOuterExtent(FVector(200.f));
				EffectorActor->Modify();
				NotifyPropertyModified(EffectorActor, OuterExtentProperty, EPropertyChangeType::ValueSet);
			}
		}
		else if (EffectorActor->GetType() == ECEClonerEffectorType::Plane)
		{
			FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
			EffectorActor->SetFlags(RF_Transactional);
			EffectorActor->SetPlaneSpacing(200.f);
			EffectorActor->Modify();
			NotifyPropertyModified(EffectorActor, PlaneSpacingProperty, EPropertyChangeType::ValueSet);
		}
		else if (EffectorActor->GetType() == ECEClonerEffectorType::Sphere)
		{
			if (ComponentHitProxy->bInnerZone)
			{
				FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
				EffectorActor->SetFlags(RF_Transactional);
				EffectorActor->SetInnerRadius(50.f);
				EffectorActor->Modify();
				NotifyPropertyModified(EffectorActor, InnerRadiusProperty, EPropertyChangeType::ValueSet);
			}
			else
			{
				FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
				EffectorActor->SetFlags(RF_Transactional);
				EffectorActor->SetOuterRadius(200.f);
				EffectorActor->Modify();
				NotifyPropertyModified(EffectorActor, OuterRadiusProperty, EPropertyChangeType::ValueSet);
			}
		}
	}
	
	return true;
}

bool FAvaEffectorActorVisualizer::IsEditing() const
{
	if (bEditingInnerZone || bEditingOuterZone)
	{
		return true;
	}
	
	return Super::IsEditing();
}

void FAvaEffectorActorVisualizer::EndEditing()
{
	Super::EndEditing();

	EffectorActorWeak.Reset();
	bEditingInnerZone = false;
	bEditingOuterZone = false;
}

#undef LOCTEXT_NAMESPACE
