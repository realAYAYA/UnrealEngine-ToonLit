// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTextVisualizer.h"
#include "AvaField.h"
#include "AvaShapeSprites.h"
#include "AvaText3DComponent.h"
#include "EditorViewportClient.h"
#include "Engine/Texture2D.h"
#include "IAvalancheComponentVisualizersModule.h"
#include "LevelEditor/AvaLevelEditorUtils.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Text3DComponent.h"
#include "TextureResource.h"

IMPLEMENT_HIT_PROXY(HAvaTextMaxTextHeightProxy, HAvaHitProxy)
IMPLEMENT_HIT_PROXY(HAvaTextMaxTextHeightHandleProxy, HAvaHitProxy)
IMPLEMENT_HIT_PROXY(HAvaTextMaxTextWidthProxy, HAvaHitProxy)
IMPLEMENT_HIT_PROXY(HAvaTextMaxTextWidthHandleProxy, HAvaHitProxy)
IMPLEMENT_HIT_PROXY(HAvaTextScaleProportionallyProxy, HAvaHitProxy)

IMPLEMENT_HIT_PROXY(HAvaTextEditGradientProxy, HAvaHitProxy)
IMPLEMENT_HIT_PROXY(HAvaTextGradientLineStartHandleProxy, HAvaHitProxy)
IMPLEMENT_HIT_PROXY(HAvaTextGradientLineEndHandleProxy, HAvaHitProxy)
IMPLEMENT_HIT_PROXY(HAvaTextGradientCenterHandleProxy, HAvaHitProxy)
IMPLEMENT_HIT_PROXY(HAvaTextGradientSmoothnessHandleProxy, HAvaHitProxy)

namespace UE::Ava::Private
{
	struct FTextVisualizerStatics
	{
		inline static constexpr float GradientRotHandleScale = 0.005f;
		inline static constexpr float GradientOffsetHandleScale = 0.01f;
		inline static constexpr float GradientSmoothnessHandleScale = 0.1f;
		inline static constexpr float GradientHandleMaxLength = 50.0f;
		inline static constexpr float GradientSmoothnessHandleOffset = -10;
	};
}

#define LOCTEXT_NAMESPACE "AvaTextVisualizer"

FAvaTextVisualizer::FAvaTextVisualizer()
	: FAvaVisualizerBase()
{
	using namespace UE::AvaCore;

	HasMaxHeightProperty        = GetProperty<UAvaText3DComponent>(GET_MEMBER_NAME_CHECKED(UAvaText3DComponent, bHasMaxHeight));
	HasMaxWidthProperty         = GetProperty<UAvaText3DComponent>(GET_MEMBER_NAME_CHECKED(UAvaText3DComponent, bHasMaxWidth));
	MaxHeightProperty           = GetProperty<UAvaText3DComponent>(GET_MEMBER_NAME_CHECKED(UAvaText3DComponent, MaxHeight));
	MaxWidthProperty            = GetProperty<UAvaText3DComponent>(GET_MEMBER_NAME_CHECKED(UAvaText3DComponent, MaxWidth));
	ScaleProportionallyProperty = GetProperty<UAvaText3DComponent>(GET_MEMBER_NAME_CHECKED(UAvaText3DComponent, bScaleProportionally));
	GradientProperty            = GetProperty<UAvaText3DComponent>(GET_MEMBER_NAME_CHECKED(UAvaText3DComponent, GradientSettings));

	ResetEditingFlags();
	bShowGradientControls = false;
}

UActorComponent* FAvaTextVisualizer::GetEditedComponent() const
{
	return TextComponent.Get();
}

TMap<UObject*, TArray<FProperty*>> FAvaTextVisualizer::GatherEditableProperties(UObject* InObject) const
{
	if (UAvaText3DComponent* Component = Cast<UAvaText3DComponent>(InObject))
	{
		return {{
			Component,
			{
				HasMaxHeightProperty,
				MaxWidthProperty,
				HasMaxHeightProperty,
				MaxHeightProperty,
				ScaleProportionallyProperty,
				GradientProperty
			}
		}};
	}

	return {};
}

