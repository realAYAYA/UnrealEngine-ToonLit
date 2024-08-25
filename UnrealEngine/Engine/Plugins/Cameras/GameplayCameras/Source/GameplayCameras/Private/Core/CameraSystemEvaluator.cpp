// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CameraSystemEvaluator.h"

#include "Camera/CameraTypes.h"
#include "Core/CameraAsset.h"
#include "Core/CameraEvaluationContext.h"
#include "Core/CameraMode.h"
#include "Core/DefaultRootCameraNode.h"
#include "IGameplayCamerasModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraSystemEvaluator)

DECLARE_CYCLE_STAT(TEXT("Camera System Eval"), CameraSystemEval_Total, STATGROUP_CameraSystem);

UCameraSystemEvaluator::UCameraSystemEvaluator(const FObjectInitializer& ObjectInit)
	: Super(ObjectInit)
{
	RootNode = ObjectInit.CreateDefaultSubobject<UDefaultRootCameraNode>(this, TEXT("RootNode"));

	ContextStack.Initialize(this);
}

void UCameraSystemEvaluator::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UCameraSystemEvaluator* TypedThis = CastChecked<UCameraSystemEvaluator>(InThis);
	TypedThis->ContextStack.AddReferencedObjects(Collector);
	TypedThis->Instantiator.AddReferencedObjects(Collector);
}

void UCameraSystemEvaluator::PushEvaluationContext(UCameraEvaluationContext* EvaluationContext)
{
	ContextStack.PushContext(EvaluationContext);
}

void UCameraSystemEvaluator::RemoveEvaluationContext(UCameraEvaluationContext* EvaluationContext)
{
	ContextStack.RemoveContext(EvaluationContext);
}

void UCameraSystemEvaluator::PopEvaluationContext()
{
	ContextStack.PopContext();
}

void UCameraSystemEvaluator::Update(const FCameraSystemEvaluationUpdateParams& Params)
{
	SCOPE_CYCLE_COUNTER(CameraSystemEval_Total);

	// Get the active evaluation context.
	FCameraEvaluationContextInfo ActiveContextInfo = ContextStack.GetActiveContext();
	if (UNLIKELY(!ActiveContextInfo.IsValid()))
	{
		Result.bIsValid = false;
		return;
	}

	// Run the camera director, and activate any camera mode(s) it returns to us.
	UCameraDirector* ActiveDirector = ActiveContextInfo.CameraDirector;
	if (ActiveDirector)
	{
		FCameraDirectorRunParams DirectorParams;
		DirectorParams.DeltaTime = Params.DeltaTime;
		DirectorParams.OwnerContext = ActiveContextInfo.EvaluationContext;

		FCameraDirectorRunResult DirectorResult;

		ActiveDirector->Run(DirectorParams, DirectorResult);

		if (DirectorResult.ActiveCameraModes.Num() == 1)
		{
			FActivateCameraModeParams CameraModeParams;
			CameraModeParams.Evaluator = this;
			CameraModeParams.EvaluationContext = ActiveContextInfo.EvaluationContext;
			CameraModeParams.CameraMode = DirectorResult.ActiveCameraModes[0];
			RootNode->ActivateCameraMode(CameraModeParams);
		}
	}

	// Run the root camera node.
	FCameraNodeRunParams NodeParams;
	NodeParams.Evaluator = this;
	NodeParams.DeltaTime = Params.DeltaTime;

	RootNodeResult.Reset();

	RootNode->Run(NodeParams, RootNodeResult);

	Result.CameraPose = RootNodeResult.CameraPose;
	Result.bIsCameraCut = RootNodeResult.bIsCameraCut;
	Result.bIsValid = true;
}

void UCameraSystemEvaluator::GetEvaluatedCameraView(FMinimalViewInfo& DesiredView)
{
	const FCameraPose& CameraPose = Result.CameraPose;
	DesiredView.Location = CameraPose.GetLocation();
	DesiredView.Rotation = CameraPose.GetRotation();
	DesiredView.FOV = CameraPose.GetEffectiveFieldOfView();
}

FCameraRuntimeInstantiator& UCameraSystemEvaluator::GetRuntimeInstantiator()
{
	return Instantiator;
}

