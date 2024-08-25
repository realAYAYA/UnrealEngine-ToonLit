// Copyright Epic Games, Inc. All Rights Reserved.

#include "Annotations/SmartObjectSlotEntranceAnnotation.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectVisualizationContext.h"
#include "SceneManagement.h" // FPrimitiveDrawInterface
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AI/Navigation/NavAgentInterface.h"
#include "SmartObjectUserComponent.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerCategory.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectSlotEntranceAnnotation)

namespace UE::SmartObject::Annotations
{
	const ANavigationData* GetDefaultNavData(const UWorld& World)
	{
		const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
		if (!NavSys)
		{
			return nullptr;
		}
		return NavSys->GetDefaultNavDataInstance();
	}

	const ANavigationData* GetNavDataForActor(const UWorld& World, const AActor* UserActor)
	{
		if (const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World))
		{
			const INavAgentInterface* NavAgent = Cast<INavAgentInterface>(UserActor);
			if (NavAgent)
			{
				const FNavAgentProperties& NavAgentProps = NavAgent->GetNavAgentPropertiesRef();
				return NavSys->GetNavDataForProps(NavAgentProps, NavAgent->GetNavAgentLocation());
			}
			return NavSys->GetDefaultNavDataInstance();
		}
				
		return nullptr;
	}

	bool ProjectNavigationLocation(const ANavigationData& NavData, const FVector Location, const FBox& SearchBounds, const FSharedConstNavQueryFilter& NavigationFilter, const AActor* InstigatorActor, FNavLocation& OutNavLocation)
	{
		return NavData.ProjectPoint(Location, OutNavLocation, SearchBounds.GetExtent(), NavigationFilter, InstigatorActor)
			&& OutNavLocation.HasNodeRef()
			&& SearchBounds.IsInsideOrOn(OutNavLocation.Location);
	}

	bool TraceGroundLocation(const UWorld& World, const FVector Location, const FBox& SearchBounds, const FSmartObjectTraceParams& TraceParameters, const FCollisionQueryParams& CollisionQueryParams, FVector& OutGroundLocation)
	{
		const FVector TraceStart(Location.X, Location.Y, SearchBounds.Max.Z);
		const FVector TraceEnd(Location.X, Location.Y, SearchBounds.Min.Z);

		FHitResult HitResult;
		bool bHasHit = false;

		if (TraceParameters.Type == ESmartObjectTraceType::ByChannel)
		{
			bHasHit = World.LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, UEngineTypes::ConvertToCollisionChannel(TraceParameters.TraceChannel), CollisionQueryParams);
		}
		else if (TraceParameters.Type == ESmartObjectTraceType::ByProfile)
		{
			bHasHit = World.LineTraceSingleByProfile(HitResult, TraceStart, TraceEnd, TraceParameters.CollisionProfile.Name, CollisionQueryParams);
		}
		else if (TraceParameters.Type == ESmartObjectTraceType::ByObjectTypes)
		{
			const FCollisionObjectQueryParams ObjectQueryParams(TraceParameters.ObjectTypes);
			bHasHit = World.LineTraceSingleByObjectType(HitResult, TraceStart, TraceEnd, ObjectQueryParams, CollisionQueryParams);
		}

		if (bHasHit)
		{
			OutGroundLocation = HitResult.Location;
			return true;
		}

		return false;
	}

	bool TestCollidersOverlap(const UWorld& World, TConstArrayView<FSmartObjectAnnotationCollider> Colliders, const FSmartObjectTraceParams& TraceParameters, const FCollisionQueryParams& CollisionQueryParams)
	{
		bool bHasHit = false;

		TArray<FOverlapResult> Overlaps;

		for (const FSmartObjectAnnotationCollider& Collider : Colliders)
		{
			Overlaps.Reset();
			if (TraceParameters.Type == ESmartObjectTraceType::ByChannel)
			{
				bHasHit = World.OverlapMultiByChannel(Overlaps, Collider.Location, Collider.Rotation, UEngineTypes::ConvertToCollisionChannel(TraceParameters.TraceChannel), Collider.CollisionShape, CollisionQueryParams);
			}
			else if (TraceParameters.Type == ESmartObjectTraceType::ByProfile)
			{
				bHasHit = World.OverlapMultiByProfile(Overlaps, Collider.Location, Collider.Rotation, TraceParameters.CollisionProfile.Name, Collider.CollisionShape, CollisionQueryParams);
			}
			else if (TraceParameters.Type == ESmartObjectTraceType::ByObjectTypes)
			{
				// Overlap tests with object types will only ever return non-blocking results (due to historical reasons), so using the any variant here (blocking does not exist).
				const FCollisionObjectQueryParams ObjectQueryParams(TraceParameters.ObjectTypes);
				bHasHit = World.OverlapMultiByObjectType(Overlaps, Collider.Location, Collider.Rotation, ObjectQueryParams, Collider.CollisionShape, CollisionQueryParams);
			}

			if (bHasHit)
			{
				break;
			}
		}

		return bHasHit;
	}

	static constexpr FColor EntryColor(0, 64, 192);
	static constexpr FColor CollisionColor(255, 255, 255, 128);
	static constexpr FColor InvalidEntryColor(192, 32, 16);

	struct FVisualizationData
	{
		FVector SlotLocation;
		FVector MarkerLocation;
		FVector MarkerAxisX;
		FVector MarkerAxisY;
		FVector ValidatedLocation;
		FVector GroundLocationStart;
		FVector GroundLocationEnd;
		TArray<FSmartObjectAnnotationCollider> Colliders;
		FLinearColor MarkerColor = EntryColor;
		FLinearColor ColliderColor = CollisionColor;
	};

	void UpdateVisualizationLogic(const UWorld& World, const FSmartObjectSlotEntranceAnnotation& Annotation, const FTransform& SlotTransform, const FTransform& AnnotationTransform,
									const AActor* PreviewSmartObjectActor, const AActor* PreviewUserActor, TSubclassOf<USmartObjectSlotValidationFilter> PreviewValidationFilterClass, FVisualizationData& OutData)
	{
		const USmartObjectSlotValidationFilter* PreviewValidationFilter = PreviewValidationFilterClass.GetDefaultObject();
		if (!PreviewValidationFilter)
		{
			PreviewValidationFilter = GetDefault<USmartObjectSlotValidationFilter>(); 
		}
		
		const ANavigationData* DefaultNavData = GetDefaultNavData(World);

		const FSmartObjectSlotValidationParams& ValidationParams = PreviewValidationFilter->GetEntryValidationParams(); 
		const FSmartObjectTraceParams& GroundTraceParameters = ValidationParams.GetGroundTraceParameters();
		const FSmartObjectTraceParams& TransitionTraceParameters = ValidationParams.GetTransitionTraceParameters();

		FCollisionQueryParams GroundTraceQueryParams(SCENE_QUERY_STAT(SmartObjectTrace), GroundTraceParameters.bTraceComplex);
		FCollisionQueryParams TransitionTraceQueryParams(SCENE_QUERY_STAT(SmartObjectTrace), TransitionTraceParameters.bTraceComplex);

		GroundTraceQueryParams.bIgnoreTouches = true;
		TransitionTraceQueryParams.bIgnoreTouches = true;
		
		if (PreviewSmartObjectActor)
		{
			GroundTraceQueryParams.AddIgnoredActor(PreviewSmartObjectActor);
			TransitionTraceQueryParams.AddIgnoredActor(PreviewSmartObjectActor);
		}
		if (PreviewUserActor)
		{
			GroundTraceQueryParams.AddIgnoredActor(PreviewUserActor);
			TransitionTraceQueryParams.AddIgnoredActor(PreviewUserActor);
		}

		const FVector AnnotationWorldLocation = AnnotationTransform.GetTranslation();
		const FVector AxisX = AnnotationTransform.GetUnitAxis(EAxis::X);
		const FVector AxisY = AnnotationTransform.GetUnitAxis(EAxis::Y);

		OutData.SlotLocation = SlotTransform.GetLocation();
		OutData.MarkerColor = EntryColor;
		OutData.ColliderColor = CollisionColor;

		OutData.MarkerLocation = AnnotationWorldLocation;
		OutData.MarkerAxisX = AxisX;
		OutData.MarkerAxisY = AxisY;

		const FBox SearchBounds(AnnotationWorldLocation - ValidationParams.GetSearchExtents(), AnnotationWorldLocation + ValidationParams.GetSearchExtents());

		OutData.ValidatedLocation = AnnotationWorldLocation;

		// Validate location on navmesh
		// @todo: add visualization for missing navdata.
		if (DefaultNavData)
		{
			FSharedConstNavQueryFilter NavigationFilter;
			if (ValidationParams.GetNavigationFilter())
			{
				NavigationFilter = UNavigationQueryFilter::GetQueryFilter(*DefaultNavData, nullptr, ValidationParams.GetNavigationFilter());
			}

			FNavLocation NavLocation;
			if (ProjectNavigationLocation(*DefaultNavData, AnnotationWorldLocation, SearchBounds, NavigationFilter, /*RequesterActor*/nullptr, NavLocation))
			{
				OutData.ValidatedLocation = NavLocation.Location;
			}
			else
			{
				OutData.MarkerColor = InvalidEntryColor;
			}
		}

		// Try to trace the slot on location on ground.
		if (Annotation.bTraceGroundLocation)
		{
			FVector GroundLocation;
			if (TraceGroundLocation(World, OutData.ValidatedLocation, SearchBounds, GroundTraceParameters, GroundTraceQueryParams, GroundLocation))
			{
				OutData.ValidatedLocation = GroundLocation;
			}
			else
			{
				OutData.MarkerColor = InvalidEntryColor;
			}
		}

		if (Annotation.bCheckTransitionTrajectory)
		{
			Annotation.GetTrajectoryColliders(SlotTransform, OutData.Colliders);
			if (TestCollidersOverlap(World, OutData.Colliders, TransitionTraceParameters, TransitionTraceQueryParams))
			{
				OutData.ColliderColor = InvalidEntryColor;
			}
		}
		
		OutData.GroundLocationStart = FVector(OutData.ValidatedLocation.X, OutData.ValidatedLocation.Y, FMath::Max(OutData.ValidatedLocation.Z, OutData.MarkerLocation.Z));
		OutData.GroundLocationEnd = FVector(OutData.ValidatedLocation.X, OutData.ValidatedLocation.Y, FMath::Min(OutData.ValidatedLocation.Z, OutData.MarkerLocation.Z));
	}
} // UE::SmartObject::Annotations

