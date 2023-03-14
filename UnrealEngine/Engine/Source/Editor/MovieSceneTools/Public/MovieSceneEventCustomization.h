// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Types/SlateEnums.h"

struct FAssetData;
struct FEdGraphSchemaAction;
struct FMovieSceneEventEndpointParameters;

class UK2Node;
class UBlueprint;
class UEdGraphPin;
class UEdGraphNode;
class FMenuBuilder;
class FStructOnScope;
class IPropertyHandle;
class FDetailWidgetRow;
class UMovieSceneSection;
class UMovieSceneSequence;
class UMovieSceneEventTrack;
class UK2Node_FunctionEntry;
class IDetailChildrenBuilder;

enum class ECheckBoxState : uint8;

template <typename KeyType, typename ValueType, typename ArrayAllocator, typename SortPredicate> class TSortedMap;

enum class EAutoCreatePayload : uint8
{
	None      = 0,
	Pins      = 1 << 0,
	Variables = 1 << 1,
};
ENUM_CLASS_FLAGS(EAutoCreatePayload)

/**
 * Customization for FMovieSceneEvent structs.
 * Will deduce the event's section either from the outer objects on the details customization, or use the one provided on construction (for instanced property type customizations)
 */
class FMovieSceneEventCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	static TSharedRef<IPropertyTypeCustomization> MakeInstance(UMovieSceneSection* InSection);

	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils ) override;

private:

	/**
	 * Clear the endpoint for the event(s) represented by this property handle.
	 * @note: Does not delete the endpoint in the blueprint itself.
	 */
	void ClearEventEndpoint();


	/**
	 * Creates a single new endpoint for the event(s) represented by this property handle.
	 */
	void CreateEventEndpoint();


	/**
	 * Assigns the specified function entry to the event(s) represented by this property handle.
	 */
	void SetEventEndpoint(UK2Node* NewEndpoint, UEdGraphPin* BoundObjectPin, UK2Node* PayloadTemplate, EAutoCreatePayload AutoCreatePayload);


	/**
	 * Navigate to the definition of the endpoint specified by the event(s) represented by this property handle.
	 */
	void NavigateToDefinition();


	/**
	 * Generate the content of the main combo button menu dropdown
	 */
	TSharedRef<SWidget> GetMenuContent();


	/**
	 * Generate the content of the quick bind sub-menu dropdown (shown if the event is not already bound)
	 */
	void PopulateQuickBindSubMenu(FMenuBuilder& MenuBuilder, UMovieSceneSequence* Sequence);

	/**
	 * Generate the content of the rebind sub-menu dropdown (shown if the event is already bound)
	 */
	void PopulateRebindSubMenu(FMenuBuilder& MenuBuilder, UMovieSceneSequence* Sequence);

	/**
	 * Called when a payload variable property value is changed on the details panel
	 */
	void OnPayloadVariableChanged(TSharedRef<FStructOnScope> InStructData, TSharedPtr<IPropertyHandle> LocalVariableProperty);

private:

	/**
	 * Get the sequence that is common to all the events represented by this property handle, or nullptr if they are not all the same.
	 */
	UMovieSceneSequence* GetCommonSequence() const;


	/**
	 * Get the track that is common to all the events represented by this property handle, or nullptr if they are not all the same.
	 */
	UMovieSceneEventTrack* GetCommonTrack() const;


	/**
	 * Get the endpoint that is common to all the events represented by this property handle, or nullptr if they are not all the same.
	 */
	UK2Node* GetCommonEndpoint() const;

	TArray<UK2Node*> GetAllValidEndpoints() const;

	void IterateEndpoints(TFunctionRef<bool(UK2Node*)> Callback) const;

	/**
	 * Get all the objects that the events reside within.
	 */
	void GetEditObjects(TArray<UObject*>& OutObjects) const;

private:

	/** Get the name of the event to display on the main combo button */
	FText GetEventName() const;
	/** Get the icon of the event to display on the main combo button */
	const FSlateBrush* GetEventIcon() const;

	void HandleRebindActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedAction, ESelectInfo::Type InSelectionType, UBlueprint* Blueprint, UClass* BoundObjectPinClass);
	void HandleQuickBindActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedAction, ESelectInfo::Type InSelectionType, UBlueprint* Blueprint, FMovieSceneEventEndpointParameters Params);

	void OnBlueprintCompiled(UBlueprint*);

	ECheckBoxState GetCallInEditorCheckState() const;
	void OnSetCallInEditorCheckState(ECheckBoxState NewCheckBoxState);

	TSharedRef<SWidget> GetBoundObjectPinMenuContent();
	bool CompareBoundObjectPinName(FName InPinName) const;
	const FSlateBrush* GetBoundObjectPinIcon() const;
	FText GetBoundObjectPinText() const;
	void SetBoundObjectPinName(FName InNewBoundObjectPinName);

private:

	/** Externally supplied section that the event(s) we're reflecting reside within */
	TWeakObjectPtr<UMovieSceneSection> WeakExternalSection;

	/** A cache of the common endpoint that is only used when the menu is open to avoid re-computing it every frame. */
	TWeakObjectPtr<UK2Node_FunctionEntry> CachedCommonEndpoint;

	/** The property handle we're reflecting */
	TSharedPtr<IPropertyHandle> PropertyHandle;

	TSharedPtr<IPropertyUtilities> PropertyUtilities;

	template<typename KeyType, typename ValueType>
	using TInlineMap = TSortedMap<KeyType, ValueType, TInlineAllocator<1>>;
};
