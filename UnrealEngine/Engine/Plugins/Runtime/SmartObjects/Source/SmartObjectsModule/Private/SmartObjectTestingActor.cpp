// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectTestingActor.h"
#include "DebugRenderSceneProxy.h"
#include "SmartObjectDebugSceneProxy.h"
#include "Debug/DebugDrawService.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectTestingActor)

//----------------------------------------------------------------------//
// USmartObjectTest
//----------------------------------------------------------------------//
void USmartObjectTest::PostInitProperties()
{
	UObject::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SmartObjectTestingActor = GetTypedOuter<ASmartObjectTestingActor>();
		checkf(SmartObjectTestingActor, TEXT("SmartObjectTest are expected to be used only in ASmartObjectTestingActor"));
	}
}

bool USmartObjectTest::RunTest()
{
	if (SmartObjectTestingActor != nullptr && SmartObjectTestingActor->GetSubsystem() != nullptr)
	{
		return Run(*SmartObjectTestingActor);
	}
	return false;
}

bool USmartObjectTest::ResetTest()
{
	if (SmartObjectTestingActor != nullptr && SmartObjectTestingActor->GetSubsystem() != nullptr)
	{
		return Reset(*SmartObjectTestingActor);
	}
	return false;
}

FBox USmartObjectTest::CalcTestBounds() const
{
	if (SmartObjectTestingActor != nullptr && SmartObjectTestingActor->GetSubsystem() != nullptr)
	{
		return CalcBounds(*SmartObjectTestingActor);
	}
	return FBox(ForceInit);
}

#if UE_ENABLE_DEBUG_DRAWING
void USmartObjectTest::DebugDraw(FDebugRenderSceneProxy* DebugProxy) const
{
	if (SmartObjectTestingActor != nullptr && SmartObjectTestingActor->GetSubsystem() != nullptr)
	{
		DebugDraw(*SmartObjectTestingActor, DebugProxy);
	}
}

void USmartObjectTest::DebugDrawCanvas(UCanvas* Canvas, APlayerController* PlayerController) const
{
	if (SmartObjectTestingActor != nullptr && SmartObjectTestingActor->GetSubsystem() != nullptr)
	{
		DebugDrawCanvas(*SmartObjectTestingActor, Canvas, PlayerController);
	}
}
#endif // UE_ENABLE_DEBUG_DRAWING


//----------------------------------------------------------------------//
// USmartObjectSimpleQueryTest
//----------------------------------------------------------------------//
bool USmartObjectSimpleQueryTest::Run(ASmartObjectTestingActor& TestingActor)
{
	FSmartObjectRequest RequestWithTransformedBox = Request;
	RequestWithTransformedBox.QueryBox = Request.QueryBox.ShiftBy(TestingActor.GetActorLocation());

	TArray<FSmartObjectRequestResult> NewResults;
	TestingActor.GetSubsystemRef().FindSmartObjects(RequestWithTransformedBox, NewResults);

	// Request redraw only when results differ from previous run
	bool bResultsChanged = false;
	if (NewResults.Num() != Results.Num())
	{
		bResultsChanged = true;
	}
	else
	{
		for (int32 i = 0; i < NewResults.Num(); ++i)
		{
			if (NewResults[i] != Results[i])
			{
				bResultsChanged = true;
				break;
			}
		}
	}

	Results = MoveTemp(NewResults);
	return bResultsChanged;
}

bool USmartObjectSimpleQueryTest::Reset(ASmartObjectTestingActor& TestingActor)
{
	const bool bResultsChanged = Results.Num() > 0;
	Results.Reset();
	return bResultsChanged;
}

FBox USmartObjectSimpleQueryTest::CalcBounds(ASmartObjectTestingActor& TestingActor) const
{
	FBox BoundingBox(EForceInit::ForceInitToZero);
	BoundingBox += Request.QueryBox.ShiftBy(TestingActor.GetActorLocation());

	for (const FSmartObjectRequestResult& Result : Results)
	{
		if (Result.IsValid())
		{
			TOptional<FVector> Location = TestingActor.GetSubsystemRef().GetSlotLocation(Result);
			BoundingBox += Location.GetValue();
		}
	}

	return BoundingBox;
}

#if UE_ENABLE_DEBUG_DRAWING
void USmartObjectSimpleQueryTest::DebugDraw(ASmartObjectTestingActor& TestingActor, FDebugRenderSceneProxy* DebugProxy) const
{
	FVector TestLocation = TestingActor.GetActorLocation();
	DebugProxy->Boxes.Emplace(Request.QueryBox.ShiftBy(TestLocation), FColor::Yellow);

	const FVector Extent(10.f);
	for (const FSmartObjectRequestResult& Result : Results)
	{
		FVector Location = TestingActor.GetSubsystemRef().GetSlotLocation(Result).GetValue();

		DebugProxy->Boxes.Emplace(FBox(Location - Extent, Location + Extent), FColor::Yellow);
		DebugProxy->Lines.Emplace(Location, TestLocation, FColor::Orange);
	}
}
#endif // UE_ENABLE_DEBUG_DRAWING