#if WITH_EDITOR

void FSmartObjectSlotEntranceAnnotation::DrawVisualization(FSmartObjectVisualizationContext& VisContext) const
{
	constexpr FVector::FReal MarkerRadius = 20.0;
	constexpr FVector::FReal TickSize = 5.0;
	constexpr FVector::FReal MinArrowDrawDistance = 20.0;
	constexpr float DepthBias = 2.0f;
	constexpr bool ScreenSpace = true;
	
	const FTransform SlotTransform = VisContext.Definition.GetSlotWorldTransform(VisContext.SlotIndex, VisContext.OwnerLocalToWorld);
	const FTransform AnnotationTransform = GetAnnotationWorldTransform(SlotTransform);

	UE::SmartObject::Annotations::FVisualizationData Data;
	UE::SmartObject::Annotations::UpdateVisualizationLogic(VisContext.World, *this, SlotTransform, AnnotationTransform,
		VisContext.PreviewActor, nullptr, VisContext.PreviewValidationFilterClass, Data);

	// Draw validated location in relation to the marker locations.
	if (FVector::Distance(Data.MarkerLocation, Data.ValidatedLocation) > UE_KINDA_SMALL_NUMBER)
	{
		VisContext.PDI->DrawTranslucentLine(Data.GroundLocationStart, Data.GroundLocationEnd, Data.MarkerColor, SDPG_World, 1.0f, DepthBias, ScreenSpace);
		VisContext.PDI->DrawTranslucentLine(Data.ValidatedLocation - Data.MarkerAxisX * TickSize * 0.5, Data.ValidatedLocation + Data.MarkerAxisX * TickSize * 0.5, Data.MarkerColor, SDPG_World, 1.0f, DepthBias, ScreenSpace);
		VisContext.PDI->DrawTranslucentLine(Data.ValidatedLocation - Data.MarkerAxisY * TickSize * 0.5, Data.ValidatedLocation + Data.MarkerAxisY * TickSize * 0.5, Data.MarkerColor, SDPG_World, 1.0f, DepthBias, ScreenSpace);
	}
	
	if (VisContext.bIsAnnotationSelected)
	{
		Data.MarkerColor = VisContext.SelectedColor;
	}

	if (bIsEntry)
	{
		const FVector V0 = Data.MarkerLocation + Data.MarkerAxisX * MarkerRadius;
		const FVector V1 = Data.MarkerLocation + Data.MarkerAxisX * MarkerRadius * 0.25 + Data.MarkerAxisY * MarkerRadius;
		const FVector V2 = Data.MarkerLocation + Data.MarkerAxisX * MarkerRadius * 0.25 - Data.MarkerAxisY * MarkerRadius;
		const FVector V3 = Data.MarkerLocation + Data.MarkerAxisY * MarkerRadius;
		const FVector V4 = Data.MarkerLocation - Data.MarkerAxisY * MarkerRadius;
		VisContext.PDI->DrawTranslucentLine(V0, V1, Data.MarkerColor, SDPG_World, 4.0f, DepthBias, ScreenSpace);
		VisContext.PDI->DrawTranslucentLine(V0, V2, Data.MarkerColor, SDPG_World, 4.0f, DepthBias, ScreenSpace);
		VisContext.PDI->DrawTranslucentLine(V1, V3, Data.MarkerColor, SDPG_World, 4.0f, DepthBias, ScreenSpace);
		VisContext.PDI->DrawTranslucentLine(V2, V4, Data.MarkerColor, SDPG_World, 4.0f, DepthBias, ScreenSpace);
	}

	if (bIsExit)
	{
		const FVector V1 = Data.MarkerLocation - Data.MarkerAxisX * MarkerRadius * 0.75 + Data.MarkerAxisY * MarkerRadius;
		const FVector V2 = Data.MarkerLocation - Data.MarkerAxisX * MarkerRadius * 0.75 - Data.MarkerAxisY * MarkerRadius;
		const FVector V3 = Data.MarkerLocation + Data.MarkerAxisY * MarkerRadius;
		const FVector V4 = Data.MarkerLocation - Data.MarkerAxisY * MarkerRadius;
		VisContext.PDI->DrawTranslucentLine(V1, V2, Data.MarkerColor, SDPG_World, 4.0f, DepthBias, ScreenSpace);
		VisContext.PDI->DrawTranslucentLine(V1, V3, Data.MarkerColor, SDPG_World, 4.0f, DepthBias, ScreenSpace);
		VisContext.PDI->DrawTranslucentLine(V2, V4, Data.MarkerColor, SDPG_World, 4.0f, DepthBias, ScreenSpace);
	}

	if (!Data.Colliders.IsEmpty())
	{
		for (const FSmartObjectAnnotationCollider& Collider : Data.Colliders)
		{
			if (Collider.CollisionShape.IsCapsule())
			{
				DrawWireCapsule(VisContext.PDI, Collider.Location, Collider.Rotation.GetAxisX(), Collider.Rotation.GetAxisY(), Collider.Rotation.GetAxisZ(),
					Data.ColliderColor, Collider.CollisionShape.GetCapsuleRadius(),  Collider.CollisionShape.GetCapsuleHalfHeight(), 12, SDPG_World, 1.0f, DepthBias, ScreenSpace);
			}
			else if (Collider.CollisionShape.IsBox())
			{
				DrawOrientedWireBox(VisContext.PDI, Collider.Location, Collider.Rotation.GetAxisX(), Collider.Rotation.GetAxisY(), Collider.Rotation.GetAxisZ(),
					Collider.CollisionShape.GetExtent(), Data.ColliderColor, SDPG_World, 1.0f, DepthBias, ScreenSpace); 
					
			}
			else if (Collider.CollisionShape.IsSphere())
			{
				DrawWireSphere(VisContext.PDI, Collider.Location, Data.ColliderColor, Collider.CollisionShape.GetSphereRadius(), 12, SDPG_World, 1.0f, DepthBias, ScreenSpace);
			}
		}
	}
	
	// Tick at the center.
	VisContext.PDI->DrawTranslucentLine(Data.MarkerLocation - Data.MarkerAxisX * TickSize, Data.MarkerLocation + Data.MarkerAxisX * TickSize, Data.MarkerColor, SDPG_World, 1.0f, DepthBias, ScreenSpace);
	VisContext.PDI->DrawTranslucentLine(Data.MarkerLocation - Data.MarkerAxisY * TickSize, Data.MarkerLocation + Data.MarkerAxisY * TickSize, Data.MarkerColor, SDPG_World, 1.0f, DepthBias, ScreenSpace);

	// Arrow pointing at the the slot, if far enough from the slot.
	if (FVector::DistSquared(Data.MarkerLocation, Data.SlotLocation) > FMath::Square(MinArrowDrawDistance))
	{
		VisContext.DrawArrow(Data.MarkerLocation, Data.SlotLocation, Data.MarkerColor, 5.0f, 5.0f, /*DepthPrioGroup*/0, /*Thickness*/1.0f, DepthBias, ScreenSpace);
	}
}

