// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomCacheImporter.h"
#include "GroomAsset.h"
#include "GroomBuilder.h"
#include "GroomCache.h"
#include "GroomCacheImportOptions.h"
#include "GroomImportOptions.h"
#include "HairStrandsImporter.h"
#include "HairStrandsTranslator.h"
#include "Logging/LogMacros.h"
#include "Misc/ScopedSlowTask.h"
#include "ObjectTools.h"
#include "PackageTools.h"

#define LOCTEXT_NAMESPACE "GroomCacheImporter"

DEFINE_LOG_CATEGORY_STATIC(LogGroomCacheImporter, Log, All);

static UGroomCache* CreateGroomCache(EGroomCacheType Type, UObject*& InParent, const FString& ObjectName, const EObjectFlags Flags)
{
	// Don't do any package handling if the parent is the transient package; just use it
	UPackage* Package = nullptr;
	FString SanitizedObjectName = ObjectName;
	if (InParent != GetTransientPackage())
	{
		// Setup package name and create one accordingly
		FString NewPackageName = InParent->GetOutermost()->GetName();
		if (!NewPackageName.EndsWith(ObjectName))
		{
			// Remove the cache suffix if the parent is from a reimport
			if (NewPackageName.EndsWith("_strands_cache"))
			{
				NewPackageName.RemoveFromEnd("_strands_cache");
			}
			else if (NewPackageName.EndsWith("_guides_cache"))
			{
				NewPackageName.RemoveFromEnd("_guides_cache");
			}

			// Append the correct suffix
			NewPackageName += TEXT("_") + ObjectName;
		}

		NewPackageName = UPackageTools::SanitizePackageName(NewPackageName);

		// Parent package to place new GroomCache
		Package = CreatePackage(*NewPackageName);

		FString CompoundObjectName = FPackageName::GetShortName(NewPackageName);
		SanitizedObjectName = ObjectTools::SanitizeObjectName(CompoundObjectName);

		UGroomCache* ExistingTypedObject = FindObject<UGroomCache>(Package, *SanitizedObjectName);
		UObject* ExistingObject = FindObject<UObject>(Package, *SanitizedObjectName);

		if (ExistingTypedObject != nullptr)
		{
			ExistingTypedObject->PreEditChange(nullptr);
			return ExistingTypedObject;
		}
		else if (ExistingObject != nullptr)
		{
			// Replacing an object.  Here we go!
			// Delete the existing object
			const bool bDeleteSucceeded = ObjectTools::DeleteSingleObject(ExistingObject);

			if (bDeleteSucceeded)
			{
				// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
				CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

				// Create a package for each mesh
				Package = CreatePackage(*NewPackageName);
				InParent = Package;
			}
			else
			{
				// failed to delete
				return nullptr;
			}
		}
	}
	else
	{
		Package = GetTransientPackage();
	}

	UGroomCache* GroomCache = NewObject<UGroomCache>(Package, FName(*SanitizedObjectName), Flags | RF_Public);
	GroomCache->Initialize(Type);

	return GroomCache;
}

UGroomCache* FGroomCacheImporter::ProcessToGroomCache(FGroomCacheProcessor& Processor, const FGroomAnimationInfo& AnimInfo, FHairImportContext& ImportContext, const FString& ObjectNameSuffix)
{
	if (UGroomCache* GroomCache = CreateGroomCache(Processor.GetType(), ImportContext.Parent, ObjectNameSuffix, ImportContext.Flags))
	{
		Processor.TransferChunks(GroomCache);
		GroomCache->SetGroomAnimationInfo(AnimInfo);

		GroomCache->MarkPackageDirty();
		GroomCache->PostEditChange();
		return GroomCache;
	}
	return nullptr;
}

