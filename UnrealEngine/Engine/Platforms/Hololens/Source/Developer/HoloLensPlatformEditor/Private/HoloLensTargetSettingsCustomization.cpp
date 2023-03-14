// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensTargetSettingsCustomization.h"
#include "HoloLensTargetTypes.h"
#include "SExternalImageReference.h"
#include "IExternalImagePickerModule.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "EditorDirectories.h"
#include "Widgets/Input/SFilePathPicker.h"
#include "Widgets/Input/STextComboBox.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/FileHelper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Wincrypt.h"
#include "HoloLensTargetSettings.h"
#include "GeneralProjectSettings.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SErrorHint.h"
#include "HAL/PlatformFileManager.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"
#include "HoloLensLocalizedResourcesCustomization.h"
#include "Styling/AppStyle.h"

#include <winreg.h>

#pragma comment( lib, "Advapi32.lib" )

#pragma warning( disable : 6387 )

#define LOCTEXT_NAMESPACE "HoloLensTargetSettingsCustomization"

TSharedRef<IDetailCustomization> FHoloLensTargetSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FHoloLensTargetSettingsCustomization);
}

void FHoloLensTargetSettingsCustomization::InitSupportedPlatformVersions()
{
	PlatformVersionOptions.Empty();

	const TArray<FHoloLensSDKVersion>& SDKVersions = FHoloLensSDKVersion::GetSDKVersions();
	for (const FHoloLensSDKVersion& SDKVer : SDKVersions)
	{
		PlatformVersionOptions.Add(MakeShareable(new FString(SDKVer.VersionString)));
	}
}

void FHoloLensTargetSettingsCustomization::InitTargetDeviceFamilyOptions()
{
	TargetDeviceFamilyOptions.Empty();

	TargetDeviceFamilyOptions.Add(MakeShareable(new FString("Windows.Holographic")));
}

void FHoloLensTargetSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	InitTargetDeviceFamilyOptions();
	InitSupportedPlatformVersions();

	// Add UI for selecting the TargetDeviceFamily.
	TSharedRef<IPropertyHandle> TargetDeviceFamilyProperty = DetailBuilder.GetProperty("TargetDeviceFamily");
	AddWidgetForTargetDeviceFamily(DetailBuilder, TargetDeviceFamilyProperty);

	// Add UI for select min/max platform version.
	TSharedRef<IPropertyHandle> MinVersionProperty = DetailBuilder.GetProperty("MinimumPlatformVersion");
	// 10.0.18362.0 is the minimum 'officially' released SDK from MS that supports all the HoloLens 2 features that are needed
	AddWidgetForPlatformVersion(DetailBuilder, MinVersionProperty, nullptr, true, FString(L"10.0.18362.0"));
	TSharedRef<IPropertyHandle> MaxVersionProperty = DetailBuilder.GetProperty("MaximumPlatformVersionTested");
	AddWidgetForPlatformVersion(DetailBuilder, MaxVersionProperty, nullptr, true);

	// Add UI to select signing certificate
	IDetailCategoryBuilder& PackagingCategoryBuilder = DetailBuilder.EditCategory(FName("Packaging"));
	TSharedRef<IPropertyHandle> LocalizedResourcesProperty = DetailBuilder.GetProperty("PerCultureResources");
	DetailBuilder.HideProperty(LocalizedResourcesProperty);
	PackagingCategoryBuilder.AddCustomBuilder(MakeShared<FHoloLensLocalizedResourcesNodeBuilder>(LocalizedResourcesProperty, TEXT("")));

	FString ProjectPath = FPaths::ProjectDir() / TEXT("Build") / TEXT("HoloLens") / TEXT("SigningCertificate.pfx");

	// Load the existing signing certificate (if any)
	FText SigningCertificateCaption = LOCTEXT("SigningCertificate", "Signing Certificate");
	FText SigningCertificateTooltip = LOCTEXT("SigningCertificateTooltip", "Pfx file containing a private key used to sign the AppX file created during packaging.  The certificate subject name must exactly match the value of Package/Identity/Publisher.  Signing is required when sideloading packaged builds");

	PackagingCategoryBuilder.AddCustomRow(SigningCertificateCaption)
	.NameContent()
	[
		SNew(STextBlock)
		.Text(SigningCertificateCaption)
		.ToolTipText(SigningCertificateTooltip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SFilePathPicker)
			.BrowseButtonImage(FAppStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
			.BrowseButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.BrowseDirectory(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN))
			.FileTypeFilter(TEXT("pfx files (*.pfx)|*.pfx"))
			.IsReadOnly(true)
			.OnPathPicked(this, &FHoloLensTargetSettingsCustomization::OnCertificatePicked)
			.FilePath(this, &FHoloLensTargetSettingsCustomization::GetSigningCertificateSubjectName)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(SigningCertificateError, SErrorHint)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(LOCTEXT("NewSigningCertificate", "Generate new"))
			.OnClicked(this, &FHoloLensTargetSettingsCustomization::GenerateSigningCertificate)
		]
	];

	LoadAndValidateSigningCertificate();

	IDetailCategoryBuilder& ToolchainCategoryBuilder = DetailBuilder.EditCategory(FName("Toolchain"));

	// Make sure compiler version comes first - it's probably most important
	ToolchainCategoryBuilder.AddProperty(FName("CompilerVersion"));

	TSharedRef<IPropertyHandle> Win10SDKVersionPropertyHandle = DetailBuilder.GetProperty("Windows10SDKVersion");

	FText AutoDetectSDKCaption = LOCTEXT("AutodetectWin10SDKCaption", "Auto-detect Windows 10 SDK");
	FText AutoDetectSDKTooltip = LOCTEXT("AutodetectWin10SDKTooltip", "When enabled the project will build against the most recent version of the Windows 10 SDK supported by your compiler. This is typically the recommended behavior.  Uncheck in order to manually select a specific SDK version");
	TSharedPtr<SCheckBox> AutoDetectSDKCheckbox;
	ToolchainCategoryBuilder.AddCustomRow(AutoDetectSDKCaption)
	.NameContent()
	[
		SNew(STextBlock)
		.Text(AutoDetectSDKCaption)
		.ToolTipText(AutoDetectSDKTooltip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SAssignNew(AutoDetectSDKCheckbox, SCheckBox)
		.IsChecked(this, &FHoloLensTargetSettingsCustomization::IsAutoDetectWin10SDKChecked)
		.OnCheckStateChanged(this, &FHoloLensTargetSettingsCustomization::OnAutoDetectWin10SDKChanged, Win10SDKVersionPropertyHandle)
	];

	AddWidgetForPlatformVersion(DetailBuilder, Win10SDKVersionPropertyHandle, &WindowsSDKSelector);

	// Add capability support.
	TSharedRef<IPropertyHandle> CapabilityList = DetailBuilder.GetProperty("CapabilityList");
	DetailBuilder.HideProperty(CapabilityList);
	AddWidgetForCapability(DetailBuilder, CapabilityList, TEXT("internetClient"), LOCTEXT("InternetClientCaption", "Internet Client"), LOCTEXT("InternetClientTooltip", "Provides outbound access to the Internet and networks in public places like airports and cofee shops."), false);
	AddWidgetForCapability(DetailBuilder, CapabilityList, TEXT("internetClientServer"), LOCTEXT("InternetClientServerCaption", "Internet Client Server"), LOCTEXT("InternetClientServerTooltip", "Provides inbound and outbound access to the Internet and networks in public places like airports and cofee shops"), false);
	AddWidgetForCapability(DetailBuilder, CapabilityList, TEXT("privateNetworkClientServer"), LOCTEXT("PrivateNetworkCaption", "Private Network Client Server"), LOCTEXT("PrivateNetworkTooltip", "Provides inbound and outbound access to the Internet and networks that have an authenticated domain controller, or that the user has designated as either home or work networks.Inbound access to critical ports is always blocked."), false);
	AddWidgetForCapability(DetailBuilder, CapabilityList, TEXT("allJoyn"), LOCTEXT("AllJoynCaption", "AllJoyn"), LOCTEXT("AllJoynTooltip", "Allows AllJoyn-enabled apps and devices on a network to discover and interact with each other."), true);
	AddWidgetForCapability(DetailBuilder, CapabilityList, TEXT("codeGeneration"), LOCTEXT("CodeGenCaption", "Code Generation"), LOCTEXT("CodeGenTooltip", "Allows apps to generate code dynamically."), true);

	TSharedRef<IPropertyHandle> DeviceCapabilityList = DetailBuilder.GetProperty("DeviceCapabilityList");
	DetailBuilder.HideProperty(DeviceCapabilityList);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("microphone"), LOCTEXT("MicrophoneCaption", "Microphone"), LOCTEXT("MicrophoneTooltip", "Provides access to the microphone's audio feed, which allows the app to record audio from connected microphones. Required for Windows.Media.SpeechRecognition APIs."), false);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("webcam"), LOCTEXT("WebcamCaption", "Webcam"), LOCTEXT("WebcamTooltip", "Provides access to the webcam's video feed, which allows the app to capture snapshots and movies from connected webcams."), false);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("proximity"), LOCTEXT("ProximityCaption", "Proximity"), LOCTEXT("ProximityTooltip", "Provides capability to connect devices in close proximity to the PC via near field proximity radio or Wi-FI Direct."), true);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("location"), LOCTEXT("LocationCaption", "Location"), LOCTEXT("LocationTooltip", "Provides access to the current location, which is obtained from dedicated hardware like a GPS sensor in the PC or derived from available network information."), true);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("bluetooth"), LOCTEXT("BluetoothCaption", "Bluetooth"), LOCTEXT("BluetoothTooltip", "Allows communication with paired Bluetooth devices over the Generic Attribute (GATT) or Classic Basic Rate (RFCOMM) protocols."), true);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("gazeInput"), LOCTEXT("GazeInputCaption", "Gaze Input"), LOCTEXT("GazeInputTooltip", "Provides access to the input from gaze like eyes tracking. Required for Windows.Perception.People.EyesPose API."), false);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("wiFiControl"), LOCTEXT("WiFiControlCaption", "WiFiControl"), LOCTEXT("WiFiControlTooltip", "Allows apps to scan and connect to Wi-Fi networks."), false);

	TSharedRef<IPropertyHandle> UapCapabilityList = DetailBuilder.GetProperty("UapCapabilityList");
	DetailBuilder.HideProperty(UapCapabilityList);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("musicLibrary"), LOCTEXT("MusicLibCaption", "Music Library"), LOCTEXT("MusicLibTooltip", "Provides capability to add, change, or delete files in the Music Library for the local PC and HomeGroup PCs."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("picturesLibrary"), LOCTEXT("PicturesLibCaption", "Pictures Library"), LOCTEXT("PicturesLibTooltip", "Provides capability to add, change, or delete files in the Pictures Library for the local PC and HomeGroup PCs."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("videosLibrary"), LOCTEXT("VideosLibCaption", "Videos Library"), LOCTEXT("VideosLibTooltip", "Provides capability to add, change, or delete files in the Videos Library for the local PC and HomeGroup PCs."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("blockedChatMessages"), LOCTEXT("BlockedChatCaption", "Blocked Chat Messages"), LOCTEXT("BlockedChatTooltip", "Allows apps to read SMS and MMS messages that have been blocked by the Spam Filter app."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("chat"), LOCTEXT("ChatCaption", "Chat"), LOCTEXT("ChatTooltip", "Allows apps to read and delete Text Messages. It also allows apps to store chat messages in the system data store."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("enterpriseAuthentication"), LOCTEXT("EnterpriseAuthCaption", "Enterprise Authentication"), LOCTEXT("EnterpriseAuthTooltip", "Subject to Store policy. Provides ability to connect to enterprise intranet resources that require domain credentials.This capability is typically not needed for most apps."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("objects3D"), LOCTEXT("Objects3DCaption", "Objects 3D"), LOCTEXT("Objects3DTooltip", "Provides access to the user's 3D Objects, allowing the app to enumerate and access all files in the library without user interaction."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("phoneCall"), LOCTEXT("PhoneCallCaption", "Phone Call"), LOCTEXT("PhoneCallTooltip", "Allows apps to access all phone lines on the device and perform the following functions: place a call, access line-related metadata, access line-related triggers, set and check block list and call origination information."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("removableStorage"), LOCTEXT("RemovableStorageCaption", "Removable Storage"), LOCTEXT("RemovableStorageTooltip", "Provides capability to add, change, or delete files on removable storage devices."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("sharedUserCertificates"), LOCTEXT("ShareCertsCaption", "Shared User Certificates"), LOCTEXT("SharedCertsTooltip", "Subject to Store policy. Provides capability to access software and hardware certificates for validating a user's identity."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("userAccountInformation"), LOCTEXT("UserAccountCaption", "User Account Information"), LOCTEXT("UserAccountTooltip", "Gives apps the ability to access user's name and picture. Required to access Windows.System.UserProfile APIs."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("voipCall"), LOCTEXT("VoipCallCaption", "VOIP Call"), LOCTEXT("VoipCallTooltip", "Allows access to the VOIP calling APIs in Windows.ApplicationModel.Calls."), true);

	TSharedRef<IPropertyHandle> Uap2CapabilityList = DetailBuilder.GetProperty("Uap2CapabilityList");
	DetailBuilder.HideProperty(Uap2CapabilityList);
	AddWidgetForCapability(DetailBuilder, Uap2CapabilityList, TEXT("spatialPerception"), LOCTEXT("SpatialPerceptionCaption", "Spatial Perception"), LOCTEXT("SpatialPerceptionTooltip", "Provides access to environment data, which will be used to generate spatial maps or stages. Required to access Windows.Perception.Spatial APIs."), false);

	// If this is the first time capabilities are being accessed for the project, enable defaults.
	TSharedRef<IPropertyHandle> SetDefaultCapabilitiesProperty = DetailBuilder.GetProperty("bSetDefaultCapabilities");
	DetailBuilder.HideProperty(SetDefaultCapabilitiesProperty);
	bool bSetDefaults;
	SetDefaultCapabilitiesProperty->GetValue(bSetDefaults);
	if (bSetDefaults)
	{
		OnCapabilityStateChanged(ECheckBoxState::Checked, CapabilityList, TEXT("internetClientServer"));
		OnCapabilityStateChanged(ECheckBoxState::Checked, CapabilityList, TEXT("privateNetworkClientServer"));
		// Our AR apps expect to get spatial mesh updates, so make this opt out for users that know what they are doing
		OnCapabilityStateChanged(ECheckBoxState::Checked, Uap2CapabilityList, TEXT("spatialPerception"));

		SetDefaultCapabilitiesProperty->NotifyPreChange();
		SetDefaultCapabilitiesProperty->SetValue(false);
		SetDefaultCapabilitiesProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	}

	AudioPluginManager.BuildAudioCategory(DetailBuilder, TEXT("HoloLens"));
}

void FHoloLensTargetSettingsCustomization::OnCertificatePicked(const FString& PickedPath)
{
	FString CertificatePath = FPaths::ProjectDir() / TEXT("Build") / TEXT("HoloLens") / TEXT("SigningCertificate.pfx");
	FText FailReason;
	if (!SourceControlHelpers::CopyFileUnderSourceControl(CertificatePath, PickedPath, LOCTEXT("CertificateDescription", "Certificate"), FailReason))
	{
		FNotificationInfo Info(FailReason);
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	LoadAndValidateSigningCertificate();
}

FString FHoloLensTargetSettingsCustomization::GetNameForSigningCertificate(const FString &CertificatePath)
{
	// Display the friendly name of the certificate instead of the file name.
	FString CertificateName = LOCTEXT("CertificateNoCertificate", "No valid certificate selected").ToString();
	TArray<uint8> CertBytes;
	if (FFileHelper::LoadFileToArray(CertBytes, *CertificatePath))
	{
		CRYPT_DATA_BLOB CertBlob;
		CertBlob.cbData = CertBytes.Num();
		CertBlob.pbData = CertBytes.GetData();
		HCERTSTORE CertStore = PFXImportCertStore(&CertBlob, nullptr, 0);
		if (CertStore != nullptr)
		{
			DWORD StrType = CERT_X500_NAME_STR;
			const CERT_CONTEXT *CertContext = CertEnumCertificatesInStore(CertStore, nullptr);
			if (CertContext != nullptr)
			{
				DWORD NumCharacters = CertGetNameString(CertContext, CERT_NAME_RDN_TYPE, 0, &StrType, nullptr, 0);
				CertificateName.GetCharArray().SetNumZeroed(NumCharacters);
				CertGetNameString(CertContext, CERT_NAME_RDN_TYPE, 0, &StrType, CertificateName.GetCharArray().GetData(), CertificateName.GetAllocatedSize());
				CertFreeCertificateContext(CertContext);
			}
			CertCloseStore(CertStore, 0);
		}
		else
		{
			DWORD FailureReason = GetLastError();
			switch (FailureReason)
			{
			case ERROR_INVALID_PASSWORD:
				CertificateName = LOCTEXT("CertificatePasswordProtected", "Certificate is password protected").ToString();
				break;

			default:
				break;
			}
		}
	}

	return CertificateName;
}

void FHoloLensTargetSettingsCustomization::AddWidgetForPlatformVersion(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle, TSharedPtr<STextComboBox>* OutVersionSelector, bool bForAdvanced, const FString& SpecificVersionOverrideString)
{
	IDetailCategoryBuilder& VersionCategoryBuilder = DetailBuilder.EditCategory(FName(*PropertyHandle->GetMetaData("Category")));
	DetailBuilder.HideProperty(PropertyHandle);

	FString CurrentSelectedVersion = SpecificVersionOverrideString;
	PropertyHandle->GetValue(CurrentSelectedVersion);

	// Default to latest version when not set explicitly.  With Windows 10 automatic updates
	// it's a reasonably safe choice for users, and much more developer friendly than defaulting
	// to the oldest version.
	int32 CurrentSelectedIndex = PlatformVersionOptions.Num() - 1;
	if (CurrentSelectedVersion.IsEmpty())
	{
		OnSelectedItemChanged(PlatformVersionOptions[CurrentSelectedIndex], ESelectInfo::Direct, PropertyHandle);
	}
	else
	{
		for (int32 i = 0; i < PlatformVersionOptions.Num(); ++i)
		{
			if (*PlatformVersionOptions[i] == CurrentSelectedVersion)
			{
				CurrentSelectedIndex = i;
				break;
			}
		}
	}

	TSharedPtr<STextComboBox> Unused;
	TSharedPtr<STextComboBox>& VersionSelectorRef = OutVersionSelector != nullptr ? *OutVersionSelector : Unused;

	VersionCategoryBuilder.AddCustomRow(PropertyHandle->GetPropertyDisplayName(), bForAdvanced)
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(VersionSelectorRef, STextComboBox)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OptionsSource(&PlatformVersionOptions)
			.InitiallySelectedItem(PlatformVersionOptions[CurrentSelectedIndex])
			.OnSelectionChanged(this, &FHoloLensTargetSettingsCustomization::OnSelectedItemChanged, PropertyHandle)
		]
	];
}

void FHoloLensTargetSettingsCustomization::AddWidgetForTargetDeviceFamily(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle)
{
	IDetailCategoryBuilder& TargetCategoryBuilder = DetailBuilder.EditCategory(FName(*PropertyHandle->GetMetaData("Category")));
	DetailBuilder.HideProperty(PropertyHandle);

	FString CurrentSelectedDeviceFamily;
	PropertyHandle->GetValue(CurrentSelectedDeviceFamily);

	// Default to first option when not set explicitly.
	int32 CurrentSelectedIndex = 0;
	if (CurrentSelectedDeviceFamily.IsEmpty())
	{
		OnSelectedItemChanged(TargetDeviceFamilyOptions[CurrentSelectedIndex], ESelectInfo::Direct, PropertyHandle);
	}
	else
	{
		for (int32 i = 0; i < TargetDeviceFamilyOptions.Num(); i++)
		{
			if (*TargetDeviceFamilyOptions[i] == CurrentSelectedDeviceFamily)
			{
				CurrentSelectedIndex = i;
				break;
			}
		}
	}

	TargetCategoryBuilder.AddCustomRow(PropertyHandle->GetPropertyDisplayName())
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextComboBox)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OptionsSource(&TargetDeviceFamilyOptions)
			.InitiallySelectedItem(TargetDeviceFamilyOptions[CurrentSelectedIndex])
			.OnSelectionChanged(this, &FHoloLensTargetSettingsCustomization::OnSelectedItemChanged, PropertyHandle)
		]
	];
}

void FHoloLensTargetSettingsCustomization::OnSelectedItemChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> Handle)
{
	Handle->SetValue(*NewValue);
}

void FHoloLensTargetSettingsCustomization::AddWidgetForCapability(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> CapabilityList, const FString& CapabilityName, const FText& CapabilityCaption, const FText& CapabilityTooltip, bool bForAdvanced)
{
	IDetailCategoryBuilder& CapabilityBuilder = DetailBuilder.EditCategory(FName("Capabilities"));

	// Initialize checkbox state based on whether or not the capability currently exists in the CapabilityList.
	ECheckBoxState currentState = IsCapabilityChecked(CapabilityList, CapabilityName);
	if (currentState == ECheckBoxState::Checked)
	{
		OnCapabilityStateChanged(currentState, CapabilityList, CapabilityName);
	}

	CapabilityBuilder.AddCustomRow(CapabilityCaption, bForAdvanced)
	.NameContent()
	[
		SNew(STextBlock)
		.Text(CapabilityCaption)
		.ToolTipText(CapabilityTooltip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.VAlign(VAlign_Center)
	[
		SNew(SCheckBox)
		.IsChecked(this, &FHoloLensTargetSettingsCustomization::IsCapabilityChecked, CapabilityList, CapabilityName)
		.OnCheckStateChanged(this, &FHoloLensTargetSettingsCustomization::OnCapabilityStateChanged, CapabilityList, CapabilityName)
	];
}

ECheckBoxState FHoloLensTargetSettingsCustomization::IsCapabilityChecked(TSharedRef<IPropertyHandle> CapabilityList, const FString CapabilityName) const
{
	TArray<void*> RawData;
	CapabilityList->AccessRawData(RawData);
	TArray<FString>* RawCapabilityStringArray = reinterpret_cast<TArray<FString>*>(RawData[0]);

	int32 Index;
	bool Found = RawCapabilityStringArray->Find(CapabilityName, Index);
	return (Found ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void FHoloLensTargetSettingsCustomization::OnCapabilityStateChanged(ECheckBoxState CheckState, TSharedRef<IPropertyHandle> CapabilityList, const FString CapabilityName)
{
	bool IsEnabled = (CheckState == ECheckBoxState::Checked);
	TArray<void*> RawData;
	CapabilityList->AccessRawData(RawData);
	TArray<FString>* RawCapabilityStringArray = reinterpret_cast<TArray<FString>*>(RawData[0]);

	CapabilityList->NotifyPreChange();
	int32 Index;
	bool Found = RawCapabilityStringArray->Find(CapabilityName, Index);

	if (Found && !IsEnabled)
	{
		// Remove existing capability from the list
		RawCapabilityStringArray->RemoveAt(Index);
		CapabilityList->NotifyPostChange(EPropertyChangeType::ArrayRemove);
	}
	else if (!Found && IsEnabled)
	{
		//Add new capability to the list
		RawCapabilityStringArray->AddUnique(*CapabilityName);
		CapabilityList->NotifyPostChange(EPropertyChangeType::ArrayAdd);
	}
}

FReply FHoloLensTargetSettingsCustomization::GenerateSigningCertificate()
{
	FString WinSDKSubKey = TEXT("SOFTWARE\\Wow6432Node\\Microsoft\\Microsoft SDKs\\Windows\\v10.0");
	FString WinSDKFolder;
	if (!FPlatformMisc::QueryRegKey(HKEY_CURRENT_USER, *WinSDKSubKey, TEXT("InstallationFolder"), WinSDKFolder))
	{
		if (!FPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *WinSDKSubKey, TEXT("InstallationFolder"), WinSDKFolder))
		{
			FNotificationInfo Info(LOCTEXT("SigningCertificateFailed_SDKInstallationFolder", "Failed to generate certificate: could not find Windows 10 SDK Installation Folder"));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);

			return FReply::Handled();
		}
	}

	//make sure the company name has required syntax
	FString CompanyDistinguishedName = GetDefault<UGeneralProjectSettings>()->CompanyDistinguishedName;
	if (!CompanyDistinguishedName.Contains(TEXT("CN=")))
	{
		FNotificationInfo Info(LOCTEXT("SigningCertificateFailed_MalformedCompanyName", "Company Distinguished Name must contain 'CN=YourCompany' syntax"));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		return FReply::Handled();
	}

	FString WinSDKVersion;
	if (!FPlatformMisc::QueryRegKey(HKEY_CURRENT_USER, *WinSDKSubKey, TEXT("ProductVersion"), WinSDKVersion))
	{
		if (!FPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *WinSDKSubKey, TEXT("ProductVersion"), WinSDKVersion))
		{
			// Carry on for now, maybe it's in an unversioned folder
		}
	}
	WinSDKVersion += TEXT(".0");

	FString ArchFragment = PLATFORM_64BITS ? TEXT("x64") : TEXT("x86");
	FString MakeCert = WinSDKFolder / TEXT("bin") / WinSDKVersion / ArchFragment / TEXT("makecert.exe");
	if (!IPlatformFile::GetPlatformPhysical().FileExists(*MakeCert))
	{
		MakeCert = WinSDKFolder / TEXT("bin") / ArchFragment / TEXT("makecert.exe");
		if (!IPlatformFile::GetPlatformPhysical().FileExists(*MakeCert))
		{
			FNotificationInfo Info(LOCTEXT("SigningCertificateFailed_MissingMakecert", "Failed to generate certificate: could not locate makecert.exe"));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);

			return FReply::Handled();
		}
	}

	FString Pvk2Pfx = WinSDKFolder / TEXT("bin") / WinSDKVersion / ArchFragment / TEXT("pvk2pfx.exe");
	if (!IPlatformFile::GetPlatformPhysical().FileExists(*Pvk2Pfx))
	{
		Pvk2Pfx = WinSDKFolder / TEXT("bin") / ArchFragment / TEXT("pvk2pfx.exe");
		if (!IPlatformFile::GetPlatformPhysical().FileExists(*Pvk2Pfx))
		{
			FNotificationInfo Info(LOCTEXT("SigningCertificateFailed_MissingPvk2pfx", "Failed to generate certificate: could not locate pvk2pfx.exe"));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);

			return FReply::Handled();
		}
	}

	FString TempCerFile = FPaths::CreateTempFilename(FPlatformProcess::UserTempDir(), TEXT("SigningCertificate-"), TEXT(".cer"));
	FString TempPvkFile = FPaths::ChangeExtension(TempCerFile, TEXT(".pvk"));
	FString TempPfxFile = FPaths::ChangeExtension(TempCerFile, TEXT(".pfx"));

	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
	int32 ExitCode = 0xffffffff;
	uint32 ProcId = 0;

	FString MakeCertParams = FString::Printf(TEXT("-r -h 0 -n \"%s\" -eku %hs -pe -sv \"%s\" \"%s\""), *GetPublisherIdentityName(), szOID_PKIX_KP_CODE_SIGNING, *TempPvkFile, *TempCerFile);

	// ExecProcess is simpler, but unfortunately forces any created window to be minimized.
	// MakeCert pops UI to ask what password the new cert should have (answer is always none for this usage, but no way to say this on the command line)
	FProcHandle MakeCertProc = FPlatformProcess::CreateProc(*MakeCert, *MakeCertParams, false, false, false, &ProcId, 0, nullptr, WritePipe);
	FString MakeCertOutput;
	FString LatestOutput = FPlatformProcess::ReadPipe(ReadPipe);
	while (FPlatformProcess::IsProcRunning(MakeCertProc) || !LatestOutput.IsEmpty())
	{
		MakeCertOutput += LatestOutput;
		LatestOutput = FPlatformProcess::ReadPipe(ReadPipe);
		FPlatformProcess::Sleep(0);
	}
	bool GotReturnCode = FPlatformProcess::GetProcReturnCode(MakeCertProc, &ExitCode);
	FPlatformProcess::CloseProc(MakeCertProc);
	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
	if (!GotReturnCode || ExitCode != 0)
	{
		FText NotificationMessage = FText::FormatOrdered(LOCTEXT("SigningCertificateFailed_MakecertFailed", "Failed to generate certificate: makecert.exe encountered an error: {0}"), FText::FromString(MakeCertOutput));
		FNotificationInfo Info(NotificationMessage);
		Info.ExpireDuration = 6.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		return FReply::Handled();
	}

	FString StdOutput;
	FString StdError;
	ExitCode = 0xffffffff;
	FString Pvk2PfxParams = FString::Printf(TEXT("-pvk \"%s\" -spc \"%s\" -pfx \"%s\""), *TempPvkFile, *TempCerFile, *TempPfxFile);
	GotReturnCode = FPlatformProcess::ExecProcess(*Pvk2Pfx, *Pvk2PfxParams, &ExitCode, &StdOutput, &StdError);
	
	if (!GotReturnCode || ExitCode != 0)
	{
		FText NotificationMessage = FText::FormatOrdered(LOCTEXT("SigningCertificateFailed_Pvk2PfxFailed", "Failed to generate certificate: pvk2pfx.exe encountered an error: {0}"), FText::FromString(StdError));
		FNotificationInfo Info(NotificationMessage);
		Info.ExpireDuration = 6.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		return FReply::Handled();
	}

	OnCertificatePicked(TempPfxFile);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteFile(*TempCerFile);
	PlatformFile.DeleteFile(*TempPvkFile);
	PlatformFile.DeleteFile(*TempPfxFile);

	return FReply::Handled();
}

FString FHoloLensTargetSettingsCustomization::GetPublisherIdentityName() const
{
	FString PublisherIdentityName = GetDefault<UGeneralProjectSettings>()->CompanyDistinguishedName;
	if (PublisherIdentityName.IsEmpty())
	{
		PublisherIdentityName = TEXT("CN=NoPublisher");
	}
	return PublisherIdentityName;
}

void FHoloLensTargetSettingsCustomization::LoadAndValidateSigningCertificate()
{
	SigningCertificateError->SetError(FText::GetEmpty());
	SigningCertificateSubjectName = TEXT("Invalid Certificate");

	FString CertificatePath = FPaths::ProjectDir() / TEXT("Build") / TEXT("HoloLens") / TEXT("SigningCertificate.pfx");
	TArray<uint8> CertBytes;
	if (FFileHelper::LoadFileToArray(CertBytes, *CertificatePath, FILEREAD_Silent))
	{
		CRYPT_DATA_BLOB CertBlob;
		CertBlob.cbData = CertBytes.Num();
		CertBlob.pbData = CertBytes.GetData();
		HCERTSTORE CertStore = PFXImportCertStore(&CertBlob, nullptr, 0);
		if (CertStore != nullptr)
		{
			PCCERT_CONTEXT SigningCertificateContext = CertEnumCertificatesInStore(CertStore, nullptr);
			if (SigningCertificateContext)
			{
				DWORD StrType = CERT_X500_NAME_STR;
				DWORD NumCharacters = CertGetNameString(SigningCertificateContext, CERT_NAME_RDN_TYPE, 0, &StrType, nullptr, 0);
				SigningCertificateSubjectName.GetCharArray().SetNumZeroed(NumCharacters);
				CertGetNameString(SigningCertificateContext, CERT_NAME_RDN_TYPE, 0, &StrType, SigningCertificateSubjectName.GetCharArray().GetData(), SigningCertificateSubjectName.GetAllocatedSize());

				// The name string is a list of key/value pairs, their order may not match
				bool bNameMatches = true;
				{
					TArray<FString> IdentityNameArray;
					FString IdentityNameString = GetPublisherIdentityName();
					IdentityNameString.ParseIntoArray(IdentityNameArray, TEXT(", "), true);

					TArray<FString> SubjectNameArray;
					SigningCertificateSubjectName.ParseIntoArray(SubjectNameArray, TEXT(", "), true);

					if (IdentityNameArray.Num() != SubjectNameArray.Num())
					{
						bNameMatches = false;
					}
					else
					{
						IdentityNameArray.Sort();
						SubjectNameArray.Sort();
						const int Num = IdentityNameArray.Num();
						for (int i = 0; i < Num; i++)
						{
							if (IdentityNameArray[i] != SubjectNameArray[i])
							{
								bNameMatches = false;
								break;
							}
						}
					}
				}

				if (!bNameMatches)
				{
					FText ErrorMessage = FText::FormatOrdered(LOCTEXT("CertificateInvalidSubjectName", "Certificate subject name[{0}] does not match Package / Identity / Publisher[{1}] in AppxManifest"), FText::FromString(SigningCertificateSubjectName), FText::FromString(GetPublisherIdentityName()));
					SigningCertificateError->SetError(ErrorMessage);
				}

				// TODO - check private key, expiration, others?
				CertFreeCertificateContext(SigningCertificateContext);
			}
			else
			{
				SigningCertificateError->SetError(LOCTEXT("CertificateEmptyStore", "No certificate found in SigningCertificate.pfx file"));
			}
			CertCloseStore(CertStore, 0);
		}
		else
		{
			DWORD FailureReason = GetLastError();
			switch (FailureReason)
			{
			case ERROR_INVALID_PASSWORD:
				SigningCertificateError->SetError(LOCTEXT("CertificateFilePasswordProtected", "SigningCertificate.pfx file is password protected"));
				break;

			default:
				SigningCertificateError->SetError(LOCTEXT("CertificateUnknownError", "Unknown error loading SigningCertificate.pfx"));
				break;
			}
		}
	}
	else
	{
		SigningCertificateError->SetError(LOCTEXT("CertificateMissing", "SigningCertificate.pfx does not exist"));
	}
}

FString FHoloLensTargetSettingsCustomization::GetSigningCertificateSubjectName() const
{
	return SigningCertificateSubjectName;
}

void FHoloLensTargetSettingsCustomization::OnAutoDetectWin10SDKChanged(ECheckBoxState NewState, TSharedRef<IPropertyHandle> Win10SDKVersionPropertyHandle)
{
	Win10SDKVersionPropertyHandle->NotifyPreChange();
	if (NewState == ECheckBoxState::Checked)
	{
		Win10SDKVersionPropertyHandle->SetValue(FString());
		WindowsSDKSelector->SetSelectedItem(MakeShared<FString>());
		WindowsSDKSelector->SetEnabled(false);
	}
	else
	{
		if (PlatformVersionOptions.Num() > 0)
		{
			Win10SDKVersionPropertyHandle->SetValue(*PlatformVersionOptions[0]);
			WindowsSDKSelector->SetSelectedItem(PlatformVersionOptions[0]);
			WindowsSDKSelector->SetEnabled(true);
		}
	}
	Win10SDKVersionPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
}

ECheckBoxState FHoloLensTargetSettingsCustomization::IsAutoDetectWin10SDKChecked() const
{
	return GetDefault<UHoloLensTargetSettings>()->Windows10SDKVersion.IsEmpty() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE

#include "Windows/HideWindowsPlatformTypes.h"
