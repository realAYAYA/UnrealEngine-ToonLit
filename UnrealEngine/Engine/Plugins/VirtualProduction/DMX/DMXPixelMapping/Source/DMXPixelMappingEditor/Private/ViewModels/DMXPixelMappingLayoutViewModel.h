// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "TimerManager.h"

#include "DMXPixelMappingLayoutViewModel.generated.h"

struct FDMXPixelMappingLayoutToken;
class FDMXPixelMappingToolkit;
class UDMXPixelMappingFixtureGroupComponent;
class UDMXPixelMappingFixtureGroupItemComponent;
class UDMXPixelMappingLayoutScript;
class UDMXPixelMappingMatrixComponent;
class UDMXPixelMappingOutputComponent;
class UDMXPixelMappingRendererComponent;
class UDMXPixelMappingScreenComponent;


enum class EDMXPixelMappingLayoutViewModelMode
{
	LayoutRendererComponentChildren,
	LayoutFixtureGroupComponentChildren,
	LayoutMatrixComponentChildren,
	LayoutNone
};


/** Model for the Layout View */
UCLASS(Transient)
class UDMXPixelMappingLayoutViewModel
	: public UObject
	, public FEditorUndoClient
{
	GENERATED_BODY()

protected:
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	/** Destructor */
	UDMXPixelMappingLayoutViewModel();

	/** Destructor */
	virtual ~UDMXPixelMappingLayoutViewModel();

	/** Sets the toolkit from which the model sources */
	void SetToolkit(const TSharedRef<FDMXPixelMappingToolkit>& InToolkit);

	/** Returns the mode in which this model currently can work in */
	EDMXPixelMappingLayoutViewModelMode GetMode() const;

	/** Returns the Layout Scripts of relevant components. Looks up each relevant component and returns its script. */
	TArray<UObject*> GetLayoutScriptsObjectsSlow() const;

	/** Applies the layouts scripts currently in use on the next tick */
	void RequestApplyLayoutScripts();

	/** Delegate executed when the Model changed */
	FSimpleMulticastDelegate OnModelChanged;

	// Property name getters
	FORCEINLINE static FName GetLayoutScriptClassPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXPixelMappingLayoutViewModel, LayoutScriptClass); }

protected:
	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

private:
	/** Applies the layouts scripts currently in use */
	void ApplyLayoutScripts();

	/** Lays out children of the Renderer Component */
	void LayoutRendererComponentChildren();

	/** Lays out children of the Fixture Group Components */
	void LayoutFixtureGroupComponentChildren();

	/** Lays out children of the Matrixx Components */
	void LayoutMatrixComponentChildren();

	/** Applies the layout of the specified tokens */
	void ApplyLayoutTokens(const TArray<FDMXPixelMappingLayoutToken>& LayoutTokens) const;

	/** Called when the Layout Script Class of this Model changed */
	void OnLayoutScriptClassChanged();

	/** Called when selected Components changed */
	void OnSelectedComponentsChanged();

	/** Instantiates layout scripts for relevant components */
	void InstantiateLayoutScripts();

	/** Initializes memeber of the Layout Script in specified Component */
	void InitializeLayoutScript(UDMXPixelMappingOutputComponent* OutputComponent, UDMXPixelMappingLayoutScript* LayoutScript);

	/** Refreshes components from what's currently selected in the Pixel Mapping Toolkit */
	void RefreshComponents();

	/** Refreshes the Layout Script Class */
	void RefreshLayoutScriptClass();

	/** The toolkit from which the Model currently sources */
	TWeakPtr<FDMXPixelMappingToolkit> WeakToolkit;

	/** The Renderer Component currently in use */
	UPROPERTY()
	TWeakObjectPtr<UDMXPixelMappingRendererComponent> RendererComponent;

	/** The Screen Components currently in use */
	UPROPERTY()
	TArray<TWeakObjectPtr<UDMXPixelMappingScreenComponent>> ScreenComponents;

	/** The Fixture Group Components currently in use */
	UPROPERTY()
	TArray<TWeakObjectPtr<UDMXPixelMappingFixtureGroupComponent>> FixtureGroupComponents;

	/** The Matrix Components currently in use */
	UPROPERTY()
	TArray<TWeakObjectPtr<UDMXPixelMappingMatrixComponent>> MatrixComponents;

	/** The Layout Script class currently in use */
	UPROPERTY(EditAnywhere, Category = "Layout Script", Meta = (ShowDisplayNames))
	TSoftClassPtr<UDMXPixelMappingLayoutScript> LayoutScriptClass;

	/** Timer handle to apply the layout script, set when ApplyLayoutScript is requested. */
	FTimerHandle ApplyLayoutScriptTimerHandle;

	/** The Layout Script class currently before it was changed */
	TSoftClassPtr<UDMXPixelMappingLayoutScript> PreEditChangeLayoutScriptClass;
};
