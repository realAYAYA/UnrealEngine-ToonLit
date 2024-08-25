// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectAssetEditorViewportClient.h"
#include "Components/StaticMeshComponent.h"
#include "SmartObjectAssetToolkit.h"
#include "SmartObjectAssetEditorSettings.h"
#include "SmartObjectComponentVisualizer.h"
#include "ScopedTransaction.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectAnnotation.h"
#include "SmartObjectViewModel.h"
#include "Misc/EnumerateRange.h"

#define LOCTEXT_NAMESPACE "SmartObjectAssetToolkit"

FSmartObjectAssetEditorViewportClient::FSmartObjectAssetEditorViewportClient(const TSharedRef<const FSmartObjectAssetToolkit>& InAssetEditorToolkit, FPreviewScene* InPreviewScene, const TWeakPtr<SEditorViewport>& InEditorViewportWidget)
	: FEditorViewportClient(&InAssetEditorToolkit->GetEditorModeManager(), InPreviewScene, InEditorViewportWidget)
	, AssetEditorToolkit(InAssetEditorToolkit)
{
	EngineShowFlags.DisableAdvancedFeatures();
	bUsingOrbitCamera = true;

	// Set if the grid will be drawn
	DrawHelper.bDrawGrid = GetDefault<USmartObjectAssetEditorSettings>()->bShowGridByDefault;
	DrawHelper.bDrawWorldBox = false;
}

FSmartObjectAssetEditorViewportClient::~FSmartObjectAssetEditorViewportClient()
{
	RemoveViewModelDelegates();
	
	if (ScopedTransaction != nullptr)
	{
		delete ScopedTransaction;
		ScopedTransaction = nullptr;
	}
}

void FSmartObjectAssetEditorViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	if (View != nullptr && PDI != nullptr)
	{
		// Draw slots and annotations.
		if (const USmartObjectDefinition* Definition = SmartObjectDefinition.Get())
		{
			UE::SmartObject::Editor::Draw(*Definition, GetSelection(), FTransform::Identity, *View, *PDI, *PreviewScene->GetWorld(), PreviewActor.Get());
		}

		// Draw the object origin.
		DrawCoordinateSystem(PDI, FVector::ZeroVector, FRotator::ZeroRotator, 20.f, SDPG_World, 1.f);
	}
}


void FSmartObjectAssetEditorViewportClient::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);

	// Draw slots and annotations.
	if (const USmartObjectDefinition* Definition = SmartObjectDefinition.Get())
	{
		UE::SmartObject::Editor::DrawCanvas(*Definition, GetSelection(), FTransform::Identity, View, Canvas, *PreviewScene->GetWorld(), PreviewActor.Get());
	}
}

void FSmartObjectAssetEditorViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);

	if (!ViewModel.IsValid())
	{
		return;
	}
	
	if (Key == EKeys::LeftMouseButton)
	{
		bool bClickHandled = false;
		if (HitProxy)
		{
			const FViewportClick Click(&View, this, Key, Event, HitX, HitY);

			if (HitProxy->IsA(HSmartObjectItemProxy::StaticGetType()))
			{
				const HSmartObjectItemProxy* SlotProxy = static_cast<HSmartObjectItemProxy*>(HitProxy);
				const FGuid HitItem = SlotProxy->ItemID;

				if (IsCtrlPressed())
				{
					// Toggle selection
					if (ViewModel->IsSelected(HitItem))
					{
						ViewModel->RemoveFromSelection(HitItem);
					}
					else
					{
						ViewModel->AddToSelection(HitItem);
					}
				}
				else
				{
					// Set selection
					ViewModel->SetSelection({ HitItem });
				}

				bClickHandled = true;
			}
		}

		if (!bClickHandled)
		{
			ViewModel->ResetSelection();
		}
	}
}

