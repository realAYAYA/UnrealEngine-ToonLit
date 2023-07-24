// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseGizmos/IntervalGizmo.h"
#include "InteractiveGizmoManager.h"
#include "BaseGizmos/AxisPositionGizmo.h"

#include "BaseGizmos/GizmoComponents.h"
#include "BaseGizmos/GizmoLineHandleComponent.h"
#include "BaseGizmos/GizmoViewContext.h"

// need this to implement hover
#include "BaseGizmos/GizmoBaseComponent.h"

#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "ContextObjectStore.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IntervalGizmo)


#define LOCTEXT_NAMESPACE "UIntervalGizmo"

/**
* FFloatParameterProxyChange tracks a change to the base transform for a FloatParameter
*/
class FFloatParameterProxyChange : public FToolCommandChange
{
public:
	FGizmoFloatParameterChange To;
	FGizmoFloatParameterChange From;


	virtual void Apply(UObject* Object) override 
	{
		UGizmoLocalFloatParameterSource* ParameterSource = CastChecked<UGizmoLocalFloatParameterSource>(Object);
		ParameterSource->SetParameter(To.CurrentValue);
	}
	virtual void Revert(UObject* Object) override
	{
		UGizmoLocalFloatParameterSource* ParameterSource = CastChecked<UGizmoLocalFloatParameterSource>(Object);
		ParameterSource->SetParameter(From.CurrentValue);
	}

	virtual FString ToString() const override { return TEXT("FFloatParameterProxyChange"); }
};

/**
 * FGizmoFloatParameterChangeSource generates FFloatParameterProxyChange instances on Begin/End.
 * Instances of this class can (for example) be attached to a UGizmoTransformChangeStateTarget for use TransformGizmo change tracking.
 */
class FGizmoFloatParameterChangeSource : public IToolCommandChangeSource
{
public:
	FGizmoFloatParameterChangeSource(UGizmoLocalFloatParameterSource* ProxyIn)
	{
		Proxy = ProxyIn;
	}

	virtual ~FGizmoFloatParameterChangeSource() {}

	TWeakObjectPtr<UGizmoLocalFloatParameterSource> Proxy;
	TUniquePtr<FFloatParameterProxyChange> ActiveChange;

	virtual void BeginChange() override
	{
		if (Proxy.IsValid())
		{
			ActiveChange = MakeUnique<FFloatParameterProxyChange>();
			ActiveChange->From = Proxy->LastChange;
		}
	}
	virtual TUniquePtr<FToolCommandChange> EndChange() override
	{
		if (Proxy.IsValid())
		{
			ActiveChange->To = Proxy->LastChange;
			return MoveTemp(ActiveChange);
		}
		return TUniquePtr<FToolCommandChange>();
	}
	virtual UObject* GetChangeTarget() override
	{
		return Proxy.Get();
	}
	virtual FText GetChangeDescription() override
	{
		return LOCTEXT("FFGizmoFloatParameterChangeDescription", "GizmoFloatParameterChange");
	}
};

/**
 * This change source doesn't actually issue any valid transactions. Instead, it is a helper class 
 * that can get attached to the interval gizmo's state target to fire off BeginEditSequence and 
 * EndEditSequence on the start/end of a drag.
 */
class FIntervalGizmoChangeBroadcaster : public IToolCommandChangeSource
{
public:
	FIntervalGizmoChangeBroadcaster(UIntervalGizmo* IntervalGizmoIn) : IntervalGizmo(IntervalGizmoIn) {}

	virtual ~FIntervalGizmoChangeBroadcaster() {}

	TWeakObjectPtr<UIntervalGizmo> IntervalGizmo;

	virtual void BeginChange() override
	{
		if (IntervalGizmo.IsValid())
		{
			IntervalGizmo->BeginEditSequence();
		}
	}
	virtual TUniquePtr<FToolCommandChange> EndChange() override
	{
		if (IntervalGizmo.IsValid())
		{
			IntervalGizmo->EndEditSequence();
		}
		return TUniquePtr<FToolCommandChange>();
	}
	virtual UObject* GetChangeTarget() override
	{
		return IntervalGizmo.Get();
	}
	virtual FText GetChangeDescription() override
	{
		return LOCTEXT("FIntervalGizmoChangeBroadcaster", "IntervalGizmoEdit");
	}
};


