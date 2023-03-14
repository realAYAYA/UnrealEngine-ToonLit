// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsDiff.h"

#include "BlueprintDetailsCustomization.h"
#include "Algo/Copy.h"
#include "Algo/Transform.h"
#include "Containers/Set.h"
#include "DetailsViewArgs.h"
#include "HAL/PlatformCrt.h"
#include "IDetailsView.h"
#include "Misc/AssertionMacros.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "PropertyPath.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Engine/MemberReference.h"

class UObject;

FDetailsDiff::FDetailsDiff(const UObject* InObject, FOnDisplayedPropertiesChanged InOnDisplayedPropertiesChanged )
	: OnDisplayedPropertiesChanged( InOnDisplayedPropertiesChanged )
	, DisplayedObject( InObject )
	, DetailsView()
{
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bShowDifferingPropertiesOption = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateStatic([]{return false; }));
	if (InObject->IsA<UBlueprint>())
	{
		// create a custom property layout so that sections like interfaces will be included in the diff view
		const FOnGetDetailCustomizationInstance LayoutOptionDetails = FOnGetDetailCustomizationInstance::CreateStatic(
			&FBlueprintGlobalOptionsDetails::MakeInstanceForDiff,
			const_cast<UBlueprint *>(Cast<UBlueprint>(InObject))
		);
		DetailsView->RegisterInstancedCustomPropertyLayout(UBlueprint::StaticClass(), LayoutOptionDetails);
	}
	// Forcing all advanced properties to be displayed for now, the logic to show changes made to advance properties
	// conditionally is fragile and low priority for now:
	DetailsView->ShowAllAdvancedProperties();
	// This is a read only details view (see the property editing delegate), but const correctness at the details view
	// level would stress the type system a little:
	DetailsView->SetObject(const_cast<UObject*>(InObject));
	DetailsView->SetOnDisplayedPropertiesChanged( ::FOnDisplayedPropertiesChanged::CreateRaw(this, &FDetailsDiff::HandlePropertiesChanged) );

	DifferingProperties = DetailsView->GetPropertiesInOrderDisplayed();
}

FDetailsDiff::~FDetailsDiff()
{
	DetailsView->SetOnDisplayedPropertiesChanged(::FOnDisplayedPropertiesChanged());
}

void FDetailsDiff::HighlightProperty(const FPropertySoftPath& PropertyName)
{
	// resolve the property soft path:
	const FPropertyPath ResolvedProperty = PropertyName.ResolvePath(DisplayedObject);
	DetailsView->HighlightProperty(ResolvedProperty);
}

TSharedRef< IDetailsView > FDetailsDiff::DetailsWidget() const
{
	return DetailsView.ToSharedRef();
}

void FDetailsDiff::HandlePropertiesChanged()
{
	if( OnDisplayedPropertiesChanged.IsBound() )
	{
		DifferingProperties = DetailsView->GetPropertiesInOrderDisplayed();
		OnDisplayedPropertiesChanged.Execute();
	}
}

TArray<FPropertySoftPath> FDetailsDiff::GetDisplayedProperties() const
{
	TArray<FPropertySoftPath> Ret;
	Algo::Copy(DifferingProperties, Ret);
	return Ret;
}

