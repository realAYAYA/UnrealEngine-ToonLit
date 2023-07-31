// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class FDMXPixelMappingToolkit;
class IPropertyHandle;
class IDetailCategoryBuilder;
class UDMXPixelMappingRendererComponent;
struct EVisibility;
enum class EDMXPixelMappingRendererType : uint8;

class FDMXPixelMappingDetailCustomization_Renderer
	: public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
	{
		return MakeShared<FDMXPixelMappingDetailCustomization_Renderer>(InToolkitWeakPtr);
	}

	/** Constructor */
	FDMXPixelMappingDetailCustomization_Renderer(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
		: ToolkitWeakPtr(InToolkitWeakPtr)
	{}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	/** Called before the renderer type property changed */
	void OnRendererTypePropertyPreChange();

	/** Called when the renderer type property changed */
	void OnRendererTypePropertyChanged();

	/** Called when the input texture property changed */
	void OnInputTexturePropertyChanged();

	bool IsSelectedRendererType(EDMXPixelMappingRendererType PropertyRendererType) const;

	bool IsNotSelectedRendererType(EDMXPixelMappingRendererType PropertyRendererType) const;

	EVisibility GetInputTextureWarning() const;

	FText GetInputTextureWarningText() const;

	void AddInputTextureWarning(IDetailCategoryBuilder& InCategory);

	/** Add warning for material with domain != MD_UI */
	void AddMaterialWarning(IDetailCategoryBuilder& InCategory);

	/** Visibility for non ui material warning */
	EVisibility GetMaterialWarningVisibility() const;

	/** Resets the size of the renderer component depending on current renderer type and the resource object */
	void ResetSize();

	/** Previously selected renderer type */
	EDMXPixelMappingRendererType PreviousRendererType;
	
	TSharedPtr<IPropertyHandle> RendererTypePropertyHandle;
	TSharedPtr<IPropertyHandle> InputTexturePropertyHandle;
	TSharedPtr<IPropertyHandle> InputMaterialPropertyHandle;
	TSharedPtr<IPropertyHandle> InputWidgetPropertyHandle;

	TWeakObjectPtr<UDMXPixelMappingRendererComponent> RendererComponent;

	/** Weak reference to the DMX editor */
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;
};
