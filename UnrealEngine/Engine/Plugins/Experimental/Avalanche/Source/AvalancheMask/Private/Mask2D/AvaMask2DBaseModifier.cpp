// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mask2D/AvaMask2DBaseModifier.h"

#include "AvaMaskLog.h"
#include "AvaMaskSubsystem.h"
#include "AvaMaskUtilities.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Framework/AvaGizmoComponent.h"
#include "GeometryMaskCanvas.h"
#include "GeometryMaskReadComponent.h"
#include "GeometryMaskSubsystem.h"
#include "GeometryMaskWorldSubsystem.h"
#include "GeometryMaskWriteComponent.h"
#include "Handling/AvaHandleUtilities.h"
#include "Handling/AvaObjectHandleSubsystem.h"
#include "Handling/IAvaMaskMaterialCollectionHandle.h"
#include "Handling/IAvaMaskMaterialHandle.h"
#include "Materials/AvaMaskMaterialInstanceSubsystem.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Modifiers/ActorModifierCoreStack.h"
#include "Subsystems/ActorModifierCoreSubsystem.h"

#define LOCTEXT_NAMESPACE "AvaMask2DModifier"

namespace UE::AvaMask::Private
{
	template <typename KeyType, typename ValueType>
	ValueType& FindInPreviousOrAddToNew(
		const TMap<KeyType, ValueType>& InPrevious
		, TMap<KeyType, ValueType>& InNew
		, const KeyType& InKey
		, TIdentity_T<TUniqueFunction<ValueType()>>&& InValueFactory)
	{
		if (const ValueType* FoundValue = InPrevious.Find(InKey))
		{
			return InNew.Emplace(InKey, MoveTempIfPossible(*FoundValue));
		}

		if (ValueType* FoundValue = InNew.Find(InKey))
		{
			return *FoundValue;
		}

		return InNew.Emplace(InKey, InValueFactory());
	}

	// @note: It's crucial these remain in sync with the defaults in UGeometryMaskCanvas
	namespace CanvasPropertyDefaults
	{
		static bool bApplyBlur = false;
		static double BlurStrength = 16;
		static bool bApplyFeather = false;
		static int32 OuterFeatherRadius = 16;
		static int32 InnerFeatherRadius = 16;
	}
}

#if WITH_EDITOR
const TAvaPropertyChangeDispatcher<UAvaMask2DBaseModifier> UAvaMask2DBaseModifier::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UAvaMask2DBaseModifier, bUseParentChannel), &UAvaMask2DBaseModifier::OnUseParentChannelChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaMask2DBaseModifier, Channel), &UAvaMask2DBaseModifier::OnChannelChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaMask2DBaseModifier, bInverted), &UAvaMask2DBaseModifier::OnInvertedChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaMask2DBaseModifier, bUseBlur), &UAvaMask2DBaseModifier::OnBlurChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaMask2DBaseModifier, BlurStrength), &UAvaMask2DBaseModifier::OnBlurChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaMask2DBaseModifier, bUseFeathering), &UAvaMask2DBaseModifier::OnFeatherChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaMask2DBaseModifier, OuterFeatherRadius), &UAvaMask2DBaseModifier::OnFeatherChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaMask2DBaseModifier, InnerFeatherRadius), &UAvaMask2DBaseModifier::OnFeatherChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaMask2DBaseModifier, CanvasWeak), &UAvaMask2DBaseModifier::OnCanvasChanged }
};
#endif

void UAvaMask2DBaseModifier::SetUseParentChannel(const bool bInUseParentChannel)
{
	if (bUseParentChannel != bInUseParentChannel)
	{
		bUseParentChannel = bInUseParentChannel;
		OnUseParentChannelChanged();
	}
}

const FName UAvaMask2DBaseModifier::GetChannel() const
{
	return bUseParentChannel ? ParentChannel : Channel;
}

void UAvaMask2DBaseModifier::SetChannel(FName InChannel)
{
	if (bUseParentChannel)
	{
		if (ParentChannel != InChannel)
		{
			ParentChannel = InChannel;
			OnChannelChanged();
		}
	}
	else
	{
		if (Channel != InChannel)
		{
			Channel = InChannel;
			OnChannelChanged();
		}
	}
}

