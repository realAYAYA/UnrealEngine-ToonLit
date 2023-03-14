// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorFilterLibrary.h"

#include "EditorScriptingUtils.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorFilterLibrary)

#define LOCTEXT_NAMESPACE "EditorFilterLibrary"

/**
 *
 * Editor Scripting | Utilities
 *
 **/
namespace InternalEditorFilterLibrary
{
	template<class T, typename BinaryOperation>
	TArray<T*> Filter(const TArray<T*>& TargetArray, EEditorScriptingFilterType FilterType, BinaryOperation Op)
	{
		TArray<T*> Result;
		Result.Reserve(TargetArray.Num());

		if (FilterType == EEditorScriptingFilterType::Exclude)
		{
			for (T* Itt : TargetArray)
			{
				if (IsValid(Itt) && !Op(Itt))
				{
					Result.Add(Itt);
				}
			}
		}
		else if (FilterType == EEditorScriptingFilterType::Include)
		{
			for (T* Itt : TargetArray)
			{
				if (IsValid(Itt) && Op(Itt))
				{
					Result.Add(Itt);
				}
			}
		}

		return Result;
	}

	template<class T, typename GetStringOperation>
	TArray<T*> StringFilter(const TArray<T*>& TargetArray, const FString& SearchString, EEditorScriptingFilterType FilterType, EEditorScriptingStringMatchType StringMatch, bool bIgnoreCase, GetStringOperation Operator)
	{
		const ESearchCase::Type SearchCase = bIgnoreCase ? ESearchCase::IgnoreCase : ESearchCase::CaseSensitive;
		if (StringMatch == EEditorScriptingStringMatchType::Contains)
		{
			return InternalEditorFilterLibrary::Filter(TargetArray, FilterType, [&](T* Obj)
			{
				return Operator(Obj).Contains(SearchString, SearchCase);
			});
		}
		else if (StringMatch == EEditorScriptingStringMatchType::ExactMatch)
		{
			return InternalEditorFilterLibrary::Filter(TargetArray, FilterType, [&](T* Obj)
			{
				return Operator(Obj).Compare(SearchString, SearchCase) == 0;
			});
		}
		else
		{
			return InternalEditorFilterLibrary::Filter(TargetArray, FilterType, [&](T* Obj)
			{
				return Operator(Obj).MatchesWildcard(SearchString, SearchCase);
			});
		}
	}
}

TArray<UObject*> UEditorFilterLibrary::ByClass(const TArray<UObject*>& TargetArray, TSubclassOf<class UObject> ObjectClass, EEditorScriptingFilterType FilterType)
{
	if (ObjectClass.Get())
	{
		if (ObjectClass.Get() == UObject::StaticClass())
		{
			return InternalEditorFilterLibrary::Filter(TargetArray, FilterType, [&](UObject* Obj)
			{
				return true;
			});
		}
		else
		{
			return InternalEditorFilterLibrary::Filter(TargetArray, FilterType, [&](UObject* Obj)
			{
				return Obj->IsA(ObjectClass.Get());
			});
		}
	}
	else
	{
		return TargetArray;
	}
}

TArray<UObject*> UEditorFilterLibrary::ByIDName(const TArray<UObject*>& TargetArray, const FString& InName, EEditorScriptingStringMatchType StringMatch, EEditorScriptingFilterType FilterType)
{
	return InternalEditorFilterLibrary::StringFilter(TargetArray, InName, FilterType, StringMatch, true, [&](UObject* Obj)
	{
		return Obj->GetName();
	});
}

TArray<AActor*> UEditorFilterLibrary::ByActorLabel(const TArray<AActor*>& TargetArray, const FString& InName, EEditorScriptingStringMatchType StringMatch, EEditorScriptingFilterType FilterType, bool bIgnoreCase)
{
	return InternalEditorFilterLibrary::StringFilter(TargetArray, InName, FilterType, StringMatch, bIgnoreCase, [&](AActor* Obj)
	{
		return Obj->GetActorLabel();
	});
}

TArray<AActor*> UEditorFilterLibrary::ByActorTag(const TArray<AActor*>& TargetArray, FName Tag, EEditorScriptingFilterType FilterType)
{
	return InternalEditorFilterLibrary::Filter(TargetArray, FilterType, [&](AActor* Obj)
	{
		return Obj->ActorHasTag(Tag);
	});
}

TArray<AActor*> UEditorFilterLibrary::ByLayer(const TArray<AActor*>& TargetArray, FName LayerName, EEditorScriptingFilterType FilterType)
{
	return InternalEditorFilterLibrary::Filter(TargetArray, FilterType, [&](AActor* Obj)
	{
		return Obj->Layers.Contains(LayerName);
	});
}

TArray<AActor*> UEditorFilterLibrary::ByLevelName(const TArray<AActor*>& TargetArray, FName LevelName, EEditorScriptingFilterType FilterType)
{
	FString LevelNameStr = LevelName.ToString();
	return InternalEditorFilterLibrary::Filter(TargetArray, FilterType, [&](AActor* Obj)
	{
		return FPackageName::GetShortName(Obj->GetOutermost()) == LevelNameStr;
	});
}

TArray<AActor*> UEditorFilterLibrary::BySelection(const TArray<AActor*>& TargetArray, EEditorScriptingFilterType FilterType)
{
	return InternalEditorFilterLibrary::Filter(TargetArray, FilterType, [&](AActor* Obj)
	{
		return Obj->IsSelected();
	});
}

#undef LOCTEXT_NAMESPACE

