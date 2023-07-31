// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_RemapCurvesFromMesh.h"

#include "CurveExpressionModule.h"
#include "ExpressionEvaluator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_RemapCurvesFromMesh)

void FAnimNode_RemapCurvesFromMesh::Initialize_AnyThread(
	const FAnimationInitializeContext& Context
	)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	Super::Initialize_AnyThread(Context);
	SourcePose.Initialize(Context);
}


void FAnimNode_RemapCurvesFromMesh::CacheBones_AnyThread(
	const FAnimationCacheBonesContext& Context
	)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	Super::CacheBones_AnyThread(Context);
	SourcePose.CacheBones(Context);
}
	

void FAnimNode_RemapCurvesFromMesh::VerifyExpressions()
{
	using namespace CurveExpression::Evaluator;

	if (!ExpressionEngine.IsSet() || CurveExpressions.IsEmpty())
	{
		return;
	}

	FEngine VerificationEngine(
		ExpressionEngine->GetConstantValues(),
		EParseFlags::ValidateConstants);

	for (const TPair<FName, FString>& ExpressionPair: CurveExpressions)
	{
		if (!CurveNameToUIDMap.Contains(ExpressionPair.Key))
		{
			UE_LOG(LogCurveExpression, Warning, TEXT("Target curve '%s' does not exist."), *ExpressionPair.Key.ToString());
		}

		TOptional<FParseError> Error = VerificationEngine.Verify(ExpressionPair.Value);
		if (Error.IsSet())
		{
			UE_LOG(LogCurveExpression, Warning, TEXT("Expression error in '%s': %s"), *ExpressionPair.Value, *Error->Message);
		}
	}
}


void FAnimNode_RemapCurvesFromMesh::Update_AnyThread(
	const FAnimationUpdateContext& Context
	)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread)

	// Run update on input pose nodes
	SourcePose.Update(Context);

	// Evaluate any BP logic plugged into this node
	GetEvaluateGraphExposedInputs().Execute(Context);
	
	if (bExpressionsImmutable && ExpressionEngine.IsSet() && !CurveExpressions.IsEmpty() && CachedExpressions.IsEmpty())
	{
		CachedExpressions.Reset();
		for (const TPair<FName, FString>& ExpressionPair: CurveExpressions)
		{
			if (CurveNameToUIDMap.Contains(ExpressionPair.Key))
			{
				using namespace CurveExpression::Evaluator;

				TVariant<FExpressionObject, FParseError> Result = ExpressionEngine->Parse(ExpressionPair.Value);
				if (const FExpressionObject* Expression = Result.TryGet<FExpressionObject>())
				{
					CachedExpressions.Add(ExpressionPair.Key, *Expression);
				}
			}
		}
	}			
}


void FAnimNode_RemapCurvesFromMesh::Evaluate_AnyThread(
	FPoseContext& Output
	)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)

	FPoseContext SourceData(Output);
	SourcePose.Evaluate(SourceData);
	
	Output = SourceData;
	
	// If we have an expression engine, evaluate the expressions that have a matching target curve.
	// If the expressions are not immutable between compiles, then we need to reparse them each time. 
	if (ExpressionEngine.IsSet())
	{
		if (bExpressionsImmutable)
		{
			for (const TTuple<FName, CurveExpression::Evaluator::FExpressionObject>& ExpressionItem: CachedExpressions)
			{
				if (const SmartName::UID_Type* UID = CurveNameToUIDMap.Find(ExpressionItem.Key))
				{
					Output.Curve.Set(*UID, ExpressionEngine->Execute(ExpressionItem.Value));
				}
			}
		}
		else
		{
			for (const TTuple<FName, FString>& ExpressionItem: CurveExpressions)
			{
				if (const SmartName::UID_Type* UID = CurveNameToUIDMap.Find(ExpressionItem.Key))
				{
					TOptional<float> Result = ExpressionEngine->Evaluate(ExpressionItem.Value);
					if (Result.IsSet())
					{
						Output.Curve.Set(*UID, *Result);
					}
				}
			}
		}
	}
}


