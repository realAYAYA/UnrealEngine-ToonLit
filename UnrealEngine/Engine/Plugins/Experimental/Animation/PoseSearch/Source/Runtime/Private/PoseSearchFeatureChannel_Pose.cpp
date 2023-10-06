// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_Pose.h"
#include "Animation/Skeleton.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchDerivedDataKey.h"
#include "PoseSearchFeatureChannel_Heading.h"
#include "PoseSearchFeatureChannel_Phase.h"
#include "PoseSearchFeatureChannel_Position.h"
#include "PoseSearchFeatureChannel_Velocity.h"

#define LOCTEXT_NAMESPACE "PoseSearchFeatureChannels"

UPoseSearchFeatureChannel_Pose::UPoseSearchFeatureChannel_Pose()
{
	// defaulting UPoseSearchFeatureChannel_Pose for a meaningful locomotion setup
	Weight = 1.f;
	SampledBones.Add(FPoseSearchBone({ {"foot_l"}, int32(EPoseSearchBoneFlags::Position | EPoseSearchBoneFlags::Velocity), 1.f, FLinearColor::Green }));
	SampledBones.Add(FPoseSearchBone({ {"foot_r"}, int32(EPoseSearchBoneFlags::Position | EPoseSearchBoneFlags::Velocity), 1.f, FLinearColor::Green}));
}

void UPoseSearchFeatureChannel_Pose::Finalize(UPoseSearchSchema* Schema)
{
	SubChannels.Reset();

	for (int32 ChannelBoneIdx = 0; ChannelBoneIdx != SampledBones.Num(); ++ChannelBoneIdx)
	{
		const FPoseSearchBone& SampledBone = SampledBones[ChannelBoneIdx];
		if (EnumHasAnyFlags(SampledBone.Flags, EPoseSearchBoneFlags::Position))
		{
			UPoseSearchFeatureChannel_Position* Position = NewObject<UPoseSearchFeatureChannel_Position>(this, NAME_None, RF_Transient);
			Position->Bone = SampledBone.Reference;
			Position->Weight = SampledBone.Weight * Weight;
			Position->SampleTimeOffset = 0.f;
			Position->DebugColor = SampledBone.DebugColor;
			Position->InputQueryPose = InputQueryPose;
			SubChannels.Add(Position);
		}

		if (EnumHasAnyFlags(SampledBone.Flags, EPoseSearchBoneFlags::Rotation))
		{
			UPoseSearchFeatureChannel_Heading* HeadingX = NewObject<UPoseSearchFeatureChannel_Heading>(this, NAME_None, RF_Transient);
			HeadingX->Bone = SampledBone.Reference;
			HeadingX->Weight = SampledBone.Weight * Weight;
			HeadingX->SampleTimeOffset = 0.f;
			HeadingX->HeadingAxis = EHeadingAxis::X;
			HeadingX->DebugColor = SampledBone.DebugColor;
			HeadingX->InputQueryPose = InputQueryPose;
			SubChannels.Add(HeadingX);

			UPoseSearchFeatureChannel_Heading* HeadingY = NewObject<UPoseSearchFeatureChannel_Heading>(this, NAME_None, RF_Transient);
			HeadingY->Bone = SampledBone.Reference;
			HeadingY->Weight = SampledBone.Weight * Weight;
			HeadingY->SampleTimeOffset = 0.f;
			HeadingY->HeadingAxis = EHeadingAxis::Y;
			HeadingY->DebugColor = SampledBone.DebugColor;
			HeadingY->InputQueryPose = InputQueryPose;
			SubChannels.Add(HeadingY);
		}

		if (EnumHasAnyFlags(SampledBone.Flags, EPoseSearchBoneFlags::Velocity))
		{
			UPoseSearchFeatureChannel_Velocity* Velocity = NewObject<UPoseSearchFeatureChannel_Velocity>(this, NAME_None, RF_Transient);
			Velocity->Bone = SampledBone.Reference;
			Velocity->Weight = SampledBone.Weight * Weight;
			Velocity->SampleTimeOffset = 0.f;
			Velocity->DebugColor = SampledBone.DebugColor;
			Velocity->InputQueryPose = InputQueryPose;
			Velocity->bUseCharacterSpaceVelocities = bUseCharacterSpaceVelocities;
			SubChannels.Add(Velocity);
		}

		if (EnumHasAnyFlags(SampledBone.Flags, EPoseSearchBoneFlags::Phase))
		{
			UPoseSearchFeatureChannel_Phase* Phase = NewObject<UPoseSearchFeatureChannel_Phase>(this, NAME_None, RF_Transient);
			Phase->Bone = SampledBone.Reference;
			Phase->Weight = SampledBone.Weight * Weight;
			Phase->DebugColor = SampledBone.DebugColor;
			Phase->InputQueryPose = InputQueryPose;
			SubChannels.Add(Phase);
		}
	}

	Super::Finalize(Schema);
}

#if WITH_EDITOR
FString UPoseSearchFeatureChannel_Pose::GetLabel() const
{
	TStringBuilder<256> Label;
	if (const UPoseSearchFeatureChannel* OuterChannel = Cast<UPoseSearchFeatureChannel>(GetOuter()))
	{
		Label.Append(OuterChannel->GetLabel());
		Label.Append(TEXT("_"));
	}
	Label.Append(TEXT("Pose"));
	return Label.ToString();
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE