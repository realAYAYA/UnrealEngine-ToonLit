// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerCategory_SmartObject.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectAnnotation.h"
#include "Math/ColorList.h"
#include "Engine/World.h"
#include "Misc/EnumerateRange.h"

#if WITH_GAMEPLAY_DEBUGGER && WITH_SMARTOBJECT_DEBUG

FGameplayDebuggerCategory_SmartObject::FGameplayDebuggerCategory_SmartObject()
{
	SetDataPackReplication<FReplicationData>(&DataPack, EGameplayDebuggerDataPack::Persistent);

	bShowOnlyWithDebugActor = false;

	const FGameplayDebuggerInputHandlerConfig InstanceTagsKeyConfig(TEXT("ToggleInstanceTags"), EKeys::Add.GetFName(), FGameplayDebuggerInputModifier::Shift);
	BindKeyPress(InstanceTagsKeyConfig, this, &FGameplayDebuggerCategory_SmartObject::ToggleInstanceTags, EGameplayDebuggerInputMode::Replicated);

	const FGameplayDebuggerInputHandlerConfig SlotDetailsKeyConfig(TEXT("ToggleSlotDetails"), EKeys::Multiply.GetFName(), FGameplayDebuggerInputModifier::Shift);
	BindKeyPress(SlotDetailsKeyConfig, this, &FGameplayDebuggerCategory_SmartObject::ToggleSlotDetails, EGameplayDebuggerInputMode::Replicated);

	const FGameplayDebuggerInputHandlerConfig AnnotationsKeyConfig(TEXT("ToggleAnnotations"), EKeys::Subtract.GetFName(), FGameplayDebuggerInputModifier::Shift);
	BindKeyPress(AnnotationsKeyConfig, this, &FGameplayDebuggerCategory_SmartObject::ToggleAnnotations, EGameplayDebuggerInputMode::Replicated);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_SmartObject::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_SmartObject());
}

void FGameplayDebuggerCategory_SmartObject::ToggleInstanceTags()
{
	DataPack.bDisplayInstanceTags ^= true;
	MarkRenderStateDirty();
}

void FGameplayDebuggerCategory_SmartObject::ToggleSlotDetails()
{
	DataPack.bDisplaySlotDetails ^= true;
	if (!DataPack.bDisplaySlotDetails)
	{
		// Disabling SlotDetails also disables Annotations
		DataPack.bDisplayAnnotations = false;
	}

	MarkRenderStateDirty();
}

void FGameplayDebuggerCategory_SmartObject::ToggleAnnotations()
{
	DataPack.bDisplayAnnotations ^= true;
	if (DataPack.bDisplayAnnotations)
	{
		// Enabling Annotations requires SlotDetails
		DataPack.bDisplaySlotDetails = true;
	}
	
	MarkRenderStateDirty();
}