bool FAvaTextVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (Click.GetKey() != EKeys::LeftMouseButton || !VisProxy)
	{
		EndEditing();
		return Super::VisProxyHandleClick(InViewportClient, VisProxy, Click);
	}	
	
	if (VisProxy->IsA(HAvaTextMaxTextHeightProxy::StaticGetType()))
	{
		if (const HAvaTextMaxTextHeightProxy* const MaxHeightProxy = static_cast<HAvaTextMaxTextHeightProxy*>(VisProxy))
		{
			EndEditing();

			if (UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(const_cast<UActorComponent*>(MaxHeightProxy->Component.Get())))
			{
				FScopedTransaction Transaction(LOCTEXT("ToggleText3DHasMaxHeight", "Toggle Text 3D Has Max Height"));

				if (AActor* const Text3DActor = AvaTextComponent->GetOwner())
				{
					Text3DActor->Modify();
					AvaTextComponent->Modify();

					// If max height is off, we will toggle it on later.
					if (!AvaTextComponent->HasMaxHeight() && AvaTextComponent->GetMaxHeight() == GetDefault<UAvaText3DComponent>()->GetMaxHeight())
					{
						// Set a sensible default value
						Bounds = GetComponentBounds(AvaTextComponent);
						float NewMaxHeight = (Bounds.Max.Z - Bounds.Min.Z) * 1.25f;
						AvaTextComponent->SetMaxHeight(NewMaxHeight);
					}					

					AvaTextComponent->SetHasMaxHeight(!AvaTextComponent->HasMaxHeight());
				}
			}

			return true;
		}
	}
	else if (VisProxy->IsA(HAvaTextMaxTextHeightHandleProxy::StaticGetType()))
	{
		if (const HAvaTextMaxTextHeightHandleProxy* const MaxHeightHandleProxy = static_cast<HAvaTextMaxTextHeightHandleProxy*>(VisProxy))
		{
			EndEditing();

			if (UAvaText3DComponent* AvaTextComponent = Cast<UAvaText3DComponent>(const_cast<UActorComponent*>(MaxHeightHandleProxy->Component.Get())))
			{
				TextComponent = AvaTextComponent;
				bEditingHeight = true;
				StartEditing(InViewportClient, AvaTextComponent);
			}

			return true;
		}
	}

	if (VisProxy->IsA(HAvaTextMaxTextWidthProxy::StaticGetType()))
	{
		if (const HAvaTextMaxTextWidthProxy* const MaxWidthProxy = static_cast<HAvaTextMaxTextWidthProxy*>(VisProxy))
		{
			EndEditing();

			if (UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(const_cast<UActorComponent*>(MaxWidthProxy->Component.Get())))
			{
				FScopedTransaction Transaction(LOCTEXT("ToggleText3DHasMaxWidth", "Toggle Text 3D Has Max Width"));

				if (AActor* const Text3DActor = AvaTextComponent->GetOwner())
				{
					Text3DActor->Modify();
					AvaTextComponent->Modify();

					// If max width is off, we will toggle it on later.
					if (!AvaTextComponent->HasMaxWidth() && AvaTextComponent->GetMaxWidth() == GetDefault<UAvaText3DComponent>()->GetMaxWidth())
					{
						// Set a sensible default value
						Bounds = GetComponentBounds(AvaTextComponent);
						float NewMaxWidth = (Bounds.Max.Y - Bounds.Min.Y) * 1.25f;
						AvaTextComponent->SetMaxWidth(NewMaxWidth);
					}

					AvaTextComponent->SetHasMaxWidth(!AvaTextComponent->HasMaxWidth());
				}
			}

			return true;
		}
	}
	else if (VisProxy->IsA(HAvaTextMaxTextWidthHandleProxy::StaticGetType()))
	{
		if (const HAvaTextMaxTextWidthHandleProxy* const MaxWidthHandleProxy = static_cast<HAvaTextMaxTextWidthHandleProxy*>(VisProxy))
		{
			EndEditing();

			if (UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(const_cast<UActorComponent*>(MaxWidthHandleProxy->Component.Get())))
			{
				TextComponent = AvaTextComponent;
				bEditingWidth = true;
				StartEditing(InViewportClient, AvaTextComponent);
			}

			return true;
		}
	}
	else if (VisProxy->IsA(HAvaTextScaleProportionallyProxy::StaticGetType()))
	{
		if (const HAvaTextScaleProportionallyProxy* const ScaleProportionallyProxy = static_cast<HAvaTextScaleProportionallyProxy*>(VisProxy))
		{
			EndEditing();

			if (UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(const_cast<UActorComponent*>(ScaleProportionallyProxy->Component.Get())))
			{
				FScopedTransaction Transaction(LOCTEXT("ToggleText3DScaleProportionally", "Toggle Text 3D Scale Proportionally"));

				if (AActor* const Text3DActor = AvaTextComponent->GetOwner())
				{
					Text3DActor->Modify();
					AvaTextComponent->Modify();
					AvaTextComponent->SetScaleProportionally(!AvaTextComponent->ScalesProportionally());
				}
			}
			
			return true;
		}
	}

	// this one just toggles gradient control on/off, in case the actor has it
	if (VisProxy->IsA(HAvaTextEditGradientProxy::StaticGetType()))
	{
		EndEditing();			
		bShowGradientControls = !bShowGradientControls;

		return true;
	}
	else if (VisProxy->IsA(HAvaTextGradientLineStartHandleProxy::StaticGetType()))
	{
		if (const HAvaTextGradientLineStartHandleProxy* const GradientStartHandleProxy = static_cast<HAvaTextGradientLineStartHandleProxy*>(VisProxy))
		{
			EndEditing();

			if (UAvaText3DComponent* AvaTextComponent = Cast<UAvaText3DComponent>(const_cast<UActorComponent*>(GradientStartHandleProxy->Component.Get())))
			{
				TextComponent = AvaTextComponent;
				bEditingGradientRotation_StartHandle = true;
				GradientEditBeginLocation_StartHandle = GetGradientStartHandleLocation(AvaTextComponent);
				GradientEditBeginLocation_Center = GetGradientCenterHandleLocation(AvaTextComponent);
				StartEditing(InViewportClient, AvaTextComponent);
			}

			return true;
		}
	}
	else if (VisProxy->IsA(HAvaTextGradientLineEndHandleProxy::StaticGetType()))
	{
		if (const HAvaTextGradientLineEndHandleProxy* const GradientEndHandleProxy = static_cast<HAvaTextGradientLineEndHandleProxy*>(VisProxy))
		{
			EndEditing();

			if (UAvaText3DComponent* AvaTextComponent = Cast<UAvaText3DComponent>(const_cast<UActorComponent*>(GradientEndHandleProxy->Component.Get())))
			{
				TextComponent = AvaTextComponent;
				bEditingGradientRotation_EndHandle = true;
				GradientEditBeginLocation_EndHandle = GetGradientEndHandleLocation(AvaTextComponent);
				GradientEditBeginLocation_Center = GetGradientCenterHandleLocation(AvaTextComponent);
				StartEditing(InViewportClient, AvaTextComponent);
			}

			return true;
		}
	}
	else if (VisProxy->IsA(HAvaTextGradientCenterHandleProxy::StaticGetType()))
	{
		if (const HAvaTextGradientCenterHandleProxy* const GradientCenterHandleProxy = static_cast<HAvaTextGradientCenterHandleProxy*>(VisProxy))
		{
			EndEditing();

			if (UAvaText3DComponent* AvaTextComponent = Cast<UAvaText3DComponent>(const_cast<UActorComponent*>(GradientCenterHandleProxy->Component.Get())))
			{
				TextComponent = AvaTextComponent;
				bEditingGradientOffset = true;
				StartEditing(InViewportClient, AvaTextComponent);
			}

			return true;
		}
	}
	else if (VisProxy->IsA(HAvaTextGradientSmoothnessHandleProxy::StaticGetType()))
	{
		if (const HAvaTextGradientSmoothnessHandleProxy* const GradientSmoothnessHandleProxy = static_cast<HAvaTextGradientSmoothnessHandleProxy*>(VisProxy))
		{
			EndEditing();

			if (UAvaText3DComponent* AvaTextComponent = Cast<UAvaText3DComponent>(const_cast<UActorComponent*>(GradientSmoothnessHandleProxy->Component.Get())))
			{
				TextComponent = AvaTextComponent;
				bEditingGradientSmoothness = true;
				StartEditing(InViewportClient, AvaTextComponent);
				return true;
			}			
		}
	}

	return false;
}

FVector FAvaTextVisualizer::GetGradientSmoothnessHandleLocation(const UAvaText3DComponent* InAvaTextComponent) const
{
	using namespace UE::Ava::Private;
	
	return GetGradientCenterHandleLocation(InAvaTextComponent) + FVector(0, FTextVisualizerStatics::GradientSmoothnessHandleOffset, 0);
}