void UAvaMask2DBaseModifier::SetIsInverted(const bool bInInvert)
{
	if (bInverted != bInInvert)
	{
		bInverted = bInInvert;
		OnInvertedChanged();
	}
}

void UAvaMask2DBaseModifier::UseBlur(bool bInUseBlur)
{
	if (bUseBlur != bInUseBlur)
	{
		bUseBlur = bInUseBlur;
		OnBlurChanged();
	}
}

void UAvaMask2DBaseModifier::SetBlurStrength(float InBlurStrength)
{
	InBlurStrength = FMath::Max(0.0f, InBlurStrength);
	if (!FMath::IsNearlyEqual(BlurStrength, InBlurStrength))
	{
		BlurStrength = InBlurStrength;
		OnBlurChanged();
	}
}

void UAvaMask2DBaseModifier::UseFeathering(bool bInUseFeathering)
{
	if (bUseFeathering != bInUseFeathering)
	{
		bUseFeathering = bInUseFeathering;
		OnFeatherChanged();
	}
}

void UAvaMask2DBaseModifier::SetOuterFeatherRadius(int32 InFeatherRadius)
{
	InFeatherRadius = FMath::Max(0, InFeatherRadius);
	if (OuterFeatherRadius != InFeatherRadius)
	{
		OuterFeatherRadius = InFeatherRadius;
		OnFeatherChanged();
	}
}

void UAvaMask2DBaseModifier::SetInnerFeatherRadius(int32 InFeatherRadius)
{
	InFeatherRadius = FMath::Max(0, InFeatherRadius);
	if (InnerFeatherRadius != InFeatherRadius)
	{
		InnerFeatherRadius = InFeatherRadius;
		OnFeatherChanged();
	}
}

void UAvaMask2DBaseModifier::OnBlurChanged()
{
	MarkModifierDirty();
}

void UAvaMask2DBaseModifier::OnFeatherChanged()
{
	MarkModifierDirty();
}

void UAvaMask2DBaseModifier::OnCanvasChanged()
{
	CanvasParamsToLocal();
	
	OnBlurChanged();
	OnFeatherChanged();
}

void UAvaMask2DBaseModifier::CanvasParamsToLocal()
{
	if (UGeometryMaskCanvas* Canvas = GetCurrentCanvas())
	{
		bUseBlur = Canvas->IsBlurApplied();
		BlurStrength = Canvas->GetBlurStrength();

		bUseFeathering = Canvas->IsFeatherApplied();
		OuterFeatherRadius = Canvas->GetOuterFeatherRadius();
		InnerFeatherRadius = Canvas->GetInnerFeatherRadius();
	}
}

void UAvaMask2DBaseModifier::LocalParamsToCanvas()
{
	if (UGeometryMaskCanvas* Canvas = GetCurrentCanvas())
	{
		// Only set if not defaults
		
		const bool bModifierUseBlur = bUseBlur;
		if (bModifierUseBlur != UE::AvaMask::Private::CanvasPropertyDefaults::bApplyBlur)
		{
			Canvas->SetApplyBlur(bModifierUseBlur);
		}

		const double ModifierBlurStrength = BlurStrength;		
		if (!FMath::IsNearlyEqual(ModifierBlurStrength, UE::AvaMask::Private::CanvasPropertyDefaults::BlurStrength))
		{
			const double CanvasBlurStrength = Canvas->GetBlurStrength();
			Canvas->SetBlurStrength(FMath::Max(ModifierBlurStrength, CanvasBlurStrength));
		}

		const bool bModifierUseFeathering = bUseFeathering;
		if (bModifierUseFeathering != UE::AvaMask::Private::CanvasPropertyDefaults::bApplyFeather)
		{
			Canvas->SetApplyFeather(bModifierUseFeathering);
		}

		const int32 ModifierOuterFeatherRadius = OuterFeatherRadius;
		if (ModifierOuterFeatherRadius != UE::AvaMask::Private::CanvasPropertyDefaults::OuterFeatherRadius)
		{
			const int32 CanvasOuterFeatherRadius = Canvas->GetOuterFeatherRadius();
			Canvas->SetOuterFeatherRadius(FMath::Max(ModifierOuterFeatherRadius, CanvasOuterFeatherRadius));
		}

		const int32 ModifierInnerFeatherRadius = InnerFeatherRadius;
		if (ModifierInnerFeatherRadius != UE::AvaMask::Private::CanvasPropertyDefaults::InnerFeatherRadius)
		{
			const int32 CanvasInnerFeatherRadius = Canvas->GetInnerFeatherRadius();
			Canvas->SetInnerFeatherRadius(FMath::Max(ModifierInnerFeatherRadius, CanvasInnerFeatherRadius));
		}
	}
}

