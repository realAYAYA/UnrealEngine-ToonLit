// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectFilter/ObjectMixerEditorObjectFilter.h"

#include "GameFramework/Actor.h"
#include "Kismet2/ComponentEditorUtils.h"

FText UObjectMixerObjectFilter::GetRowDisplayName(UObject* InObject, const bool bIsHybridRow) const
{
	if (IsValid(InObject))
	{
		if (const AActor* AsActor = Cast<AActor>(InObject))
		{
			return FText::FromString(AsActor->GetActorLabel());
		}

		if (bIsHybridRow)
		{
			if (const AActor* Outer = InObject->GetTypedOuter<AActor>())
			{
				return FText::FromString(Outer->GetActorLabel());
			}
		}
		
		if (const UActorComponent* AsActorComponent = Cast<UActorComponent>(InObject))
		{
			const FName ComponentName = FComponentEditorUtils::FindVariableNameGivenComponentInstance(AsActorComponent);
			if (ComponentName != NAME_None)
			{
				return FText::FromName(ComponentName);
			}
		}
		
		return FText::FromString(InObject->GetName());
	}

	return FText::GetEmpty();
}

FText UObjectMixerObjectFilter::GetRowTooltipText(UObject* InObject, const bool bIsHybridRow) const
{
	if (IsValid(InObject))
	{
		if (const AActor* Outer = InObject->GetTypedOuter<AActor>())
		{
			return FText::Format(INVTEXT("{0} ({1})"), FText::FromString(Outer->GetActorLabel()), FText::FromString(InObject->GetName()));
		}
		
		if (const AActor* AsActor = Cast<AActor>(InObject))
		{
			return FText::FromString(AsActor->GetActorLabel());
		}
		
		if (const UActorComponent* AsActorComponent = Cast<UActorComponent>(InObject))
		{
			const FName ComponentName = FComponentEditorUtils::FindVariableNameGivenComponentInstance(AsActorComponent);
			if (ComponentName != NAME_None)
			{
				if (!ComponentName.IsEqual(AsActorComponent->GetFName()))
				{
					return
					   FText::Format(INVTEXT("{0} ({1})"),
						   FText::FromName(ComponentName),
						   FText::FromName(AsActorComponent->GetFName())
					   );
				}

				return FText::FromName(ComponentName);
			}
		}
		
		return FText::FromString(InObject->GetName());
	}

	return FText::GetEmpty();
}

bool UObjectMixerObjectFilter::GetRowEditorVisibility(UObject* InObject) const
{
	if (IsValid(InObject))
	{
		TObjectPtr<AActor> Actor = Cast<AActor>(InObject);
	
		if (!Actor)
		{
			Actor = InObject->GetTypedOuter<AActor>();
		}

		return Actor ? !Actor->IsTemporarilyHiddenInEditor() : false;
	}

	return false;
}

void UObjectMixerObjectFilter::OnSetRowEditorVisibility(UObject* InObject, bool bNewIsVisible) const
{
	if (IsValid(InObject))
	{
		TObjectPtr<AActor> Actor = Cast<AActor>(InObject);
	
		if (!Actor)
		{
			Actor = InObject->GetTypedOuter<AActor>();
		}

		if (Actor)
		{
			Actor->SetIsTemporarilyHiddenInEditor(!bNewIsVisible);
		}
	}
}

TSet<FName> UObjectMixerObjectFilter::GetColumnsToShowByDefault() const
{
	return {};
}

TSet<FName> UObjectMixerObjectFilter::GetColumnsToExclude() const
{
	return {};
}

EObjectMixerInheritanceInclusionOptions UObjectMixerObjectFilter::GetObjectMixerPropertyInheritanceInclusionOptions() const
{
	return EObjectMixerInheritanceInclusionOptions::None;
}

EObjectMixerInheritanceInclusionOptions UObjectMixerObjectFilter::GetObjectMixerPlacementClassInclusionOptions() const
{
	return EObjectMixerInheritanceInclusionOptions::None;
}

bool UObjectMixerObjectFilter::ShouldIncludeUnsupportedProperties() const
{
	return false;
}

