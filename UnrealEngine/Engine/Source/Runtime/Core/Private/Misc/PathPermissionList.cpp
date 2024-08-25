// Copyright Epic Games, Inc. All Rights Reserved.

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Misc/NamePermissionList.h"
#include "Misc/PathViews.h"
#include "Misc/StringBuilder.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

bool FPathPermissionList::PassesFilter(const FStringView Item) const
{
	if (DenyListAll.Num() > 0)
	{
		return false;
	}

	VerifyItemMatchesListType(Item);

	if (AllowList.Num() > 0 || DenyList.Num() > 0)
	{
		const uint32 ItemHash = GetTypeHash(Item);

		if (AllowList.Num() > 0 && !AllowList.ContainsByHash(ItemHash, Item))
		{
			return false;
		}

		if (DenyList.ContainsByHash(ItemHash, Item))
		{
			return false;
		}
	}

	return true;
}

bool FPathPermissionList::PassesFilter(const FName Item) const
{
	return PassesFilter(FNameBuilder(Item));
}

bool FPathPermissionList::PassesFilter(const TCHAR* Item) const
{
	return PassesFilter(FStringView(Item));
}

bool FPathPermissionList::PassesStartsWithFilter(const FStringView Item, const bool bAllowParentPaths) const
{
	VerifyItemMatchesListType(Item);

	if (AllowList.Num() > 0)
	{
		bool bPassedAllowList = false;
		for (const auto& Other : AllowList)
		{
			if (FPathViews::IsParentPathOf(Other.Key, Item))
			{
				bPassedAllowList = true;
				break;
			}

			if (bAllowParentPaths)
			{
				// If allowing parent paths (eg, when filtering folders), then we must also check if the item has a AllowList child path
				if (FPathViews::IsParentPathOf(Item, Other.Key))
				{
					bPassedAllowList = true;
					break;
				}
			}
		}

		if (!bPassedAllowList)
		{
			return false;
		}
	}

	if (DenyList.Num() > 0)
	{
		for (const auto& Other : DenyList)
		{
			if (FPathViews::IsParentPathOf(Other.Key, Item))
			{
				return false;
			}
		}
	}

	if (DenyListAll.Num() > 0)
	{
		return false;
	}

	return true;
}

bool FPathPermissionList::PassesStartsWithFilter(const FName Item, const bool bAllowParentPaths) const
{
	return PassesStartsWithFilter(FNameBuilder(Item), bAllowParentPaths);
}

bool FPathPermissionList::PassesStartsWithFilter(const TCHAR* Item, const bool bAllowParentPaths) const
{
	return PassesStartsWithFilter(FStringView(Item), bAllowParentPaths);
}

bool FPathPermissionList::AddDenyListItem(const FName OwnerName, const FStringView Item)
{
	VerifyItemMatchesListType(Item);

	const uint32 ItemHash = GetTypeHash(Item);

	FPermissionListOwners* Owners = DenyList.FindByHash(ItemHash, Item);
	const bool bFilterChanged = (Owners == nullptr);
	if (!Owners)
	{
		Owners = &DenyList.AddByHash(ItemHash, FString(Item));
	}

	Owners->AddUnique(OwnerName);
	
	if (bFilterChanged && !bSuppressOnFilterChanged)
	{
		OnFilterChanged().Broadcast();
	}

	return bFilterChanged;
}

bool FPathPermissionList::AddDenyListItem(const FName OwnerName, const FName Item)
{
	return AddDenyListItem(OwnerName, FNameBuilder(Item));
}

bool FPathPermissionList::AddDenyListItem(const FName OwnerName, const TCHAR* Item)
{
	return AddDenyListItem(OwnerName, FStringView(Item));
}

bool FPathPermissionList::RemoveDenyListItem(const FName OwnerName, const FStringView Item)
{
	const uint32 ItemHash = GetTypeHash(Item);

	FPermissionListOwners* Owners = DenyList.FindByHash(ItemHash, Item);
	if (Owners && Owners->Remove(OwnerName) == 1)
	{
		if (Owners->Num() == 0)
		{
			DenyList.RemoveByHash(ItemHash, Item);
		}

		if (!bSuppressOnFilterChanged)
		{
			OnFilterChanged().Broadcast();
		}
		return true;
	}

	return false;
}

bool FPathPermissionList::RemoveDenyListItem(const FName OwnerName, const FName Item)
{
	return RemoveDenyListItem(OwnerName, FNameBuilder(Item));
}

bool FPathPermissionList::RemoveDenyListItem(const FName OwnerName, const TCHAR* Item)
{
	return RemoveDenyListItem(OwnerName, FStringView(Item));
}

bool FPathPermissionList::AddAllowListItem(const FName OwnerName, const FStringView Item)
{
	VerifyItemMatchesListType(Item);

	const uint32 ItemHash = GetTypeHash(Item);

	FPermissionListOwners* Owners = AllowList.FindByHash(ItemHash, Item);
	const bool bFilterChanged = (Owners == nullptr);
	if (!Owners)
	{
		Owners = &AllowList.AddByHash(ItemHash, FString(Item));
	}

	Owners->AddUnique(OwnerName);

	if (bFilterChanged && !bSuppressOnFilterChanged)
	{
		OnFilterChanged().Broadcast();
	}

	return bFilterChanged;
}

