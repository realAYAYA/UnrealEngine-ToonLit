// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "EditorFilterLibrary.generated.h"

UENUM()
enum class EEditorScriptingFilterType : uint8
{
	Include,
	Exclude
};

UENUM(BlueprintType)
enum class EEditorScriptingStringMatchType : uint8
{
	Contains,
	MatchesWildcard,
	ExactMatch
};

/**
 * Utility class to filter a list of objects. Object should be in the World Editor.
 */
UCLASS()
class EDITORSCRIPTINGUTILITIES_API UEditorFilterLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Filter the array based on the Object's class.
	 * @param	TargetArray		Array of Object to filter. The array will not change.
	 * @param	ObjectClass		The Class of the object.
	 * @param	FilterType		Should include or not the array's item if it respects the condition.
	 * @return	The filtered list.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Utilities | Filter", meta = (DisplayName="Filter by Class", DeterminesOutputType = "ObjectClass"))
	static TArray<class UObject*> ByClass(const TArray<class UObject*>& TargetArray
		, TSubclassOf<class UObject> ObjectClass
		, EEditorScriptingFilterType FilterType = EEditorScriptingFilterType::Include);

	/**
	 * Filter the array based on the Object's ID name.
	 * @param	TargetArray		Array of Object to filter. The array will not change.
	 * @param	NameSubString	The text the Object's ID name.
	 * @param	FilterType		Should include or not the array's item if it respects the condition.
	 * @param	StringMatch		Contains the NameSubString OR matches with the wildcard *? OR exactly the same value.
	 * @return	The filtered list.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Utilities | Filter", meta = (DisplayName = "Filter by ID Name", DeterminesOutputType = "TargetArray"))
	static TArray<class UObject*> ByIDName(const TArray<class UObject*>& TargetArray
		, const FString& NameSubString
		, EEditorScriptingStringMatchType StringMatch = EEditorScriptingStringMatchType::Contains
		, EEditorScriptingFilterType FilterType = EEditorScriptingFilterType::Include);

	/**
	 * Filter the array based on the Actor's label (what we see in the editor)
	 * @param	TargetArray		Array of Actor to filter. The array will not change.
	 * @param	NameSubString	The text the Actor's Label.
	 * @param	FilterType		Should include or not the array's item if it respects the condition.
	 * @param	StringMatch		Contains the NameSubString OR matches with the wildcard *? OR exactly the same value.
	 * @param	bIgnoreCase		Determines case sensitivity options for string comparisons.
	 * @return	The filtered list.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Utilities | Filter", meta = (DisplayName = "Filter by Actor Label", DeterminesOutputType = "TargetArray"))
	static TArray<class AActor*> ByActorLabel(const TArray<class AActor*>& TargetArray
		, const FString& NameSubString
		, EEditorScriptingStringMatchType StringMatch = EEditorScriptingStringMatchType::Contains
		, EEditorScriptingFilterType FilterType = EEditorScriptingFilterType::Include
		, bool bIgnoreCase = true);

	/**
	 * Filter the array by Tag the Actor contains
	 * @param	TargetArray		Array of Actor to filter. The array will not change.
	 * @param	Tag				The exact name of the Tag the actor contains.
	 * @param	FilterType		Should include or not the array's item if it respects the condition.
	 * @return	The filtered list.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Utilities | Filter", meta = (DisplayName = "Filter by Actor Tag", DeterminesOutputType = "TargetArray"))
	static TArray<class AActor*> ByActorTag(const TArray<class AActor*>& TargetArray, FName Tag, EEditorScriptingFilterType FilterType = EEditorScriptingFilterType::Include);

	/**
	 * Filter the array by Layer the Actor belongs to.
	 * @param	TargetArray		Array of Actor to filter. The array will not change.
	 * @param	LayerName		The exact name of the Layer the actor belongs to.
	 * @param	FilterType		Should include or not the array's item if it respects the condition.
	 * @return	The filtered list.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Utilities | Filter", meta = (DisplayName = "Filter by Layer", DeterminesOutputType = "TargetArray"))
	static TArray<class AActor*> ByLayer(const TArray<class AActor*>& TargetArray, FName LayerName, EEditorScriptingFilterType FilterType = EEditorScriptingFilterType::Include);

	/**
	 * Filter the array by Level the Actor belongs to.
	 * @param	TargetArray		Array of Actor to filter. The array will not change.
	 * @param	LevelName		The name of the Level the actor belongs to (same name as in the ContentBrowser).
	 * @param	FilterType		Should include or not the array's item if it respects the condition.
	 * @return	The filtered list.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Utilities | Filter", meta = (DisplayName = "Filter by Level Name", DeterminesOutputType = "TargetArray"))
	static TArray<class AActor*> ByLevelName(const TArray<class AActor*>& TargetArray, FName LevelName, EEditorScriptingFilterType FilterType = EEditorScriptingFilterType::Include);

	/**
	 * Filter the array based on Object's selection.
	 * @param	TargetArray		Array of Object to filter. The array will not change.
	 * @param	FilterType		Should include or not the array's item if it respects the condition.
	 * @return	The filtered list.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Utilities | Filter", meta = (DisplayName = "Filter by Selection", DeterminesOutputType = "TargetArray"))
	static TArray<class AActor*> BySelection(const TArray<class AActor*>& TargetArray, EEditorScriptingFilterType FilterType = EEditorScriptingFilterType::Include);
};