bool FAvaTextVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	if (const UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(GetEditedComponent()))
	{
		if (bEditingWidth)
		{
			OutLocation = GetWidthHandleLocation(AvaTextComponent);
			return true;
		}

		if (bEditingHeight)
		{
			OutLocation = GetHeightHandleLocation(AvaTextComponent);
			return true;
		}

		if (bEditingGradientRotation_StartHandle)
		{
			OutLocation = GetGradientStartHandleLocation(AvaTextComponent);
			return true;
		}
		
		if (bEditingGradientRotation_EndHandle)
		{
			OutLocation = GetGradientEndHandleLocation(AvaTextComponent);
			return true;
		}
		
		if (bEditingGradientOffset)
		{
			OutLocation = GetGradientCenterHandleLocation(AvaTextComponent);
			return true;
		}

		if (bEditingGradientSmoothness)
		{
			OutLocation = GetGradientSmoothnessHandleLocation(AvaTextComponent);
			return true;
		}		
	}

	return Super::GetWidgetLocation(ViewportClient, OutLocation);
}

bool FAvaTextVisualizer::GetWidgetMode(const FEditorViewportClient* ViewportClient, UE::Widget::EWidgetMode& Mode) const
{
	if (bEditingWidth || bEditingHeight)
	{
		Mode = UE::Widget::WM_Translate;
		return true;
	}

	if (bEditingGradientRotation_EndHandle || bEditingGradientRotation_StartHandle)
	{
		Mode = UE::Widget::WM_Translate;
		return true;
	}

	if (bEditingGradientOffset)
	{
		Mode = UE::Widget::WM_Translate;
		return true;
	}

	if (bEditingGradientSmoothness)
	{
		Mode = UE::Widget::WM_Scale;
		return true;
	}

	return Super::GetWidgetMode(ViewportClient, Mode);
}

bool FAvaTextVisualizer::GetWidgetAxisList(const FEditorViewportClient* ViewportClient, UE::Widget::EWidgetMode WidgetMode, 
	EAxisList::Type& AxisList) const
{
	if (bEditingWidth)
	{
		AxisList = EAxisList::Y;
		return true;
	}

	if (bEditingHeight)
	{
		AxisList = EAxisList::Z;
		return true;
	}

	if (bEditingGradientRotation_EndHandle || bEditingGradientRotation_StartHandle)
	{
		AxisList = EAxisList::Type::Screen;
		return true;
	}

	if (bEditingGradientOffset)
	{
		AxisList = EAxisList::Type::Screen;
		return true;
	}

	if (bEditingGradientSmoothness)
	{
		AxisList = EAxisList::Type::Z;
		return true;
	}

	return Super::GetWidgetAxisList(ViewportClient, WidgetMode, AxisList);
}

bool FAvaTextVisualizer::GetWidgetAxisListDragOverride(const FEditorViewportClient* ViewportClient,
	UE::Widget::EWidgetMode WidgetMode, EAxisList::Type& AxisList) const
{
	if (bEditingGradientOffset)
	{
		AxisList = EAxisList::YZ;
		return true;
	}

	if (bEditingGradientRotation_EndHandle || bEditingGradientRotation_StartHandle)
	{
		AxisList = EAxisList::YZ;
		return true;
	}

	return Super::GetWidgetAxisListDragOverride(ViewportClient, WidgetMode, AxisList);
}