//----------------------------------------------------------------------//
// USmartObjectTestRenderingComponent
//----------------------------------------------------------------------//
FBoxSphereBounds USmartObjectTestRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (const ASmartObjectTestingActor* TestingActor = Cast<ASmartObjectTestingActor>(GetOwner()))
	{
		return TestingActor->CalcTestsBounds();
	}
	return FBox(ForceInit);
}

void USmartObjectTestRenderingComponent::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ensureMsgf(Cast<ASmartObjectTestingActor>(GetOwner()), TEXT("SmartObjectTestRenderingComponent is expected to be used only by SmartObjectTestingActor"));
	}
}

#if UE_ENABLE_DEBUG_DRAWING
void USmartObjectTestRenderingComponent::DebugDraw(FDebugRenderSceneProxy* DebugProxy)
{
	if (ASmartObjectTestingActor* TestingActor = Cast<ASmartObjectTestingActor>(GetOwner()))
	{
		TestingActor->ExecuteOnEachTest([DebugProxy](const USmartObjectTest& Test) { Test.DebugDraw(DebugProxy); });
	}
}

void USmartObjectTestRenderingComponent::DebugDrawCanvas(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (ASmartObjectTestingActor* TestingActor = Cast<ASmartObjectTestingActor>(GetOwner()))
	{
		TestingActor->ExecuteOnEachTest([Canvas, PlayerController](const USmartObjectTest& Test) { Test.DebugDrawCanvas(Canvas, PlayerController); });
	}
}
#endif // UE_ENABLE_DEBUG_DRAWING


//----------------------------------------------------------------------//
// ASmartObjectTestingActor
//----------------------------------------------------------------------//
ASmartObjectTestingActor::ASmartObjectTestingActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RenderingComponent = CreateDefaultSubobject<USmartObjectTestRenderingComponent>(TEXT("RenderingComp"));
	RootComponent = RenderingComponent;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SetCanBeDamaged(false);
}

void ASmartObjectTestingActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	SmartObjectSubsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(GetWorld());
}

void ASmartObjectTestingActor::ExecuteOnEachTest(const TFunctionRef<void(USmartObjectTest&)> ExecFunc)
{
	for (USmartObjectTest* Test : Tests)
	{
		if (Test != nullptr)
		{
			ExecFunc(*Test);
		}
	}
}

void ASmartObjectTestingActor::ExecuteOnEachTest(const TFunctionRef<void(const USmartObjectTest&)> ExecFunc) const
{
	for (const USmartObjectTest* Test : Tests)
	{
		if (Test != nullptr)
		{
			ExecFunc(*Test);
		}
	}
}

void ASmartObjectTestingActor::RunTests()
{
	bool bRedrawRequired = false;
	ExecuteOnEachTest([&bRedrawRequired](USmartObjectTest& Test) { bRedrawRequired = Test.RunTest() || bRedrawRequired; });

	if (bRedrawRequired)
	{
		RenderingComponent->MarkRenderStateDirty();
	}
}

void ASmartObjectTestingActor::ResetTests()
{
	bool bRedrawRequired = false;
	ExecuteOnEachTest([&bRedrawRequired](USmartObjectTest& Test) { bRedrawRequired = Test.ResetTest() || bRedrawRequired; });

	if (bRedrawRequired)
	{
		RenderingComponent->MarkRenderStateDirty();
	}
}

bool ASmartObjectTestingActor::ShouldTickIfViewportsOnly() const
{
	// Allow the actor to be ticked in the Editor world without a running simulation
	return true;
}

void ASmartObjectTestingActor::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bRunTestsEachFrame)
	{
		RunTests();
	}
}

FBox ASmartObjectTestingActor::CalcTestsBounds() const
{
	FBox CombinedBounds(ForceInit);
	ExecuteOnEachTest([&CombinedBounds](const USmartObjectTest& Test) { CombinedBounds += Test.CalcTestBounds(); });

	return CombinedBounds;
}

#if WITH_EDITOR
void ASmartObjectTestingActor::PostEditMove(const bool bFinished)
{
	Super::PostEditMove(bFinished);

	RunTests();

	// Force refresh since test results might be the same but reference position has changed
	RenderingComponent->MarkRenderStateDirty();
}

void ASmartObjectTestingActor::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// Force refresh for any change since it might affect test debug draw
	RenderingComponent->MarkRenderStateDirty();
}

void ASmartObjectTestingActor::DebugInitializeSubsystemRuntime()
{
#if WITH_SMARTOBJECT_DEBUG
	if (SmartObjectSubsystem != nullptr)
	{
		SmartObjectSubsystem->DebugInitializeRuntime();
	}

	ResetTests();
#endif // WITH_SMARTOBJECT_DEBUG
}

void ASmartObjectTestingActor::DebugCleanupSubsystemRuntime()
{
#if WITH_SMARTOBJECT_DEBUG
	if (SmartObjectSubsystem  != nullptr)
	{
		SmartObjectSubsystem->DebugCleanupRuntime();
	}

	ResetTests();
#endif // WITH_SMARTOBJECT_DEBUG
}

#endif // WITH_EDITOR
