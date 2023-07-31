// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectKey.h"

struct FAssetData;
struct ITakeRecorderSourceTreeItem;

class SScrollBox;
class ULevelSequence;
class IDetailsView;
class UTakeRecorderSource;
class STakeRecorderSources;

template<class> class TSubclassOf;

/**
 * Widget used by both the take preset asset editor, and take recorder panel that allows editing the take information for an externally provided level sequence
 */
class SLevelSequenceTakeEditor : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SLevelSequenceTakeEditor)
		: _LevelSequence(nullptr)
		{}

		SLATE_ATTRIBUTE(ULevelSequence*, LevelSequence)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/**
	 * Construct a button that can add sources to this widget's preset
	 */
	TSharedRef<SWidget> MakeAddSourceButton();

	/**
	 * Add a new externally controlled settings object to the details UI on this widget
	 */
	void AddExternalSettingsObject(UObject* InObject);

	/**
	 * Removes an externally controlled settings object from the details UI on this widget
	 * @return True if it was removed, false otherwise
	 */
	bool RemoveExternalSettingsObject(UObject* InObject);

private:

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	/**
	 * Check to see whether the level sequence ptr has changed, and propagate that change if necessary
	 */
	void CheckForNewLevelSequence();

	void AddDetails(const TPair<const UClass*, TArray<UObject*> >& Pair, TArray<FObjectKey>& PreviousClasses);

	/**
	 * Update the details panel for the current selection
	 */
	void UpdateDetails();

private:

	TSharedRef<SWidget> OnGenerateSourcesMenu();

	void AddSourceFromClass(TSubclassOf<UTakeRecorderSource> SourceClass);
	bool CanAddSourceFromClass(TSubclassOf<UTakeRecorderSource> SourceClass);
	
	void OnSourcesSelectionChanged(TSharedPtr<ITakeRecorderSourceTreeItem>, ESelectInfo::Type);

private:

	bool bRequestDetailsRefresh;
	TAttribute<ULevelSequence*> LevelSequenceAttribute;
	TWeakObjectPtr<ULevelSequence> CachedLevelSequence;

	TSharedPtr<STakeRecorderSources> SourcesWidget;
	TSharedPtr<SScrollBox> DetailsBox;
	TMap<FObjectKey, TSharedPtr<IDetailsView>> ClassToDetailsView;

	TArray<TWeakObjectPtr<>> ExternalSettingsObjects;
};