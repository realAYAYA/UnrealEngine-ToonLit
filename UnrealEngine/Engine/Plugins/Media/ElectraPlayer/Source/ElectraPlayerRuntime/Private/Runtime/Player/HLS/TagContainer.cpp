// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Parser.h"

namespace Electra
{
	namespace HLSPlaylistParser
	{

		void TTagContainer::AddTag(FString& TagKey, FTagContent& Tag)
		{
			Tags.FindOrAdd(TagKey).Add(MoveTemp(Tag));
        }

        bool TTagContainer::ContainsTag(const FString& TagName) const
        {
			return Tags.Contains(TagName);
        }

		int TTagContainer::TagNum(const FString& TagName) const
		{
			const TArray<FTagContent>* Content = Tags.Find(TagName);
			if (Content == nullptr)
			{
				return 0;
			}

			return Content->Num();
		}

        EPlaylistError TTagContainer::ValidateTags(const TMap<FString, FMediaTag>& TagDefinitions,
                                                   const FMediaTagOption& Target)
        {
			for (auto& Elem : TagDefinitions)
			{
				FMediaTag Tag = Elem.Value;
				if (Tag.HasOption(Target))
				{
					if (Tag.HasOption(Required) && !Tags.Contains(Elem.Key))
					{
						return EPlaylistError::MissingRequiredTag;
					}
				}
			}

            return EPlaylistError::None;
        }
    }
} // namespace Electra


