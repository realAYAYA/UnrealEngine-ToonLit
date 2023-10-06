// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigAssetUserData.h"

void UControlRigShapeLibraryLink::SetShapeLibrary(UControlRigShapeLibrary* InShapeLibrary)
{
	InvalidateCache();
	ShapeLibrary = InShapeLibrary;
	ShapeNames.Reset();
	if(ShapeLibrary)
	{
		for(const FControlRigShapeDefinition& Shape : ShapeLibrary->Shapes)
		{
			ShapeNames.Add(Shape.ShapeName);
		}
	}
}

const UNameSpacedUserData::FUserData* UControlRigShapeLibraryLink::GetUserData(const FString& InPath, FString* OutErrorMessage) const
{
	// rely on super to look it up from the cache
	if(const FUserData* ResultFromSuper = Super::GetUserData(InPath))
	{
		return ResultFromSuper;
	}

	if(ShapeLibrary)
	{
		if(InPath.Equals(GET_MEMBER_NAME_STRING_CHECKED(UControlRigShapeLibraryLink, ShapeLibrary), ESearchCase::CaseSensitive))
		{
			static const FProperty* ShapeLibraryProperty = FindPropertyByName(StaticClass(), GET_MEMBER_NAME_CHECKED(UControlRigShapeLibraryLink, ShapeLibrary));
			return StoreCacheForUserData({InPath, ShapeLibraryProperty, (const uint8*)ShapeLibrary});
		}
		if(InPath.Equals(DefaultShapePath, ESearchCase::CaseSensitive))
		{
			static const FProperty* DefaultShapeProperty = FindPropertyByName(FControlRigShapeDefinition::StaticStruct(), GET_MEMBER_NAME_CHECKED(FControlRigShapeDefinition, ShapeName));
			return StoreCacheForUserData({InPath, DefaultShapeProperty, (const uint8*)&ShapeLibrary->DefaultShape.ShapeName});
		}
		if(InPath.Equals(ShapeNamesPath, ESearchCase::CaseSensitive))
		{
			static const FProperty* ShapeNamesProperty = FindPropertyByName(StaticClass(), GET_MEMBER_NAME_CHECKED(UControlRigShapeLibraryLink, ShapeNames));
			return StoreCacheForUserData({InPath, ShapeNamesProperty, (const uint8*)&ShapeNames});
		}
		if(OutErrorMessage && OutErrorMessage->IsEmpty())
		{
			(*OutErrorMessage) = FString::Printf(PathNotFoundFormat, *InPath);
		}
		return nullptr;
	}

	if(OutErrorMessage && OutErrorMessage->IsEmpty())
	{
		(*OutErrorMessage) = FString::Printf(ShapeLibraryNullFormat, *InPath);
	}
	return nullptr;
}

const TArray<const UNameSpacedUserData::FUserData*>& UControlRigShapeLibraryLink::GetUserDataArray(const FString& InParentPath, FString* OutErrorMessage) const
{
	const TArray<const FUserData*>& ResultFromSuper = Super::GetUserDataArray(InParentPath);
	if(!ResultFromSuper.IsEmpty())
	{
		return ResultFromSuper;
	}

	if(ShapeLibrary)
	{
		// UControlRigShapeLibraryLink doesn't offer any arrays other than the top level 
		if(InParentPath.IsEmpty())
		{
			const FUserData* ShapeLibraryUserData = GetUserData(GET_MEMBER_NAME_STRING_CHECKED(UControlRigShapeLibraryLink, ShapeLibrary), OutErrorMessage);
			const FUserData* DefaultShapeUserData = GetUserData(DefaultShapePath, OutErrorMessage);
			const FUserData* ShapeNamesUserData = GetUserData(ShapeNamesPath, OutErrorMessage);
			check(ShapeLibraryUserData);
			check(DefaultShapeUserData);
			check(ShapeNamesUserData);
			return StoreCacheForUserDataArray(InParentPath, {ShapeLibraryUserData, DefaultShapeUserData, ShapeNamesUserData});
		}
		return EmptyUserDatas;
	}

	if(OutErrorMessage && OutErrorMessage->IsEmpty())
	{
		(*OutErrorMessage) = FString::Printf(ShapeLibraryNullFormat, *InParentPath);
	}
	return EmptyUserDatas;
}

#if WITH_EDITOR

void UControlRigShapeLibraryLink::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const FProperty* ShapeLibraryProperty= FindPropertyByName(GetClass(), GET_MEMBER_NAME_CHECKED(UControlRigShapeLibraryLink, ShapeLibrary));
	if(PropertyChangedEvent.Property == ShapeLibraryProperty ||
		PropertyChangedEvent.MemberProperty == ShapeLibraryProperty)
	{
		SetShapeLibrary(ShapeLibrary);
	}
}

#endif
