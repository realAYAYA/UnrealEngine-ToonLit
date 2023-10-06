// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimAssetFindReplaceCurves.h"

#include "ISkeletonEditorModule.h"
#include "SAnimAssetFindReplace.h"
#include "Animation/PoseAsset.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "String/ParseTokens.h"

#define LOCTEXT_NAMESPACE "AnimAssetFindReplaceCurves"

FString UAnimAssetFindReplaceCurves::GetFindResultStringFromAssetData(const FAssetData& InAssetData) const
{
	if(GetFindString().IsEmpty())
	{
		return FString();
	}

	auto GetMatchingCurveNamesForAsset = [this](const FAssetData& InAssetData, TArray<FString>& OutCurveNames)
	{
		const FString TagValue = InAssetData.GetTagValueRef<FString>(USkeleton::CurveNameTag);
		if (!TagValue.IsEmpty())
		{
			UE::String::ParseTokens(TagValue, *USkeleton::CurveTagDelimiter, [this, &OutCurveNames](FStringView InToken)
			{
				if(GetFindWholeWord())
				{
					if(InToken.Compare(GetFindString(), GetSearchCase()) == 0)
					{
						OutCurveNames.Add(FString(InToken));
					}
				}
				else
				{
					if(UE::String::FindFirst(InToken, GetFindString(), GetSearchCase()) != INDEX_NONE)
					{
						OutCurveNames.Add(FString(InToken));
					}
				}
			}, UE::String::EParseTokensOptions::SkipEmpty);
		}
	};

	TStringBuilder<128> Builder;
	TArray<FString> CurveNames;
	GetMatchingCurveNamesForAsset(InAssetData, CurveNames);
	if(CurveNames.Num() > 0)
	{
		for(int32 NameIndex = 0; NameIndex < CurveNames.Num(); ++NameIndex)
		{
			Builder.Append(CurveNames[NameIndex]);
			if(NameIndex != CurveNames.Num() - 1)
			{
				Builder.Append(TEXT(", "));
			}
		}
	}
	return FString(Builder.ToString());
}

TConstArrayView<UClass*> UAnimAssetFindReplaceCurves::GetSupportedAssetTypes() const
{
	static UClass* Types[] = { UPoseAsset::StaticClass(), UAnimSequenceBase::StaticClass(), USkeleton::StaticClass(), USkeletalMesh::StaticClass() };
	return Types;
}