void FSmartObjectSlotEntranceAnnotation::DrawVisualizationHUD(FSmartObjectVisualizationContext& VisContext) const
{
	// @todo: move this into a setting.
	constexpr FVector::FReal MaxDrawDistance = 1500.0;
	constexpr FVector::FReal FadeDrawDistance = MaxDrawDistance * 0.75;

	const FTransform SlotTransform = VisContext.Definition.GetSlotWorldTransform(VisContext.SlotIndex, VisContext.OwnerLocalToWorld);
	const FTransform AnnotationTransform = GetAnnotationWorldTransform(SlotTransform);
	const FVector AnnotationWorldLocation = AnnotationTransform.GetTranslation();

	const FVector::FReal Distance = VisContext.GetDistanceToCamera(AnnotationWorldLocation);
	if (Distance < MaxDrawDistance)
	{
		FString Text = FString::Printf(TEXT("S%d NAV%d \n"), VisContext.SlotIndex, VisContext.AnnotationIndex);
		if (SelectionPriority != ESmartObjectEntrancePriority::Normal)
		{
			Text += UEnum::GetDisplayValueAsText(SelectionPriority).ToString();
		}
		if (Tags.IsValid())
		{
			Text += Tags.ToString();
		}
		
		FLinearColor Color = FLinearColor::White;
		Color.A = static_cast<float>(FMath::Clamp(1.0 - (Distance - FadeDrawDistance) / (MaxDrawDistance - FadeDrawDistance), 0.0, 1.0));
		
		VisContext.DrawString(AnnotationWorldLocation, *Text, Color);
	}
}

