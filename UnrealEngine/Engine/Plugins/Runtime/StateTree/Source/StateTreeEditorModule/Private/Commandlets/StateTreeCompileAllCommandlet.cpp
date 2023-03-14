// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/StateTreeCompileAllCommandlet.h"
#include "StateTree.h"
#include "PackageHelperFunctions.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"
#include "StateTreeEditor.h"
#include "StateTreeCompiler.h"
#include "StateTreeDelegates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeCompileAllCommandlet)

DEFINE_LOG_CATEGORY_STATIC(LogStateTreeCompile, Log, Log);

UStateTreeCompileAllCommandlet::UStateTreeCompileAllCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UStateTreeCompileAllCommandlet::Main(const FString& Params)
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	// want everything in upper case, it's a mess otherwise
	const FString ParamsUpperCase = Params.ToUpper();
	const TCHAR* Parms = *ParamsUpperCase;
	ParseCommandLine(Parms, Tokens, Switches);

	// Source control
	bool bNoSourceControl = Switches.Contains(TEXT("nosourcecontrol"));
	FScopedSourceControl SourceControl;
	SourceControlProvider = bNoSourceControl ? nullptr : &SourceControl.GetProvider();

	// Load assets
	UE_LOG(LogStateTreeCompile, Display, TEXT("Loading Asset Registry..."));
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
	AssetRegistryModule.Get().SearchAllAssets(/*bSynchronousSearch =*/true);
	UE_LOG(LogStateTreeCompile, Display, TEXT("Finished Loading Asset Registry."));
	
	UE_LOG(LogStateTreeCompile, Display, TEXT("Gathering All StateTrees From Asset Registry..."));
	TArray<FAssetData> StateTreeAssetList;
	AssetRegistryModule.Get().GetAssetsByClass(UStateTree::StaticClass()->GetClassPathName(), StateTreeAssetList, /*bSearchSubClasses*/false);

	int32 Counter = 0;
	for (const FAssetData& Asset : StateTreeAssetList)
	{
		const FString ObjectPath = Asset.GetObjectPathString();
		UE_LOG(LogStateTreeCompile, Display, TEXT("Loading and Compiling: '%s' [%d/%d]..."), *ObjectPath, Counter+1, StateTreeAssetList.Num());

		UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(Asset.GetClass(), /*Outer*/nullptr, *ObjectPath, /*FileName*/nullptr, LOAD_NoWarn));
		if (StateTree == nullptr)
		{
			UE_LOG(LogStateTreeCompile, Error, TEXT("Failed to Load: '%s'."), *ObjectPath);
		}
		else
		{
			CompileAndSaveStateTree(*StateTree);
		}
		Counter++;
	}
		
	return 0;
}

bool UStateTreeCompileAllCommandlet::CompileAndSaveStateTree(UStateTree& StateTree) const
{
	UPackage* Package = StateTree.GetPackage();
	const FString PackageFileName = SourceControlHelpers::PackageFilename(Package);

	// Compile the StateTree asset.
	UE::StateTree::Editor::ValidateAsset(StateTree);
	const uint32 EditorDataHash = UE::StateTree::Editor::CalcAssetHash(StateTree);

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);

	const bool bSuccess = Compiler.Compile(StateTree);

	if (bSuccess)
	{
		// Success
		StateTree.LastCompiledEditorDataHash = EditorDataHash;
		UE::StateTree::Delegates::OnPostCompile.Broadcast(StateTree);
		UE_LOG(LogStateTreeCompile, Log, TEXT("Compile StateTree %s succeeded."), *PackageFileName, EditorDataHash);
	}
	else
	{
		// Make sure not to leave stale data on failed compile.
		StateTree.ResetCompiled();
		StateTree.LastCompiledEditorDataHash = 0;

		UE_LOG(LogStateTreeCompile, Error, TEXT("Failed to compile StateTree %s, errors follow."), *PackageFileName);
		Log.DumpToLog(LogStateTreeCompile);

		return false;
	}

	// Check out the StateTree asset
	if (SourceControlProvider != nullptr)
	{
		const FSourceControlStatePtr SourceControlState = SourceControlProvider->GetState(PackageFileName, EStateCacheUsage::ForceUpdate);

		if (SourceControlState.IsValid())
		{
			FString OtherCheckedOutUser;
			if (SourceControlState->IsCheckedOutOther(&OtherCheckedOutUser))
			{
				UE_LOG(LogStateTreeCompile, Error, TEXT("Overwriting package %s already checked out by %s, will not submit"), *PackageFileName, *OtherCheckedOutUser);
				return false;
			}
			else if (!SourceControlState->IsCurrent())
			{
				UE_LOG(LogStateTreeCompile, Error, TEXT("Overwriting package %s (not at head revision), will not submit"), *PackageFileName);
				return false;
			}
			else if (SourceControlState->IsCheckedOut() || SourceControlState->IsAdded())
			{
				UE_LOG(LogStateTreeCompile, Log, TEXT("Package %s already checked out"), *PackageFileName);
				return true;
			}
			else if (SourceControlState->IsSourceControlled())
			{
				UE_LOG(LogStateTreeCompile, Log, TEXT("Checking out package %s from source control"), *PackageFileName);
				return SourceControlProvider->Execute(ISourceControlOperation::Create<FCheckOut>(), PackageFileName) == ECommandResult::Succeeded;
			}
		}
	}

	// Save StateTree asset.
	if (!SavePackageHelper(Package, PackageFileName))
	{
		UE_LOG(LogStateTreeCompile, Error, TEXT("Failed to save %s."), *PackageFileName);
		return false;
	}

	UE_LOG(LogStateTreeCompile, Log, TEXT("Compile and save %s succeeded."), *PackageFileName);

	return true;
}