bool UAnimAssetFindReplaceCurves::ShouldFilterOutAsset(const FAssetData& InAssetData, bool& bOutIsOldAsset) const
{
	FString TagValue;
	if(InAssetData.GetTagValue<FString>(USkeleton::CurveNameTag, TagValue))
	{
		bOutIsOldAsset = false;

		if(GetFindString().IsEmpty())
		{
			return true;
		}
		
		if(GetSkeletonFilter().IsValid() )
		{
			if(InAssetData.GetClass() != USkeleton::StaticClass())
			{
				FString SkeletonPath;
				if(InAssetData.GetTagValue<FString>(TEXT("Skeleton"), SkeletonPath))
				{
					if(SkeletonPath != GetSkeletonFilter().GetExportTextName())
					{
						return true;
					}
				}
			}
			else
			{
				if(InAssetData.ToSoftObjectPath() != GetSkeletonFilter().ToSoftObjectPath())
				{
					return true;
				}
			}
		}

		bool bFoundMatch = false;
		UE::String::ParseTokens(TagValue, *USkeleton::CurveTagDelimiter, [this, &bFoundMatch](FStringView InToken)
		{
			if(NameMatches(InToken))
			{
				bFoundMatch = true;
			}
		}, UE::String::EParseTokensOptions::SkipEmpty);

		if(bFoundMatch)
		{
			return false;
		}
	}
	else
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		UClass* AssetClass = InAssetData.GetClass();
		if(AssetClass->IsChildOf(UAnimSequenceBase::StaticClass()))
		{
			// Check the package object version - the asset was saving empty tags for curves, so the absence of curves is
			// not the same as an empty value
			FAssetPackageData PackageData;
			const UE::AssetRegistry::EExists PackageExists = AssetRegistryModule.Get().TryGetAssetPackageData(InAssetData.PackageName, PackageData);
			if (PackageExists == UE::AssetRegistry::EExists::Exists)
			{
				bOutIsOldAsset = PackageData.FileVersionUE < VER_UE4_SKELETON_ADD_SMARTNAMES;
			}
			else
			{
				// Does not exist or unknown - treat it as 'old'
				bOutIsOldAsset = true;
			}
		}
		else if(AssetClass->IsChildOf(UPoseAsset::StaticClass()))
		{
			// Check the package custom version - the asset was saving empty tags for curves, so the absence of curves is
			// not the same as an empty value
			FAssetPackageData PackageData;
			const UE::AssetRegistry::EExists PackageExists = AssetRegistryModule.Get().TryGetAssetPackageData(InAssetData.PackageName, PackageData);
			if (PackageExists == UE::AssetRegistry::EExists::Exists)
			{
				for(const UE::AssetRegistry::FPackageCustomVersion& CustomVersion : PackageData.GetCustomVersions())
				{
					if(CustomVersion.Key == FAnimPhysObjectVersion::GUID)
					{
						bOutIsOldAsset = CustomVersion.Version < FAnimPhysObjectVersion::SmartNameRefactorForDeterministicCooking;
					}
				}

				// No FAnimPhysObjectVersion, treat as old
				bOutIsOldAsset = true;
			}
			else
			{
				// Does not exist or unknown - treat it as 'old'
				bOutIsOldAsset = true;
			}
		}
		else if(AssetClass->IsChildOf(USkeleton::StaticClass()))
		{
			bOutIsOldAsset = true;
		}
		else if(AssetClass->IsChildOf(USkeletalMesh::StaticClass()))
		{
			// Skeletal meshes didn't have curves before, so cant be 'old' 
			bOutIsOldAsset = false;
		}
		else
		{
			// Assume unknown assets are not 'old'
			bOutIsOldAsset = false;
		}
	}

	return true;
}