#if WITH_EDITOR
void UAvaMask2DBaseModifier::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UAvaMask2DBaseModifier::OnModifierAdded(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierAdded(InReason);

	TRACE_BOOKMARK(TEXT("UAvaMask2DModifier::OnModifierAdded"));

	OnChannelChanged();

	if (InReason == EActorModifierCoreEnableReason::User
		|| InReason == EActorModifierCoreEnableReason::Duplicate)
	{
		SetupChannelName();
	}

	if (InReason == EActorModifierCoreEnableReason::Load)
	{
		LocalParamsToCanvas();
	}
	else
	{
		CanvasParamsToLocal();
		MarkModifierDirty();
	}
}

void UAvaMask2DBaseModifier::OnModifierRemoved(EActorModifierCoreDisableReason InReason)
{
	Super::OnModifierRemoved(InReason);

	TRACE_BOOKMARK(TEXT("UAvaMask2DModifier::OnModifierRemoved"));

	if (InReason == EActorModifierCoreDisableReason::Destroyed)
	{
		CanvasParamsToLocal();
	}

	TArray<AActor*> ActorDataActors;
	ActorDataActors.Reserve(ActorData.Num());	
	Algo::Transform(ActorData, ActorDataActors, [](const TPair<TWeakObjectPtr<AActor>, FAvaMask2DActorData>& InActorDataPair)
	{
		return InActorDataPair.Key.Get();
	});
	
	for (AActor* Actor : ActorDataActors)
	{
		if (!Actor)
		{
			continue;
		}
		
		RemoveFromActor(Actor);
	}
}

void UAvaMask2DBaseModifier::OnModifiedActorTransformed()
{
	// This is needed to override parent behavior (to disable it)
}

void UAvaMask2DBaseModifier::SavePreState()
{
	Super::SavePreState();

	TMap<TWeakObjectPtr<AActor>, FAvaMask2DActorData> PreviousActorDatas = ActorData;
	
	TMap<TWeakObjectPtr<AActor>, FAvaMask2DActorData> NewActorDatas;
	NewActorDatas.Reserve(PreviousActorDatas.Num());

	ForEachActor<AActor>(
		[this, &NewActorDatas](AActor* InActor)
		{
			SaveActorPreState(InActor, NewActorDatas.Emplace(InActor));
			return true;
		}
		, EActorModifierCoreLookup::SelfAndAllChildren);

	ActorData = NewActorDatas;
}

void UAvaMask2DBaseModifier::RestorePreState()
{
	Super::RestorePreState();

	TGuardValue<bool> RestoreStateGuard(bIsRestoring, true);
	
	TMap<TWeakObjectPtr<AActor>, FAvaMask2DActorData> ActorDataCopy = ActorData;
	for (const TPair<TWeakObjectPtr<AActor>, FAvaMask2DActorData>& ActorDataPair : ActorDataCopy)
	{
		if (AActor* Actor = ActorDataPair.Key.Get())
		{
			RestoreActorPreState(Actor, ActorDataPair.Value);
		}
	}
	
	TArray<AActor*> ActorDataActors;
	ActorDataActors.Reserve(ActorData.Num());	
	Algo::Transform(ActorData, ActorDataActors, [](const TPair<TWeakObjectPtr<AActor>, FAvaMask2DActorData>& InActorDataPair)
	{
		return InActorDataPair.Key.Get();
	});
	
	for (AActor* Actor : ActorDataActors)
	{
		if (!Actor)
		{
			continue;
		}
		
		RemoveFromActor(Actor);
	}
}

void UAvaMask2DBaseModifier::Apply()
{
	LocalParamsToCanvas();
}