bool FAvaTextVisualizer::HandleInputDeltaInternal(FEditorViewportClient* InViewportClient, FViewport* InViewport, const FVector& InAccumulatedTranslation,
	const FRotator& InAccumulatedRotation, const FVector& InAccumulatedScale)
{
	using namespace UE::Ava::Private;
	
	if (bEditingWidth)
	{
		if (UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(GetEditedComponent()))
		{
			if (GetViewportWidgetMode(InViewportClient) == UE::Widget::WM_Translate)
			{
				if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::Y)
				{
					float MaxWidth = InitialMaxWidth;

					switch (AvaTextComponent->GetHorizontalAlignment())
					{
						case EText3DHorizontalTextAlignment::Left:
							MaxWidth += InAccumulatedTranslation.Y;
							break;

						case EText3DHorizontalTextAlignment::Center:
							MaxWidth += InAccumulatedTranslation.Y * 2.f;
							break;

						case EText3DHorizontalTextAlignment::Right:
							MaxWidth -= InAccumulatedTranslation.Y;
							break;
					}

					MaxWidth = FMath::Max(1.f, MaxWidth);

					if (MaxWidth != InitialMaxWidth)
					{
						bHasBeenModified = true;
					}

					if (AActor* const Text3DActor = AvaTextComponent->GetOwner())
					{
						Text3DActor->Modify();
						AvaTextComponent->Modify();
						AvaTextComponent->SetMaxWidth(MaxWidth);
					}

					return true;
				}
			}
		}

		EndEditing();
		return true;
	}

	if (bEditingHeight)
	{
		if (UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(GetEditedComponent()))
		{
			if (GetViewportWidgetMode(InViewportClient) == UE::Widget::WM_Translate)
			{
				if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::Z)
				{
					float MaxHeight = InitialMaxHeight;

					switch (AvaTextComponent->GetVerticalAlignment())
					{
						case EText3DVerticalTextAlignment::FirstLine:
							MaxHeight += InAccumulatedTranslation.Z / TopHeightFraction;
							break;

						case EText3DVerticalTextAlignment::Top:
							MaxHeight -= InAccumulatedTranslation.Z;
							break;

						case EText3DVerticalTextAlignment::Center:
							MaxHeight += InAccumulatedTranslation.Z * 2.f;
							break;

						case EText3DVerticalTextAlignment::Bottom:
							MaxHeight += InAccumulatedTranslation.Z;
							break;
					}

					MaxHeight = FMath::Max(1.f, MaxHeight);

					if (MaxHeight != InitialMaxHeight)
					{
						bHasBeenModified = true;
					}

					if (AActor* const Text3DActor = AvaTextComponent->GetOwner())
					{
						Text3DActor->Modify();
						AvaTextComponent->Modify();
						AvaTextComponent->SetMaxHeight(MaxHeight);
					}

					return true;
				}
			}
		}

		EndEditing();
		return true;
	}

	if (bEditingGradientRotation_EndHandle || bEditingGradientRotation_StartHandle)
	{
		if (UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(GetEditedComponent()))
		{
			if (GetViewportWidgetMode(InViewportClient) == UE::Widget::WM_Translate)
			{
				if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::Z)
				{
					float GradientRot = InitialGradientRotation;
					float RotIncrZ = InAccumulatedTranslation.Z * FTextVisualizerStatics::GradientRotHandleScale;

					FVector HandleToCenter;

					if (bEditingGradientRotation_EndHandle)
					{
					    HandleToCenter = GradientEditBeginLocation_EndHandle - GradientEditBeginLocation_Center;
					}
					else if (bEditingGradientRotation_StartHandle)
					{
					    HandleToCenter = GradientEditBeginLocation_StartHandle - GradientEditBeginLocation_Center;
					}
					
					if (HandleToCenter.Y < 0)
                    {
                        RotIncrZ *= -1;
                    }
					
					GradientRot -= RotIncrZ;
					
					if (GradientRot != InitialGradientRotation)
					{
						bHasBeenModified = true;
					}

					if (AActor* const Text3DActor = AvaTextComponent->GetOwner())
					{
						Text3DActor->Modify();
						AvaTextComponent->Modify();

						if (AvaTextComponent->GradientSettings.Direction != EAvaGradientDirection::Custom)
						{
							if (AvaTextComponent->GradientSettings.Direction == EAvaGradientDirection::Vertical)
							{
								AvaTextComponent->GradientSettings.Rotation = 0;
							}
							else if (AvaTextComponent->GradientSettings.Direction == EAvaGradientDirection::Horizontal)
							{
								AvaTextComponent->GradientSettings.Rotation = 0.75;
							}

							InitialGradientRotation = AvaTextComponent->GradientSettings.Rotation;
							
							AvaTextComponent->GradientSettings.Direction = EAvaGradientDirection::Custom;

							const FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

							for (const FName& DetailViewId : FAvaLevelEditorUtils::GetDetailsViewNames())
							{
								if (const TSharedPtr<IDetailsView> DetailView = PropertyModule.FindDetailView(DetailViewId))
								{
									DetailView->ForceRefresh();
								}
							}
						}
						else
						{
							AvaTextComponent->GradientSettings.Rotation = GradientRot;
						}

						AvaTextComponent->RefreshGradient();
					}
					return true;
				}
			}
		}
		
		EndEditing();
		return true;
	}

	if (bEditingGradientOffset)
	{
		if (UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(GetEditedComponent()))
		{
			if (GetViewportWidgetMode(InViewportClient) == UE::Widget::WM_Translate)
			{
				if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::YZ)
				{
					if (AActor* const Text3DActor = AvaTextComponent->GetOwner())
					{
						Text3DActor->Modify();
						AvaTextComponent->Modify();

						float GradientOffset = InitialGradientOffset;
						
						const float OffsetIncr = InAccumulatedTranslation.Length() * FTextVisualizerStatics::GradientOffsetHandleScale;

						FVector GradientDir = Text3DActor->GetActorUpVector().RotateAngleAxis(-AvaTextComponent->GradientSettings.Rotation * 360, Text3DActor->GetActorForwardVector());
						GradientDir.Normalize();

						const float DeltaDotGradDir = InAccumulatedTranslation.Dot(GradientDir);
						
						GradientOffset -= OffsetIncr * FMath::Sign(DeltaDotGradDir);
						GradientOffset = FMath::Clamp(GradientOffset, 0.0, 1.0);

						if (GradientOffset != InitialGradientOffset)
						{
							bHasBeenModified = true;
						}
						
						AvaTextComponent->GradientSettings.Offset = GradientOffset;
						AvaTextComponent->RefreshGradient();
						return true;
					}
				}
			}
		}

		EndEditing();
		return true;
	}

	if (bEditingGradientSmoothness)
	{
		if (UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(GetEditedComponent()))
		{
			if (GetViewportWidgetMode(InViewportClient) == UE::Widget::WM_Scale)
			{
				if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::Z)
				{
					if (AActor* const Text3DActor = AvaTextComponent->GetOwner())
					{
						Text3DActor->Modify();
						AvaTextComponent->Modify();

						float GradientSmoothness = InitialGradientSmoothness;
						
						const float SmoothnessIncr = InAccumulatedScale.Z * FTextVisualizerStatics::GradientSmoothnessHandleScale;
						
						GradientSmoothness += SmoothnessIncr;
						GradientSmoothness = FMath::Clamp(GradientSmoothness, 0.0, 1.0);

						if (GradientSmoothness != InitialGradientSmoothness)
						{
							bHasBeenModified = true;
						}

						AvaTextComponent->GradientSettings.Smoothness = GradientSmoothness;
						AvaTextComponent->RefreshGradient();
						return true;
					}
				}
			}
		}

		EndEditing();
		return true;
	}

	return Super::HandleInputDeltaInternal(InViewportClient, InViewport, InAccumulatedTranslation, InAccumulatedRotation, InAccumulatedScale);
}

void FAvaTextVisualizer::TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove)
{
	if (bEditingGradientRotation_EndHandle || bEditingGradientRotation_StartHandle)
	{
		if (const UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(GetEditedComponent()))
		{
			// handles positions need to be refreshed every time we release the mouse, so that clockwise vs. counterclockwise interaction works properly
			GradientEditBeginLocation_StartHandle = GetGradientStartHandleLocation(AvaTextComponent);
			GradientEditBeginLocation_EndHandle = GetGradientEndHandleLocation(AvaTextComponent);
			GradientEditBeginLocation_Center = GetGradientCenterHandleLocation(AvaTextComponent);
		}
	}

	FAvaVisualizerBase::TrackingStopped(InViewportClient, bInDidMove);
}