void FGameplayDebuggerCategory_SmartObject::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	UWorld* World = GetDataWorld(OwnerPC, DebugActor);
	check(World);

	USmartObjectSubsystem* Subsystem = World->GetSubsystem<USmartObjectSubsystem>();
	if (Subsystem == nullptr)
	{
		AddTextLine(FString::Printf(TEXT("{Red}SmartObjectSubsystem instance is missing")));
		return;
	}
	
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	bool bApplyCulling = GetViewPoint(OwnerPC, ViewLocation, ViewDirection);

	FColor DebugColor = FColor::Yellow;
	const FColor FreeColor = FColorList::LimeGreen;
	const FColor ClaimedColor = FColorList::Gold;
	const FColor OccupiedColor = FColorList::Red;
	const FColor SlotDisabledColor = FColorList::LightGrey;
	const FColor ObjectDisabledColor = FColorList::Black;

	uint32 NumActiveObjects = 0;
	const TMap<FSmartObjectHandle, FSmartObjectRuntime>& RuntimeSmartObjects = Subsystem->DebugGetRuntimeObjects();
	ensureAlways(IsInGameThread() || IsInParallelGameThread());
	for (auto& RuntimeSmartObjectEntry : RuntimeSmartObjects)
	{
		const FSmartObjectRuntime& SmartObjectRuntime = RuntimeSmartObjectEntry.Value;
		NumActiveObjects += SmartObjectRuntime.IsEnabled() ? 1 : 0;

		// Instance tags or if slot details are not displayed we display a single shape for the whole object
		if (DataPack.bDisplayInstanceTags || !DataPack.bDisplaySlotDetails)
		{
			FVector Location = SmartObjectRuntime.GetTransform().GetLocation();
			if (!bApplyCulling || IsLocationInViewCone(ViewLocation, ViewDirection, Location))
			{
				if (!DataPack.bDisplaySlotDetails)
				{
					AddShape(FGameplayDebuggerShape::MakeBox(Location, FVector(50), /*Thickness*/3, DebugColor));
				}

				if (DataPack.bDisplayInstanceTags)
				{
					FString TagsAsString = SmartObjectRuntime.GetTags().ToStringSimple();
					if (!TagsAsString.IsEmpty())
					{
						// Using small dummy shape to display tags
						AddShape(FGameplayDebuggerShape::MakePoint(Location, /*Radius*/ 1.0f, FColorList::White, TagsAsString));
					}
				}	
			}
		}

		// Slot details following this point, skip if not displayed
		if (!DataPack.bDisplaySlotDetails)
		{
			continue;
		}

		for (TConstEnumerateRef<FSmartObjectRuntimeSlot> RuntimeSlot : EnumerateRange(SmartObjectRuntime.GetSlots()))
		{
			const FTransform SlotTransform = RuntimeSlot->GetSlotWorldTransform(SmartObjectRuntime.GetTransform());

			if (bApplyCulling && !IsLocationInViewCone(ViewLocation, ViewDirection, SlotTransform.GetLocation()))
			{
				continue;
			}

			constexpr float DebugArrowThickness = 2.f;
			constexpr float DebugCircleRadius = 40.f;
			constexpr float DebugArrowHeadSize = 10.f;
			float SlotSize = DebugCircleRadius;
			ESmartObjectSlotShape SlotShape = ESmartObjectSlotShape::Circle;

			const FSmartObjectSlotDefinition& SlotDefinition = SmartObjectRuntime.GetDefinition().GetSlot(RuntimeSlot.GetIndex());

#if WITH_EDITORONLY_DATA
			DebugColor = SlotDefinition.DEBUG_DrawColor;
			SlotShape = SlotDefinition.DEBUG_DrawShape;
			SlotSize = SlotDefinition.DEBUG_DrawSize;
#endif
			const FVector Pos = SlotTransform.GetLocation() + FVector(0.0f, 0.0f, 2.0f);
			const FVector Dir = SlotTransform.GetRotation().GetForwardVector();

			FColor StateColor = FColor::Silver;
			if (!RuntimeSlot->IsEnabled())
			{
				if (SmartObjectRuntime.IsEnabled())
				{
					// Slot is disabled but not the parent object 
					StateColor = SlotDisabledColor;
				}
				else
				{
					// Parent is disabled
					StateColor = ObjectDisabledColor;
					
					// Using small dummy shape to display tags
					FString DisableFlagsAsString(SmartObjectRuntime.DebugGetDisableFlagsString());
					AddShape(FGameplayDebuggerShape::MakePoint(Pos, /*Radius*/ 1.0f, FColorList::White, DisableFlagsAsString));
				}
			}
			else
			{
				switch (RuntimeSlot->GetState())
				{
				case ESmartObjectSlotState::Free:
					StateColor = FreeColor;
					break;
				case ESmartObjectSlotState::Claimed:
					StateColor = ClaimedColor;
					break;
				case ESmartObjectSlotState::Occupied:
					StateColor = OccupiedColor;
					break;
				default:
					ensureMsgf(false, TEXT("Unsupported value: %s"), *UEnum::GetValueAsString(RuntimeSlot->GetState()));
				}
			}

			const FVector AxisX = SlotTransform.GetUnitAxis(EAxis::X);
			const FVector AxisY = SlotTransform.GetUnitAxis(EAxis::Y);
			if (SlotShape == ESmartObjectSlotShape::Circle)
			{
				AddShape(FGameplayDebuggerShape::MakeCircle(Pos, AxisX, AxisY, SlotSize, DebugColor));
				AddShape(FGameplayDebuggerShape::MakeCircle(Pos, AxisX, AxisY, 0.75f * SlotSize, /* Thickness */5.f, StateColor));
			}
			else if (SlotShape == ESmartObjectSlotShape::Rectangle)
			{
				AddShape(FGameplayDebuggerShape::MakeRectangle(Pos, AxisX, AxisY, SlotSize * 2.0f, SlotSize * 2.0f, DebugColor));
				AddShape(FGameplayDebuggerShape::MakeRectangle(Pos, AxisX, AxisY, .75f * SlotSize * 2.0f, .75f * SlotSize * 2.0f, /* Thickness */5.f, StateColor));
			}
			
			AddShape(FGameplayDebuggerShape::MakeArrow(Pos, Pos + Dir * 2.0f * SlotSize, DebugArrowHeadSize, DebugArrowThickness, DebugColor));

			if (DataPack.bDisplayInstanceTags)
			{
				FString TagsAsString = RuntimeSlot->GetTags().ToStringSimple();
				if (!TagsAsString.IsEmpty())
				{
					// Using small dummy shape to display tags
					AddShape(FGameplayDebuggerShape::MakePoint(Pos, /*Radius*/ 1.0f, FColorList::White, TagsAsString));
				}
			}

			// Let annotations debug draw too
			if (DataPack.bDisplayAnnotations)
			{
				FSmartObjectAnnotationGameplayDebugContext DebugContext(*this, SmartObjectRuntime.GetDefinition());
				DebugContext.SmartObjectOwnerActor = SmartObjectRuntime.GetOwnerActor();
				DebugContext.DebugActor = DebugActor;
				DebugContext.SlotTransform = SlotTransform;
				DebugContext.ViewLocation = ViewLocation;
				DebugContext.ViewDirection = ViewDirection;
			
				for (const FSmartObjectDefinitionDataProxy& DataProxy : SlotDefinition.DefinitionData)
				{
					if (const FSmartObjectSlotAnnotation* Annotation = DataProxy.Data.GetPtr<FSmartObjectSlotAnnotation>())
					{
						Annotation->CollectDataForGameplayDebugger(DebugContext);
					}
				}
			}
			
			// Look if the slot has an active user; if so and it's an actor then display a segment between it and the slot.
			if (const FSmartObjectActorUserData* ActorUser = RuntimeSlot->GetUserData().GetPtr<const FSmartObjectActorUserData>())
			{
				if (const AActor* Actor = ActorUser->UserActor.Get())
				{
					AddShape(FGameplayDebuggerShape::MakeSegment(Pos, Actor->GetActorLocation(), DebugArrowThickness, DebugColor, GetNameSafe(Actor)));
				}
			}
		}
	}

	const uint32 NumRuntimeObjects = Subsystem->DebugGetNumRuntimeObjects();
	const uint32 NumRegisteredComponents = Subsystem->DebugGetNumRegisteredComponents();
	const uint32 NumCollectionEntries = Subsystem->GetSmartObjectContainer().GetEntries().Num();

	AddTextLine(FString::Printf(TEXT(
		"{White}Collection entries = {Green}%d\n"
		"{White}Runtime objects (Active / Inactive) = {Green}%s {White}/ {Grey}%s\n"
		"{White}Registered components = {Green}%s"),
		NumCollectionEntries,
		*LexToString(NumActiveObjects), *LexToString(NumRuntimeObjects-NumActiveObjects),
		*LexToString(NumRegisteredComponents)));
}

void FGameplayDebuggerCategory_SmartObject::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);

	CanvasContext.Printf(TEXT("Display:\n"
								"[{yellow}%s{white}]:{%s}Instance Tags\n"
								"[{yellow}%s{white}]:{%s}Slots Details\n"
								"[{yellow}%s{white}]:{%s}Annotations\n"),
		*GetInputHandlerDescription(0),
		DataPack.bDisplayInstanceTags
			? *FGameplayDebuggerCanvasStrings::ColorNameEnabled
			: *FGameplayDebuggerCanvasStrings::ColorNameDisabled,
		*GetInputHandlerDescription(1),
		DataPack.bDisplaySlotDetails
			? *FGameplayDebuggerCanvasStrings::ColorNameEnabled
			: *FGameplayDebuggerCanvasStrings::ColorNameDisabled,
		*GetInputHandlerDescription(2),
		DataPack.bDisplayAnnotations
			? *FGameplayDebuggerCanvasStrings::ColorNameEnabled
			: *FGameplayDebuggerCanvasStrings::ColorNameDisabled);
}

void FGameplayDebuggerCategory_SmartObject::FReplicationData::Serialize(FArchive& Ar)
{
	Ar << bDisplayAnnotations;
	Ar << bDisplayInstanceTags;
	Ar << bDisplaySlotDetails;
}

#endif // WITH_GAMEPLAY_DEBUGGER && WITH_SMARTOBJECT_DEBUG
