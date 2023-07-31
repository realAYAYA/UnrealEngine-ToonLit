// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContextualAnimEdMode.h"
#include "EngineUtils.h"
#include "ContextualAnimSceneAsset.h"
#include "ContextualAnimSelectionCriterion.h"
#include "ContextualAnimUtilities.h"
#include "ContextualAnimViewportClient.h"
#include "ContextualAnimAssetEditorToolkit.h"
#include "ContextualAnimViewModel.h"
#include "Animation/AnimMontage.h"
#include "AnimNotifyState_IKWindow.h"
#include "SkeletalDebugRendering.h"
#include "CanvasTypes.h"
#include "CanvasItem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

const FEditorModeID FContextualAnimEdMode::EdModeId = TEXT("ContextualAnimEdMode");

IMPLEMENT_HIT_PROXY(HSelectionCriterionHitProxy, HHitProxy);

FContextualAnimEdMode::FContextualAnimEdMode()
{
}

FContextualAnimEdMode::~FContextualAnimEdMode()
{
}

void FContextualAnimEdMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);
}

void FContextualAnimEdMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);

	// @TODO: This should not be initialized here
	FContextualAnimViewportClient* ViewportClient = static_cast<FContextualAnimViewportClient*>(Viewport->GetClient());
	if (!ViewModel)
	{
		ViewModel = ViewportClient->GetAssetEditorToolkit()->GetViewModel();
	}

	if (ViewModel)
	{
		if (const UContextualAnimSceneAsset* SceneAsset = ViewModel->GetSceneAsset())
		{
			const FContextualAnimSceneBindings& Bindings = ViewModel->SceneBindings;
			const FName PrimaryRole = SceneAsset->GetPrimaryRole();
			const FContextualAnimSceneBinding* PrimaryBinding = Bindings.FindBindingByRole(PrimaryRole);
			
			if(PrimaryBinding == nullptr)
			{
				return;
			}

			const FTransform PrimaryTransform = PrimaryBinding->GetContext().GetTransform();
			if (const FContextualAnimSceneSection* Section = SceneAsset->GetSection(Bindings.GetSectionIdx()))
			{
				// Draw Scene Pivots
				if (const FContextualAnimSet* AnimSet = Section->GetAnimSet(Bindings.GetAnimSetIdx()))
				{
					for (const FTransform& ScenePivot : AnimSet->ScenePivots)
					{
						DrawCoordinateSystem(PDI, ScenePivot.GetLocation(), ScenePivot.Rotator(), 50.f, SDPG_Foreground);
					}
				}

				// Attempt to draw selection criteria and entry poses only when playback time is at 0 and simulate mode is not playing
				const float PlaybackTime = ViewModel->GetPlaybackTime();
				if (PlaybackTime <= 0.f && !ViewModel->IsSimulateModePlaying())
				{
					for (int32 AnimSetIdx = 0; AnimSetIdx < Section->GetNumAnimSets(); AnimSetIdx++)
					{
						const bool bIsActiveSet = AnimSetIdx == Bindings.GetAnimSetIdx();

						for (const FContextualAnimTrack& AnimTrack : Section->GetAnimSet(AnimSetIdx)->Tracks)
						{
							const FContextualAnimSceneBinding* Binding = Bindings.FindBindingByRole(AnimTrack.Role);

							if (Binding == nullptr)
							{
								continue;
							}

							bool bPassesSelectionCriteria = true;
							for (int32 CriterionIdx = 0; CriterionIdx < AnimTrack.SelectionCriteria.Num(); CriterionIdx++)
							{
								if (const UContextualAnimSelectionCriterion* Criterion = AnimTrack.SelectionCriteria[CriterionIdx])
								{
									const bool bPassesCurrentSelectionCriterion = Criterion->DoesQuerierPassCondition(PrimaryBinding->GetContext(), Binding->GetContext());
									if (!bPassesCurrentSelectionCriterion && bPassesSelectionCriteria)
									{
										bPassesSelectionCriteria = false;
									}

									// Draw Selection Criteria
									const EMultiOptionDrawMode SelectionCriteriaDrawMode = ViewportClient->GetSelectionCriteriaDrawMode();
									if (SelectionCriteriaDrawMode == EMultiOptionDrawMode::All || (SelectionCriteriaDrawMode == EMultiOptionDrawMode::Single && bIsActiveSet))
									{
										// Draw with dashed lines if this is not the active AnimSet and simulate mode is inactive
										const bool bUseDashedLines = !bIsActiveSet && ViewModel->IsSimulateModeInactive();

										FLinearColor DrawColor = FLinearColor::White;
										float Thickness = 1.f;
										if (bPassesCurrentSelectionCriterion)
										{
											DrawColor = FLinearColor::Green;
											Thickness = 2.f;
										}

										//@TODO: Each SelectionCriterion should implement this, and here we should just call "Criterion->Draw()"
										if (const UContextualAnimSelectionCriterion_TriggerArea* Spatial = Cast<UContextualAnimSelectionCriterion_TriggerArea>(Criterion))
										{
											const int32 LastIndex = Spatial->PolygonPoints.Num() - 1;
											for (int32 Idx = 0; Idx <= LastIndex; Idx++)
											{
												const FVector P0 = PrimaryTransform.TransformPositionNoScale(Spatial->PolygonPoints[Idx]);
												const FVector P1 = PrimaryTransform.TransformPositionNoScale(Spatial->PolygonPoints[Idx == LastIndex ? 0 : Idx + 1]);

												if (bUseDashedLines)
												{
													DrawDashedLine(PDI, P0, P1, DrawColor, 10.f, SDPG_Foreground);
													DrawDashedLine(PDI, P0 + FVector::UpVector * Spatial->Height, P1 + FVector::UpVector * Spatial->Height, DrawColor, 10.f, SDPG_Foreground);
													DrawDashedLine(PDI, P0, P0 + FVector::UpVector * Spatial->Height, DrawColor, 10.f, SDPG_Foreground);
												}
												else
												{
													PDI->DrawLine(P0, P1, DrawColor, SDPG_Foreground, Thickness);
													PDI->DrawLine(P0 + FVector::UpVector * Spatial->Height, P1 + FVector::UpVector * Spatial->Height, DrawColor, SDPG_Foreground, Thickness);
													PDI->DrawLine(P0, P0 + FVector::UpVector * Spatial->Height, DrawColor, SDPG_Foreground, Thickness);
												}

												// Draw hit proxy points only for the active set and if simulate mode is not active
												if (ViewModel->IsSimulateModeInactive() && bIsActiveSet)
												{
													PDI->SetHitProxy(new HSelectionCriterionHitProxy(AnimTrack.Role, CriterionIdx, Idx));
													PDI->DrawPoint(P0, FLinearColor::Black, 15.f, SDPG_Foreground);
													PDI->SetHitProxy(nullptr);

													PDI->SetHitProxy(new HSelectionCriterionHitProxy(AnimTrack.Role, CriterionIdx, Idx + 4));
													PDI->DrawPoint(P0 + FVector::UpVector * Spatial->Height, FLinearColor::Black, 15.f, SDPG_Foreground);
													PDI->SetHitProxy(nullptr);
												}
											}
										}
										else if (const UContextualAnimSelectionCriterion_Cone* Cone = Cast<UContextualAnimSelectionCriterion_Cone>(Criterion))
										{
											FVector Origin, Direction;
											if (Cone->Mode == EContextualAnimCriterionConeMode::ToPrimary)
											{
												const FTransform QuerierTransform = Section->GetAlignmentTransformForRoleRelativeToOtherRole(AnimSetIdx, AnimTrack.Role, PrimaryRole, 0.f) * PrimaryTransform;
												Origin = QuerierTransform.GetLocation();
												Direction = QuerierTransform.GetRotation().GetForwardVector().RotateAngleAxis(Cone->Offset, FVector::UpVector);
											}
											else if (Cone->Mode == EContextualAnimCriterionConeMode::FromPrimary)
											{
												Origin = PrimaryTransform.GetLocation();
												Direction = PrimaryTransform.GetRotation().GetForwardVector().RotateAngleAxis(Cone->Offset, FVector::UpVector);
											}

											UContextualAnimUtilities::DrawSector(*PDI, Origin, Direction, 0.f, Cone->Distance, -Cone->HalfAngle, Cone->HalfAngle, DrawColor, SDPG_World, Thickness, bUseDashedLines);
										}
									}
								}
							}

							// Draw Entry Poses
							const EMultiOptionDrawMode EntryPosesDrawMode = ViewportClient->GetEntryPosesDrawMode();
							if (EntryPosesDrawMode == EMultiOptionDrawMode::All || (EntryPosesDrawMode == EMultiOptionDrawMode::Single && bIsActiveSet))
							{
								if (AnimTrack.Role != PrimaryRole && AnimTrack.Animation)
								{
									const float Time = 0.f;
									const FTransform RelativeToPrimary = Section->GetAlignmentTransformForRoleRelativeToOtherRole(AnimSetIdx, AnimTrack.Role, PrimaryRole, Time);
									FTransform Transform = (SceneAsset->GetMeshToComponentForRole(AnimTrack.Role) * RelativeToPrimary) * PrimaryTransform;

									if (ACharacter* PrimaryAsCharacter = Cast<ACharacter>(PrimaryBinding->GetActor()))
									{
										const float CapsuleHalfHeight = PrimaryAsCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
										Transform.SetLocation(Transform.GetLocation() - (FVector::UpVector * CapsuleHalfHeight));
									}

									UContextualAnimUtilities::DrawPose(PDI, AnimTrack.Animation, Time, Transform, bPassesSelectionCriteria ? FColor::Green : FColor::White, 1.f);
								}
							}
						}
					}
				}

				// Draw IK Targets
				const EMultiOptionDrawMode IKTargetsDrawMode = ViewportClient->GetIKTargetsDrawMode();
				if (IKTargetsDrawMode != EMultiOptionDrawMode::None)
				{
					for (const FContextualAnimSceneBinding& Binding : Bindings)
					{
						if (IKTargetsDrawMode == EMultiOptionDrawMode::All || (IKTargetsDrawMode == EMultiOptionDrawMode::Single && Binding.GetActor() == ViewModel->GetSelectedActor()))
						{
							DrawIKTargetsForBinding(*PDI, Binding);
						}
					}
				}
			}
		}
	}
}

void FContextualAnimEdMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FEdMode::DrawHUD(ViewportClient, Viewport, View, Canvas);

	if (ViewModel)
	{
		FCanvasTextItem TextItem(FVector2D(10.f, 40.f), ViewModel->GetSelectionDebugText(), GEngine->GetSmallFont(), FLinearColor::White);
		Canvas->DrawItem(TextItem);
	}
}

void FContextualAnimEdMode::DrawIKTargetsForBinding(FPrimitiveDrawInterface& PDI, const FContextualAnimSceneBinding& Binding) const
{
	const TArray<FContextualAnimIKTargetDefinition>& IKTargetDefs = ViewModel->SceneBindings.GetIKTargetDefContainerFromBinding(Binding).IKTargetDefs;
	for (const FContextualAnimIKTargetDefinition& IKTargetDef : IKTargetDefs)
	{
		if (const FContextualAnimSceneBinding* TargetBinding = ViewModel->SceneBindings.FindBindingByRole(IKTargetDef.TargetRoleName))
		{
			if (USkeletalMeshComponent* TargetSkelMeshComp = TargetBinding->GetSkeletalMeshComponent())
			{
				const float Alpha = UAnimNotifyState_IKWindow::GetIKAlphaValue(IKTargetDef.GoalName, Binding.GetAnimMontageInstance());

				if (IKTargetDef.Provider == EContextualAnimIKTargetProvider::Bone)
				{
					//@TODO:...
				}
				else if (IKTargetDef.Provider == EContextualAnimIKTargetProvider::Autogenerated)
				{
					const FTransform ParentTransform = TargetSkelMeshComp->GetSocketTransform(IKTargetDef.TargetBoneName);

					const FTransform TargetTransform = ViewModel->SceneBindings.GetIKTargetTransformFromBinding(Binding, IKTargetDef.GoalName, Binding.GetAnimMontageTime()) * ParentTransform;

					FLinearColor Color = Alpha > 0.f ? FLinearColor(FColor::MakeRedToGreenColorFromScalar(Alpha)) : FLinearColor::White;

					FVector Start = ParentTransform.GetLocation();
					FVector End = TargetTransform.GetLocation();

					const float Radius = 1.f;
					SkeletalDebugRendering::DrawWireBone(&PDI, Start, End, Color, SDPG_Foreground, Radius);
					SkeletalDebugRendering::DrawAxes(&PDI, FTransform(End), SDPG_Foreground, 0.f, Radius);
				}
			}
		}
	}
}

