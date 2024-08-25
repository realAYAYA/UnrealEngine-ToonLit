// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/DefaultRootCameraNode.h"

#include "Core/BlendStackCameraNode.h"
#include "Core/CameraMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DefaultRootCameraNode)

namespace UE::Cameras::Private
{

TObjectPtr<UBlendStackCameraNode> CreateBlendStack(
		UObject* This, const FObjectInitializer& ObjectInit,
		const FName& Name, bool bAutoPop = true, bool bBlendFirstCameraMode = false)
{
	TObjectPtr<UBlendStackCameraNode> NewBlendStack = ObjectInit.CreateDefaultSubobject<UBlendStackCameraNode>(
			This, Name);
	NewBlendStack->bAutoPop = bAutoPop;
	NewBlendStack->bBlendFirstCameraMode = bBlendFirstCameraMode;
	return NewBlendStack;
}

}  // namespace UE::Cameras::Private

UDefaultRootCameraNode::UDefaultRootCameraNode(const FObjectInitializer& ObjectInit)
	: Super(ObjectInit)
{
	using namespace UE::Cameras::Private;

	BaseLayer = CreateBlendStack(this, ObjectInit, TEXT("BaseLayer"), false, true);
	MainLayer = CreateBlendStack(this, ObjectInit, TEXT("MainLayer"));
	GlobalLayer = CreateBlendStack(this, ObjectInit, TEXT("GlobalLayer"), false, true);
	VisualLayer = CreateBlendStack(this, ObjectInit, TEXT("VisualLayer"), false, true);
}

FCameraNodeChildrenView UDefaultRootCameraNode::OnGetChildren()
{
	return FCameraNodeChildrenView({ BaseLayer, MainLayer, GlobalLayer, VisualLayer });
}

void UDefaultRootCameraNode::OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult)
{
	BaseLayer->Run(Params, OutResult);

	MainLayer->Run(Params, OutResult);

	GlobalLayer->Run(Params, OutResult);

	VisualLayer->Run(Params, OutResult);
}

void UDefaultRootCameraNode::OnActivateCameraMode(const FActivateCameraModeParams& Params)
{
	UBlendStackCameraNode* TargetStack = nullptr;
	switch (Params.Layer)
	{
		case ECameraModeLayer::Base:
			TargetStack = CastChecked<UBlendStackCameraNode>(BaseLayer);
			break;
		case ECameraModeLayer::Main:
			TargetStack = CastChecked<UBlendStackCameraNode>(MainLayer);
			break;
		case ECameraModeLayer::Global:
			TargetStack = CastChecked<UBlendStackCameraNode>(GlobalLayer);
			break;
		case ECameraModeLayer::Visual:
			TargetStack = CastChecked<UBlendStackCameraNode>(VisualLayer);
			break;
	}

	if (ensure(TargetStack))
	{
		FBlendStackCameraPushParams PushParams;
		PushParams.Evaluator = Params.Evaluator;
		PushParams.EvaluationContext = Params.EvaluationContext;
		PushParams.CameraMode = Params.CameraMode;
		TargetStack->Push(PushParams);
	}
}

