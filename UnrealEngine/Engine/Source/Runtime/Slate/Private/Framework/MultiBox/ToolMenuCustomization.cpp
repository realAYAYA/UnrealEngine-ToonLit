// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/MultiBox/ToolMenuBase.h"

FName FCustomizedToolMenu::GetEntrySectionName(const FName InEntryName) const
{
	for (auto& It : EntryOrder)
	{
		if (It.Value.Names.Contains(InEntryName))
		{
			return It.Key;
		}
	}

	return NAME_None;
}

FCustomizedToolMenuEntry* FCustomizedToolMenu::FindEntry(const FName InEntryName)
{
	return Entries.Find(InEntryName);
	//return Entries.FindByPredicate([=](const FCustomizedToolMenuEntry& Entry) { return Entry.Name == InEntryName; });
}

const FCustomizedToolMenuEntry* FCustomizedToolMenu::FindEntry(const FName InEntryName) const
{
	return Entries.Find(InEntryName);
	//return Entries.FindByPredicate([=](const FCustomizedToolMenuEntry& Entry) { return Entry.Name == InEntryName; });
}

FCustomizedToolMenuEntry* FCustomizedToolMenu::AddEntry(const FName InEntryName)
{
	return &Entries.FindOrAdd(InEntryName);
}

ECustomizedToolMenuVisibility FCustomizedToolMenu::GetEntryVisiblity(const FName InEntryName) const
{
	if (const FCustomizedToolMenuEntry* Found = FindEntry(InEntryName))
	{
		return Found->Visibility;
	}

	return ECustomizedToolMenuVisibility::None;
}

bool FCustomizedToolMenu::IsEntryHidden(const FName InEntryName) const
{
	return GetEntryVisiblity(InEntryName) == ECustomizedToolMenuVisibility::Hidden;
}

FCustomizedToolMenuSection* FCustomizedToolMenu::FindSection(const FName InSectionName)
{
	return Sections.Find(InSectionName);
	//return Sections.FindByPredicate([=](const FCustomizedToolMenuSection& Section) { return Section.Name == InSectionName; });
}

const FCustomizedToolMenuSection* FCustomizedToolMenu::FindSection(const FName InSectionName) const
{
	return Sections.Find(InSectionName);
	//return Sections.FindByPredicate([=](const FCustomizedToolMenuSection& Section) { return Section.Name == InSectionName; });
}

FCustomizedToolMenuSection* FCustomizedToolMenu::AddSection(const FName InSectionName)
{
	return &Sections.FindOrAdd(InSectionName);
}

ECustomizedToolMenuVisibility FCustomizedToolMenu::GetSectionVisiblity(const FName InSectionName) const
{
	if (const FCustomizedToolMenuSection* Found = FindSection(InSectionName))
	{
		return Found->Visibility;
	}

	return ECustomizedToolMenuVisibility::None;
}

bool FCustomizedToolMenu::IsSectionHidden(const FName InEntryName) const
{
	return GetSectionVisiblity(InEntryName) == ECustomizedToolMenuVisibility::Hidden;
}

FName FCustomizedToolMenuHierarchy::GetEntrySectionName(const FName InEntryName) const
{
	for (int32 i = Hierarchy.Num() - 1; i >= 0; --i)
	{
		if (Hierarchy[i])
		{
			FName SectionName = Hierarchy[i]->GetEntrySectionName(InEntryName);
			if (SectionName != NAME_None)
			{
				return SectionName;
			}
		}
	}

	return NAME_None;
}

bool FCustomizedToolMenuHierarchy::IsEntryHidden(const FName InEntryName) const
{
	for (int32 i = Hierarchy.Num() - 1; i >= 0; --i)
	{
		if (Hierarchy[i])
		{
			if (const FCustomizedToolMenuEntry* Found = Hierarchy[i]->FindEntry(InEntryName))
			{
				if (Found->Visibility == ECustomizedToolMenuVisibility::Hidden)
				{
					return true;
				}
				else if (Found->Visibility == ECustomizedToolMenuVisibility::Visible)
				{
					return false;
				}
			}
		}
	}

	return false;
}

bool FCustomizedToolMenuHierarchy::IsSectionHidden(const FName InSectionName) const
{
	for (int32 i = Hierarchy.Num() - 1; i >= 0; --i)
	{
		if (Hierarchy[i])
		{
			if (const FCustomizedToolMenuSection* Found = Hierarchy[i]->FindSection(InSectionName))
			{
				if (Found->Visibility == ECustomizedToolMenuVisibility::Hidden)
				{
					return true;
				}
				else if (Found->Visibility == ECustomizedToolMenuVisibility::Visible)
				{
					return false;
				}
			}
		}
	}

	return false;
}

FCustomizedToolMenu FCustomizedToolMenuHierarchy::GenerateFlattened() const
{
	static auto HandleCustomizedToolMenu = [](FCustomizedToolMenu& Result, const FCustomizedToolMenu* Current)
	{
		if (!Current)
		{
			return;
		}

		if (Current->SectionOrder.Num() > 0)
		{
			Result.SectionOrder = Current->SectionOrder;
		}

		for (const auto& SectionEntryOrderIterator : Current->EntryOrder)
		{
			Result.EntryOrder.Add(SectionEntryOrderIterator.Key, SectionEntryOrderIterator.Value);
		}

		for (const auto& EntryIterator : Current->Entries)
		{
			if (FCustomizedToolMenuEntry* ExistingEntry = Result.FindEntry(EntryIterator.Key))
			{
				if (EntryIterator.Value.Visibility != ECustomizedToolMenuVisibility::None)
				{
					ExistingEntry->Visibility = EntryIterator.Value.Visibility;
				}
			}
			else
			{
				Result.Entries.Add(EntryIterator.Key, EntryIterator.Value);
			}
		}

		for (const auto& SectionIterator : Current->Sections)
		{
			if (FCustomizedToolMenuSection* ExistingSection = Result.FindSection(SectionIterator.Key))
			{
				if (SectionIterator.Value.Visibility != ECustomizedToolMenuVisibility::None)
				{
					ExistingSection->Visibility = SectionIterator.Value.Visibility;
				}
			}
			else
			{
				Result.Sections.Add(SectionIterator.Key, SectionIterator.Value);
			}
		}

		Result.MenuPermissions.Append(Current->MenuPermissions);
	};

	// Process parents first then children
	// Each customization has chance to override what has already been customized before it
	FCustomizedToolMenu Destination;

	for (const FCustomizedToolMenu* Current : Hierarchy)
	{
		HandleCustomizedToolMenu(Destination, Current);
	}

	for (const FCustomizedToolMenu* Current : RuntimeHierarchy)
	{
		HandleCustomizedToolMenu(Destination, Current);
	}

	return Destination;
}