AIntervalGizmoActor::AIntervalGizmoActor()
{
	// root component is a hidden sphere
	USphereComponent* SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("GizmoCenter"));
	RootComponent = SphereComponent;
	SphereComponent->InitSphereRadius(1.0f);
	SphereComponent->SetVisibility(false);
	SphereComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

AIntervalGizmoActor* AIntervalGizmoActor::ConstructDefaultIntervalGizmo(UWorld* World, UGizmoViewContext* GizmoViewContext)
{
	FActorSpawnParameters SpawnInfo;
	AIntervalGizmoActor* NewActor = World->SpawnActor<AIntervalGizmoActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);

	const FLinearColor MintGreen(152 / 255.f, 1.f, 152 / 255.f);
	
	// add components 
	NewActor->UpIntervalComponent      = AddDefaultLineHandleComponent(World, NewActor, GizmoViewContext, MintGreen, FVector(0, 1, 0), FVector(0, 0, 1));
	NewActor->DownIntervalComponent    = AddDefaultLineHandleComponent(World, NewActor, GizmoViewContext, MintGreen, FVector(0, 1, 0), FVector(0, 0, 1));
	NewActor->ForwardIntervalComponent = AddDefaultLineHandleComponent(World, NewActor, GizmoViewContext, MintGreen, FVector(1, 0, 0), FVector(0, 1, 0));


	return NewActor;
}



UInteractiveGizmo* UIntervalGizmoBuilder::BuildGizmo(const FToolBuilderState& SceneState) const
{
	UIntervalGizmo* NewGizmo = NewObject<UIntervalGizmo>(SceneState.GizmoManager);
	NewGizmo->SetWorld(SceneState.World);

	UGizmoViewContext* GizmoViewContext = SceneState.ToolManager->GetContextObjectStore()->FindContext<UGizmoViewContext>();
	check(GizmoViewContext && GizmoViewContext->IsValidLowLevel());

	// use default gizmo actor if client has not given us a new builder
	NewGizmo->SetGizmoActorBuilder(GizmoActorBuilder ? GizmoActorBuilder : MakeShared<FIntervalGizmoActorFactory>(GizmoViewContext));

	// override default hover function if proposed
	if (UpdateHoverFunction)
	{
		NewGizmo->SetUpdateHoverFunction(UpdateHoverFunction);
	}

	if (UpdateCoordSystemFunction)
	{
		NewGizmo->SetUpdateCoordSystemFunction(UpdateCoordSystemFunction);
	}

	return NewGizmo;
}

// Init static FName
FString UIntervalGizmo::GizmoName = TEXT("IntervalGizmo");

void UIntervalGizmo::SetWorld(UWorld* WorldIn)
{
	this->World = WorldIn;
}

void UIntervalGizmo::SetGizmoActorBuilder(TSharedPtr<FIntervalGizmoActorFactory> Builder)
{
	GizmoActorBuilder = Builder;
}

void UIntervalGizmo::SetUpdateHoverFunction(TFunction<void(UPrimitiveComponent*, bool)> HoverFunction)
{
	UpdateHoverFunction = HoverFunction;
}

void UIntervalGizmo::SetUpdateCoordSystemFunction(TFunction<void(UPrimitiveComponent*, EToolContextCoordinateSystem)> CoordSysFunction)
{
	UpdateCoordSystemFunction = CoordSysFunction;
}

void UIntervalGizmo::SetWorldAlignmentFunctions(TUniqueFunction<bool()>&& ShouldAlignDestinationIn, TUniqueFunction<bool(const FRay&, FVector&)>&& DestinationAlignmentRayCasterIn)
{
	// Save these so that any later gizmo resets (using SetActiveTarget) keep the settings.
	ShouldAlignDestination = MoveTemp(ShouldAlignDestinationIn);
	DestinationAlignmentRayCaster = MoveTemp(DestinationAlignmentRayCasterIn);

	for (UInteractiveGizmo* SubGizmo : this->ActiveGizmos)
	{
		if (UAxisPositionGizmo* CastGizmo = Cast<UAxisPositionGizmo>(SubGizmo))
		{
			CastGizmo->ShouldUseCustomDestinationFunc = [this]() { return ShouldAlignDestination(); };
			CastGizmo->CustomDestinationFunc =
				[this](const UAxisPositionGizmo::FCustomDestinationParams& Params, FVector& OutputPoint) {
				return DestinationAlignmentRayCaster(*Params.WorldRay, OutputPoint);
			};
			CastGizmo->bCustomDestinationAlignsAxisOrigin = false; // We're aligning the endpoints of the intervals
		}
	}
}