TSet<UClass*> UObjectMixerObjectFilter::GetParentAndChildClassesFromSpecifiedClasses(
	const TSet<UClass*>& InSpecifiedClasses, EObjectMixerInheritanceInclusionOptions Options)
{
	// 'None' means we only want the specified classes
	if (Options == EObjectMixerInheritanceInclusionOptions::None)
	{
		return InSpecifiedClasses;
	}
	
	TSet<UClass*> ReturnValue;

	auto GetChildClassesLambda = [](const UClass* Class, TSet<UClass*>& OutReturnValue, const bool bRecursive)
	{
		TArray<UClass*> DerivedClasses;
		GetDerivedClasses(Class, DerivedClasses, bRecursive);
		OutReturnValue.Append(DerivedClasses);
	};

	for (UClass* Class : InSpecifiedClasses)
	{
		ReturnValue.Add(Class);

		// Super Classes

			// Immediate only
		if (Options == EObjectMixerInheritanceInclusionOptions::IncludeOnlyImmediateParent ||
			Options == EObjectMixerInheritanceInclusionOptions::IncludeOnlyImmediateParentAndChildren ||
			Options == EObjectMixerInheritanceInclusionOptions::IncludeOnlyImmediateParentAndAllChildren)
		{
			if (UClass* Super = Class->GetSuperClass())
			{
				ReturnValue.Add(Super);
			}
		}
			// All Parents
		if (Options == EObjectMixerInheritanceInclusionOptions::IncludeAllParents ||
			Options == EObjectMixerInheritanceInclusionOptions::IncludeAllParentsAndChildren ||
			Options == EObjectMixerInheritanceInclusionOptions::IncludeAllParentsAndOnlyImmediateChildren)
		{
			if (UClass* Super = Class->GetSuperClass())
			{
				while (Super)
				{
					ReturnValue.Add(Super);
					Super = Super->GetSuperClass();
				}
			}
		}

		// Child Classes

			// Immediate only
		if (Options == EObjectMixerInheritanceInclusionOptions::IncludeOnlyImmediateChildren ||
			Options == EObjectMixerInheritanceInclusionOptions::IncludeOnlyImmediateParentAndChildren ||
			Options == EObjectMixerInheritanceInclusionOptions::IncludeAllParentsAndOnlyImmediateChildren)
		{
			GetChildClassesLambda(Class, ReturnValue, false);
		}
			// All Children
		if (Options == EObjectMixerInheritanceInclusionOptions::IncludeAllChildren ||
			Options == EObjectMixerInheritanceInclusionOptions::IncludeAllParentsAndChildren ||
			Options == EObjectMixerInheritanceInclusionOptions::IncludeOnlyImmediateParentAndAllChildren)
		{
			GetChildClassesLambda(Class, ReturnValue, true);
		}
	}

	return ReturnValue;
}

TSet<UClass*> UObjectMixerObjectFilter::GetParentAndChildClassesFromSpecifiedClasses(
	const TSet<TSubclassOf<AActor>>& InSpecifiedClasses, EObjectMixerInheritanceInclusionOptions Options)
{
	TSet<UClass*> AsUClasses;
	
	for (TSubclassOf<AActor> ActorClass : InSpecifiedClasses)
	{
		AsUClasses.Add(ActorClass);
	}

	return GetParentAndChildClassesFromSpecifiedClasses(AsUClasses, Options);
}

EFieldIterationFlags UObjectMixerObjectFilter::GetDesiredFieldIterationFlags(const bool bIncludeInheritedProperties)
{
	return bIncludeInheritedProperties ? EFieldIterationFlags::IncludeSuper : EFieldIterationFlags::Default;
}

TSet<FName> UObjectMixerObjectFilter::GenerateIncludeListFromExcludeList(const TSet<FName>& ExcludeList) const
{
	TSet<FName> IncludeList;

	const EObjectMixerInheritanceInclusionOptions Options =
		GetObjectMixerPropertyInheritanceInclusionOptions();
	TSet<UClass*> SpecifiedClasses =
		GetParentAndChildClassesFromSpecifiedClasses(GetObjectClassesToFilter(), Options);
	
	for (const UClass* Class : SpecifiedClasses)
	{
		for (TFieldIterator<FProperty> FieldIterator(Class); FieldIterator; ++FieldIterator)
		{
			if (const FProperty* Property = *FieldIterator)
			{
				IncludeList.Add(Property->GetFName());
			}
		}
	}

	return IncludeList.Difference(ExcludeList);
}