void FSmartObjectSlotEntranceAnnotation::AdjustWorldTransform(const FTransform& SlotTransform, const FVector& DeltaTranslation, const FRotator& DeltaRotation)
{
	if (!DeltaTranslation.IsZero())
	{
		const FVector LocalTranslation = SlotTransform.InverseTransformVector(DeltaTranslation);
		Offset += FVector3f(LocalTranslation);
	}

	if (!DeltaRotation.IsZero())
	{
		const FRotator3f LocalRotation = FRotator3f(SlotTransform.InverseTransformRotation(DeltaRotation.Quaternion()).Rotator());
		Rotation += LocalRotation;
		Rotation.Normalize();
	}
}

#endif // WITH_EDITOR

void FSmartObjectSlotEntranceAnnotation::GetTrajectoryColliders(const FTransform& SlotTransform, TArray<FSmartObjectAnnotationCollider>& OutColliders) const
{
	const FVector SlotWorldLocation = SlotTransform.GetLocation();
	const FVector AnnotationWorldLocation = SlotTransform.TransformPosition(FVector(Offset));

	const FVector SegmentStart = AnnotationWorldLocation + FVector(0, 0, TrajectoryStartHeightOffset);
	const FVector SegmentEnd = SlotWorldLocation + FVector(0, 0, TrajectorySlotHeightOffset);
	const FVector Center = (SegmentStart + SegmentEnd) * 0.5;
	const FVector Dir = SegmentEnd - SegmentStart;

	FVector Up;
	float Length = 0.0f;
	Dir.ToDirectionAndLength(Up, Length);

	FSmartObjectAnnotationCollider& Collider = OutColliders.AddDefaulted_GetRef();
	Collider.Location = Center;
	Collider.Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Up);
	Collider.CollisionShape = FCollisionShape::MakeCapsule(TransitionCheckRadius, Length * 0.5f + TransitionCheckRadius);
}