void UIntervalGizmo::Setup()
{
	UInteractiveGizmo::Setup();

	UpdateHoverFunction = [](UPrimitiveComponent* Component, bool bHovering)
	{
		if (Cast<UGizmoBaseComponent>(Component) != nullptr)
		{
			Cast<UGizmoBaseComponent>(Component)->UpdateHoverState(bHovering);
		}
	};

	UpdateCoordSystemFunction = [](UPrimitiveComponent* Component, EToolContextCoordinateSystem CoordSystem)
	{
		if (Cast<UGizmoBaseComponent>(Component) != nullptr)
		{
			Cast<UGizmoBaseComponent>(Component)->UpdateWorldLocalState(CoordSystem == EToolContextCoordinateSystem::World);
		}
	};

	GizmoActor = GizmoActorBuilder->CreateNewGizmoActor(World);
}

void UIntervalGizmo::Shutdown()
{
	ClearActiveTarget();

	if (GizmoActor)
	{
		GizmoActor->Destroy();
		GizmoActor = nullptr;
	}

	ClearSources();

}

void UIntervalGizmo::Tick(float DeltaTime)
{
	EToolContextCoordinateSystem CoordSystem = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem();
	check(CoordSystem == EToolContextCoordinateSystem::World || CoordSystem == EToolContextCoordinateSystem::Local)
		bool bUseLocalAxes =
		(GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem() == EToolContextCoordinateSystem::Local);

	// Update gizmo location.
	{
		USceneComponent* GizmoComponent = GizmoActor->GetRootComponent();
		// move gizmo to target location
		FTransform TargetTransform = TransformProxy->GetTransform();
		FVector SaveScale = TargetTransform.GetScale3D();
		TargetTransform.SetScale3D(FVector(1, 1, 1));
		GizmoComponent->SetWorldTransform(TargetTransform);
	}
	// Update the lengths
	{
		if (GizmoActor->UpIntervalComponent != nullptr && UpIntervalSource != nullptr)
		{
			GizmoActor->UpIntervalComponent->Length = UpIntervalSource->GetParameter();
		}
		if (GizmoActor->DownIntervalComponent != nullptr && DownIntervalSource != nullptr)
		{
			GizmoActor->DownIntervalComponent->Length = DownIntervalSource->GetParameter();
		}
		if (GizmoActor->ForwardIntervalComponent != nullptr && ForwardIntervalSource != nullptr)
		{
			GizmoActor->ForwardIntervalComponent->Length = ForwardIntervalSource->GetParameter();
		}
	}

	if (UpdateCoordSystemFunction)
	{
		for (UPrimitiveComponent* Component : ActiveComponents)
		{
			UpdateCoordSystemFunction(Component, CoordSystem);
		}
	}
}