TArray<UGroomCache*> FGroomCacheImporter::ImportGroomCache(const FString& SourceFilename, TSharedPtr<IGroomTranslator> Translator, const FGroomAnimationInfo& AnimInfo, FHairImportContext& HairImportContext, UGroomAsset* GroomAssetForCache)
{
	bool bSuccess = true;
	FGroomCacheProcessor StrandsProcessor(EGroomCacheType::Strands, AnimInfo.Attributes);
	FGroomCacheProcessor GuidesProcessor(EGroomCacheType::Guides, AnimInfo.Attributes);
	if (Translator->BeginTranslation(SourceFilename))
	{
		// Sample one extra frame so that we can interpolate between EndFrame - 1 and EndFrame
		const int32 NumFrames = AnimInfo.NumFrames + 1;
		FScopedSlowTask SlowTask(NumFrames, LOCTEXT("ImportGroomCache", "Importing GroomCache frames"));
		SlowTask.MakeDialog();

		const TArray<FHairGroupData>& GroomHairGroupsData = GroomAssetForCache->HairGroupsData;

		// Each frame is translated into a HairDescription and processed into HairGroupData
		for (int32 FrameIndex = AnimInfo.StartFrame; FrameIndex < AnimInfo.EndFrame + 1; ++FrameIndex)
		{
			const uint32 CurrentFrame = FrameIndex - AnimInfo.StartFrame;

			FTextBuilder TextBuilder;
			TextBuilder.AppendLineFormat(LOCTEXT("ImportGroomCacheFrame", "Importing GroomCache frame {0} of {1}"), FText::AsNumber(CurrentFrame), FText::AsNumber(NumFrames));
			SlowTask.EnterProgressFrame(1.f, TextBuilder.ToText());

			FHairDescription FrameHairDescription;
			if (Translator->Translate(FrameIndex * AnimInfo.SecondsPerFrame, FrameHairDescription, HairImportContext.ImportOptions->ConversionSettings))
			{
				FHairDescriptionGroups HairDescriptionGroups;
				if (!FGroomBuilder::BuildHairDescriptionGroups(FrameHairDescription, HairDescriptionGroups))
				{
					bSuccess = false;
					break;
				}

				const uint32 GroupCount = HairDescriptionGroups.HairGroups.Num();

				TArray<FHairGroupInfoWithVisibility> HairGroupsInfo = GroomAssetForCache->HairGroupsInfo;
				TArray<FHairDescriptionGroup> HairGroupsData;
				HairGroupsData.SetNum(GroupCount);
				for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
				{
					const FHairDescriptionGroup& HairGroup = HairDescriptionGroups.HairGroups[GroupIndex];
					FHairDescriptionGroup& HairGroupData = HairGroupsData[GroupIndex];
					FGroomBuilder::BuildData(HairGroup, GroomAssetForCache->HairGroupsInterpolation[GroupIndex], HairGroupsInfo[GroupIndex], HairGroupData.Strands, HairGroupData.Guides);
				}

				// Validate that the GroomCache has the same topology as the static groom
				if (HairGroupsData.Num() == GroomHairGroupsData.Num())
				{
					for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
					{
						if (HairGroupsData[GroupIndex].Strands.GetNumPoints() != GroomHairGroupsData[GroupIndex].Strands.BulkData.GetNumPoints())
						{
							bSuccess = false;
							UE_LOG(LogGroomCacheImporter, Warning, TEXT("GroomCache frame %d does not have the same number of vertices as the static groom (%u instead of %u). Aborting GroomCache import."),
								FrameIndex, HairGroupsData[GroupIndex].Strands.GetNumPoints(), GroomHairGroupsData[GroupIndex].Strands.BulkData.GetNumPoints());
							break;
						}
					}
				}
				else
				{
					bSuccess = false;
					UE_LOG(LogGroomCacheImporter, Warning, TEXT("GroomCache does not have the same number of groups as the static groom (%d instead of %d). Aborting GroomCache import."),
						HairGroupsData.Num(), GroomHairGroupsData.Num());
				}

				if (!bSuccess)
				{
					break;
				}

				// The HairGroupData is converted into animated groom data by the GroomCacheProcessor
				StrandsProcessor.AddGroomSample(MoveTemp(HairGroupsData));
				GuidesProcessor.AddGroomSample(MoveTemp(HairGroupsData));
			}
		}
	}
	else
	{
		bSuccess = false;
	}
	Translator->EndTranslation();

	TArray<UGroomCache*> GroomCaches;
	if (bSuccess)
	{
		// Once the processing has completed successfully, the data is transferred to the GroomCache
		UGroomCache* GroomCache = ProcessToGroomCache(StrandsProcessor, AnimInfo, HairImportContext, "strands_cache");
		if (GroomCache)
		{
			GroomCaches.Add(GroomCache);
		}

		GroomCache = ProcessToGroomCache(GuidesProcessor, AnimInfo, HairImportContext, "guides_cache");
		if (GroomCache)
		{
			GroomCaches.Add(GroomCache);
		}
	}
	return GroomCaches;
}

void FGroomCacheImporter::SetupImportSettings(FGroomCacheImportSettings& ImportSettings, const FGroomAnimationInfo& AnimInfo)
{
	// Prepare the import settings for display
	// GroomCache options are only shown if there's a valid groom animation
	ImportSettings.bImportGroomCache = ImportSettings.bImportGroomCache && AnimInfo.IsValid();

	if (ImportSettings.bImportGroomCache)
	{
		if (ImportSettings.FrameEnd == 0)
		{
			ImportSettings.FrameEnd = AnimInfo.EndFrame;
		}
	}
}

void FGroomCacheImporter::ApplyImportSettings(FGroomCacheImportSettings& ImportSettings, FGroomAnimationInfo& AnimInfo)
{
	// Harmonize the values between what's in the settings (set by the user) and the animation info (extracted from the Alembic) used for importing
	// The user settings usually take precedence over the animation info
	if (ImportSettings.bImportGroomCache)
	{
		if (ImportSettings.bSkipEmptyFrames)
		{
			// Skipping empty frames will start from the beginning of the animation range or beyond if specified by the user
			if (ImportSettings.FrameStart > AnimInfo.StartFrame)
			{
				AnimInfo.StartFrame = ImportSettings.FrameStart;
			}
			else
			{
				ImportSettings.FrameStart = AnimInfo.StartFrame;
			}
		}
		else
		{
			// Otherwise, just take the value set by the user
			AnimInfo.StartFrame = ImportSettings.FrameStart;
		}

		if (ImportSettings.FrameEnd == 0)
		{
			// If the user manually set the end to 0, use the actual end of the animation range
			ImportSettings.FrameEnd = AnimInfo.EndFrame;
		}
		else
		{
			// Otherwise, just take the value set by the user
			AnimInfo.EndFrame = ImportSettings.FrameEnd;
		}

		// Sanity check
		if (ImportSettings.FrameEnd <= ImportSettings.FrameStart)
		{
			ImportSettings.FrameEnd = ImportSettings.FrameStart + 1;
			AnimInfo.EndFrame = ImportSettings.FrameStart + 1;
		}

		// EndFrame is not included and must have at least 1 frame
		AnimInfo.NumFrames = FMath::Max(AnimInfo.EndFrame - AnimInfo.StartFrame, 1);

		// Compute the duration as it is not known yet
		AnimInfo.Duration = AnimInfo.NumFrames * AnimInfo.SecondsPerFrame;
	}
}

#undef LOCTEXT_NAMESPACE