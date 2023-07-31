// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonDetails.h"

#include "Animation/Skeleton.h"
#include "DetailLayoutBuilder.h"
#include "Misc/AssertionMacros.h"

TSharedRef<IDetailCustomization> FSkeletonDetails::MakeInstance()
{
	return MakeShareable(new FSkeletonDetails());
}

void FSkeletonDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(USkeleton, BoneTree));
}