FVector FSmartObjectAssetEditorViewportClient::GetWidgetLocation() const
{
	if (bIsManipulating)
	{
		// Return cached location during manipulation to avoid feedback effects.
		return CachedWidgetLocation;
	}
	
	FVector Result = FVector::ZeroVector; 

	if (!ViewModel.IsValid())
	{
		return Result;
	}

	const USmartObjectDefinition* Definition = SmartObjectDefinition.Get();
	if (Definition != nullptr)
	{
		int32 NumSlots = 0;
		FVector AccumulatedSlotLocation = FVector::ZeroVector;
		const FTransform OwnerLocalToWorld = FTransform::Identity;
		const TConstArrayView<FSmartObjectSlotDefinition> SlotDefinitions = Definition->GetSlots();
		
		for (int32 Index = 0; Index < SlotDefinitions.Num(); ++Index)
		{
			const FTransform SlotTransform = Definition->GetSlotWorldTransform(Index, OwnerLocalToWorld);

			const FSmartObjectSlotDefinition& SlotDefinition = SlotDefinitions[Index];

			if (ViewModel->IsSelected(SlotDefinition.ID))
			{
				AccumulatedSlotLocation += SlotTransform.GetLocation();
				NumSlots++;
			}

			for (TConstEnumerateRef<const FSmartObjectDefinitionDataProxy> DataProxy : EnumerateRange(SlotDefinition.DefinitionData))
			{
				if (const FSmartObjectSlotAnnotation* Annotation = DataProxy->Data.GetPtr<FSmartObjectSlotAnnotation>())
				{
					if (ViewModel->IsSelected(DataProxy->ID))
					{
						if (Annotation->HasTransform())
						{
							const FTransform AnnotationTransform = Annotation->GetAnnotationWorldTransform(SlotTransform);
							AccumulatedSlotLocation += AnnotationTransform.GetLocation();
							NumSlots++;
						}
					}
				}
			}
				
		}

		if (NumSlots > 0)
		{
			Result = AccumulatedSlotLocation / NumSlots;
		}
	}

	CachedWidgetLocation = Result;
	
	return Result;
}

FMatrix FSmartObjectAssetEditorViewportClient::GetWidgetCoordSystem() const
{
	return FMatrix::Identity;
}

ECoordSystem FSmartObjectAssetEditorViewportClient::GetWidgetCoordSystemSpace() const
{
	return WidgetCoordSystemSpace;
}

UE::Widget::EWidgetMode FSmartObjectAssetEditorViewportClient::GetWidgetMode() const
{
	if (!ViewModel.IsValid())
	{
		return UE::Widget::EWidgetMode::WM_None;
	}

	bool bIsWidgetValid = false;

	const USmartObjectDefinition* Definition = SmartObjectDefinition.Get();
	if (Definition != nullptr)
	{
		const TConstArrayView<FSmartObjectSlotDefinition> SlotDefinitions = Definition->GetSlots();

		for (const FSmartObjectSlotDefinition& SlotDefinition : SlotDefinitions)
		{
			if (ViewModel->IsSelected(SlotDefinition.ID))
			{
				bIsWidgetValid = true;
				break;
			}

			for (const FSmartObjectDefinitionDataProxy& DataProxy : SlotDefinition.DefinitionData)
			{
				if (const FSmartObjectSlotAnnotation* Annotation = DataProxy.Data.GetPtr<FSmartObjectSlotAnnotation>())
				{
					if (ViewModel->IsSelected(DataProxy.ID))
					{
						if (Annotation->HasTransform())
						{
							bIsWidgetValid = true;
							break;
						}
					}
				}
			}

			if (bIsWidgetValid)
			{
				break;
			}
		}
	}
	
	return bIsWidgetValid ? WidgetMode : UE::Widget::EWidgetMode::WM_None;
}

bool FSmartObjectAssetEditorViewportClient::CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const
{
	return	NewMode == UE::Widget::EWidgetMode::WM_Translate
			|| NewMode == UE::Widget::EWidgetMode::WM_TranslateRotateZ
			|| NewMode == UE::Widget::EWidgetMode::WM_Rotate;
}

void FSmartObjectAssetEditorViewportClient::SetWidgetMode(UE::Widget::EWidgetMode NewMode)
{
	FEditorViewportClient::SetWidgetMode(NewMode);
	WidgetMode = NewMode;
}

void FSmartObjectAssetEditorViewportClient::SetWidgetCoordSystemSpace(ECoordSystem NewCoordSystem)
{
	// Empty, we support only world for the time being.
}

void FSmartObjectAssetEditorViewportClient::BeginTransaction(FText Text)
{
	if (ScopedTransaction)
	{
		ScopedTransaction->Cancel();
		delete ScopedTransaction;
		ScopedTransaction = nullptr;
	}

	ScopedTransaction = new FScopedTransaction(Text);
	check(ScopedTransaction);
}

void FSmartObjectAssetEditorViewportClient::EndTransaction()
{
	if (ScopedTransaction)
	{
		delete ScopedTransaction;
		ScopedTransaction = nullptr;
	}
}

void FSmartObjectAssetEditorViewportClient::TrackingStarted(const struct FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge)
{
	if (!bIsManipulating && bIsDraggingWidget)
	{
		bIsManipulating = true;

		// Begin transaction
		BeginTransaction(LOCTEXT("ModifySlots", "Modify Slots(s)"));
		bIsManipulating = true;

	}
}

void FSmartObjectAssetEditorViewportClient::TrackingStopped()
{
	if (bIsManipulating)
	{
		// End transaction
		bIsManipulating = false;
		EndTransaction();
	}
}

bool FSmartObjectAssetEditorViewportClient::InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale)
{
	USmartObjectDefinition* Definition = SmartObjectDefinition.Get();
	if (Definition == nullptr)
	{
		return false;
	}

	if (!ViewModel.IsValid())
	{
		return false;
	}

	Definition->SetFlags(RF_Transactional);
	Definition->Modify();

	bool bResult = false;

	// Update the cached location so that the widget moves during drag.
	CachedWidgetLocation += Drag;
	
	if (bIsManipulating && CurrentAxis != EAxisList::None)
	{
		const FTransform OwnerLocalToWorld = FTransform::Identity;
		const TArrayView<FSmartObjectSlotDefinition> SlotDefinitions = Definition->GetMutableSlots();
		
		for (int32 Index = 0; Index < Definition->GetSlots().Num(); ++Index)
		{
			FSmartObjectSlotDefinition& SlotDefinition = SlotDefinitions[Index];
			const FTransform SlotTransform = Definition->GetSlotWorldTransform(Index, OwnerLocalToWorld);
			
			// Do not move annotations if slot is selected, or else, the annotation will get the adjustment in double (assumes annotations are generally relative to slot).
			if (ViewModel->IsSelected(SlotDefinition.ID))
			{
				FVector SlotDrag = Drag;
				if (!Rot.IsZero())
				{
					// Rotate around gizmo pivot
					const FVector SlotOffset = SlotTransform.GetTranslation() - CachedWidgetLocation;
					if (!SlotOffset.IsNearlyZero())
					{
						const FVector RotatedSlotOffset = Rot.RotateVector(SlotOffset);
						SlotDrag += RotatedSlotOffset - SlotOffset;
					}
				}
				check(OwnerLocalToWorld.EqualsNoScale(FTransform::Identity));
				if (!SlotDrag.IsZero())
				{
					SlotDefinition.Offset += FVector3f(SlotDrag);
				}
				if (!Rot.IsZero())
				{
					SlotDefinition.Rotation += FRotator3f(Rot);
					SlotDefinition.Rotation.Normalize();
				}
			}
			else
			{
				for (FSmartObjectDefinitionDataProxy& DataProxy : SlotDefinition.DefinitionData)
				{
					if (FSmartObjectSlotAnnotation* Annotation = DataProxy.Data.GetMutablePtr<FSmartObjectSlotAnnotation>())
					{
						if (ViewModel->IsSelected(DataProxy.ID))
						{
							const FTransform AnnotationTransform = Annotation->GetAnnotationWorldTransform(SlotTransform);
							FVector AnnotationDrag = Drag;
							if (!Rot.IsZero())
							{
								// Rotate around gizmo pivot
								const FVector AnnotationOffset = AnnotationTransform.GetTranslation() - CachedWidgetLocation;
								if (!AnnotationOffset.IsNearlyZero())
								{
									const FVector RotatedSlotOffset = Rot.RotateVector(AnnotationOffset);
									AnnotationDrag += RotatedSlotOffset - AnnotationOffset;
								}
							}

							Annotation->AdjustWorldTransform(SlotTransform, AnnotationDrag, Rot);
						}
					}
				}
			}

			bResult = true;
		}
	}
	
	return bResult;
}

