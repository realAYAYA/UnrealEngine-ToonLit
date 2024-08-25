// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition.h"
#include "AssetDefinitionRegistry.h"
#include "Misc/AssetFilterData.h"
#include "EditorFramework/AssetImportData.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition"

FAssetCategoryPath EAssetCategoryPaths::Basic(LOCTEXT("Basic", "Basic"));
FAssetCategoryPath EAssetCategoryPaths::Animation(LOCTEXT("Animation", "Animation"));
FAssetCategoryPath EAssetCategoryPaths::Material(LOCTEXT("Material", "Material"));
FAssetCategoryPath EAssetCategoryPaths::Audio(LOCTEXT("Audio", "Audio"));
FAssetCategoryPath EAssetCategoryPaths::Physics(LOCTEXT("Physics", "Physics"));
FAssetCategoryPath EAssetCategoryPaths::UI(LOCTEXT("UserInterface", "User Interface"));
FAssetCategoryPath EAssetCategoryPaths::Misc(LOCTEXT("Miscellaneous", "Miscellaneous"));
FAssetCategoryPath EAssetCategoryPaths::Gameplay(LOCTEXT("Gameplay", "Gameplay"));
FAssetCategoryPath EAssetCategoryPaths::AI(LOCTEXT("AI", "Artificial Intelligence"));
FAssetCategoryPath EAssetCategoryPaths::Blueprint(LOCTEXT("Blueprint", "Blueprint"));
FAssetCategoryPath EAssetCategoryPaths::Texture(LOCTEXT("Texture", "Texture"));
FAssetCategoryPath EAssetCategoryPaths::Foliage(LOCTEXT("Foliage", "Foliage"));
FAssetCategoryPath EAssetCategoryPaths::Input(LOCTEXT("Input", "Input"));
FAssetCategoryPath EAssetCategoryPaths::FX(LOCTEXT("FX", "FX"));
FAssetCategoryPath EAssetCategoryPaths::Cinematics(LOCTEXT("Cinematics", "Cinematics"));
FAssetCategoryPath EAssetCategoryPaths::Media(LOCTEXT("Media", "Media"));
FAssetCategoryPath EAssetCategoryPaths::World(LOCTEXT("World", "World"));

FAssetCategoryPath::FAssetCategoryPath(const FText& InCategory)
{
	CategoryPath = { TPair<FName, FText>(FName(*FTextInspector::GetSourceString(InCategory)), InCategory) };
}

FAssetCategoryPath::FAssetCategoryPath(const FText& InCategory, const FText& InSubCategory)
{
	CategoryPath = {
		TPair<FName, FText>(FName(*FTextInspector::GetSourceString(InCategory)), InCategory),
		TPair<FName, FText>(FName(*FTextInspector::GetSourceString(InSubCategory)), InSubCategory)
	};
}

FAssetCategoryPath::FAssetCategoryPath(const FAssetCategoryPath& InCategory, const FText& InSubCategory)
{
    CategoryPath.Append(InCategory.CategoryPath);
    CategoryPath.Add(TPair<FName, FText>(FName(*FTextInspector::GetSourceString(InSubCategory)), InSubCategory));
}

FAssetCategoryPath::FAssetCategoryPath(TConstArrayView<FText> InCategoryPath)
{
	check(InCategoryPath.Num() > 0);
	
	for (const FText& CategoryChunk : InCategoryPath)
	{
		CategoryPath.Add(TPair<FName, FText>(FName(*FTextInspector::GetSourceString(CategoryChunk)), CategoryChunk));
	}
}

// UAssetDefinition
//---------------------------------------------------------------------------

UAssetDefinition::UAssetDefinition()
{
}

void UAssetDefinition::PostCDOContruct()
{
	Super::PostCDOContruct();

	if (CanRegisterStatically())
	{
		UAssetDefinitionRegistry::Get()->RegisterAssetDefinition(this);
	}
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::Misc };
	return Categories;
}