bool FAvaTextVisualizer::ResetValue(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy)
{
	if (!HitProxy->IsA(HAvaTextMaxTextHeightProxy::StaticGetType())
		&& !HitProxy->IsA(HAvaTextMaxTextWidthProxy::StaticGetType())
		&& !HitProxy->IsA(HAvaTextScaleProportionallyProxy::StaticGetType())
		&& !HitProxy->IsA(HAvaTextEditGradientProxy::StaticGetType()))
	{
		return Super::ResetValue(InViewportClient, HitProxy);
	}

	UAvaText3DComponent* AvaTextComponent = TextComponent.Get();

	if (!AvaTextComponent)
	{
		return Super::ResetValue(InViewportClient, HitProxy);
	}

	AvaTextComponent->SetFlags(RF_Transactional);

	if (HitProxy->IsA(HAvaTextMaxTextWidthProxy::StaticGetType()))
	{
		FScopedTransaction Transaction(NSLOCTEXT("AvaText3DVisualizer", "VisualizerResetValue", "Visualizer Reset Value"));
		AvaTextComponent->Modify();

		AvaTextComponent->SetHasMaxWidth(false);
		AvaTextComponent->SetMaxWidth(GetDefault<UAvaText3DComponent>()->MaxWidth);

		FAvaVisualizerBase::NotifyPropertyModified(AvaTextComponent, MaxWidthProperty, EPropertyChangeType::ValueSet);
	}
	else if (HitProxy->IsA(HAvaTextMaxTextHeightProxy::StaticGetType()))
	{
		FScopedTransaction Transaction(NSLOCTEXT("AvaTextVisualizer", "VisualizerResetValue", "Visualizer Reset Value"));
		AvaTextComponent->Modify();

		AvaTextComponent->SetHasMaxHeight(false);
		AvaTextComponent->SetMaxHeight(GetDefault<UAvaText3DComponent>()->MaxHeight);

		FAvaVisualizerBase::NotifyPropertyModified(AvaTextComponent, MaxHeightProperty, EPropertyChangeType::ValueSet);
	}
	else if (HitProxy->IsA(HAvaTextScaleProportionallyProxy::StaticGetType()))
	{
		FScopedTransaction Transaction(NSLOCTEXT("AvaTextVisualizer", "VisualizerResetValue", "Visualizer Reset Value"));
		AvaTextComponent->Modify();

		AvaTextComponent->SetScaleProportionally(false);

		FAvaVisualizerBase::NotifyPropertyModified(AvaTextComponent, ScaleProportionallyProperty, EPropertyChangeType::ValueSet);
	}

	return true;
}

bool FAvaTextVisualizer::IsEditing() const
{
	if (bEditingWidth || bEditingHeight ||
		bEditingGradientRotation_StartHandle || bEditingGradientRotation_EndHandle || bEditingGradientOffset || bEditingGradientSmoothness)
	{
		return true;
	}

	return Super::IsEditing();
}

void FAvaTextVisualizer::ResetEditingFlags()
{
	bEditingWidth = false;
	bEditingHeight = false;	
	bEditingGradientRotation_StartHandle = false;
	bEditingGradientRotation_EndHandle = false;
	bEditingGradientOffset = false;
	bEditingGradientSmoothness = false;
}


void FAvaTextVisualizer::EndEditing()
{
	Super::EndEditing();
	
	ResetEditingFlags();
}

FVector FAvaTextVisualizer::GetWidthHandleLocation(const UText3DComponent* InText3DComp) const
{
	const FTransform Transform = InText3DComp->GetComponentTransform();
	const FBox BoundsMax = GetBoundsMax(InText3DComp);
	const float CoordZ = (BoundsMax.Min.Z + BoundsMax.Max.Z) * 0.5f;
	FVector IconLocation;

	switch (InText3DComp->GetHorizontalAlignment())
	{
		default:
		case EText3DHorizontalTextAlignment::Left:
			IconLocation = Transform.TransformPositionNoScale(FVector(0.f, BoundsMax.Max.Y + 10.f, CoordZ));
			break;

		case EText3DHorizontalTextAlignment::Center:
			IconLocation = Transform.TransformPositionNoScale(FVector(0.f, BoundsMax.Max.Y + 10.f, CoordZ));
			break;

		case EText3DHorizontalTextAlignment::Right:
			IconLocation = Transform.TransformPositionNoScale(FVector(0.f, BoundsMax.Min.Y - 10.f, CoordZ));
			break;
	}

	return IconLocation;
}

FVector FAvaTextVisualizer::GetHeightHandleLocation(const UText3DComponent* InText3DComp) const
{
	const FTransform Transform = InText3DComp->GetComponentTransform();
	const FBox BoundsMax = GetBoundsMax(InText3DComp);
	const float CoordY = (InText3DComp->GetHorizontalAlignment() != EText3DHorizontalTextAlignment::Right)
		? BoundsMax.Min.Y - 10.f
		: BoundsMax.Max.Y + 10.f;

	FVector IconLocation;

	switch (InText3DComp->GetVerticalAlignment())
	{
		case EText3DVerticalTextAlignment::FirstLine:
			IconLocation = Transform.TransformPositionNoScale(FVector(0.f, CoordY, BoundsMax.Max.Z));
			break;

		case EText3DVerticalTextAlignment::Top:
			IconLocation = Transform.TransformPositionNoScale(FVector(0.f, CoordY, BoundsMax.Min.Z));
			break;

		case EText3DVerticalTextAlignment::Center:
			IconLocation = Transform.TransformPositionNoScale(FVector(0.f, CoordY, BoundsMax.Max.Z));
			break;

		case EText3DVerticalTextAlignment::Bottom:
			IconLocation = Transform.TransformPositionNoScale(FVector(0.f, CoordY, BoundsMax.Max.Z));
			break;
	}

	return IconLocation;
}

void FAvaTextVisualizer::GetTextActorGradientControlsLocations(const UAvaText3DComponent* InAvaTextComponent, FVector& OutGradientCenterLocation, FVector& OutGradientStartLocation, FVector& OutGradientEndLocation) const
{
	using namespace UE::Ava::Private;
	if (IsValid(InAvaTextComponent))
	{
		if (const AActor* const Text3DActor = InAvaTextComponent->GetOwner())
		{
			const AActor* TextAsActor = Cast<AActor>(Text3DActor);
			check(TextAsActor);

			FVector BoundsOrigin;
			FVector BoundsExtent;
			
			Text3DActor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
			const float GradientLineHalfLength = FMath::Min(BoundsExtent.Length(), FTextVisualizerStatics::GradientHandleMaxLength);
			FVector GradientCenterLocation = BoundsOrigin;

			const FVector GradientDir = InAvaTextComponent->GetGradientDirection();
			OutGradientStartLocation = GradientCenterLocation - GradientDir * GradientLineHalfLength;
			OutGradientEndLocation = GradientCenterLocation + GradientDir * GradientLineHalfLength;	

			// we need to move gradient center along its direction, base on offset amount
			const float GradientAsNormalizedPos = InAvaTextComponent->GradientSettings.Offset * 2.0 - 1.0;

			GradientCenterLocation -= GradientDir * GradientAsNormalizedPos * GradientLineHalfLength;
			OutGradientCenterLocation = GradientCenterLocation;
		}
	}
}

