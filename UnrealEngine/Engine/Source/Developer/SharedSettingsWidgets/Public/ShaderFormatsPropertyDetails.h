// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DetailLayoutBuilder.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class ITargetPlatform;

enum class ECheckBoxState : uint8;

/**
 * Helper which implements details panel customizations for a device profiles parent property
 */
class SHAREDSETTINGSWIDGETS_API FShaderFormatsPropertyDetails
: public TSharedFromThis<FShaderFormatsPropertyDetails>
{
public:
	typedef FText(Deprecated_GetFriendlyNameFromRHINameFnc)(const FString&);

	typedef FText (GetFriendlyNameFromRHINameFnc)(FName);
	typedef bool (FilterShaderPlatformFnc)(FName);
	
	/**
	 * Constructor for the parent property details view
	 *
	 * @param InDetailsBuilder - Where we are adding our property view to
	 * @param InProperty - The category name to override
	 * @param InTitle - Title for display
	 */
	FShaderFormatsPropertyDetails(IDetailLayoutBuilder* InDetailBuilder, const TCHAR* InProperty = TEXT("TargetedRHIs"), const TCHAR* InTitle = TEXT("Targeted RHIs"));

	/** Simple delegate for updating shader version warning */
	void SetOnUpdateShaderWarning(const FSimpleDelegate& Delegate);
	
	/** Create the UI to select which windows shader formats we are targeting */
	void CreateTargetShaderFormatsPropertyView(ITargetPlatform* TargetPlatform, GetFriendlyNameFromRHINameFnc* FriendlyNameFnc, FilterShaderPlatformFnc* FilterShaderPlatformFunc = nullptr, ECategoryPriority::Type InPriority = ECategoryPriority::Default);

	UE_DEPRECATED(5.1, "CreateTargetShaderFormatsPropertyView now gets RHI names via FName instead of FString. Please change your callback function as this is a breaking change.")
	void CreateTargetShaderFormatsPropertyView(ITargetPlatform* TargetPlatform, Deprecated_GetFriendlyNameFromRHINameFnc* FriendlyNameFnc)
	{
	}
	
	/**
	 * @param InRHIName - The input RHI to check
	 * @returns Whether this RHI is currently enabled
	 */
	ECheckBoxState IsTargetedRHIChecked(FName InRHIName) const;
	
private:
	
	// Supported/Targeted RHI check boxes
	void OnTargetedRHIChanged(ECheckBoxState InNewValue, FName InRHIName);
	
private:
	
	/** A handle to the detail view builder */
	IDetailLayoutBuilder* DetailBuilder;
	
	/** Access to the Parent Property */
	TSharedPtr<IPropertyHandle> ShaderFormatsPropertyHandle;

	/** The category name to override */
	FString Property;


	/** Title for display */
	FName Title;

	/** Preserve shader format order when writing to property */
	TMap<FName, int> ShaderFormatOrder;
};