void UAvaMask2DBaseModifier::SaveActorPreState(AActor* InActor, FAvaMask2DActorData& InActorData)
{
	// Get MaterialCollectionHandle
	const TSharedPtr<IAvaMaskMaterialCollectionHandle>& MaterialCollectionHandle =
		UE::Ava::Internal::FindOrAddHandleByLambda(
			MaterialCollectionHandles
			, InActor
			, [this, InActor]()
			{
				TSharedPtr<IAvaMaskMaterialCollectionHandle> Handle = GetObjectHandleSubsystem()->MakeHandle<IAvaMaskMaterialCollectionHandle>(InActor, UE::AvaMask::Internal::HandleTag);
				if (!Handle->OnSourceMaterialsChanged().IsBoundToObject(this))
				{
					Handle->OnSourceMaterialsChanged().BindUObject(this, &UAvaMask2DBaseModifier::OnMaterialsChanged);
				}
				return Handle;
			});

	FInstancedStruct& HandleData = MaterialCollectionHandleData.FindOrAdd(InActor, MaterialCollectionHandle->MakeDataStruct());
	MaterialCollectionHandle->SaveOriginalState(HandleData);
}

void UAvaMask2DBaseModifier::RestoreActorPreState(AActor* InActor, const FAvaMask2DActorData& InActorData)
{
	// Get MaterialCollectionHandle
    const TSharedPtr<IAvaMaskMaterialCollectionHandle>& MaterialCollectionHandle =
    	UE::Ava::Internal::FindOrAddHandleByLambda(
    		MaterialCollectionHandles
    		, InActor
    		, [this, InActor]()
    		{
    			return GetObjectHandleSubsystem()->MakeHandle<IAvaMaskMaterialCollectionHandle>(InActor, UE::AvaMask::Internal::HandleTag);
    		});

    FInstancedStruct& MaterialCollectionData = MaterialCollectionHandleData.FindOrAdd(InActor, MaterialCollectionHandle->MakeDataStruct());
    MaterialCollectionHandle->ApplyOriginalState(MaterialCollectionData);
}

void UAvaMask2DBaseModifier::OnSceneTreeTrackedActorParentChanged(
	int32 InIdx
	, const TArray<TWeakObjectPtr<AActor>>& InPreviousParentActor
	, const TArray<TWeakObjectPtr<AActor>>& InNewParentActor)
{
	Super::OnSceneTreeTrackedActorParentChanged(InIdx, InPreviousParentActor, InNewParentActor);

	// We don't use the provided parent here, instead we just use the event to trigger custom parent discovery
	if (bUseParentChannel)
	{
		if (TryResolveParentChannel())
		{
			OnChannelChanged();
		}
	}
}

void UAvaMask2DBaseModifier::OnSceneTreeTrackedActorChildrenChanged(int32 InIdx, const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors)
{
	Super::OnSceneTreeTrackedActorChildrenChanged(InIdx, InPreviousChildrenActors, InNewChildrenActors);

	TSet<TWeakObjectPtr<AActor>> UnparentedActors = InPreviousChildrenActors.Difference(InNewChildrenActors);
	for (TWeakObjectPtr<AActor>& ActorWeak : UnparentedActors)
	{
		if (AActor* Actor = ActorWeak.Get())
		{
			RemoveFromActor(Actor);
		}
	}
}

FName UAvaMask2DBaseModifier::GenerateUniqueMaskName() const
{
	const AActor* ModifierActor = GetModifiedActor();
	if (!ModifierActor)
	{
		return NAME_None;
	}

	return FName(ModifierActor->GetActorNameOrLabel() + TEXT("_Mask"));
}

UAvaMask2DBaseModifier* UAvaMask2DBaseModifier::FindMaskModifierOnActor(const AActor* InActor)
{
	// Get from modifier, if present
	if (const UActorModifierCoreSubsystem* ModifierSubsystem = UActorModifierCoreSubsystem::Get())
	{
		const UActorModifierCoreStack* ModifierStack = ModifierSubsystem->GetActorModifierStack(InActor);
		if (!ModifierStack)
		{
			return nullptr;
		}

		UAvaMask2DBaseModifier* FoundMaskModifier = nullptr;
		
		TArray<UAvaMask2DBaseModifier*> FoundModifiers;
		ModifierStack->GetClassModifiers<UAvaMask2DBaseModifier>(FoundModifiers);
		if (!FoundModifiers.IsEmpty())
		{
			FoundMaskModifier = FoundModifiers.Last();
		}

		return FoundMaskModifier;
	}

	return nullptr;
}