EAssetCommandResult UAssetDefinition::GetSourceFiles(const FAssetSourceFilesArgs& InArgs, TFunctionRef<bool(const FAssetSourceFilesResult& InSourceFile)> SourceFileFunc) const
{
	bool bFoundSomeData = false;

	FString SourceFileTagData;
	FAssetSourceFilesResult Result;
	for (const FAssetData& Asset : InArgs.Assets)
	{
		if (Asset.GetTagValue(UObject::SourceFileTagName(), SourceFileTagData))
		{
			TOptional<FAssetImportInfo> ImportInfoOptional = FAssetImportInfo::FromJson(SourceFileTagData);
			if (ImportInfoOptional.IsSet())
			{
				bFoundSomeData = true;
				FAssetImportInfo& ImportInfo = ImportInfoOptional.GetValue();

				for (FAssetImportInfo::FSourceFile& SourceFiles : ImportInfo.SourceFiles)
				{
					Result.FilePath = MoveTemp(SourceFiles.RelativeFilename);
					Result.DisplayLabel = MoveTemp(SourceFiles.DisplayLabelName);
					Result.Timestamp = MoveTemp(SourceFiles.Timestamp);
					Result.FileHash = MoveTemp(SourceFiles.FileHash);
				
					if (InArgs.FilePathFormat == EPathUse::AbsolutePath)
					{
						Result.FilePath = UAssetImportData::ResolveImportFilename(FStringView(Result.FilePath), Asset.PackageName.ToString());
					}

					if (!SourceFileFunc(Result))
					{
						return EAssetCommandResult::Handled;
					}
				}
			}
		}
	}

	return bFoundSomeData ? EAssetCommandResult::Handled : EAssetCommandResult::Unhandled;
}

EAssetCommandResult UAssetDefinition::GetSourceFiles(const FAssetData& InAsset, TFunctionRef<void(const FAssetImportInfo& AssetImportData)> SourceFileFunc) const
{
	FString SourceFileTagData;
	if (InAsset.GetTagValue(UObject::SourceFileTagName(), SourceFileTagData))
	{
		TOptional<FAssetImportInfo> ImportInfo = FAssetImportInfo::FromJson(SourceFileTagData);
		if (ImportInfo.IsSet())
		{
			SourceFileFunc(ImportInfo.GetValue());
			
			return EAssetCommandResult::Handled;
		}
	}
    
	return EAssetCommandResult::Unhandled;
}

bool UAssetDefinition::CanRegisterStatically() const
{
	return !GetClass()->HasAnyClassFlags(CLASS_Abstract);
}

void UAssetDefinition::BuildFilters(TArray<FAssetFilterData>& OutFilters) const
{
	const TSoftClassPtr<UObject> AssetClassPtr = GetAssetClass();

	if (const UClass* AssetClass = AssetClassPtr.Get())
	{
		// If this asset definition doesn't have any categories it can't have any filters.  Filters need to have a
		// category to be displayed.
		if (GetAssetCategories().Num() == 0)
		{
			return;
		}
		
		// By default we don't advertise filtering if the class is abstract for the asset definition.  Odds are,
		// if they've registered an abstract class as an asset definition, they mean to use it for subclasses.
		if (IncludeClassInFilter == EIncludeClassInFilter::Always || (IncludeClassInFilter == EIncludeClassInFilter::IfClassIsNotAbstract && !AssetClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			FAssetFilterData DefaultFilter;
			DefaultFilter.Name = AssetClassPtr.ToSoftObjectPath().ToString();
			DefaultFilter.DisplayText = GetAssetDisplayName();
			DefaultFilter.FilterCategories = GetAssetCategories();
			DefaultFilter.Filter.ClassPaths.Add(AssetClassPtr.ToSoftObjectPath().GetAssetPath());
			DefaultFilter.Filter.bRecursiveClasses = true;
			OutFilters.Add(MoveTemp(DefaultFilter));
		}
	}
}

#undef LOCTEXT_NAMESPACE