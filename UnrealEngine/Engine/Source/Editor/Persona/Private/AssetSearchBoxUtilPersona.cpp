// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetSearchBoxUtilPersona.h"
#include "ReferenceSkeleton.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Editor.h"

SAssetSearchBoxForBones::SAssetSearchBoxForBones()
{
	if(GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

SAssetSearchBoxForBones::~SAssetSearchBoxForBones()
{
	if(GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

void SAssetSearchBoxForBones::Construct( const FArguments& InArgs, const class UObject* Outer, TSharedPtr<class IPropertyHandle> BoneNameProperty )
{
	check(Outer);

	BonePropertyHandle = BoneNameProperty;
	// set delegate on property change
	// this doesn't work for undo still
	BonePropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &SAssetSearchBoxForBones::RefreshName));

	const USkeleton* Skeleton = NULL;

	TArray<FAssetSearchBoxSuggestion> PossibleSuggestions;
	if( const USkeletalMesh* SkeletalMesh = Cast<const USkeletalMesh>(Outer) )
	{
		if( InArgs._IncludeSocketsForSuggestions.Get() )
		{
			for( auto SocketIt=const_cast<USkeletalMesh*>(SkeletalMesh)->GetMeshOnlySocketList().CreateConstIterator(); SocketIt; ++SocketIt )
			{
				PossibleSuggestions.Add(FAssetSearchBoxSuggestion::MakeSimpleSuggestion((*SocketIt)->SocketName.ToString()));
			}
		}
		Skeleton = SkeletalMesh->GetSkeleton();
	}
	else
	{
		Skeleton = Cast<const USkeleton>(Outer);
	}
	if( Skeleton && InArgs._IncludeSocketsForSuggestions.Get() )
	{
		for( auto SocketIt=Skeleton->Sockets.CreateConstIterator(); SocketIt; ++SocketIt )
		{
			PossibleSuggestions.Add(FAssetSearchBoxSuggestion::MakeSimpleSuggestion((*SocketIt)->SocketName.ToString()));
		}
	}
	check(Skeleton);

	const TArray<FMeshBoneInfo>& Bones = Skeleton->GetReferenceSkeleton().GetRefBoneInfo();
	for( auto BoneIt=Bones.CreateConstIterator(); BoneIt; ++BoneIt )
	{
		PossibleSuggestions.Add(FAssetSearchBoxSuggestion::MakeSimpleSuggestion(BoneIt->Name.ToString()));
	}

	// create the asset search box
	ChildSlot
	[
		SAssignNew(SearchBox, SAssetSearchBox)
		.InitialText(GetBoneName())
		.HintText(InArgs._HintText)
		.OnTextCommitted(InArgs._OnTextCommitted)
		.PossibleSuggestions(PossibleSuggestions)
		.DelayChangeNotificationsWhileTyping( true )
		.MustMatchPossibleSuggestions(InArgs._MustMatchPossibleSuggestions)
	];
}

void SAssetSearchBoxForBones::RefreshName()
{
	if (SearchBox.IsValid())
	{
		SearchBox->SetText(GetBoneName());
	}
}

FText SAssetSearchBoxForBones::GetBoneName() const
{
	FName CurValue;
	if (BonePropertyHandle.IsValid())
	{
		BonePropertyHandle->GetValue(CurValue);
	}
	return CurValue.IsNone() ? FText::GetEmpty() : FText::FromName(CurValue);
}

SAssetSearchBoxForCurves::SAssetSearchBoxForCurves()
{
	if(GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

SAssetSearchBoxForCurves::~SAssetSearchBoxForCurves()
{
	if(GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

void SAssetSearchBoxForCurves::Construct(const FArguments& InArgs, const class USkeleton* InSkeleton, TSharedPtr<class IPropertyHandle> CurveNameProperty)
{
	check(InSkeleton);

	CurveNamePropertyHandle = CurveNameProperty;

	Skeleton = MakeWeakObjectPtr(const_cast<USkeleton*>(InSkeleton));
	
	// create the asset search box
	ChildSlot
		[
			SAssignNew(SearchBox, SAssetSearchBox)
			.InitialText(GetCurveName())
			.HintText(InArgs._HintText)
			.OnTextCommitted(InArgs._OnTextCommitted)
			.PossibleSuggestions(this, &SAssetSearchBoxForCurves::GetCurveSearchSuggestions)
			.DelayChangeNotificationsWhileTyping(true)
			.MustMatchPossibleSuggestions(InArgs._MustMatchPossibleSuggestions)
		];
}

FText SAssetSearchBoxForCurves::GetCurveName() const
{
	FName CurValue;
	if (CurveNamePropertyHandle.IsValid())
	{
		CurveNamePropertyHandle->GetValue(CurValue);
	}
	return CurValue.IsNone() ? FText::GetEmpty() : FText::FromName(CurValue);
}

void SAssetSearchBoxForCurves::RefreshName()
{
	if (SearchBox.IsValid())
	{
		SearchBox->SetText(GetCurveName());
	}
}

TArray<FAssetSearchBoxSuggestion> SAssetSearchBoxForCurves::GetCurveSearchSuggestions() const
{
	TArray<FAssetSearchBoxSuggestion> PossibleSuggestions;
	if (USkeleton* Skel = Skeleton.Get())
	{
		if (const FSmartNameMapping* Mapping = Skel->GetSmartNameContainer(USkeleton::AnimCurveMappingName))
		{
			TArray<FName> Names; 
			Mapping->FillNameArray(Names);
			for (const FName& Name : Names)
			{
				PossibleSuggestions.Add(FAssetSearchBoxSuggestion::MakeSimpleSuggestion(Name.ToString()));
			}
		}
	}

	return PossibleSuggestions;
}
