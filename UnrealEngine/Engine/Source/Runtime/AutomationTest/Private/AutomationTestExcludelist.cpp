// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutomationTestExcludelist.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AutomationTestExcludelist)

#if WITH_EDITOR
	#include "HAL/PlatformFileManager.h"
	#include "ISourceControlOperation.h"
	#include "SourceControlOperations.h"
	#include "ISourceControlProvider.h"
	#include "ISourceControlModule.h"
	#include "GenericPlatform/GenericPlatformFile.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAutomationTestExcludelist, Log, All);

static const FString FunctionalTestsPreFix = TEXT("Project.Functional Tests.");

void UAutomationTestExcludelist::OverrideConfigSection(FString& SectionName)
{
	SectionName = TEXT("AutomationTestExcludelist");
}

UAutomationTestExcludelist* UAutomationTestExcludelist::Get()
{
	return GetMutableDefault<UAutomationTestExcludelist>();
}

void SortExcludelist(TArray<FAutomationTestExcludelistEntry>& List)
{
	// Sort in alphabetical order, shortest to longest of FullTestName property.
	// That is to naturally gives priority to parent suite over invidual test exclusion when calling GetExcludeTestEntry(TestName).
	List.Sort([](const FAutomationTestExcludelistEntry& A, const FAutomationTestExcludelistEntry& B)
	{
		return A.FullTestName < B.FullTestName;
	});
}

void UAutomationTestExcludelist::PostInitProperties()
{
	Super::PostInitProperties();

	for (auto& Entry : ExcludeTest)
	{
		if (Entry.IsEmpty())
		{
			Entry.FullTestName = GetFullTestName(Entry);
		}
	}

	SortExcludelist(ExcludeTest);
}

FString UAutomationTestExcludelist::GetFullTestName(const FAutomationTestExcludelistEntry& ExcludelistEntry)
{
	if (!ExcludelistEntry.IsEmpty())
	{
		return ExcludelistEntry.FullTestName;
	}

	FString ListName = ExcludelistEntry.Test.ToString().TrimStartAndEnd();

	bool IsFunctionalTest = ListName.StartsWith(FunctionalTestsPreFix);

	// Backcomp - merge Map and Test properties of Functional Test
	FString Map = ExcludelistEntry.Map.ToString().TrimStartAndEnd();
	if (Map.StartsWith(TEXT("/")) && !IsFunctionalTest)
	{
		ListName = FunctionalTestsPreFix + Map + TEXT(".") + ListName;
		IsFunctionalTest = true;
	}
	// Backcomp - Convert package path by using dot syntax instead of /
	if (IsFunctionalTest)
	{
		ListName = ListName.Replace(TEXT("./Game/"), TEXT(".")).Replace(TEXT("/"), TEXT("."));
	}

	return ListName.ToLower();
}

void UAutomationTestExcludelist::AddToExcludeTest(const FString& TestName, const FAutomationTestExcludelistEntry& ExcludelistEntry)
{
	auto NewEntry = FAutomationTestExcludelistEntry(ExcludelistEntry);
	NewEntry.Test = *(TestName.TrimStartAndEnd());
	if (!NewEntry.Map.IsNone())
	{
		NewEntry.Map = TEXT("");
	}

	NewEntry.FullTestName = GetFullTestName(NewEntry);

	ExcludeTest.Add(NewEntry);
	SortExcludelist(ExcludeTest);
}

void UAutomationTestExcludelist::RemoveFromExcludeTest(const FString& TestName)
{
	if (TestName.IsEmpty())
		return;

	const FString NameToCompare = TestName.TrimStartAndEnd().ToLower();

	for (int i = 0; i < ExcludeTest.Num(); ++i)
	{
		if (ExcludeTest[i].FullTestName == NameToCompare)
		{
			ExcludeTest.RemoveAt(i);
			return;
		}
	}
}

bool UAutomationTestExcludelist::IsTestExcluded(const FString& TestName, const FString& RHI, FName* OutReason, bool* OutWarn)
{
	if (auto Entry = GetExcludeTestEntry(TestName, RHI))
	{
		if (OutReason != nullptr)
		{
			*OutReason = Entry->Reason;
		}

		if (OutWarn != nullptr)
		{
			*OutWarn = Entry->Warn;
		}

		return true;
	}

	return false;
}