void UAvaMask2DBaseModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("Mask"));
	InMetadata.SetCategory(TEXT("Rendering"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Allows to use a custom mask texture on attached actors materials"));
#endif
}

UActorComponent* UAvaMask2DBaseModifier::FindOrAddMaskComponent(TSubclassOf<UActorComponent> InComponentClass, AActor* InActor)
{
	if (!InActor || !InComponentClass)
	{
		return nullptr;
	}

	// Skip adding to actors that already write to a mask
	if (InComponentClass->ImplementsInterface(UGeometryMaskReadInterface::StaticClass()))
	{
		if (InActor->FindComponentByInterface(UGeometryMaskWriteInterface::StaticClass()))
		{
			UE_LOG(LogAvaMask, Display, TEXT("Attempting to add a Mask Read component to an Actor that has a Mask Write component: '%s'"), *InActor->GetName());
			return nullptr;
		}
	}

	UActorComponent* MaskComponent = UE::AvaMask::Internal::FindOrAddComponent(InComponentClass, InActor);
	SetupMaskComponent(MaskComponent);

	return MaskComponent;
}

bool UAvaMask2DBaseModifier::ActorSupportsMaskReadWrite(const AActor* InActor)
{
	// Currently only applies to primitive components
	return InActor->FindComponentByClass<UPrimitiveComponent>() != nullptr;
}

bool UAvaMask2DBaseModifier::TryResolveParentChannel()
{
	bool bParentChannelWasFound = false;
	if (const AActor* ActorModified = GetModifiedActor())
	{
		const AActor* Parent = ActorModified->GetAttachParentActor();

		const IGeometryMaskReadInterface* ReadComponent = nullptr;
		while (ReadComponent == nullptr && Parent)
		{
			ReadComponent = Cast<IGeometryMaskReadInterface>(Parent->FindComponentByInterface<UGeometryMaskReadInterface>());
			Parent = Parent->GetAttachParentActor();
		}

		if (ReadComponent)
		{
			ParentChannel = ReadComponent->GetParameters().CanvasName;
			bParentChannelWasFound = true;
		}
		else
		{
			// If parent invalid or not found, just use the previously specified one (not from parent)
			ParentChannel = Channel;
		}
	}

	return bParentChannelWasFound;
}

void UAvaMask2DBaseModifier::SetupMaskComponent(UActorComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	if (const UGeometryMaskCanvas* Canvas = GetCurrentCanvas())
	{
		if (FAvaMask2DActorData* StoredActorData = ActorData.Find(InComponent->GetOwner()))
		{
			StoredActorData->CanvasTextureWeak = Canvas->GetTexture();
		}
	}
	else
	{
		UE_LOG(LogAvaMask, Warning, TEXT("Expected canvas to be valid"));
	}
}

void UAvaMask2DBaseModifier::RemoveFromActor(AActor* InActor)
{
	ActorData.Remove(InActor);
	
	MaterialCollectionHandleData.Remove(InActor);
	MaterialCollectionHandles.Remove(InActor);
	
	UE::AvaMask::Internal::RemoveComponentByInterface<UGeometryMaskReadInterface>(InActor);
	UE::AvaMask::Internal::RemoveComponentByInterface<UGeometryMaskWriteInterface>(InActor);
	if (UAvaGizmoComponent* FoundComponent = InActor->FindComponentByClass<UAvaGizmoComponent>())
	{
		FoundComponent->DestroyComponent();
	}
}

void UAvaMask2DBaseModifier::OnChannelChanged()
{
	// Reset/invalidate cached
	{
		for (TPair<TWeakObjectPtr<AActor>, FAvaMask2DActorData>& ActorDataPair : ActorData)
		{
			ActorDataPair.Value.CanvasTextureWeak.Reset();
		}
		
		LastResolvedCanvasName = NAME_None;
		CanvasWeak.Reset();
	}

#if WITH_EDITOR
	if (!Channel.IsNone())
	{
		if (UAvaMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UAvaMaskSubsystem>())
		{
			Subsystem->SetLastSpecifiedChannelName(Channel);
		}
	}
#endif

	TryResolveCanvas();
	
	MarkModifierDirty();
}

void UAvaMask2DBaseModifier::OnInvertedChanged()
{
	MarkModifierDirty();
}

void UAvaMask2DBaseModifier::SetupChannelName()
{
	// Already named, leave as-is
	if (!Channel.IsNone())
	{
		return;
	}

	AutoChannelName = GenerateUniqueMaskName();	

#if WITH_EDITOR
	if (const UAvaMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UAvaMaskSubsystem>())
	{
		Channel = Subsystem->GetLastSpecifiedChannelName();
	}

	if (Channel.IsNone())
	{
		Channel = TEXT("0");
	}
#else
	Channel = TEXT("0");
#endif
}

void UAvaMask2DBaseModifier::OnUseParentChannelChanged()
{
	if (bUseParentChannel)
	{
		if (TryResolveParentChannel())
		{
			OnChannelChanged();
		}
	}

	MarkModifierDirty();
}

void UAvaMask2DBaseModifier::OnMaskSetCanvas(const UGeometryMaskCanvas* InCanvas, AActor* InActor)
{
	if (!InCanvas)
	{
		return;
	}

	if (FAvaMask2DActorData* StoredActorData = ActorData.Find(InActor))
	{
		// Cache canvas texture
		StoredActorData->CanvasTextureWeak = InCanvas->GetTexture();
	}
}

void UAvaMask2DBaseModifier::TryResolveCanvas()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	if (UGeometryMaskWorldSubsystem* MaskSubsystem = World->GetSubsystem<UGeometryMaskWorldSubsystem>())
	{
		UGeometryMaskCanvas* Canvas = MaskSubsystem->GetNamedCanvas(GetChannel());
		LastResolvedCanvasName = Canvas->GetCanvasName();
		CanvasWeak = Canvas;
	}
}

