// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorAnimCurveBones.cpp: Montage classes that contains slots
=============================================================================*/ 

#include "Animation/EditorAnimCurveBoneLinks.h"

#define LOCTEXT_NAMESPACE "UEditorAnimCurveBoneLinks"

UEditorAnimCurveBoneLinks::UEditorAnimCurveBoneLinks(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MaxLOD (0xFF)
{
}

void UEditorAnimCurveBoneLinks::Initialize(const TWeakPtr<class IEditableSkeleton> InEditableSkeleton, const FSmartName& InCurveName, FOnAnimCurveBonesChange OnChangeIn)
{
	AnimCurveMetaData = nullptr;
	CurveName = InCurveName.DisplayName;
	OnChange = OnChangeIn;
	SetFlags(RF_Transactional);
}

void UEditorAnimCurveBoneLinks::Initialize(UAnimCurveMetaData* InAnimCurveMetaData, const FName& InCurveName, FOnAnimCurveBonesChange OnChangeIn)
{
	AnimCurveMetaData = InAnimCurveMetaData;
	CurveName = InCurveName;
	OnChange = OnChangeIn;
	SetFlags(RF_Transactional);
}

void UEditorAnimCurveBoneLinks::Refresh(const FSmartName& InCurveName, const TArray<FBoneReference>& CurrentLinks, uint8 InMaxLOD)
{
	if (AnimCurveMetaData.IsValid())
	{
		// double check the name
		CurveName = InCurveName.DisplayName;
		ConnectedBones = CurrentLinks;
		MaxLOD = InMaxLOD;
	}
}

void UEditorAnimCurveBoneLinks::Refresh(const FName& InCurveName, const TArray<FBoneReference>& CurrentLinks, uint8 InMaxLOD)
{
	if (AnimCurveMetaData.IsValid())
	{
		// double check the name
		CurveName = InCurveName;
		ConnectedBones = CurrentLinks;
		MaxLOD = InMaxLOD;
	}
}

void UEditorAnimCurveBoneLinks::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(OnChange.IsBound())
	{
		OnChange.Execute(this);
	}
}

#undef LOCTEXT_NAMESPACE