FVector FAvaTextVisualizer::GetGradientEndHandleLocation(const UAvaText3DComponent* InAvaTextComponent) const
{
	if (IsValid(InAvaTextComponent))
	{
		FVector GradientCenter;
		FVector GradientStart;
		FVector GradientEnd;
		
		GetTextActorGradientControlsLocations(TextComponent.Get(), GradientCenter, GradientStart, GradientEnd);

		return GradientEnd;
	}

	return FVector::Zero();
}

FVector FAvaTextVisualizer::GetGradientCenterHandleLocation(const UAvaText3DComponent* InAvaTextComponent) const
{
	if (IsValid(InAvaTextComponent))
	{
		FVector GradientCenter;
		FVector GradientStart;
		FVector GradientEnd;
		
		GetTextActorGradientControlsLocations(TextComponent.Get(), GradientCenter, GradientStart, GradientEnd);

		return GradientCenter;
	}

	return FVector::Zero();
}

FVector FAvaTextVisualizer::GetGradientStartHandleLocation(const UAvaText3DComponent* InAvaTextComponent) const
{
	if (IsValid(InAvaTextComponent))
	{
		FVector GradientCenter;
		FVector GradientStart;
		FVector GradientEnd;
		
		GetTextActorGradientControlsLocations(TextComponent.Get(), GradientCenter, GradientStart, GradientEnd);

		return GradientStart;
	}

	return FVector::Zero();
}

void FAvaTextVisualizer::StoreTextMetrics(const UText3DComponent* InText3DComp)
{
	Bounds = GetComponentBounds(InText3DComp);
	TArray<FString> Lines;
	InText3DComp->GetText().ToString().ParseIntoArrayLines(Lines);
	LineCount = Lines.Num();
	LineHeight = (Bounds.GetSize().Z - (InText3DComp->GetLineSpacing() * (static_cast<float>(LineCount) - 1.f))) / static_cast<float>(LineCount);
}

void FAvaTextVisualizer::DrawVisualizationNotEditing(const UActorComponent* InComponent, const FSceneView* InView, 
	FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex)
{
	Super::DrawVisualizationNotEditing(InComponent, InView, InPDI, InOutIconIndex);

	const UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(InComponent);

	if (!AvaTextComponent)
	{
		return;
	}
	
	StoreTextMetrics(AvaTextComponent);

	DrawMaxTextSizeVisualization(AvaTextComponent, InView, InPDI);

	DrawMaxTextWidthButton(AvaTextComponent, InView, InPDI, InOutIconIndex, AvaTextComponent->HasMaxWidth() ? Enabled : Disabled);
	++InOutIconIndex;

	DrawMaxTextHeightButton(AvaTextComponent, InView, InPDI, InOutIconIndex, AvaTextComponent->HasMaxHeight() ? Enabled : Disabled);
	++InOutIconIndex;

	DrawScaleProportionallyButton(AvaTextComponent, InView, InPDI, InOutIconIndex, AvaTextComponent->ScalesProportionally() ? Enabled : Disabled);
	++InOutIconIndex;

	if (AvaTextComponent->HasMaxWidth())
	{
		DrawMaxTextWidthHandle(AvaTextComponent, InView, InPDI, Inactive);
	}

	if (AvaTextComponent->HasMaxHeight())
	{
		DrawMaxTextHeightHandle(AvaTextComponent, InView, InPDI, Inactive);
	}
	
	if (AvaTextComponent->GetColoringStyle() == EAvaTextColoringStyle::Gradient)
	{
		DrawEditGradientButton(AvaTextComponent, InView, InPDI, InOutIconIndex, bShowGradientControls ? Enabled : Disabled);

		if (bShowGradientControls)
		{
			DrawGradientHandles(AvaTextComponent, InView, InPDI);
		}
	}
}

void FAvaTextVisualizer::DrawVisualizationEditing(const UActorComponent* InComponent, const FSceneView* InView, 
	FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex)
{
	Super::DrawVisualizationNotEditing(InComponent, InView, InPDI, InOutIconIndex);

	const UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(InComponent);

	if (!AvaTextComponent)
	{
		return;
	}

	StoreTextMetrics(AvaTextComponent);

	DrawMaxTextSizeVisualization(AvaTextComponent, InView, InPDI);

	DrawMaxTextWidthButton(AvaTextComponent, InView, InPDI, InOutIconIndex, AvaTextComponent->HasMaxWidth() ? Enabled : Disabled);
	++InOutIconIndex;

	DrawMaxTextHeightButton(AvaTextComponent, InView, InPDI, InOutIconIndex, AvaTextComponent->HasMaxHeight() ? Enabled : Disabled);
	++InOutIconIndex;

	DrawScaleProportionallyButton(AvaTextComponent, InView, InPDI, InOutIconIndex, AvaTextComponent->ScalesProportionally() ? Enabled : Disabled);
	++InOutIconIndex;

	DrawMaxTextWidthHandle(AvaTextComponent, InView, InPDI, Inactive);
	DrawMaxTextHeightHandle(AvaTextComponent, InView, InPDI, Inactive);
	
	if (AvaTextComponent->GetColoringStyle() == EAvaTextColoringStyle::Gradient)
	{
		DrawEditGradientButton(AvaTextComponent, InView, InPDI, InOutIconIndex, bShowGradientControls ? Enabled : Disabled);
		++InOutIconIndex;

		if (bShowGradientControls)
		{
			DrawGradientHandles(AvaTextComponent, InView, InPDI);
		}
	}
}

void FAvaTextVisualizer::StoreInitialValues()
{
	Super::StoreInitialValues();

	if (!TextComponent.IsValid())
	{
		return;
	}

	bInitialMaxWidthEnabled = TextComponent->HasMaxHeight();
	bInitialMaxHeightEnabled = TextComponent->HasMaxHeight();
	InitialMaxWidth = TextComponent->GetMaxWidth();
	InitialMaxHeight = TextComponent->GetMaxHeight();
	bInitialScaleProportionally = TextComponent->ScalesProportionally();
	
	InitialGradientRotation = TextComponent->GetGradientSettings().Rotation;
	InitialGradientOffset = TextComponent->GetGradientSettings().Offset;
	InitialGradientSmoothness = TextComponent->GetGradientSettings().Smoothness;
}

