// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PropertyViewBase.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

#include "SinglePropertyView.generated.h"

class ISinglePropertyView;
class UObject;
struct FFrame;
struct FPropertyChangedEvent;

/**
 * The single property view allows you to display the value of an object's property.
 */
UCLASS()
class SCRIPTABLEEDITORWIDGETS_API USinglePropertyView : public UPropertyViewBase
{
	GENERATED_BODY()

private:
	/** The name of the property to display. */
	UPROPERTY(EditAnywhere, Category = "View")
	FName PropertyName;

	/** Override for the property name that will be displayed instead of the property name. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "View")
	FText NameOverride;

public:
	UFUNCTION(BlueprintCallable, Category = "View")
	FName GetPropertyName() const;

	UFUNCTION(BlueprintCallable, Category = "View")
	void SetPropertyName(FName NewPropertyName);

	UFUNCTION(BlueprintCallable, Category = "View")
	FText GetNameOverride() const;

	UFUNCTION(BlueprintCallable, Category = "View")
	void SetNameOverride(FText NewPropertyName);

private:
	void InternalSinglePropertyChanged();

	//~ UPropertyViewBase interface
private:
	virtual void BuildContentWidget() override;
	virtual void OnObjectChanged() override;
	//~ End of UPropertyViewBase interface

	//~ UWidget interface
public:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End of UWidget interface

	// UObject interface
public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

private:
	TSharedPtr<ISinglePropertyView> SinglePropertyViewWidget;
};