void UAnimAssetFindReplaceCurves::ReplaceInAsset(const FAssetData& InAssetData) const
{
	if(UObject* Asset = InAssetData.GetAsset())
	{
		if(UAnimSequenceBase* AnimSequenceBase = Cast<UAnimSequenceBase>(Asset))
		{
			Asset->MarkPackageDirty();

			if(GetFindWholeWord())
			{
				const FAnimationCurveIdentifier FindCurveId(FName(GetFindString()), ERawCurveTrackTypes::RCT_Float);
				const FAnimationCurveIdentifier ReplaceCurveId(FName(GetReplaceString()), ERawCurveTrackTypes::RCT_Float);
				IAnimationDataController::FScopedBracket ScopedBracket(AnimSequenceBase->GetController(), LOCTEXT("ReplaceCurves", "Replace Curves"));
				AnimSequenceBase->GetController().RenameCurve(FindCurveId, ReplaceCurveId);
			}
			else
			{
				TArray<TPair<FAnimationCurveIdentifier, FAnimationCurveIdentifier>> FindReplacePairs;
				const TArray<FFloatCurve>& Curves = AnimSequenceBase->GetDataModel()->GetFloatCurves();
				for(const FFloatCurve& Curve : Curves)
				{
					FString CurveName = Curve.GetName().ToString();
					if(NameMatches(CurveName))
					{
						const FAnimationCurveIdentifier FindCurveId(*CurveName, ERawCurveTrackTypes::RCT_Float);
						const FString NewName = CurveName.Replace(*GetFindStringRef(), *GetReplaceStringRef(), GetSearchCase());
						const FAnimationCurveIdentifier ReplaceCurveId(*NewName, ERawCurveTrackTypes::RCT_Float);

						FindReplacePairs.Emplace(FindCurveId, ReplaceCurveId);
					}
				}

				if(FindReplacePairs.Num() > 0)
				{
					IAnimationDataController::FScopedBracket ScopedBracket(AnimSequenceBase->GetController(), LOCTEXT("ReplaceCurves", "Replace Curves"));

					for(const TPair<FAnimationCurveIdentifier, FAnimationCurveIdentifier>& FindReplacePair : FindReplacePairs)
					{
						AnimSequenceBase->GetController().RenameCurve(FindReplacePair.Key, FindReplacePair.Value);
					}
				}
			}
		}
		else if(UPoseAsset* PoseAsset = Cast<UPoseAsset>(Asset))
		{
			if(GetFindWholeWord())
			{
				Asset->Modify();
				
				const FName FindCurveName(GetFindString());
				const FName ReplaceCurveName(GetReplaceString());
				PoseAsset->RenamePoseOrCurveName(FindCurveName, ReplaceCurveName);
			}
			else
			{
				TArray<TPair<FName, FName>> FindReplacePairs;

				for(const FName& PoseName : PoseAsset->GetPoseFNames())
				{
					FString CurveName = PoseName.ToString();
					if(NameMatches(CurveName))
					{
						const FName FindCurveName(*CurveName);
						const FString NewName = CurveName.Replace(*GetFindStringRef(), *GetReplaceStringRef(), GetSearchCase());
						const FName ReplaceCurveName(*NewName);

						FindReplacePairs.Emplace(FindCurveName, ReplaceCurveName);
					}
				}

				if(FindReplacePairs.Num() > 0)
				{
					Asset->Modify();

					for(const TPair<FName, FName>& FindReplacePair : FindReplacePairs)
					{
						PoseAsset->RenamePoseOrCurveName(FindReplacePair.Key, FindReplacePair.Value);
					}
				}
			}
		}
		else if(USkeleton* Skeleton = Cast<USkeleton>(Asset))
		{
			if(GetFindWholeWord())
			{
				Asset->Modify();

				const FName FindCurveName(GetFindString());
				const FName ReplaceCurveName(GetReplaceString());
				Skeleton->RenameCurveMetaData(FindCurveName, ReplaceCurveName);
			}
			else
			{
				TArray<TPair<FName, FName>> FindReplacePairs;

				Skeleton->ForEachCurveMetaData([this, &FindReplacePairs](FName InCurveName, const FCurveMetaData& InMetaData)
				{
					const FString CurveNameString = InCurveName.ToString();
					if(NameMatches(CurveNameString))
					{
						const FName FindCurveName(InCurveName);
						const FString NewName = CurveNameString.Replace(*GetFindStringRef(), *GetReplaceStringRef(), GetSearchCase());
						const FName ReplaceCurveName(*NewName);

						FindReplacePairs.Emplace(FindCurveName, ReplaceCurveName);
					}
				});

				if(FindReplacePairs.Num() > 0)
				{
					Asset->Modify();

					for(const TPair<FName, FName>& FindReplacePair : FindReplacePairs)
					{
						Skeleton->RenameCurveMetaData(FindReplacePair.Key, FindReplacePair.Value);
					}
				}
			}
		}
		else if(USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset))
		{
			if(UAnimCurveMetaData* AnimCurveMetaData = SkeletalMesh->GetAssetUserData<UAnimCurveMetaData>())
			{
				if(GetFindWholeWord())
				{
					Asset->Modify();

					const FName FindCurveName(GetFindString());
					const FName ReplaceCurveName(GetReplaceString());
					AnimCurveMetaData->RenameCurveMetaData(FindCurveName, ReplaceCurveName);
				}
				else
				{
					TArray<TPair<FName, FName>> FindReplacePairs;

					AnimCurveMetaData->ForEachCurveMetaData([this, &FindReplacePairs](FName InCurveName, const FCurveMetaData& InMetaData)
					{
						const FString CurveNameString = InCurveName.ToString();
						if(NameMatches(CurveNameString))
						{
							const FName FindCurveName(InCurveName);
							const FString NewName = CurveNameString.Replace(*GetFindStringRef(), *GetReplaceStringRef(), GetSearchCase());
							const FName ReplaceCurveName(*NewName);

							FindReplacePairs.Emplace(FindCurveName, ReplaceCurveName);
						}
					});

					if(FindReplacePairs.Num() > 0)
					{
						Asset->Modify();

						for(const TPair<FName, FName>& FindReplacePair : FindReplacePairs)
						{
							AnimCurveMetaData->RenameCurveMetaData(FindReplacePair.Key, FindReplacePair.Value);
						}
					}
				}
			}
		}
	}
}