bool FPathPermissionList::AddAllowListItem(const FName OwnerName, const FName Item)
{
	return AddAllowListItem(OwnerName, FNameBuilder(Item));
}

bool FPathPermissionList::AddAllowListItem(const FName OwnerName, const TCHAR* Item)
{
	return AddAllowListItem(OwnerName, FStringView(Item));
}

bool FPathPermissionList::AddDenyListAll(const FName OwnerName)
{
	const int32 OldNum = DenyListAll.Num();
	DenyListAll.AddUnique(OwnerName);

	const bool bFilterChanged = OldNum != DenyListAll.Num();
	if (bFilterChanged && !bSuppressOnFilterChanged)
	{
		OnFilterChanged().Broadcast();
	}

	return bFilterChanged;
}

bool FPathPermissionList::RemoveAllowListItem(const FName OwnerName, const FStringView Item)
{
	const uint32 ItemHash = GetTypeHash(Item);

	FPermissionListOwners* Owners = AllowList.FindByHash(ItemHash, Item);
	if (Owners && Owners->Remove(OwnerName) == 1)
	{
		if (Owners->Num() == 0)
		{
			AllowList.RemoveByHash(ItemHash, Item);
		}

		if (!bSuppressOnFilterChanged)
		{
			OnFilterChanged().Broadcast();
		}
		return true;
	}

	return false;
}

bool FPathPermissionList::RemoveAllowListItem(const FName OwnerName, const FName Item)
{
	return RemoveAllowListItem(OwnerName, FNameBuilder(Item));
}

bool FPathPermissionList::RemoveAllowListItem(const FName OwnerName, const TCHAR* Item)
{
	return RemoveAllowListItem(OwnerName, FStringView(Item));
}

bool FPathPermissionList::HasFiltering() const
{
	return DenyList.Num() > 0 || AllowList.Num() > 0 || DenyListAll.Num() > 0;
}

TArray<FName> FPathPermissionList::GetOwnerNames() const
{
	TArray<FName> OwnerNames;

	for (const auto& It : DenyList)
	{
		for (const auto& OwnerName : It.Value)
		{
			OwnerNames.AddUnique(OwnerName);
		}
	}

	for (const auto& It : AllowList)
	{
		for (const auto& OwnerName : It.Value)
		{
			OwnerNames.AddUnique(OwnerName);
		}
	}

	for (const auto& OwnerName : DenyListAll)
	{
		OwnerNames.AddUnique(OwnerName);
	}

	return OwnerNames;
}

bool FPathPermissionList::UnregisterOwner(const FName OwnerName)
{
	bool bFilterChanged = false;

	for (auto It = DenyList.CreateIterator(); It; ++It)
	{
		It->Value.Remove(OwnerName);
		if (It->Value.Num() == 0)
		{
			It.RemoveCurrent();
			bFilterChanged = true;
		}
	}

	for (auto It = AllowList.CreateIterator(); It; ++It)
	{
		It->Value.Remove(OwnerName);
		if (It->Value.Num() == 0)
		{
			It.RemoveCurrent();
			bFilterChanged = true;
		}
	}

	bFilterChanged |= (DenyListAll.Remove(OwnerName) > 0);

	if (bFilterChanged && !bSuppressOnFilterChanged)
	{
		OnFilterChanged().Broadcast();
	}

	return bFilterChanged;
}

bool FPathPermissionList::UnregisterOwners(const TArray<FName>& OwnerNames)
{
	bool bFilterChanged = false;
	{
		TGuardValue<bool> Guard(bSuppressOnFilterChanged, true);

		for (const FName& OwnerName : OwnerNames)
		{
			bFilterChanged |= UnregisterOwner(OwnerName);
		}
	}

	if (bFilterChanged && !bSuppressOnFilterChanged)
	{
		OnFilterChanged().Broadcast();
	}

	return bFilterChanged;
}

bool FPathPermissionList::Append(const FPathPermissionList& Other)
{
	ensureAlwaysMsgf(ListType == Other.ListType, TEXT("Trying to combine PathPermissionLists of different types"));

	bool bFilterChanged = false;
	{
		TGuardValue<bool> Guard(bSuppressOnFilterChanged, true);

		for (const auto& It : Other.DenyList)
		{
			for (const auto& OwnerName : It.Value)
			{
				bFilterChanged |= AddDenyListItem(OwnerName, It.Key);
			}
		}

		for (const auto& It : Other.AllowList)
		{
			for (const auto& OwnerName : It.Value)
			{
				bFilterChanged |= AddAllowListItem(OwnerName, It.Key);
			}
		}

		for (const auto& OwnerName : Other.DenyListAll)
		{
			bFilterChanged |= AddDenyListAll(OwnerName);
		}
	}

	if (bFilterChanged && !bSuppressOnFilterChanged)
	{
		OnFilterChanged().Broadcast();
	}

	return bFilterChanged;
}

