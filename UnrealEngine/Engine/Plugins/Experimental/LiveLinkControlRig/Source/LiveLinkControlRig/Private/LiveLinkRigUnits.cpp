// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkRigUnits.h"
#include "Units/RigUnitContext.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "Roles/LiveLinkAnimationRole.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"

ILiveLinkClient* LiveLinkControlRigUtilities::TryGetLiveLinkClient()
{
	static ILiveLinkClient* LiveLinkClient = nullptr;

	if (LiveLinkClient == nullptr)
	{
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		}
	}
	return LiveLinkClient;
}

FRigUnit_LiveLinkEvaluteFrameAnimation_Execute()
{
	switch (Context.State)
	{
	case EControlRigState::Init:
	case EControlRigState::Update:
	{
		// Ensure a default value in case the evaluation fails
		SubjectFrame = FSubjectFrameHandle();

		// Try to update SubjectFrame from Live Link
		if (ILiveLinkClient* LiveLinkClient = LiveLinkControlRigUtilities::TryGetLiveLinkClient())
		{
			FLiveLinkSubjectFrameData FrameData;
			if (LiveLinkClient->EvaluateFrame_AnyThread(SubjectName, ULiveLinkAnimationRole::StaticClass(), FrameData))
			{
				FLiveLinkBlueprintDataStruct BlueprintStruct(FSubjectFrameHandle::StaticStruct(), &SubjectFrame);
				GetDefault<ULiveLinkAnimationRole>()->InitializeBlueprintData(FrameData, BlueprintStruct);
			}
		}

		// Debug Drawing if required
		if (bDrawDebug && Context.DrawInterface != nullptr)
		{
			/*
			FRigBoneHierarchy DebugHierarchy;

			for (int BoneIdx = 0; BoneIdx < SubjectFrame.GetNumberOfTransforms(); ++BoneIdx)
			{
				FLiveLinkTransform CurrentTransform;
				SubjectFrame.GetTransformByIndex(BoneIdx, CurrentTransform);
				FName CurrentBoneName;
				CurrentTransform.GetName(CurrentBoneName);
				FName CurrentParentName(NAME_None);
				if (CurrentTransform.HasParent())
				{
					FLiveLinkTransform ParentTransform;
					CurrentTransform.GetParent(ParentTransform);
					ParentTransform.GetName(CurrentParentName);
				}
				FTransform LocalSpaceTransform, GlobalSpaceTransform;
				CurrentTransform.GetTransformParentSpace(LocalSpaceTransform);
				CurrentTransform.GetTransformRootSpace(GlobalSpaceTransform);
				DebugHierarchy.Add(CurrentBoneName, CurrentParentName, ERigBoneType::User, LocalSpaceTransform, LocalSpaceTransform, GlobalSpaceTransform);
			}
			Context.DrawInterface->DrawHierarchy(DebugDrawOffset, DebugHierarchy, EControlRigDrawHierarchyMode::Axes, 1.0f, DebugColor, 1.0f);
			*/
		}
	}
		break;
	default:
		checkNoEntry(); // Execute is only defined for Init and Update
		break;
	}
}

FRigUnit_LiveLinkGetTransformByName_Execute()
{
	switch (Context.State)
	{
	case EControlRigState::Init:
	case EControlRigState::Update:
	{
		// Ensure a default value in case the evaluation fails
		Transform = FTransform::Identity;

		// Try to update Transform from the Subject Frame
		FLiveLinkTransform LiveLinkTransform;
		SubjectFrame.GetTransformByName(TransformName, LiveLinkTransform);
		switch (Space)
		{
		case EBoneGetterSetterMode::GlobalSpace:
			LiveLinkTransform.GetTransformRootSpace(Transform);
			break;
		case EBoneGetterSetterMode::LocalSpace:
			LiveLinkTransform.GetTransformParentSpace(Transform);
			break;
		default:
			checkNoEntry(); // Unknown bone space. We should never hit this
			break;
		}
	}
		break;
	default:
		checkNoEntry(); // Execute is only defined for Init and Update
		break;
	}
}

FRigUnit_LiveLinkGetParameterValueByName_Execute()
{
	switch (Context.State)
	{
	case EControlRigState::Init:
	case EControlRigState::Update:
	{
		float NewValue;
		if (SubjectFrame.GetCurveValueByName(ParameterName, NewValue))
		{
			Value = NewValue;
		}
		else
		{
			Value = 0.0f;
		}
	}
		break;
	default:
		checkNoEntry(); // Execute is only defined for Init and Update
		break;
	}
}

FRigUnit_LiveLinkEvaluteFrameTransform_Execute()
{
	switch (Context.State)
	{
	case EControlRigState::Init:
	case EControlRigState::Update:
	{
		// Ensure a default value in case the evaluation fails
		Transform = FTransform::Identity;

		// Try to update Transform from Live Link
		if (ILiveLinkClient* LiveLinkClient = LiveLinkControlRigUtilities::TryGetLiveLinkClient())
		{
			FLiveLinkSubjectFrameData FrameData;
			if (LiveLinkClient->EvaluateFrame_AnyThread(SubjectName, ULiveLinkTransformRole::StaticClass(), FrameData))
			{
				FLiveLinkTransformFrameData* TransformData = FrameData.FrameData.Cast<FLiveLinkTransformFrameData>();
				Transform = TransformData->Transform;
			}
		}

		// Debug Drawing if required
		if (bDrawDebug && Context.DrawInterface != nullptr)
		{
			// Rather than drawing as an axis guide, draw as a hierarchy with a dummy root bone which makes it clearer what the origin of the transform
			// space is

			/*
			FRigBoneHierarchy DebugHierarchy;

			FName RootName("root");
			FName TransformName("transform");
			DebugHierarchy.Add(RootName, FName(NAME_None), ERigBoneType::User, FTransform::Identity, FTransform::Identity, FTransform::Identity);
			DebugHierarchy.Add(TransformName, RootName, ERigBoneType::User, Transform, Transform, Transform);

			Context.DrawInterface->DrawHierarchy(DebugDrawOffset, DebugHierarchy, EControlRigDrawHierarchyMode::Axes, 1.0f, DebugColor, 1.0f);
			*/
		}
	}
		break;
	default:
		checkNoEntry(); // Execute is only defined for Init and Update
		break;
	}
}