void UIntervalGizmo::SetActiveTarget(UTransformProxy* TransformTargetIn, UGizmoLocalFloatParameterSource* UpInterval , UGizmoLocalFloatParameterSource* DownInterval, UGizmoLocalFloatParameterSource* ForwardInterval, IToolContextTransactionProvider* TransactionProvider)
{
	if (TransformProxy != nullptr)
	{
		ClearActiveTarget();
		ClearSources();

	}

	// This state target emits an explicit FChange that moves the GizmoActor root component during undo/redo.
	// It also opens/closes the Transaction that saves/restores the target object locations.
	if (TransactionProvider == nullptr)
	{
		TransactionProvider = GetGizmoManager();
	}

	TransformProxy = TransformTargetIn;

	// parameters and init lengths for each interval
	UpIntervalSource      = UpInterval;
	DownIntervalSource    = DownInterval;
	ForwardIntervalSource = ForwardInterval;

	// Get the parameter source to notify our delegate of any changes
	UpIntervalSource->OnParameterChanged.AddWeakLambda(this, [this](IGizmoFloatParameterSource*, FGizmoFloatParameterChange Change) {
		OnIntervalChanged.Broadcast(this, FVector::ZAxisVector, Change.CurrentValue);
		});
	DownIntervalSource->OnParameterChanged.AddWeakLambda(this, [this](IGizmoFloatParameterSource*, FGizmoFloatParameterChange Change) {
		OnIntervalChanged.Broadcast(this, -FVector::ZAxisVector, -Change.CurrentValue);
		});
	ForwardIntervalSource->OnParameterChanged.AddWeakLambda(this, [this](IGizmoFloatParameterSource*, FGizmoFloatParameterChange Change) {
		OnIntervalChanged.Broadcast(this, FVector::YAxisVector, Change.CurrentValue);
		});

	USceneComponent* GizmoComponent = GizmoActor->GetRootComponent();

	// move gizmo to target location
	FTransform TargetTransform = TransformTargetIn->GetTransform();
	FVector SaveScale = TargetTransform.GetScale3D();
	TargetTransform.SetScale3D(FVector(1, 1, 1));
	GizmoComponent->SetWorldTransform(TargetTransform);

	
	// TargetTransform tracks location of GizmoComponent. Note that TransformUpdated is not called during undo/redo transactions!
	// We currently rely on the transaction system to undo/redo target object locations. This will not work during runtime...
	GizmoComponent->TransformUpdated.AddLambda(
		[this, SaveScale](USceneComponent* Component, EUpdateTransformFlags /*UpdateTransformFlags*/, ETeleportType /*Teleport*/) {
		//this->GetGizmoManager()->DisplayMessage(TEXT("TRANSFORM UPDATED"), EToolMessageLevel::Internal);
		FTransform NewXForm = Component->GetComponentToWorld();
		NewXForm.SetScale3D(SaveScale);
		this->TransformProxy->SetTransform(NewXForm);
	});

	


	StateTarget = UGizmoTransformChangeStateTarget::Construct(GizmoComponent,
		LOCTEXT("UIntervalGizmoTransaction", "Interval"), TransactionProvider, this);
	StateTarget->DependentChangeSources.Add(MakeUnique<FTransformProxyChangeSource>(TransformProxy));
	StateTarget->DependentChangeSources.Add(MakeUnique<FGizmoFloatParameterChangeSource>(UpIntervalSource));
	StateTarget->DependentChangeSources.Add(MakeUnique<FGizmoFloatParameterChangeSource>(DownIntervalSource));
	StateTarget->DependentChangeSources.Add(MakeUnique<FGizmoFloatParameterChangeSource>(ForwardIntervalSource));

	// Have the state target notify us of the start/end of drags
	StateTarget->DependentChangeSources.Add(MakeUnique<FIntervalGizmoChangeBroadcaster>(this));

	// root component provides local Y/Z axis, identified by AxisIndex
	AxisYSource = UGizmoComponentAxisSource::Construct(GizmoComponent, 1, true, this);
	AxisZSource = UGizmoComponentAxisSource::Construct(GizmoComponent, 2, true, this);

	
	
	if (GizmoActor->UpIntervalComponent != nullptr)
	{
		float IntervalMin = 0.f;
		float IntervalMax = FLT_MAX;
		AddIntervalHandleGizmo(GizmoComponent, GizmoActor->UpIntervalComponent, AxisZSource, UpIntervalSource, IntervalMin, IntervalMax, StateTarget);
		ActiveComponents.Add(GizmoActor->UpIntervalComponent);
	}

	if (GizmoActor->DownIntervalComponent != nullptr)
	{
		float IntervalMin = -FLT_MAX;
		float IntervalMax = 0.f;
		AddIntervalHandleGizmo(GizmoComponent, GizmoActor->DownIntervalComponent, AxisZSource, DownIntervalSource, IntervalMin, IntervalMax, StateTarget);
		ActiveComponents.Add(GizmoActor->DownIntervalComponent);
	}

	if (GizmoActor->ForwardIntervalComponent != nullptr)
	{
		float IntervalMin = -FLT_MAX;
		float IntervalMax = FLT_MAX;
		AddIntervalHandleGizmo(GizmoComponent, GizmoActor->ForwardIntervalComponent, AxisYSource, ForwardIntervalSource, IntervalMin, IntervalMax, StateTarget);
		ActiveComponents.Add(GizmoActor->ForwardIntervalComponent);
	}

}

