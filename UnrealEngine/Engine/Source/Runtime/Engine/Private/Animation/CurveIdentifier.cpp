// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimData/CurveIdentifier.h"
#include "EngineLogs.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CurveIdentifier)

#if WITH_EDITOR
FAnimationCurveIdentifier UAnimationCurveIdentifierExtensions::GetCurveIdentifier(USkeleton* InSkeleton, FName Name, ERawCurveTrackTypes CurveType)
{
	FAnimationCurveIdentifier Identifier;

	if (InSkeleton)
	{
		const FName MappingName = CurveType == ERawCurveTrackTypes::RCT_Float ? USkeleton::AnimCurveMappingName : USkeleton::AnimTrackCurveMappingName;
		const bool bExists = InSkeleton->GetSmartNameByName(MappingName, Name, Identifier.InternalName);
		if (!bExists)
		{
			InSkeleton->AddSmartNameAndModify(MappingName, Name, Identifier.InternalName);
		}

		Identifier.CurveType = CurveType;
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("Invalid Skeleton provided for GetCurveIdentifier"));
	}

	return Identifier;
}

FAnimationCurveIdentifier UAnimationCurveIdentifierExtensions::FindCurveIdentifier(const USkeleton* InSkeleton, FName Name, ERawCurveTrackTypes CurveType)
{
	FAnimationCurveIdentifier Identifier;

	if (InSkeleton)
	{
		const FName MappingName = CurveType == ERawCurveTrackTypes::RCT_Float ? USkeleton::AnimCurveMappingName : USkeleton::AnimTrackCurveMappingName;
		const bool bExists = InSkeleton->GetSmartNameByName(MappingName, Name, Identifier.InternalName);
		Identifier.CurveType = CurveType;
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("Invalid Skeleton provided for FindCurveIdentifier"));
	}

	return Identifier;
}

TArray<FAnimationCurveIdentifier> UAnimationCurveIdentifierExtensions::GetCurveIdentifiers(USkeleton* InSkeleton, ERawCurveTrackTypes CurveType)
{
	TArray<FAnimationCurveIdentifier> Identifiers;
	if (InSkeleton)
	{
		const FName MappingName = CurveType == ERawCurveTrackTypes::RCT_Float ? USkeleton::AnimCurveMappingName : USkeleton::AnimTrackCurveMappingName;
		const FSmartNameMapping* SmartNameContainer = InSkeleton->GetSmartNameContainer(MappingName);
		if (SmartNameContainer)
		{
			TArray<SmartName::UID_Type> UIDs;
			SmartNameContainer->FillUidArray(UIDs);

			for (const SmartName::UID_Type& UID : UIDs)
			{
				FSmartName SmartName;
				if (SmartNameContainer->FindSmartNameByUID(UID, SmartName))
				{

					Identifiers.Add(FAnimationCurveIdentifier(SmartName, CurveType));
				}
			}
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("Unable to find smartname mapping %s in Skeleton (%s)"), *MappingName.ToString(), *InSkeleton->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("Invalid Skeleton provided for GetCurveIdentifiers"));
	}

	return Identifiers;
}

bool UAnimationCurveIdentifierExtensions::GetTransformChildCurveIdentifier(FAnimationCurveIdentifier& InOutIdentifier, ETransformCurveChannel Channel, EVectorCurveChannel Axis)
{
	if (InOutIdentifier.IsValid())
	{
		if (InOutIdentifier.CurveType == ERawCurveTrackTypes::RCT_Transform)
		{
			InOutIdentifier.Channel = Channel;
			InOutIdentifier.Axis = Axis;

			return true;
		}
	}

	return false;
}
#endif // WITH_EDITOR