UTexture* UAvaMask2DBaseModifier::TryResolveCanvasTexture(AActor* InActor, FAvaMask2DActorData& InActorData)
{
	if (!InActor)
	{
		return nullptr;
	}

	// Get cached version
	if (UTexture* CanvasTexture = InActorData.CanvasTextureWeak.Get())
	{
		return CanvasTexture;
	}
	
	if (!GetWorld())
	{
		return nullptr;
	}

	// Try get canvas texture if already available
	UGeometryMaskWorldSubsystem* MaskSubsystem = GetWorld()->GetSubsystem<UGeometryMaskWorldSubsystem>();
	if (!MaskSubsystem)
	{
		return nullptr;
	}
			
	if (const UGeometryMaskCanvas* Canvas = MaskSubsystem->GetNamedCanvas(Channel))
	{
		// Cache canvas texture
		UTexture* CanvasTexture = Canvas->GetTexture();
		InActorData.CanvasTextureWeak = CanvasTexture;

		return CanvasTexture;
	}

	return nullptr;
}

UAvaObjectHandleSubsystem* UAvaMask2DBaseModifier::GetObjectHandleSubsystem()
{
	if (UAvaObjectHandleSubsystem* Subsystem = ObjectHandleSubsystem.Get())
	{
		return Subsystem;
	}

	ObjectHandleSubsystem = GEngine->GetEngineSubsystem<UAvaObjectHandleSubsystem>();
	return ObjectHandleSubsystem.Get();
}

UAvaMaskMaterialInstanceSubsystem* UAvaMask2DBaseModifier::GetMaterialInstanceSubsystem()
{
	if (UAvaMaskMaterialInstanceSubsystem* Subsystem = MaterialInstanceSubsystem.Get())
	{
		return Subsystem;
	}

	MaterialInstanceSubsystem = GEngine->GetEngineSubsystem<UAvaMaskMaterialInstanceSubsystem>();
	return MaterialInstanceSubsystem.Get();
}

UGeometryMaskCanvas* UAvaMask2DBaseModifier::GetCurrentCanvas()
{
	if (LastResolvedCanvasName.IsNone() || LastResolvedCanvasName != GetChannel())
	{
		LastResolvedCanvasName = NAME_None;
		CanvasWeak.Reset();
	}

	if (!CanvasWeak.IsValid())
	{
		TryResolveCanvas();
	}
	
	if (UGeometryMaskCanvas* Canvas = CanvasWeak.Get())
	{
		return Canvas;
	}

	return nullptr;
}

void UAvaMask2DBaseModifier::OnMaterialsChanged(UPrimitiveComponent* InPrimitiveComponent, const TArray<TSharedPtr<IAvaMaskMaterialHandle>>& InMaterialHandles)
{
	MarkModifierDirty();
}

#undef LOCTEXT_NAMESPACE