bool FContextualAnimEdMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (HitProxy != nullptr)
	{
		if (HitProxy->IsA(HActor::StaticGetType()))
		{
			HActor* ActorHitProxy = static_cast<HActor*>(HitProxy);
			ViewModel->UpdateSelection(ActorHitProxy->Actor);
			return true;
		}
		else if (HitProxy->IsA(HSelectionCriterionHitProxy::StaticGetType()))
		{
			HSelectionCriterionHitProxy* CriterionHitProxy = static_cast<HSelectionCriterionHitProxy*>(HitProxy);
			ViewModel->UpdateSelection(CriterionHitProxy->Role, CriterionHitProxy->IndexPair.Key, CriterionHitProxy->IndexPair.Value);
			return true;
		}
	}

	ViewModel->ClearSelection();
	return false; // unhandled
}

bool FContextualAnimEdMode::GetHitResultUnderCursor(FHitResult& OutHitResult, FEditorViewportClient* InViewportClient, const FViewportClick& Click) const
{
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(InViewportClient->Viewport, InViewportClient->GetScene(), InViewportClient->EngineShowFlags).SetRealtimeUpdate(InViewportClient->IsRealtime()));
	FSceneView* View = InViewportClient->CalcSceneView(&ViewFamily);
	FViewportCursorLocation Cursor(View, InViewportClient, Click.GetClickPos().X, Click.GetClickPos().Y);
	const auto ViewportType = InViewportClient->GetViewportType();

	const FVector RayStart = Cursor.GetOrigin();
	const FVector RayEnd = RayStart + Cursor.GetDirection() * HALF_WORLD_MAX;

	return InViewportClient->GetWorld()->LineTraceSingleByChannel(OutHitResult, RayStart, RayEnd, ECC_WorldStatic, FCollisionQueryParams::DefaultQueryParam);
}