FAutomationTestExcludelistEntry* UAutomationTestExcludelist::GetExcludeTestEntry(const FString& TestName, const FString& RHI)
{
	if (TestName.IsEmpty())
		return nullptr;

	const FString NameToCompare = TestName.TrimStartAndEnd().ToLower();

	FAutomationTestExcludelistEntry* OutEntry = nullptr;

	for (auto& Entry : ExcludeTest)
	{
		if(NameToCompare.StartsWith(Entry.FullTestName))
		{
			if (NameToCompare.Len() == Entry.FullTestName.Len() || NameToCompare.Mid(Entry.FullTestName.Len(), 1) == TEXT("."))
			{
				if (Entry.RHIs.Num() == 0)
				{
					return &Entry;
				}
				if (RHI.IsEmpty())
				{
					OutEntry = &Entry;
					continue;
				}
				if (Entry.RHIs.Contains(*RHI))
				{
					return &Entry;
				}
			}
		}
	}

	return OutEntry;
}

#if WITH_EDITOR
bool CheckOutOrAddFile(const FString& InFileToCheckOut)
{
	bool bSuccessfullyCheckedOutOrAddedFile = false;
	if (ISourceControlModule::Get().IsEnabled())
	{
		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(InFileToCheckOut, EStateCacheUsage::Use);

		TArray<FString> FilesToBeCheckedOut;
		FilesToBeCheckedOut.Add(InFileToCheckOut);

		if (SourceControlState.IsValid())
		{
			if (SourceControlState->IsSourceControlled())
			{
				if (SourceControlState->IsDeleted())
				{
					UE_LOG(LogAutomationTestExcludelist, Error, TEXT("The configuration file is marked for deletion."));
				}
				else if (SourceControlState->CanCheckout() || SourceControlState->IsCheckedOutOther() || FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*InFileToCheckOut))
				{
					ECommandResult::Type CommandResult = SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut);
					if (CommandResult == ECommandResult::Failed)
					{
						UE_LOG(LogAutomationTestExcludelist, Error, TEXT("Failed to check out the configuration file."));
					}
					else if (CommandResult == ECommandResult::Cancelled)
					{
						UE_LOG(LogAutomationTestExcludelist, Warning, TEXT("Checkout was cancelled."));
					}
					else
					{
						bSuccessfullyCheckedOutOrAddedFile = true;
					}
				}
			}
			else if (!SourceControlState->IsUnknown())
			{
				if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*InFileToCheckOut))
				{
					return true;
				}

				ECommandResult::Type CommandResult = SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilesToBeCheckedOut);

				if (CommandResult == ECommandResult::Failed)
				{
					UE_LOG(LogAutomationTestExcludelist, Error, TEXT("Failed to check out the configuration file."));
				}
				else if (CommandResult == ECommandResult::Cancelled)
				{
					UE_LOG(LogAutomationTestExcludelist, Warning, TEXT("Checkout was cancelled.."));
				}
				else
				{
					bSuccessfullyCheckedOutOrAddedFile = true;
				}
			}
		}
	}
	return bSuccessfullyCheckedOutOrAddedFile;
}

bool MakeWritable(const FString& InFileToMakeWritable)
{
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*InFileToMakeWritable))
	{
		return true;
	}

	return FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*InFileToMakeWritable, false);
}
#endif

void UAutomationTestExcludelist::SaveConfig()
{
#if WITH_EDITOR
	FString ConfigFilename = GetConfigFilename();
	bool bIsWritable = FPlatformFileManager::Get().GetPlatformFile().FileExists(*ConfigFilename) && !FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*ConfigFilename);
	if (!bIsWritable)
	{
		bIsWritable = CheckOutOrAddFile(ConfigFilename);
		if (!bIsWritable)
		{
			UE_LOG(LogAutomationTestExcludelist, Warning, TEXT("Config file '%s' is readonly and could not be checked out. File will be marked writable."), *ConfigFilename);
			bIsWritable = MakeWritable(ConfigFilename);
		}
	}

	if (!bIsWritable)
	{
		UE_LOG(LogAutomationTestExcludelist, Error, TEXT("Failed to make the configuration file '%s' writable."), *ConfigFilename);
	}
	else
#endif
	{
		UObject::TryUpdateDefaultConfigFile();
	}
}


