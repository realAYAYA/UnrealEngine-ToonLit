// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "TargetPlatformAudioCustomization.h"

class SErrorHint;
class STextComboBox;

class FHoloLensTargetSettingsCustomization : public IDetailCustomization
{
public:

	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface
private:

	void AddWidgetForCapability(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> CapabilityList, const FString& CapabilityName, const FText& CapabilityCaption, const FText& CapabilityTooltip, bool bForAdvanced);
	void AddWidgetForPlatformVersion(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle, TSharedPtr<STextComboBox>* OutVersionSelector = nullptr, bool bForAdvanced = false, const FString& SpecificVersionOverrideString = FString());
	void AddWidgetForTargetDeviceFamily(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle);
	static FString GetNameForSigningCertificate(const FString &CertificatePath);
	void InitSupportedPlatformVersions();
	void InitTargetDeviceFamilyOptions();
	ECheckBoxState IsCapabilityChecked(TSharedRef<IPropertyHandle> CapabilityList, const FString CapabilityName) const;
	void OnCapabilityStateChanged(ECheckBoxState CheckState, TSharedRef<IPropertyHandle> CapabilityList, const FString CapabilityName);
	void OnCertificatePicked(const FString& PickedPath);
	void OnSelectedItemChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> HandlePtr);
	void OnAutoDetectWin10SDKChanged(ECheckBoxState NewState, TSharedRef<IPropertyHandle> Win10SDKVersionPropertyHandle);
	ECheckBoxState IsAutoDetectWin10SDKChecked() const;
	EVisibility GetManualWin10SDKWidgetVisibility() const;

	FString GetSigningCertificateSubjectName() const;
	FReply GenerateSigningCertificate();
	FString GetPublisherIdentityName() const;
	void LoadAndValidateSigningCertificate();

	TArray<TSharedPtr<FString>> PlatformVersionOptions;
	TArray<TSharedPtr<FString>> TargetDeviceFamilyOptions;

	FString SigningCertificateSubjectName;
	TSharedPtr<SErrorHint> SigningCertificateError;
	TSharedPtr<STextComboBox> WindowsSDKSelector;

	FAudioPluginWidgetManager AudioPluginManager;
};