void UIntervalGizmo::ClearSources()
{
	UpIntervalSource = nullptr;
	DownIntervalSource = nullptr;
	ForwardIntervalSource = nullptr;
}

void UIntervalGizmo::ClearActiveTarget()
{
	for (UInteractiveGizmo* Gizmo : ActiveGizmos)
	{
		GetGizmoManager()->DestroyGizmo(Gizmo);
	}
	ActiveGizmos.SetNum(0);
	ActiveComponents.SetNum(0);
	
	ClearSources();

	TransformProxy = nullptr;
}

FTransform UIntervalGizmo::GetGizmoTransform() const
{
	return TransformProxy->GetTransform();
}

UInteractiveGizmo* UIntervalGizmo::AddIntervalHandleGizmo(
	USceneComponent* RootComponent,
	UPrimitiveComponent* HandleComponent,
	IGizmoAxisSource* AxisSource,
	IGizmoFloatParameterSource* FloatParameterSource,
	float MinParameter,
	float MaxParameter,
	IGizmoStateTarget* StateTargetIn)
{
	// create axis-position gizmo, axis-position parameter will drive translation
	UAxisPositionGizmo* IntervalGizmo = Cast<UAxisPositionGizmo>(GetGizmoManager()->CreateGizmo(
		UInteractiveGizmoManager::DefaultAxisPositionBuilderIdentifier));
	check(IntervalGizmo);

	// axis source provides the scale axis
	IntervalGizmo->AxisSource = Cast<UObject>(AxisSource);


	// parameter source maps axis-parameter-change to change in interval length
	IntervalGizmo->ParameterSource = UGizmoAxisIntervalParameterSource::Construct(FloatParameterSource, MinParameter, MaxParameter, this);

	// sub-component provides hit target
	UGizmoComponentHitTarget* HitTarget = UGizmoComponentHitTarget::Construct(HandleComponent, this);
	if (this->UpdateHoverFunction)
	{
		HitTarget->UpdateHoverFunction = [HandleComponent, this](bool bHovering) { this->UpdateHoverFunction(HandleComponent, bHovering); };
	}
	IntervalGizmo->HitTarget = HitTarget;

	IntervalGizmo->StateTarget = Cast<UObject>(StateTargetIn);

	IntervalGizmo->ShouldUseCustomDestinationFunc = [this]() { return ShouldAlignDestination(); };
	IntervalGizmo->CustomDestinationFunc =
		[this](const UAxisPositionGizmo::FCustomDestinationParams& Params, FVector& OutputPoint) {
		return DestinationAlignmentRayCaster(*Params.WorldRay, OutputPoint);
	};

	ActiveGizmos.Add(IntervalGizmo);

	return IntervalGizmo;
}

float UGizmoAxisIntervalParameterSource::GetParameter() const
{
	return FloatParameterSource->GetParameter();
}

void UGizmoAxisIntervalParameterSource::SetParameter(float NewValue) 
{

	NewValue = FMath::Clamp(NewValue, MinParameter, MaxParameter);

	FloatParameterSource->SetParameter(NewValue);

}

void UGizmoAxisIntervalParameterSource::BeginModify()
{
	FloatParameterSource->BeginModify();
}

void UGizmoAxisIntervalParameterSource::EndModify()
{
	FloatParameterSource->EndModify();
}

UGizmoAxisIntervalParameterSource* UGizmoAxisIntervalParameterSource::Construct(
	IGizmoFloatParameterSource* FloatSourceIn,
	float ParameterMin,
	float ParameterMax,
	UObject* Outer)
{
	UGizmoAxisIntervalParameterSource* NewSource = NewObject<UGizmoAxisIntervalParameterSource>(Outer);

	NewSource->FloatParameterSource = Cast<UObject>(FloatSourceIn);

	// Clamp the initial value
	float DefaultValue = NewSource->FloatParameterSource->GetParameter();
	DefaultValue = FMath::Clamp(DefaultValue, ParameterMin, ParameterMax);
	NewSource->FloatParameterSource->SetParameter(DefaultValue);

	// record the min / max allowed
	NewSource->MinParameter = ParameterMin;
	NewSource->MaxParameter = ParameterMax;

	return NewSource;
}

#undef LOCTEXT_NAMESPACE