FPathPermissionList FPathPermissionList::CombinePathFilters(const FPathPermissionList& OtherFilter) const
{
	FPathPermissionList Result;

	if (IsDenyListAll() || OtherFilter.IsDenyListAll())
	{
		Result.AddDenyListAll(NAME_None);
	}

	for (const TPair<FString, FPermissionListOwners>& It : GetDenyList())
	{
		for (const FName& OwnerName : It.Value)
		{
			Result.AddDenyListItem(OwnerName, It.Key);
		}
	}

	for (const TPair<FString, FPermissionListOwners>& It : OtherFilter.GetDenyList())
	{
		for (const FName& OwnerName : It.Value)
		{
			Result.AddDenyListItem(OwnerName, It.Key);
		}
	}

	if (GetAllowList().Num() > 0 || OtherFilter.GetAllowList().Num() > 0)
	{
		for (const TPair<FString, FPermissionListOwners>& It : GetAllowList())
		{
			const FString& Path = It.Key;
			if (OtherFilter.PassesStartsWithFilter(Path, true))
			{
				for (const FName& OwnerName : It.Value)
				{
					Result.AddAllowListItem(OwnerName, Path);
				}
			}
		}

		for (const TPair<FString, FPermissionListOwners>& It : OtherFilter.GetAllowList())
		{
			const FString& Path = It.Key;
			if (PassesStartsWithFilter(Path, true))
			{
				for (const FName& OwnerName : It.Value)
				{
					Result.AddAllowListItem(OwnerName, Path);
				}
			}
		}

		// Block everything if none of the AllowList paths passed
		if (Result.GetAllowList().Num() == 0)
		{
			Result.AddDenyListAll(NAME_None);
		}
	}

	return Result;
}

bool FPathPermissionList::UnregisterOwnersAndAppend(const TArray<FName>& OwnerNamesToRemove, const FPathPermissionList& FiltersToAdd)
{
	bool bFilterChanged = false;
	{
		TGuardValue<bool> Guard(bSuppressOnFilterChanged, true);

		bFilterChanged |= UnregisterOwners(OwnerNamesToRemove);
		bFilterChanged |= Append(FiltersToAdd);
	}

	if (bFilterChanged && !bSuppressOnFilterChanged)
	{
		OnFilterChanged().Broadcast();
	}

	return bFilterChanged;
}

// Extracted the ensure condition into a separate function so that logs are easier to read
FORCEINLINE static bool IsClassPathNameOrNone(const FStringView Item)
{
	return !Item.Len() || Item[0] == '/' || Item == TEXTVIEW("None");
}

void FPathPermissionList::VerifyItemMatchesListType(const FStringView Item) const
{
	if (ListType == EPathPermissionListType::ClassPaths)
	{
		// Long names always have / as first character
		ensureAlwaysMsgf(IsClassPathNameOrNone(Item), TEXT("Short class name \"%.*s\" provided for PathPermissionList representing class paths"), Item.Len(), Item.GetData());
	}
}

FString FPathPermissionList::ToString() const
{
	TStringBuilder<4096> StringBuilder;

	auto SortAndAppendOwners = [&StringBuilder](FPermissionListOwners& Owners)
	{
		Owners.Sort(FNameLexicalLess());

		StringBuilder.AppendChar(TCHAR('('));
		bool bFirst = true;
		for (FName Owner : Owners)
		{
			if (bFirst)
			{
				bFirst = false;
			}
			else
			{
				StringBuilder.Append(TEXT(", "));
			}
			StringBuilder.Append(Owner.ToString());
		}
		StringBuilder.AppendChar(TCHAR(')'));
	};

	if (!DenyListAll.IsEmpty())
	{
		StringBuilder.Append(TEXT("Deny All "));
		FPermissionListOwners SortedDenyListAll = DenyListAll;
		SortAndAppendOwners(SortedDenyListAll);
		StringBuilder.Append(TEXT("\n"));;
	}

	auto AppendList = [&StringBuilder, &SortAndAppendOwners](const TMap<FString, FPermissionListOwners>& List)
	{
		TMap<FString, FPermissionListOwners> SortedList = List;
		SortedList.KeySort(TLess<FString>());
		for (TPair<FString, FPermissionListOwners>& ListEntry : SortedList)
		{
			StringBuilder.AppendChar(TCHAR('\t'));
			StringBuilder.AppendChar(TCHAR('"'));
			StringBuilder.Append(ListEntry.Key);
			StringBuilder.AppendChar(TCHAR('"'));
			StringBuilder.AppendChar(TCHAR(' '));
			SortAndAppendOwners(ListEntry.Value);
			StringBuilder.AppendChar(TCHAR('\n'));
		}
	};

	if (!DenyList.IsEmpty())
	{
		StringBuilder.Append(TEXT("Deny List\n"));
		AppendList(DenyList);
	}

	if (!AllowList.IsEmpty())
	{
		StringBuilder.Append(TEXT("Allow List\n"));
		AppendList(AllowList);
	}

	return StringBuilder.ToString();
}