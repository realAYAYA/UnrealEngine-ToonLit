// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXPixelMappingRuntimeCommon.h"

#include "UObject/Object.h"
#include "DMXPixelMapping.generated.h"

class UDMXEntityFixturePatch;
class UDMXPixelMappingBaseComponent;

class SWidget;
class UTexture;

#if WITH_EDITOR
DECLARE_DELEGATE_OneParam(FOnEditorRebuildChildrenComponentsDelegate, UDMXPixelMappingBaseComponent*)
#endif // WITH_EDITOR


/**
 * Public container of Pixel Mapping object and it using for asset
 */
UCLASS(BlueprintType, Blueprintable)
class DMXPIXELMAPPINGRUNTIME_API UDMXPixelMapping
	: public UObject
{
	GENERATED_BODY()
public:
	//~ Begin UObject implementation
	virtual void PostLoad() override;
	//~ End UObject implementation

	/** Get root component of the component tree */
	UFUNCTION(BlueprintPure, Category = "DMX|PixelMapping")
	UDMXPixelMappingRootComponent* GetRootComponent() const { return RootComponent; }

	/** Checks if nested objects are created, otherwise create them. */
	void CreateOrLoadObjects();

	/** Recurcevly preload all components in tree. */
	void PreloadWithChildren();

	/** Destroys invalid components. Useful when fixture type or fixture patch changed */
	void DestroyInvalidComponents();

	/** Returns the first component that corresponds to the patch or null if none present */
	UDMXPixelMappingBaseComponent* FindComponent(UDMXEntityFixturePatch* FixturePatch) const;

	/** Find the component by name. */
	UDMXPixelMappingBaseComponent* FindComponent(const FName& Name) const;

#if WITH_EDITOR
	/** Find the component by widget. */
	UDMXPixelMappingOutputComponent* FindComponent(TSharedPtr<SWidget> InWidget) const;
#endif // WITH_EDITOR

	/**
	 * Looking for the first component of class by given name
	 * @param InName        The name to search
	 * @return				An instance of the templated Component
	 */
	template <typename TComponentClass>
	TComponentClass* FindComponentOfClass(const FName& InName) const
	{
		TComponentClass* FoundComponent = nullptr;

		ForEachComponent([&](UDMXPixelMappingBaseComponent* InComponent) {
			if (TComponentClass* CastComponent = Cast<TComponentClass>(InComponent))
			{
				if (CastComponent->GetFName() == InName)
				{
					FoundComponent = CastComponent;
					return;
				}
			}
		});

		return FoundComponent;
	}

	/**
	 * Get all component by given class
	 * @param OutComponents        Found components
	 */
	template <typename TComponentClass>
	void GetAllComponentsOfClass(TArray<TComponentClass*>& OutComponents) const
	{
		ForEachComponent([&OutComponents](UDMXPixelMappingBaseComponent* InComponent) {
			if (TComponentClass* CastComponent = Cast<TComponentClass>(InComponent))
			{
				OutComponents.Add(CastComponent);
			}
		});
	}

	/**
	 * Iterates through components by class with given Predicate callback
	 */
	template <typename TComponentClass>
	void ForEachComponentOfClass(TComponentPredicateType<TComponentClass> Predicate) const
	{
		ForEachComponent([&Predicate](UDMXPixelMappingBaseComponent* InComponent) {
			if (TComponentClass* CastComponent = Cast<TComponentClass>(InComponent))
			{
				Predicate(CastComponent);
			}
		});
	}

	/**
	 * Get array of FName pointers by given class
	 * @param OutComponents        Found components
	 */
	template <typename TComponentClass>
	void GetAllComponentsNamesOfClass(TArray<TSharedPtr<FName>>& InComponentNames) const
	{
		ForEachComponent([&InComponentNames](UDMXPixelMappingBaseComponent* InComponent) {
			if (TComponentClass* CastComponent = Cast<TComponentClass>(InComponent))
			{
				InComponentNames.Add(MakeShared<FName>(CastComponent->GetFName()));
			}
		});
	}

	/**
	 * Recursively Iterates through all compnents
	 * @param Predicate        Callback function
	 */
	void ForEachComponent(TComponentPredicate Predicate) const;

	/** 
	 * Removes the Component from the hierarchy and all sub Components.
	 * @param InComponent        Component for remove
	 */
	void RemoveComponent(UDMXPixelMappingBaseComponent* InComponent);

public:
	/** Holds the reference to root component */
	UPROPERTY()
	TObjectPtr<UDMXPixelMappingRootComponent> RootComponent;


#if WITH_EDITORONLY_DATA
	/** Holds the Thumbnail for asset */
	UPROPERTY()
	TObjectPtr<UTexture> ThumbnailImage;

	/** DEPRECATED 4.27. Use FDMXPixelMappingMatrixComponent::GetOnMatrixChanged() instead */
	FOnEditorRebuildChildrenComponentsDelegate OnEditorRebuildChildrenComponentsDelegate_DEPRECATED;
#endif
};
