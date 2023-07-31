// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkSourceSettings.h"
#include "UObject/EnterpriseObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LiveLinkSourceSettings)

void ULiveLinkSourceSettings::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FEnterpriseObjectVersion::GUID);
}

#if WITH_EDITOR
bool ULiveLinkSourceSettings::CanEditChange(const FProperty* InProperty) const
{
	if (Super::CanEditChange(InProperty))
	{
		if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FLiveLinkSourceBufferManagementSettings, TimecodeFrameOffset)
			|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FLiveLinkSourceBufferManagementSettings, ValidTimecodeFrame)
			|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FLiveLinkSourceBufferManagementSettings, bValidTimecodeFrameEnabled)
			|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FLiveLinkSourceBufferManagementSettings, bUseTimecodeSmoothLatest))
		{
			return Mode == ELiveLinkSourceMode::Timecode;
		}

		if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FLiveLinkSourceBufferManagementSettings, ValidEngineTime)
			|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FLiveLinkSourceBufferManagementSettings, EngineTimeOffset)
			|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FLiveLinkSourceBufferManagementSettings, bValidEngineTimeEnabled))
		{
			return Mode == ELiveLinkSourceMode::EngineTime;
		}

		return true;
	}
	return false;
}
#endif //WITH_EDITOR