void FSmartObjectAssetEditorViewportClient::RemoveViewModelDelegates()
{
	if (ViewModel.IsValid()
		&& SelectionChangedHandle.IsValid())
	{
		ViewModel->GetOnSelectionChanged().Remove(SelectionChangedHandle);
	}
	
	SelectionChangedHandle.Reset();
}

void FSmartObjectAssetEditorViewportClient::SetViewModel(TSharedPtr<FSmartObjectViewModel> InViewModel)
{
	RemoveViewModelDelegates();
	
	ViewModel = InViewModel;

	if (ViewModel)
	{
		SelectionChangedHandle = ViewModel->GetOnSelectionChanged().AddRaw(this, &FSmartObjectAssetEditorViewportClient::HandleSelectionChanged);
	}
}

void FSmartObjectAssetEditorViewportClient::HandleSelectionChanged(TConstArrayView<FGuid> Selection)
{
	RedrawAllViewportsIntoThisScene();
}

void FSmartObjectAssetEditorViewportClient::SetSmartObjectDefinition(USmartObjectDefinition& InDefinition)
{
	SmartObjectDefinition = &InDefinition;
	if (Viewport)
	{
		FocusViewportOnBox(GetPreviewBounds());
	}
}

void FSmartObjectAssetEditorViewportClient::ResetPreviewActor()
{
	if (AActor* Actor = PreviewActor.Get())
	{
		PreviewScene->GetWorld()->DestroyActor(Actor);
		PreviewActor.Reset();
	}
}

void FSmartObjectAssetEditorViewportClient::SetPreviewMesh(UStaticMesh* InStaticMesh)
{
	ResetPreviewActor();
	
	AActor* Actor = PreviewScene->GetWorld()->SpawnActor<AActor>();
	UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Actor->AddComponentByClass(UStaticMeshComponent::StaticClass(), true, FTransform::Identity, false));
	if (MeshComponent)
	{
		MeshComponent->SetStaticMesh(InStaticMesh);
	}
	
	PreviewActor = Actor;

	FocusViewportOnBox(GetPreviewBounds());
}

void FSmartObjectAssetEditorViewportClient::SetPreviewActor(AActor* InActor)
{
	ResetPreviewActor();

	if (InActor != nullptr)
	{
		PreviewActor = PreviewScene->GetWorld()->SpawnActor(InActor->GetClass());
	}

	FocusViewportOnBox(GetPreviewBounds());
}

void FSmartObjectAssetEditorViewportClient::SetPreviewActorClass(const UClass* ActorClass)
{
	ResetPreviewActor();

	if (ActorClass != nullptr)
	{
		PreviewActor = PreviewScene->GetWorld()->SpawnActor(const_cast<UClass*>(ActorClass));
	}

	FocusViewportOnBox(GetPreviewBounds());
}

FBox FSmartObjectAssetEditorViewportClient::GetPreviewBounds() const
{
	FBoxSphereBounds::Builder Bounds;
	if (const AActor* Actor = PreviewActor.Get())
	{
		Bounds += Actor->GetComponentsBoundingBox();
	}

	const TSharedRef<const FSmartObjectAssetToolkit> Toolkit = AssetEditorToolkit.Pin().ToSharedRef();
	const TArray< UObject* >* EditedObjects = Toolkit->GetObjectsCurrentlyBeingEdited();
	if (EditedObjects != nullptr)
	{
		for (const UObject* EditedObject : *EditedObjects)
		{
			const USmartObjectDefinition* Definition = Cast<USmartObjectDefinition>(EditedObject);
			if (IsValid(Definition))
			{
				Bounds += Definition->GetBounds();
			}
		}
	}

	const FBoxSphereBounds DefaultBounds(FSphere(FVector::ZeroVector, 100.f));
	return Bounds.IsValid() ? FBoxSphereBounds(Bounds).GetBox() : DefaultBounds.GetBox();
}

TConstArrayView<FGuid> FSmartObjectAssetEditorViewportClient::GetSelection() const
{
	if (ViewModel.IsValid())
	{
		return ViewModel->GetSelection();
	}
	return {};
}

#undef LOCTEXT_NAMESPACE
