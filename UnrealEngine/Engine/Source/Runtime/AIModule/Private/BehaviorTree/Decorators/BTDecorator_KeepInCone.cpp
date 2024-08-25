// Copyright Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Decorators/BTDecorator_KeepInCone.h"
#include "GameFramework/Actor.h"
#include "BehaviorTree/BlackboardComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BTDecorator_KeepInCone)

UBTDecorator_KeepInCone::UBTDecorator_KeepInCone(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Keep in Cone";

	// accept only actors and vectors
	ConeOrigin.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_KeepInCone, ConeOrigin), AActor::StaticClass());
	ConeOrigin.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_KeepInCone, ConeOrigin));
	Observed.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_KeepInCone, Observed), AActor::StaticClass());
	Observed.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_KeepInCone, Observed));

	INIT_DECORATOR_NODE_NOTIFY_FLAGS();

	// KeepInCone always abort current branch
	bAllowAbortLowerPri = false;
	bAllowAbortNone = false;
	FlowAbortMode = EBTFlowAbortMode::Self;
	
	ConeOrigin.SelectedKeyName = FBlackboard::KeySelf;
	ConeHalfAngle = 45.0f;
}

void UBTDecorator_KeepInCone::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	ConeHalfAngleDot = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngle));
	
	if (bUseSelfAsOrigin)
	{
		ConeOrigin.SelectedKeyName = FBlackboard::KeySelf;
		bUseSelfAsOrigin = false;
	}

	if (bUseSelfAsObserved)
	{
		Observed.SelectedKeyName = FBlackboard::KeySelf;
		bUseSelfAsObserved = false;
	}

	UBlackboardData* BBAsset = GetBlackboardAsset();
	if (ensure(BBAsset))
	{
		ConeOrigin.ResolveSelectedKey(*BBAsset);
		Observed.ResolveSelectedKey(*BBAsset);
	}
}

bool UBTDecorator_KeepInCone::CalculateCurrentDirection(const UBehaviorTreeComponent& OwnerComp, FVector& Direction) const
{
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (BlackboardComp == nullptr)
	{
		return false;
	}

	FVector PointA = FVector::ZeroVector;
	FVector PointB = FVector::ZeroVector;
	const bool bHasPointA = BlackboardComp->GetLocationFromEntry(ConeOrigin.GetSelectedKeyID(), PointA);
	const bool bHasPointB = BlackboardComp->GetLocationFromEntry(Observed.GetSelectedKeyID(), PointB);

	if (bHasPointA && bHasPointB)
	{
		Direction = (PointB - PointA).GetSafeNormal();
		return true;
	}

	return false;
}

void UBTDecorator_KeepInCone::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	TNodeInstanceMemory* DecoratorMemory = CastInstanceNodeMemory<TNodeInstanceMemory>(NodeMemory);
	FVector InitialDir(1.0f, 0, 0);

	CalculateCurrentDirection(OwnerComp, InitialDir);
	DecoratorMemory->InitialDirection = InitialDir;
}

void UBTDecorator_KeepInCone::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	const TNodeInstanceMemory* DecoratorMemory = CastInstanceNodeMemory<TNodeInstanceMemory>(NodeMemory);
	FVector CurrentDir(1.0f, 0, 0);
	
	if (CalculateCurrentDirection(OwnerComp, CurrentDir))
	{
		const FVector::FReal Angle = DecoratorMemory->InitialDirection.CosineAngle2D(CurrentDir);
		if (Angle < ConeHalfAngleDot || (IsInversed() && Angle > ConeHalfAngleDot))
		{
			OwnerComp.RequestExecution(this);
		}
	}
}

FString UBTDecorator_KeepInCone::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: %s in %.2f degree cone of initial direction [%s-%s]"),
		*Super::GetStaticDescription(),
		*Observed.SelectedKeyName.ToString(),
		ConeHalfAngle * 2,
		*ConeOrigin.SelectedKeyName.ToString(),
		*Observed.SelectedKeyName.ToString());
}

void UBTDecorator_KeepInCone::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	const TNodeInstanceMemory* DecoratorMemory = CastInstanceNodeMemory<TNodeInstanceMemory>(NodeMemory);
	FVector CurrentDir(1.0f, 0, 0);
	
	if (CalculateCurrentDirection(OwnerComp, CurrentDir))
	{
		const FVector::FReal CurrentAngleDot = DecoratorMemory->InitialDirection.CosineAngle2D(CurrentDir);
		const FVector::FReal CurrentAngleRad = FMath::Acos(CurrentAngleDot);

		Values.Add(FString::Printf(TEXT("Angle: %.0f (%s cone)"),
			FMath::RadiansToDegrees(CurrentAngleRad),
			CurrentAngleDot < ConeHalfAngleDot ? TEXT("outside") : TEXT("inside")
			));

	}
}

uint16 UBTDecorator_KeepInCone::GetInstanceMemorySize() const
{
	return sizeof(TNodeInstanceMemory);
}

void UBTDecorator_KeepInCone::InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const
{
	InitializeNodeMemory<TNodeInstanceMemory>(NodeMemory, InitType);
}

void UBTDecorator_KeepInCone::CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const
{
	CleanupNodeMemory<TNodeInstanceMemory>(NodeMemory, CleanupType);
}

#if WITH_EDITOR

FName UBTDecorator_KeepInCone::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.KeepInCone.Icon");
}

#endif	// WITH_EDITOR