void FAnimNode_RemapCurvesFromMesh::GatherDebugData(
	FNodeDebugData& DebugData
	)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += FString::Printf(TEXT("('%s')"), *GetNameSafe(CurrentlyUsedSourceMeshComponent.IsValid() ? CurrentlyUsedSourceMeshComponent.Get()->GetSkeletalMeshAsset() : nullptr));
	DebugData.AddDebugItem(DebugLine, true);
}


void FAnimNode_RemapCurvesFromMesh::PreUpdate(
	const UAnimInstance* InAnimInstance
	)
{
	QUICK_SCOPE_CYCLE_COUNTER(FAnimNode_RemapCurvesFromMesh_PreUpdate);

	// Make sure we're using the correct source and target skeleton components, since they
	// may have changed from underneath us.
	RefreshMeshComponent(InAnimInstance->GetSkelMeshComponent());

	const USkeletalMeshComponent* CurrentMeshComponent = CurrentlyUsedSourceMeshComponent.IsValid() ? CurrentlyUsedSourceMeshComponent.Get() : nullptr;

	if (CurrentMeshComponent && CurrentMeshComponent->GetSkeletalMeshAsset() && CurrentMeshComponent->IsRegistered())
	{
		// If our source is running under leader-pose, then get bone data from there
		if(USkeletalMeshComponent* LeaderPoseComponent = Cast<USkeletalMeshComponent>(CurrentMeshComponent->LeaderPoseComponent.Get()))
		{
			CurrentMeshComponent = LeaderPoseComponent;
		}

		// re-check mesh component validity as it may have changed to leader
		if(CurrentMeshComponent->GetSkeletalMeshAsset() && CurrentMeshComponent->IsRegistered())
		{
			if (ExpressionEngine.IsSet())
			{
				if (const UAnimInstance* SourceAnimInstance = CurrentMeshComponent->GetAnimInstance())
				{
					ExpressionEngine->UpdateConstantValues(SourceAnimInstance->GetAnimationCurveList(EAnimCurveType::AttributeCurve));
				}
				else
				{
					ExpressionEngine.Reset();
				}
			}
		}
		else
		{
			ExpressionEngine.Reset();
		}
	}
}