void FAvaTextVisualizer::DrawMaxTextSizeVisualization(const UText3DComponent* InTextComponent, const FSceneView* InView, 
	FPrimitiveDrawInterface* InPDI) const
{
	if (!InTextComponent || (!InTextComponent->HasMaxWidth() && !InTextComponent->HasMaxHeight()))
	{
		return;
	}

	const FTransform Transform = InTextComponent->GetComponentTransform();
	const FBox BoundsMax = GetBoundsMax(InTextComponent);
	const FVector TopLeft = Transform.TransformPositionNoScale(FVector(0.f, BoundsMax.Min.Y, BoundsMax.Max.Z));
	const FVector TopRight = Transform.TransformPositionNoScale(FVector(0.f, BoundsMax.Max.Y, BoundsMax.Max.Z));
	const FVector BottomLeft = Transform.TransformPositionNoScale(FVector(0.f, BoundsMax.Min.Y, BoundsMax.Min.Z));
	const FVector BottomRight = Transform.TransformPositionNoScale(FVector(0.f, BoundsMax.Max.Y, BoundsMax.Min.Z));

	InPDI->DrawLine(TopLeft, TopRight, FLinearColor::Yellow, SDPG_World, 0.5f);
	InPDI->DrawLine(TopRight, BottomRight, FLinearColor::Yellow, SDPG_World, 0.5f);
	InPDI->DrawLine(BottomRight, BottomLeft, FLinearColor::Yellow, SDPG_World, 0.5f);
	InPDI->DrawLine(BottomLeft, TopLeft, FLinearColor::Yellow, SDPG_World, 0.5f);
}

bool FAvaTextVisualizer::DrawGradientCenterHandle(const UText3DComponent* InTextComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, FVector InGradientCenter)
{
	static constexpr float BaseSize = 1.f;

	UTexture2D* UVSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::SizeSprite);

	if (!UVSprite || !UVSprite->GetResource())
	{
		return true;
	}

	const FVector IconLocation = InGradientCenter;
	const float IconSize = BaseSize * GetIconSizeScale(InView, IconLocation);

	InPDI->SetHitProxy(new HAvaTextGradientCenterHandleProxy(InTextComponent));
	InPDI->DrawSprite(InGradientCenter, IconSize, IconSize, UVSprite->GetResource(), FLinearColor::White,
	                SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);		
	return false;
}

bool FAvaTextVisualizer::DrawGradientSmoothnessHandle(const UText3DComponent* InTextComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, FVector InHandleLocation)
{
	static constexpr float BaseSize = 1.5f;
		
	const UTexture2D* const UVSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::UVSprite);

	if (!UVSprite || !UVSprite->GetResource())
	{
		return true;
	}

	const FVector IconLocation = InHandleLocation;
	const float IconSize = BaseSize * GetIconSizeScale(InView, IconLocation);

	InPDI->SetHitProxy(new HAvaTextGradientSmoothnessHandleProxy(InTextComponent));
	InPDI->DrawSprite(InHandleLocation, IconSize, IconSize, UVSprite->GetResource(), FLinearColor::White,
					SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);		
	return false;
}

void FAvaTextVisualizer::DrawGradientHandles(const UAvaText3DComponent* InAvaTextComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI) const
{
	using namespace UE::Ava::Private;
	if (!InAvaTextComponent)
	{
		return;
	}

	FVector GradientCenter;
	FVector GradientLineStart;
	FVector GradientLineEnd;

	GetTextActorGradientControlsLocations(InAvaTextComponent, GradientCenter, GradientLineStart, GradientLineEnd);
	
	InPDI->DrawLine(GradientLineStart, GradientLineEnd, FLinearColor::White,SDPG_Foreground );
	
	DrawGradientLineStartHandle(InAvaTextComponent, InView, InPDI, GradientLineStart, InAvaTextComponent->GetGradientSettings().ColorB);
	DrawGradientLineEndHandle(InAvaTextComponent, InView, InPDI, GradientLineEnd, InAvaTextComponent->GetGradientSettings().ColorA);
	DrawGradientCenterHandle(InAvaTextComponent, InView, InPDI, GradientCenter);
	DrawGradientSmoothnessHandle(InAvaTextComponent, InView, InPDI, GradientCenter + FVector(0, FTextVisualizerStatics::GradientSmoothnessHandleOffset, 0));
	
}

FBox FAvaTextVisualizer::GetBoundsMax(const UText3DComponent* InText3DComp) const
{
	const FVector Scale = InText3DComp->GetComponentTransform().GetScale3D();
	FBox BoundsMax = Bounds;

	if (InText3DComp->HasMaxWidth())
	{
		switch (InText3DComp->GetHorizontalAlignment())
		{
			case EText3DHorizontalTextAlignment::Left:
				BoundsMax.Max.Y = InText3DComp->GetMaxWidth() * Scale.Y;
				break;

			case EText3DHorizontalTextAlignment::Center:
				BoundsMax.Min.Y = InText3DComp->GetMaxWidth() * -0.5f * Scale.Y;
				BoundsMax.Max.Y = InText3DComp->GetMaxWidth() * 0.5f * Scale.Y;
				break;

			case EText3DHorizontalTextAlignment::Right:
				BoundsMax.Min.Y = InText3DComp->GetMaxWidth() * -Scale.Y;
				break;
		}
	}

	if (InText3DComp->HasMaxHeight())
	{
		switch (InText3DComp->GetVerticalAlignment())
		{
			case EText3DVerticalTextAlignment::FirstLine:
				if (LineCount <= 1)
				{
					BoundsMax.Max.Z = FMath::Max(BoundsMax.Max.Z, InText3DComp->GetMaxHeight() * TopHeightFraction * Scale.Z);
				}
				else
				{
					BoundsMax.Max.Z = FMath::Max(BoundsMax.Max.Z, LineHeight * TopHeightFraction * Scale.Z * InText3DComp->GetMaxHeight() / Bounds.GetSize().Z);
				}
				break;

			case EText3DVerticalTextAlignment::Top:
				BoundsMax.Min.Z = -InText3DComp->GetMaxHeight() * Scale.Z;
				break;

			case EText3DVerticalTextAlignment::Center:
				BoundsMax.Min.Z = InText3DComp->GetMaxHeight() / -2.f * Scale.Z;
				BoundsMax.Max.Z = InText3DComp->GetMaxHeight() / 2.f * Scale.Z;
				break;

			case EText3DVerticalTextAlignment::Bottom:
				BoundsMax.Max.Z = InText3DComp->GetMaxHeight() * Scale.Z;
				break;
		}
	}

	return BoundsMax;
}