FTransform FSmartObjectSlotEntranceAnnotation::GetAnnotationWorldTransform(const FTransform& SlotTransform) const
{
	const FTransform LocalTransform = FTransform(FRotator(Rotation), FVector(Offset));
	return LocalTransform * SlotTransform;
}

FVector FSmartObjectSlotEntranceAnnotation::GetWorldLocation(const FTransform& SlotTransform) const
{
	return SlotTransform.TransformPosition(FVector(Offset));
}

FRotator FSmartObjectSlotEntranceAnnotation::GetWorldRotation(const FTransform& SlotTransform) const
{
	return SlotTransform.TransformRotation(FQuat(Rotation.Quaternion())).Rotator();
}


#if WITH_EDITORONLY_DATA
void FSmartObjectSlotEntranceAnnotation::PostSerialize(const FArchive& Ar)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (Tag_DEPRECATED.IsValid())
	{
		Tags.AddTag(Tag_DEPRECATED);
		Tag_DEPRECATED = FGameplayTag();
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif

#if WITH_GAMEPLAY_DEBUGGER
void FSmartObjectSlotEntranceAnnotation::CollectDataForGameplayDebugger(FSmartObjectAnnotationGameplayDebugContext& DebugContext) const
{
	constexpr FVector::FReal MarkerRadius = 20.0;
	constexpr FVector::FReal TickSize = 5.0;
	constexpr FVector::FReal MinArrowDrawDistance = 20.0;

	const UWorld* World = DebugContext.Category.GetWorldFromReplicator();
	if (!World)
	{
		return;
	}
	
	const FTransform AnnotationTransform = GetAnnotationWorldTransform(DebugContext.SlotTransform);

	TSubclassOf<USmartObjectSlotValidationFilter> ValidationFilterClass;
	if (DebugContext.DebugActor)
	{
		// If user actor is present, try to query some data automatically from interfaces and components. 
		if (!ValidationFilterClass.Get())
		{
			if (const USmartObjectUserComponent* UserComponent = DebugContext.DebugActor->GetComponentByClass<USmartObjectUserComponent>())
			{
				if (UserComponent->GetValidationFilter().Get())
				{
					ValidationFilterClass = UserComponent->GetValidationFilter();
				}
			}
		}
	}

	UE::SmartObject::Annotations::FVisualizationData Data;
	UE::SmartObject::Annotations::UpdateVisualizationLogic(*World, *this, DebugContext.SlotTransform, AnnotationTransform,
		DebugContext.SmartObjectOwnerActor, DebugContext.DebugActor, ValidationFilterClass, Data);

	// Draw validated location in relation to the marker locations.
	if (FVector::Distance(Data.MarkerLocation, Data.ValidatedLocation) > UE_KINDA_SMALL_NUMBER)
	{
		DebugContext.Category.AddShape(FGameplayDebuggerShape::MakeSegmentList( {
				Data.GroundLocationStart, Data.GroundLocationEnd,
				Data.ValidatedLocation - Data.MarkerAxisX * TickSize * 0.5, Data.ValidatedLocation + Data.MarkerAxisX * TickSize * 0.5,
				Data.ValidatedLocation - Data.MarkerAxisY * TickSize * 0.5, Data.ValidatedLocation + Data.MarkerAxisY * TickSize * 0.5
			}, 1.0f, Data.MarkerColor.ToFColor(/*bSRGB*/true)));
	}
	
	if (bIsEntry)
	{
		DebugContext.Category.AddShape(FGameplayDebuggerShape::MakePolyline({
				Data.MarkerLocation + Data.MarkerAxisY * MarkerRadius,
				Data.MarkerLocation + Data.MarkerAxisX * MarkerRadius * 0.25 + Data.MarkerAxisY * MarkerRadius,
				Data.MarkerLocation + Data.MarkerAxisX * MarkerRadius,
				Data.MarkerLocation + Data.MarkerAxisX * MarkerRadius * 0.25 - Data.MarkerAxisY * MarkerRadius,
				Data.MarkerLocation - Data.MarkerAxisY * MarkerRadius
			}, 4.0f, Data.MarkerColor.ToFColor(/*bSRGB*/true)));
	}

	if (bIsExit)
	{
		DebugContext.Category.AddShape(FGameplayDebuggerShape::MakePolyline({
			Data.MarkerLocation + Data.MarkerAxisY * MarkerRadius,
			Data.MarkerLocation - Data.MarkerAxisX * MarkerRadius * 0.5 + Data.MarkerAxisY * MarkerRadius,
			Data.MarkerLocation - Data.MarkerAxisX * MarkerRadius * 0.5 - Data.MarkerAxisY * MarkerRadius,
			Data.MarkerLocation - Data.MarkerAxisY * MarkerRadius
		}, 4.0f, Data.MarkerColor.ToFColor(/*bSRGB*/true)));
	}

	if (!Data.Colliders.IsEmpty())
	{
		for (const FSmartObjectAnnotationCollider& Collider : Data.Colliders)
		{
			if (Collider.CollisionShape.IsCapsule())
			{
				DebugContext.Category.AddShape(FGameplayDebuggerShape::MakeCapsule(Collider.Location, Collider.Rotation.Rotator(), Collider.CollisionShape.GetCapsuleRadius(), Collider.CollisionShape.GetCapsuleHalfHeight(), Data.ColliderColor.ToFColor(/*bSRGB*/true)));
			}
			else if (Collider.CollisionShape.IsBox())
			{
				DebugContext.Category.AddShape(FGameplayDebuggerShape::MakeBox(Collider.Location, Collider.Rotation.Rotator(), Collider.CollisionShape.GetExtent(), Data.ColliderColor.ToFColor(/*bSRGB*/true)));
			}
			else if (Collider.CollisionShape.IsSphere())
			{
				DebugContext.Category.AddShape(FGameplayDebuggerShape::MakePoint(Collider.Location, Collider.CollisionShape.GetSphereRadius(), Data.ColliderColor.ToFColor(/*bSRGB*/true)));
			}
		}
	}
	
	// Tick at the center.
	DebugContext.Category.AddShape(FGameplayDebuggerShape::MakeSegmentList( {
			Data.MarkerLocation - Data.MarkerAxisX * TickSize,
			Data.MarkerLocation + Data.MarkerAxisX * TickSize,
			Data.MarkerLocation - Data.MarkerAxisY * TickSize,
			Data.MarkerLocation + Data.MarkerAxisY * TickSize
		}, 1.0f, Data.MarkerColor.ToFColor(/*bSRGB*/true)));

	// Arrow pointing at the the slot, if far enough from the slot.
	if (FVector::DistSquared(Data.MarkerLocation, Data.SlotLocation) > FMath::Square(MinArrowDrawDistance))
	{
		DebugContext.Category.AddShape(FGameplayDebuggerShape::MakeSegment(Data.MarkerLocation, Data.SlotLocation, 1.0f, Data.MarkerColor.ToFColor(/*bSRGB*/true)));
	}

}
#endif // WITH_GAMEPLAY_DEBUGGER	