void FAnimNode_RemapCurvesFromMesh::ReinitializeMeshComponent(
	USkeletalMeshComponent* InNewSkeletalMeshComponent, 
	USkeletalMeshComponent* InTargetMeshComponent
	)
{
	CurrentlyUsedSourceMeshComponent.Reset();
	CurrentlyUsedSourceMesh.Reset();
	CurrentlyUsedTargetMesh.Reset();
	
	ExpressionEngine.Reset();
	CurveNameToUIDMap.Reset();
	CachedExpressions.Reset();

	if (InTargetMeshComponent && IsValid(InNewSkeletalMeshComponent) && InNewSkeletalMeshComponent->GetSkeletalMeshAsset())
	{
		USkeletalMesh* SourceSkelMesh = InNewSkeletalMeshComponent->GetSkeletalMeshAsset();
		USkeletalMesh* TargetSkelMesh = InTargetMeshComponent->GetSkeletalMeshAsset();
		
		if (IsValid(SourceSkelMesh) && !SourceSkelMesh->HasAnyFlags(RF_NeedPostLoad) &&
			IsValid(TargetSkelMesh) && !TargetSkelMesh->HasAnyFlags(RF_NeedPostLoad))
		{
			CurrentlyUsedSourceMeshComponent = InNewSkeletalMeshComponent;
			CurrentlyUsedSourceMesh = SourceSkelMesh;
			CurrentlyUsedTargetMesh = TargetSkelMesh;

			// Grab the source curves and use their names to seed the expression evaluation engine.
			TMap<FName, float> SourceCurves;
			const USkeleton* SourceSkeleton = SourceSkelMesh->GetSkeleton();
			if (ensureMsgf(SourceSkeleton, TEXT("Invalid null source skeleton : %s"), *GetNameSafe(TargetSkelMesh)))
			{
				const FSmartNameMapping* SourceContainer = SourceSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

				SourceContainer->Iterate([&SourceCurves](const FSmartNameMappingIterator& Iterator)
				{
					if (FName CurveName; Iterator.GetName(CurveName))
					{
						SourceCurves.Add(CurveName, 0.0f);
					}
				});

				ExpressionEngine.Emplace(MoveTemp(SourceCurves));
			}

			// Grab the target curves and build the Name -> UID mapping so that we can call
			// FBlendCurve::Set in Evalute_AnyThread 
			const USkeleton* TargetSkeleton = TargetSkelMesh->GetSkeleton();

			if (ensureMsgf(TargetSkeleton, TEXT("Invalid null target skeleton : %s"), *GetNameSafe(TargetSkelMesh)))
			{
				const FSmartNameMapping* TargetContainer = TargetSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

				// CurveExpressions isn't filled in here, since we haven't evaluated BP downstream.
				// So instead, we just take a full copy of the smart mapping.
				TargetContainer->Iterate([this](const FSmartNameMappingIterator& Iterator)
				{
					if (FName CurveName; Iterator.GetName(CurveName))
					{
						CurveNameToUIDMap.Add(CurveName, Iterator.GetIndex());
					}
				});
			}
		}
	}
}
	

void FAnimNode_RemapCurvesFromMesh::RefreshMeshComponent(
	USkeletalMeshComponent* InTargetMeshComponent
	)
{
	auto ResetMeshComponent = [this](USkeletalMeshComponent* InMeshComponent, USkeletalMeshComponent* InTargetMeshComponent)
	{
		// if current mesh exists, but not same as input mesh
		if (const USkeletalMeshComponent* CurrentMeshComponent = CurrentlyUsedSourceMeshComponent.Get())
		{
			// if component has been changed, reinitialize
			if (CurrentMeshComponent != InMeshComponent)
			{
				ReinitializeMeshComponent(InMeshComponent, InTargetMeshComponent);
			}
			// if component is still same but mesh has been changed, we have to reinitialize
			else if (CurrentMeshComponent->GetSkeletalMeshAsset() != CurrentlyUsedSourceMesh.Get())
			{
				ReinitializeMeshComponent(InMeshComponent, InTargetMeshComponent);
			}
			else if (InTargetMeshComponent)
			{
				// see if target mesh has changed
				if (InTargetMeshComponent->GetSkeletalMeshAsset() != CurrentlyUsedTargetMesh.Get())
				{
					ReinitializeMeshComponent(InMeshComponent, InTargetMeshComponent);
				}
			}
		}
		// if not valid, but input mesh is
		else if (!CurrentMeshComponent && InMeshComponent)
		{
			ReinitializeMeshComponent(InMeshComponent, InTargetMeshComponent);
		}
	};

	if(SourceMeshComponent.IsValid())
	{
		ResetMeshComponent(SourceMeshComponent.Get(), InTargetMeshComponent);
	}
	else if (bUseAttachedParent)
	{
		if (InTargetMeshComponent)
		{
			if (USkeletalMeshComponent* ParentComponent = Cast<USkeletalMeshComponent>(InTargetMeshComponent->GetAttachParent()))
			{
				ResetMeshComponent(ParentComponent, InTargetMeshComponent);
			}
			else
			{
				CurrentlyUsedSourceMeshComponent.Reset();
			}
		}
		else
		{
			CurrentlyUsedSourceMeshComponent.Reset();
		}
	}
	else
	{
		CurrentlyUsedSourceMeshComponent.Reset();
	}
}

