// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowNodeColorsRegistry.h"

#include "Dataflow/DataflowNode.h"
#include "Misc/LazySingleton.h"

namespace Dataflow
{
	FNodeColorsRegistry::FNodeColorsRegistry()
	{
		UDataflowSettings* DataflowSettings = GetMutableDefault<UDataflowSettings>();
		DataflowSettingsChangedDelegateHandle = DataflowSettings->GetOnDataflowSettingsChangedDelegate().AddRaw(this, &FNodeColorsRegistry::NodeColorsChangedInSettings);

		const FNodeColorsMap NodeColorsMap = DataflowSettings->GetNodeColorsMap();

		for (auto& Elem : NodeColorsMap)
		{
			if (ColorsMap.Contains(Elem.Key))
			{
				ColorsMap[Elem.Key] = Elem.Value;
			}
			else
			{
				ColorsMap.Add(Elem.Key, Elem.Value);
			}
		}
	}

	FNodeColorsRegistry::~FNodeColorsRegistry()
	{
		if (UObjectInitialized())
		{
			if (IsClassLoaded<UDataflowSettings>())
			{
				UDataflowSettings* DataflowSettings = GetMutableDefault<UDataflowSettings>();
				DataflowSettings->GetOnDataflowSettingsChangedDelegate().Remove(DataflowSettingsChangedDelegateHandle);
			}
		}
	}

	FNodeColorsRegistry& FNodeColorsRegistry::Get()
	{
		return TLazySingleton<FNodeColorsRegistry>::Get();
	}

	void FNodeColorsRegistry::TearDown()
	{
		return TLazySingleton<FNodeColorsRegistry>::TearDown();
	}

	void FNodeColorsRegistry::RegisterNodeColors(const FName& Category, const FNodeColors& NodeColors)
	{
		if (!ColorsMap.Contains(Category))
		{
			ColorsMap.Add(Category, NodeColors);
		}

		// Register colors in DataflowSettings
		GetMutableDefault<UDataflowSettings>()->RegisterColors(Category, NodeColors);
	}

	FLinearColor FNodeColorsRegistry::GetNodeTitleColor(const FName& Category)
	{
		if (ColorsMap.Contains(Category))
		{
			return ColorsMap[Category].NodeTitleColor;
		}
		else
		{
			// Check if any of the parent category registered
			FString CategoryString = Category.ToString();
			if (CategoryString.Contains(TEXT("|")))
			{
				do {
					FString LfString, RtString;
					if (CategoryString.Split(TEXT("|"), &LfString, &RtString, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
					{
						if (ColorsMap.Contains(FName(*LfString)))
						{
							return ColorsMap[FName(*LfString)].NodeTitleColor;
						}

						CategoryString = LfString;
					}
				} while (CategoryString.Contains(TEXT("|")));
			}
		}
		return FNodeColors().NodeTitleColor;
	}

	FLinearColor FNodeColorsRegistry::GetNodeBodyTintColor(const FName& Category)
	{
		if (ColorsMap.Contains(Category))
		{
			return ColorsMap[Category].NodeBodyTintColor;
		}
		else
		{
			// Check if any of the parent category registered
			FString CategoryString = Category.ToString();
			if (CategoryString.Contains(TEXT("|")))
			{
				do {
					FString LfString, RtString;
					if (CategoryString.Split(TEXT("|"), &LfString, &RtString, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
					{
						if (ColorsMap.Contains(FName(*LfString)))
						{
							return ColorsMap[FName(*LfString)].NodeBodyTintColor;
						}

						CategoryString = LfString;
					}
				} while (CategoryString.Contains(TEXT("|")));
			}
		}
		return FNodeColors().NodeBodyTintColor;
	}

	void FNodeColorsRegistry::NodeColorsChangedInSettings(const FNodeColorsMap& NodeColorsMap)
	{
		for (auto& Elem : NodeColorsMap)
		{
			if (ColorsMap.Contains(Elem.Key))
			{
				ColorsMap[Elem.Key] = Elem.Value;
			}
			else
			{
				ColorsMap.Add(Elem.Key, Elem.Value);
			}
		}
	}

}