void UAnimAssetFindReplaceCurves::RemoveInAsset(const FAssetData& InAssetData) const
{
	if(UObject* Asset = InAssetData.GetAsset())
	{
		if(UAnimSequenceBase* AnimSequenceBase = Cast<UAnimSequenceBase>(Asset))
		{
			Asset->MarkPackageDirty();

			if(GetFindWholeWord())
			{
				const FAnimationCurveIdentifier CurveId(FName(GetFindString()), ERawCurveTrackTypes::RCT_Float);
				IAnimationDataController::FScopedBracket ScopedBracket(AnimSequenceBase->GetController(), LOCTEXT("RemoveCurves", "Remove Curves"));
				AnimSequenceBase->GetController().RemoveCurve(CurveId);
			}
			else
			{
				TSet<FAnimationCurveIdentifier> CurveIdsToRemove;
				const TArray<FFloatCurve>& Curves = AnimSequenceBase->GetDataModel()->GetFloatCurves();
				for(const FFloatCurve& Curve : Curves)
				{
					FString CurveName = Curve.GetName().ToString();
					if(NameMatches(CurveName))
					{
						const FAnimationCurveIdentifier CurveId(*CurveName, ERawCurveTrackTypes::RCT_Float);
						CurveIdsToRemove.Add(CurveId);
					}
				}

				if(CurveIdsToRemove.Num() > 0)
				{
					IAnimationDataController::FScopedBracket ScopedBracket(AnimSequenceBase->GetController(), LOCTEXT("RemoveCurves", "Remove Curves"));
					for(const FAnimationCurveIdentifier& CurveIdToRemove : CurveIdsToRemove)
					{
						AnimSequenceBase->GetController().RemoveCurve(CurveIdToRemove);
					}
				}
			}
		}
		else if(UPoseAsset* PoseAsset = Cast<UPoseAsset>(Asset))
		{
			if(GetFindWholeWord())
			{
				Asset->Modify();
				PoseAsset->RemovePoseOrCurveNames({ FName(GetFindString()) });
			}
			else
			{
				TArray<FName> CurveIdsToRemove;

				for(const FName& PoseName : PoseAsset->GetPoseFNames())
				{
					if(NameMatches(PoseName.ToString()))
					{
						CurveIdsToRemove.AddUnique(PoseName);
					}
				}

				for(const FName& CurveName : PoseAsset->GetCurveFNames())
				{
					if(NameMatches(CurveName.ToString()))
					{
						CurveIdsToRemove.AddUnique(CurveName);
					}
				}
				
				if(CurveIdsToRemove.Num() > 0)
				{
					Asset->Modify();
					PoseAsset->RemovePoseOrCurveNames(CurveIdsToRemove);
				}
			}
		}
		else if(USkeleton* Skeleton = Cast<USkeleton>(Asset))
		{
			if(GetFindWholeWord())
			{
				Asset->Modify();
				Skeleton->RemoveCurveMetaData(FName(GetFindString()));
			}
			else
			{
				TArray<FName> CurvesToRemove;
				Skeleton->ForEachCurveMetaData([this, &CurvesToRemove](FName InCurveName, const FCurveMetaData& InCurveMetaData)
				{
				 	if(NameMatches(InCurveName.ToString()))
				 	{
						 CurvesToRemove.AddUnique(InCurveName);
					 }
				});

				if(CurvesToRemove.Num() > 0)
				{
					Asset->Modify();
					Skeleton->RemoveCurveMetaData(CurvesToRemove);
				}
			}
		}
	}
}

void UAnimAssetFindReplaceCurves::GetAutoCompleteNames(TArrayView<FAssetData> InAssetDatas, TSet<FString>& OutUniqueNames) const
{
	for (const FAssetData& AssetData : InAssetDatas)
	{
		const FString TagValue = AssetData.GetTagValueRef<FString>(USkeleton::CurveNameTag);
		if (!TagValue.IsEmpty())
		{
			UE::String::ParseTokens(TagValue, *USkeleton::CurveTagDelimiter, [&OutUniqueNames](FStringView InToken)
			{
				OutUniqueNames.Add(FString(InToken));
			}, UE::String::EParseTokensOptions::SkipEmpty);
		}
	}
}

#undef LOCTEXT_NAMESPACE