// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamBaseActorCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "VCamBaseActor.h"
#include "Algo/ForEach.h"
#include "UObject/PropertyIterator.h"

namespace UE::VCamCoreEditor::Private
{
	namespace
	{
		void SetCategoryPriorities(IDetailLayoutBuilder& DetailBuilder, UClass* Class, ECategoryPriority::Type Priority)
		{
			TSet<FName> CategoryNames;
			for (TFieldIterator<FField> FieldIt(Class); FieldIt; ++FieldIt)
			{
				if (FieldIt->HasMetaData(TEXT("Category")))
				{
					CategoryNames.Add(*FieldIt->GetMetaData(TEXT("Category")));
				}
			}

			Algo::ForEach(CategoryNames, [&DetailBuilder, Priority](FName Category)
			{
				DetailBuilder.EditCategory(Category, FText::GetEmpty(), Priority);
			});
		}
	}
	
	TSharedRef<IDetailCustomization> FVCamBaseActorCustomization::MakeInstance()
	{
		return MakeShared<FVCamBaseActorCustomization>();
	}

	void FVCamBaseActorCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		DetailBuilder.SortCategories([](const TMap<FName, IDetailCategoryBuilder*>& CategoryMap)
		{
			for (const TPair<FName, IDetailCategoryBuilder*>& Pair : CategoryMap)
			{
				int32 SortOrder = Pair.Value->GetSortOrder();
				const FName CategoryName = Pair.Key;

				if (CategoryName == "TransformCommon")
				{
					SortOrder = 0;
				}
				else if(CategoryName == "VirtualCamera")
				{
					SortOrder = 1;
				}
				else if(CategoryName == "Current Camera Settings")
				{
					SortOrder = 2;
				}
				else if(CategoryName == "CameraOptions")
				{
					SortOrder = 3;
				}
				else
				{
					const int32 ValueSortOrder = Pair.Value->GetSortOrder();
					if (ValueSortOrder >= SortOrder && ValueSortOrder < SortOrder + 10)
					{
						SortOrder += 10;
					}
					else
					{
						continue;
					}
				}

				Pair.Value->SetSortOrder(SortOrder);
			}

			return;
		});
	}
}