void FDetailsDiff::DiffAgainst(const FDetailsDiff& Newer, TArray< FSingleObjectDiffEntry > &OutDifferences, bool bSortByDisplayOrder) const
{
	TSharedPtr< class IDetailsView > OldDetailsView = DetailsView;
	TSharedPtr< class IDetailsView > NewDetailsView = Newer.DetailsView;

	const TArray<TWeakObjectPtr<UObject>>& OldSelectedObjects = OldDetailsView->GetSelectedObjects();
	const TArray<TWeakObjectPtr<UObject>>& NewSelectedObjects = NewDetailsView->GetSelectedObjects();

	const TArray<FPropertyPath> OldProperties = OldDetailsView->GetPropertiesInOrderDisplayed();
	const TArray<FPropertyPath> NewProperties = NewDetailsView->GetPropertiesInOrderDisplayed();

	TSet<FPropertySoftPath> OldPropertiesSet;
	TSet<FPropertySoftPath> NewPropertiesSet;

	Algo::Transform(OldProperties, OldPropertiesSet, [](const FPropertyPath& Entry) { return FPropertySoftPath(Entry); });
	Algo::Transform(NewProperties, NewPropertiesSet, [](const FPropertyPath& Entry) { return FPropertySoftPath(Entry); });

	// detect removed properties:
	const TSet<FPropertySoftPath> RemovedProperties = OldPropertiesSet.Difference(NewPropertiesSet);
	for (const FPropertySoftPath& RemovedProperty : RemovedProperties)
	{
		// @todo: (doc) label these as removed, rather than added to a
		FSingleObjectDiffEntry Entry(RemovedProperty, EPropertyDiffType::PropertyAddedToA);
		OutDifferences.Push(Entry);
	}

	// detect added properties:
	const TSet<FPropertySoftPath> AddedProperties = NewPropertiesSet.Difference(OldPropertiesSet);
	for (const FPropertySoftPath& AddedProperty : AddedProperties)
	{
		FSingleObjectDiffEntry Entry(AddedProperty, EPropertyDiffType::PropertyAddedToB);
		OutDifferences.Push(Entry);
	}

	// check for changed properties
	const TSet<FPropertySoftPath> CommonProperties = NewPropertiesSet.Intersect(OldPropertiesSet);
	for (const FPropertySoftPath& CommonProperty : CommonProperties)
	{
		// get value, diff:
		check(NewSelectedObjects.Num() == 1);
		FResolvedProperty OldProperty = CommonProperty.Resolve(OldSelectedObjects[0].Get());
		FResolvedProperty NewProperty = CommonProperty.Resolve(NewSelectedObjects[0].Get());

		TArray<FPropertySoftPath> DifferingSubProperties;

		if (!DiffUtils::Identical(OldProperty, NewProperty, CommonProperty, DifferingSubProperties))
		{
			for (const FPropertySoftPath& DifferingSubProperty : DifferingSubProperties)
			{
				OutDifferences.Push(FSingleObjectDiffEntry(DifferingSubProperty, EPropertyDiffType::PropertyValueChanged));
			}
		}
	}

	if (bSortByDisplayOrder)
	{
		TArray<FSingleObjectDiffEntry> AllDifferingProperties = OutDifferences;
		OutDifferences.Reset();

		// OrderedProperties will contain differences in the order they are displayed:
		TArray< const FSingleObjectDiffEntry* > OrderedProperties;

		// create differing properties list based on what is displayed by the old properties..
		TArray<FPropertySoftPath> SoftOldProperties = GetDisplayedProperties();
		TArray<FPropertySoftPath> SoftNewProperties = Newer.GetDisplayedProperties();

		const auto FindAndPushDiff = [&OrderedProperties, &AllDifferingProperties](const FPropertySoftPath& PropertyIdentifier) -> bool
		{
			bool bDiffers = false;
			for (const auto& Difference : AllDifferingProperties)
			{
				if (Difference.Identifier == PropertyIdentifier)
				{
					bDiffers = true;
					// if there are any nested differences associated with PropertyIdentifier, add those
					// as well:
					OrderedProperties.AddUnique(&Difference);
				}
				else if (Difference.Identifier.IsSubPropertyMatch(PropertyIdentifier))
				{
					bDiffers = true;
					OrderedProperties.AddUnique(&Difference);
				}
			}
			return bDiffers;
		};

		// zip the two sets of properties, zip iterators are not trivial to write in c++,
		// so this procedural stuff will have to do:
		int IterOld = 0;
		int IterNew = 0;
		while (IterOld < SoftOldProperties.Num() || IterNew < SoftNewProperties.Num())
		{
			const bool bOldIterValid = IterOld < SoftOldProperties.Num();
			const bool bNewIterValid = IterNew < SoftNewProperties.Num();

			// We've reached the end of the new list, but still have properties in the old list.
			// Continue over the old list to catch any remaining diffs.
			if (bOldIterValid && !bNewIterValid)
			{
				FindAndPushDiff(SoftOldProperties[IterOld]);
				++IterOld;
			}
			// We've reached the end of the old list, but still have properties in the new list.
			// Continue over the new list to catch any remaining diffs.
			else if (!bOldIterValid && bNewIterValid)
			{
				FindAndPushDiff(SoftNewProperties[IterNew]);
				++IterNew;
			}
			else
			{
				// If both properties have the same path, check to ensure the property hasn't changed.
				if (SoftOldProperties[IterOld] == SoftNewProperties[IterNew])
				{
					FindAndPushDiff(SoftOldProperties[IterOld]);
					++IterNew;
					++IterOld;
				}
				else
				{
					// If the old property is different, add it to the list and increment the old iter.
					// This indicates the property was removed.
					if (FindAndPushDiff(SoftOldProperties[IterOld]))
					{
						++IterOld;
					}
					// If the new property is different, add it to the list and increment the new iter.
					// This indicates the property was added.
					else if (FindAndPushDiff(SoftNewProperties[IterNew]))
					{
						++IterNew;
					}
					// Neither property was different.
					// This indicates the iterators were just out of step from a previous addition or removal.
					else
					{
						++IterOld;
						++IterNew;
					}
				}
			}
		}

		// Readd to OutDifferences
		for (const FSingleObjectDiffEntry* Difference : OrderedProperties)
		{
			OutDifferences.Add(*Difference);
		}
	}
}