void FAvaTextVisualizer::DrawMaxTextWidthButton(const UText3DComponent* InTextComponent, const FSceneView* InView, 
	FPrimitiveDrawInterface* InPDI, int32 InIconIndex, const FLinearColor& InColor) const
{
	if (!InTextComponent)
	{
		return;
	}

	UTexture2D* UVSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::TextMaxWidthSprite);

	if (!UVSprite || !UVSprite->GetResource())
	{
		return;
	}

	FVector IconLocation;
	float IconSize;
	GetIconMetrics(InView, InIconIndex, IconLocation, IconSize);

	InPDI->SetHitProxy(new HAvaTextMaxTextWidthProxy(InTextComponent));
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, UVSprite->GetResource(), InColor,
		SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

void FAvaTextVisualizer::DrawMaxTextHeightButton(const UText3DComponent* InTextComponent, const FSceneView* InView, 
	FPrimitiveDrawInterface* InPDI, int32 InIconIndex, const FLinearColor& InColor) const
{
	if (!InTextComponent)
	{
		return;
	}

	const UTexture2D* const UVSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::TextMaxHeightSprite);

	if (!UVSprite || !UVSprite->GetResource())
	{
		return;
	}

	FVector IconLocation;
	float IconSize;
	GetIconMetrics(InView, InIconIndex, IconLocation, IconSize);

	InPDI->SetHitProxy(new HAvaTextMaxTextHeightProxy(InTextComponent));
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, UVSprite->GetResource(), InColor,
		SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

void FAvaTextVisualizer::DrawScaleProportionallyButton(const UText3DComponent* InTextComponent, const FSceneView* InView, 
	FPrimitiveDrawInterface* InPDI, int32 InIconIndex, const FLinearColor& InColor) const
{
	if (!InTextComponent)
	{
		return;
	}

	const UTexture2D* const UVSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::TextScaleProportionallySprite);

	if (!UVSprite || !UVSprite->GetResource())
	{
		return;
	}

	FVector IconLocation;
	float IconSize;
	GetIconMetrics(InView, InIconIndex, IconLocation, IconSize);

	InPDI->SetHitProxy(new HAvaTextScaleProportionallyProxy(InTextComponent));
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, UVSprite->GetResource(), InColor,
		SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

void FAvaTextVisualizer::DrawEditGradientButton(
	const UText3DComponent* InTextComponent
	, const FSceneView* InView
	, FPrimitiveDrawInterface* InPDI
	, int32 InIconIndex
	, const FLinearColor& InColor) const
{
	if (!InTextComponent)
	{
		return;
	}

	// todo: proper sprite
	const UTexture2D* const UVSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::LinearGradientSprite);

	if (!UVSprite || !UVSprite->GetResource())
	{
		return;
	}

	FVector IconLocation;
	float IconSize;
	GetIconMetrics(InView, InIconIndex, IconLocation, IconSize);
	InPDI->SetHitProxy(new HAvaTextEditGradientProxy(InTextComponent));
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, UVSprite->GetResource(), InColor,
		SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

void FAvaTextVisualizer::DrawMaxTextWidthHandle(
	const UText3DComponent* InTextComponent
	, const FSceneView* InView
	, FPrimitiveDrawInterface* InPDI
	, const FLinearColor& InColor) const
{
	static constexpr float BaseSize = 1.f;

	if (!InTextComponent)
	{
		return;
	}

	const UTexture2D* const UVSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::SizeSprite);

	if (!UVSprite || !UVSprite->GetResource())
	{
		return;
	}

	const FVector IconLocation = GetWidthHandleLocation(InTextComponent);
	const float IconSize = BaseSize * GetIconSizeScale(InView, IconLocation);

	InPDI->SetHitProxy(new HAvaTextMaxTextWidthHandleProxy(InTextComponent));
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, UVSprite->GetResource(), InColor,
		SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

void FAvaTextVisualizer::DrawMaxTextHeightHandle(const UText3DComponent* InTextComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI,
	const FLinearColor& InColor) const
{
	static constexpr  float BaseSize = 1.f;

	if (!InTextComponent)
	{
		return;
	}

	const UTexture2D* const UVSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::SizeSprite);

	if (!UVSprite || !UVSprite->GetResource())
	{
		return;
	}

	const FVector IconLocation = GetHeightHandleLocation(InTextComponent);
	const float IconSize = BaseSize * GetIconSizeScale(InView, IconLocation);

	InPDI->SetHitProxy(new HAvaTextMaxTextHeightHandleProxy(InTextComponent));
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, UVSprite->GetResource(), InColor,
		SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

void FAvaTextVisualizer::DrawGradientLineStartHandle(const UAvaText3DComponent* InAvaTextComponent, const FSceneView* InView, 
	FPrimitiveDrawInterface* InPDI, const FVector& InLocation, const FLinearColor& InColor) const
{
	static constexpr float BaseSize = 1.5f;

	if (!InAvaTextComponent)
	{
		return;
	}

	const UTexture2D* const UVSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::ColorSelectionSprite);

	if (!UVSprite || !UVSprite->GetResource())
	{
		return;
	}
	
	const FVector IconLocation = InLocation;
	const float IconSize = BaseSize * GetIconSizeScale(InView, IconLocation);

	// in case gradient center handle is on top of this handle, or almost on top of it, don't enable interaction
	if (FVector::Distance(InLocation, GetGradientCenterHandleLocation(InAvaTextComponent)) > BaseSize/2.0)
	{
		InPDI->SetHitProxy(new HAvaTextGradientLineStartHandleProxy(InAvaTextComponent));
	}
	
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, UVSprite->GetResource(), InColor,
		SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

void FAvaTextVisualizer::DrawGradientLineEndHandle(const UAvaText3DComponent* InAvaTextComponent, const FSceneView* InView, 
	FPrimitiveDrawInterface* InPDI, const FVector& InLocation, const FLinearColor& InColor) const
{
	static constexpr float BaseSize = 1.5f;

	if (!InAvaTextComponent)
	{
		return;
	}

	const UTexture2D* const UVSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::ColorSelectionSprite);

	if (!UVSprite || !UVSprite->GetResource())
	{
		return;
	}

	const FVector IconLocation = InLocation;
	const float IconSize = BaseSize * GetIconSizeScale(InView, IconLocation);

	// in case gradient center handle is on top of this handle, or almost on top of it, don't enable interaction
	if (FVector::Distance(InLocation, GetGradientCenterHandleLocation(InAvaTextComponent)) > BaseSize/2.0)
	{
		InPDI->SetHitProxy(new HAvaTextGradientLineEndHandleProxy(InAvaTextComponent));
	}
	
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, UVSprite->GetResource(), InColor,
		SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

#undef LOCTEXT_NAMESPACE