bool FContextualAnimEdMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (ViewModel)
	{
		return ViewModel->StartTracking();
	}

	return FEdMode::StartTracking(InViewportClient, InViewport);
}

bool FContextualAnimEdMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (ViewModel)
	{
		return ViewModel->EndTracking();
	}

	return FEdMode::EndTracking(InViewportClient, InViewport);
}

bool FContextualAnimEdMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();

	if (CurrentAxis != EAxisList::None)
	{
		if (ViewModel)
		{
			return ViewModel->ProcessInputDelta(InDrag, InRot, InScale);
		}
	}

	return false; // unhandled
}

bool FContextualAnimEdMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (ViewModel && ViewModel->IsSimulateModePaused())
	{
		if (Key == EKeys::Enter && Event == IE_Released)
		{
			ViewModel->StartSimulation();
			return true;
		}
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

bool FContextualAnimEdMode::AllowWidgetMove()
{
	return ShouldDrawWidget();
}

bool FContextualAnimEdMode::ShouldDrawWidget() const
{
	if (ViewModel)
	{
		return ViewModel->ShouldPreviewSceneDrawWidget();
	}

	return false;
}

bool FContextualAnimEdMode::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	if (ViewModel)
	{
		return ViewModel->GetCustomDrawingCoordinateSystem(InMatrix, InData);
	}

	return false;
}

bool FContextualAnimEdMode::GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	return GetCustomDrawingCoordinateSystem(InMatrix, InData);
}

FVector FContextualAnimEdMode::GetWidgetLocation() const
{
	if (ViewModel)
	{
		return ViewModel->GetWidgetLocationFromSelection();
	}

	return FVector::ZeroVector;
}
