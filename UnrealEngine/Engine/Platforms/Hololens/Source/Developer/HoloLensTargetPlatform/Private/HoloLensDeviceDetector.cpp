// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHoloLensDeviceDetector.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Misc/Base64.h"
#include "Containers/UnrealString.h"
#include "Dom/JsonObject.h"
#include "HoloLensTargetPlatform.h"

#pragma warning(push)
#pragma warning(disable:4265 4459 6101)

#include "Serialization/JsonSerializer.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

#define sealed final 

#include <windows.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/implements.h>
#include <wrl/wrappers/corewrappers.h>
#include <Windows.Devices.Enumeration.h>
#include <Windows.foundation.h>
#include <Windows.foundation.collections.h>
#undef sealed  
#pragma warning(pop)

#include <vector>
#include <memory>

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Devices::Enumeration;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

namespace 
{
	typedef __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_Windows__CDevices__CEnumeration__CDeviceInformation AddedHandler;
	typedef __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_Windows__CDevices__CEnumeration__CDeviceInformationUpdate UpdateHandler;
	typedef __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_IInspectable EnumerationCompletedHandler;
	typedef __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_IInspectable StoppedHandler;

	class StringCollectionIterator : public RuntimeClass<IIterator<HSTRING>>
	{
		InspectableClass(L"Library1.StringCollectionIterator", BaseTrust)
	public:

		virtual /* propget */ HRESULT STDMETHODCALLTYPE get_Current(_Out_ HSTRING *current)
		{
			if ((CurrentIdx >= InternalVector->size()))
			{
				return E_INVALIDARG;
			}

			*current = (*InternalVector)[CurrentIdx];
			return S_OK;
		}

		virtual /* propget */ HRESULT STDMETHODCALLTYPE get_HasCurrent(_Out_ boolean *hasCurrent)
		{
			*hasCurrent = (CurrentIdx < InternalVector->size());
			return S_OK;
		}

		virtual HRESULT STDMETHODCALLTYPE MoveNext(_Out_ boolean *hasCurrent)
		{
			*hasCurrent = (++CurrentIdx < InternalVector->size());
			return S_OK;
		}

		std::shared_ptr<std::vector<HSTRING>> InternalVector;
		size_t CurrentIdx = 0;
	};

	class StringCollection : public RuntimeClass<IIterable<HSTRING>>
	{
		InspectableClass(L"Library1.StringCollection", BaseTrust)
	public:

		virtual HRESULT STDMETHODCALLTYPE First(_Outptr_result_maybenull_ IIterator<HSTRING> **first)
		{
			ComPtr<StringCollectionIterator> iter = Make<StringCollectionIterator>();
			iter->InternalVector = InternalVector;
			return iter.CopyTo(first);
		}

		std::shared_ptr<std::vector<HSTRING>> InternalVector = std::make_shared<std::vector<HSTRING>>();
	};

}

namespace HoloLensDeviceTypes
{
	const FName HoloLens = "Windows.HoloLens";
	const FName HoloLensEmulation = "Windows.HoloLensEmulation";
}

class FHoloLensDeviceDetector : public IHoloLensDeviceDetector
{
public:
	FHoloLensDeviceDetector();
	virtual ~FHoloLensDeviceDetector();

	virtual void StartDeviceDetection();
	virtual void StopDeviceDetection();
	virtual void TryAddDevice(const FString& DeviceId, const FString& DeviceUserFriendlyName, const FString& Username, const FString& Password);

	virtual FOnDeviceDetected& OnDeviceDetected()			{ return DeviceDetected; }
	virtual const TArray<FHoloLensDeviceInfo> GetKnownDevices()	{ return KnownDevices; }

private:

	void OnDeviceWatcherDeviceAdded(IDeviceInformation* Info);

	void AddDevice(const FHoloLensDeviceInfo& Info);


private:

	ComPtr < IDeviceWatcher > DeviceWatcher;
	EventRegistrationToken addedToken;
	EventRegistrationToken updatedToken;
	EventRegistrationToken removedToken;

	FOnDeviceDetected DeviceDetected;
	FCriticalSection DevicesLock;
	TArray<FHoloLensDeviceInfo> KnownDevices;
};


IHoloLensDeviceDetectorPtr IHoloLensDeviceDetector::Create()
{
	return TSharedPtr<IHoloLensDeviceDetector, ESPMode::ThreadSafe>(new FHoloLensDeviceDetector);
}



FHoloLensDeviceDetector::FHoloLensDeviceDetector()
{
}

FHoloLensDeviceDetector::~FHoloLensDeviceDetector()
{
	StopDeviceDetection();
}

void FHoloLensDeviceDetector::StartDeviceDetection()
{
	if (DeviceWatcher)
	{
		return;
	}

#if APPXPACKAGING_ENABLE

	// We cannot use RoInitialize(multithreaded) because this is called from the main thread and other systems need single threaded apartment, and use CoInitialize to set that up.
	// If we do need to use RoInitialize we would need to move the winrt code here to a separate thread.
	bool bSuccess = FWindowsPlatformMisc::CoInitialize();
	if (!bSuccess) 
	{ 
		return; 
	}
	//HRESULT hr1;
	//hr1 = ::RoInitialize(RO_INIT_MULTITHREADED);
	//if (FAILED(hr1)) { return; }

	HRESULT hr;
	ComPtr <IDeviceInformationStatics2> DeviceInformationStatics2;
	hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_Devices_Enumeration_DeviceInformation).Get(), &DeviceInformationStatics2);
	if (FAILED(hr)) { return; }

	ComPtr<StringCollection> AdditionalProperties = Make<StringCollection>();

	HSTRING AqsFilter = HStringReference(TEXT(
		"System.Devices.AepService.ProtocolId:={4526e8c1-8aac-4153-9b16-55e86ada0e54} AND "
		"System.Devices.Dnssd.Domain:=\"local\" AND "
		"System.Devices.Dnssd.ServiceName:=\"_wdp._tcp\" "
		"")).Get();

	AdditionalProperties->InternalVector->push_back(HStringReference(L"System.Devices.Dnssd.HostName").Get());
	AdditionalProperties->InternalVector->push_back(HStringReference(L"System.Devices.Dnssd.ServiceName").Get());
	AdditionalProperties->InternalVector->push_back(HStringReference(L"System.Devices.Dnssd.PortNumber").Get());
	AdditionalProperties->InternalVector->push_back(HStringReference(L"System.Devices.Dnssd.TextAttributes").Get());
	AdditionalProperties->InternalVector->push_back(HStringReference(L"System.Devices.IpAddress").Get());

	hr = DeviceInformationStatics2->CreateWatcherWithKindAqsFilterAndAdditionalProperties(
		AqsFilter,
		AdditionalProperties.Get(),
		DeviceInformationKind::DeviceInformationKind_AssociationEndpointService,
		&DeviceWatcher);
	if (FAILED(hr)) { return; }

	hr = DeviceWatcher->add_Added(Callback<AddedHandler>([this](IDeviceWatcher* watcher, IDeviceInformation* info) -> HRESULT
	{
		//devices added
		OnDeviceWatcherDeviceAdded(info);
		return S_OK;
	}).Get(), &addedToken);

	//these callbacks should be kept for continious monitoring for new devices
	hr = DeviceWatcher->add_Updated(Callback<UpdateHandler>([](IDeviceWatcher* watcher, IDeviceInformationUpdate* info) -> HRESULT
	{
		return S_OK;
	}).Get(), &updatedToken);


	hr = DeviceWatcher->add_Removed(Callback<UpdateHandler>([](IDeviceWatcher* watcher, IDeviceInformationUpdate* info) -> HRESULT
	{
		return S_OK;
	}).Get(), &removedToken);


	DeviceWatcher->Start();
#endif
}

void FHoloLensDeviceDetector::StopDeviceDetection()
{
	if (DeviceWatcher != nullptr)
	{
		DeviceWatcher->remove_Added(addedToken);
		DeviceWatcher->remove_Updated(updatedToken);
		DeviceWatcher->remove_Removed(removedToken);

		DeviceWatcher->Stop();
		DeviceWatcher = nullptr;
	}
}


void FHoloLensDeviceDetector::OnDeviceWatcherDeviceAdded(IDeviceInformation* Info)
{
	SslCertDisabler disabler;
	FHoloLensDeviceInfo NewDevice = {};
	uint32 WdpPort = 0;
	FString DeviceIp;
	FString Protocol = TEXT("http");
	FName DeviceType = NAME_None;

	NewDevice.IsLocal = 0;

	HString hstr;
	HRESULT hr;

	hr = Info->get_Id(hstr.ReleaseAndGetAddressOf());
	if (FAILED(hr)) { return; }

	NewDevice.WindowsDeviceId = hstr.GetRawBuffer(nullptr);

	{
		ComPtr<IMapView<HSTRING, IInspectable*>> Properties;
		ComPtr<IInspectable> Inspectable;
		ComPtr<IPropertyValue> PropertyValue;

		UINT32 uiValue;
		HSTRING * pArrayString;

		hr = Info->get_Properties(&Properties);
		if (FAILED(hr)) { return; }

		//Hostname
		hr = Properties->Lookup(HStringReference(L"System.Devices.Dnssd.HostName").Get(), &Inspectable);
		if (FAILED(hr)) { return; }

		hr = Inspectable.As(&PropertyValue);
		if (FAILED(hr)) { return; }

		hr = PropertyValue->GetString(hstr.ReleaseAndGetAddressOf());
		if (FAILED(hr)) { return; }

		NewDevice.HostName = hstr.GetRawBuffer(nullptr);
		NewDevice.HostName.RemoveFromEnd(TEXT(".local"));

		//Port
		hr = Properties->Lookup(HStringReference(L"System.Devices.Dnssd.PortNumber").Get(), &Inspectable);
		if (FAILED(hr)) { return; }

		hr = Inspectable.As(&PropertyValue);
		if (FAILED(hr)) { return; }

		hr = PropertyValue->GetUInt32(&uiValue);
		if (FAILED(hr)) { return; }

		WdpPort = uiValue;

		//Attributes
		hr = Properties->Lookup(HStringReference(L"System.Devices.Dnssd.TextAttributes").Get(), &Inspectable);
		if (FAILED(hr)) { return; }

		hr = Inspectable.As(&PropertyValue);
		if (FAILED(hr)) { return; }

		hr = PropertyValue->GetStringArray(&uiValue, &pArrayString);
		if (FAILED(hr)) { return; }

		for (UINT32 i = 0; i < uiValue; ++i)
		{
			*hstr.ReleaseAndGetAddressOf() = pArrayString[i];
			const TCHAR* attrValue = hstr.GetRawBuffer(nullptr);

			//parse hstr
			FString Architecture;
			uint32 SecurePort;
			if (FParse::Value(attrValue, TEXT("S="), SecurePort))
			{
				Protocol = TEXT("https");
				WdpPort = SecurePort;
			}
			else if (FParse::Value(attrValue, TEXT("D="), NewDevice.DeviceTypeName))
			{

			}
			else if (FParse::Value(attrValue, TEXT("A="), Architecture))
			{
				if (Architecture == TEXT("AMD64"))
				{
					NewDevice.Architecture = EHoloLensArchitecture::X64;
				}
				else if (Architecture == TEXT("x86"))
				{
					NewDevice.Architecture = EHoloLensArchitecture::X86;
				}
				else if (Architecture == TEXT("ARM64"))
				{
					NewDevice.Architecture = EHoloLensArchitecture::ARM64;
				}
				else if (Architecture == TEXT("ARM"))
				{
					NewDevice.Architecture = EHoloLensArchitecture::ARM32;
				}
				else
				{
					//defaulting
					NewDevice.Architecture = EHoloLensArchitecture::X86;
				}
			}
		}

		//IP Address
		hr = Properties->Lookup(HStringReference(L"System.Devices.IpAddress").Get(), &Inspectable);
		if (FAILED(hr)) { return; }

		hr = Inspectable.As(&PropertyValue);
		if (FAILED(hr)) { return; }

		hr = PropertyValue->GetStringArray(&uiValue, &pArrayString);
		if (FAILED(hr) || pArrayString == nullptr) { return; }

		if (uiValue == 0) { return; }

		*hstr.ReleaseAndGetAddressOf() = pArrayString[0];

		DeviceIp = hstr.GetRawBuffer(nullptr);
	}

	// Xbox always requires authentication & https, so no point in even trying
	if (NewDevice.DeviceTypeName == TEXT("Windows.Xbox"))
	{
		UE_LOG(LogHoloLensTargetPlatform, Verbose, TEXT("ignoring %s %s"), *NewDevice.DeviceTypeName.ToString(), *DeviceIp);
		return;
	}

	NewDevice.WdpUrl = FString::Printf(TEXT("%s://%s:%d/"), *Protocol, *DeviceIp, WdpPort);


	// Now make a test request against the device to determine whether or not the Device Portal requires authentication.
	// If it does, the user will have to add it manually so we can collect the username and password.
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> TestRequest = FHttpModule::Get().CreateRequest();
	TestRequest->SetVerb(TEXT("GET"));
	TestRequest->SetURL(NewDevice.WdpUrl);
	TestRequest->OnProcessRequestComplete().BindLambda(
		[=](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		bool RequiresCredentials = false;
		bool ReachedDevicePortal = false;
		if (bSucceeded && HttpResponse.IsValid())
		{
			int32 ResponseCode = HttpResponse->GetResponseCode();
			RequiresCredentials = ResponseCode == EHttpResponseCodes::Denied;
			// Do not add the device if we failed on credentials because there is currently no way to recover from creating a device with the wrong credentials.
			// By failing here we make it possible to manually connect with the correct user/password later.
			ReachedDevicePortal = EHttpResponseCodes::IsOk(ResponseCode);
			if (RequiresCredentials)
			{
				UE_LOG(LogHoloLensTargetPlatform, Warning, TEXT("HoloLens device automatic connect attempt denied (http response code %i). Perhaps the user or password is wrong? Use 'add unlisted device' to connect."), ResponseCode);
			}
			else if (!ReachedDevicePortal)
			{
				UE_LOG(LogHoloLensTargetPlatform, Warning, TEXT("HoloLens device automatic connect attempt failed with http response code %i. You may be able to use 'add unlisted device' to connect"), ResponseCode);
			}
		}
		else
		{
			UE_LOG(LogHoloLensTargetPlatform, Warning, TEXT("HoloLens device automatic connect attempt failed without http response. Previous log lines may contain more information about the failed request (often bad ip/port). You may be able to use 'add unlisted device' to connect"));
		}

		if (ReachedDevicePortal || NewDevice.IsLocal)
		{
			FHoloLensDeviceInfo ValidatedDevice = NewDevice;
			ValidatedDevice.RequiresCredentials = RequiresCredentials;
			AddDevice(ValidatedDevice);
		}
	});
	TestRequest->ProcessRequest();
}



bool GetJsonField(FString& OutVal, const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName)
{
	OutVal = TEXT("");

	auto JsonValue = JsonObject->Values.Find(FieldName);

	if (!JsonValue)
	{
		return false;
	}

	if ((*JsonValue)->Type != EJson::String)
	{
		return false;
	}

	(*JsonValue)->AsArgumentType(OutVal);
	return true;
}


bool GetJsonField(int64& OutVal, const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName)
{
	OutVal = 0;
	double Value;

	auto JsonValue = JsonObject->Values.Find(FieldName);

	if (!JsonValue)
	{
		return false;
	}

	if ((*JsonValue)->Type != EJson::Number)
	{
		return false;
	}

	(*JsonValue)->AsArgumentType(Value);
	OutVal = (int32)Value;
	return true;
}



void FHoloLensDeviceDetector::TryAddDevice(const FString& DeviceId, const FString& DeviceUserFriendlyName, const FString& Username, const FString& Password)
{
	SslCertDisabler disabler;
	
	auto DeviceInfo = MakeShared<FHoloLensDeviceInfo, ESPMode::ThreadSafe>();

	DeviceInfo->HostName = DeviceUserFriendlyName;
	DeviceInfo->WdpUrl = DeviceId;
	DeviceInfo->IsLocal = 0;
	DeviceInfo->Username = Username;
	DeviceInfo->Password = Password;
	DeviceInfo->RequiresCredentials = Username.IsEmpty() && Password.IsEmpty() ? 0 : 1;
	DeviceInfo->Architecture = EHoloLensArchitecture::Unknown;
	if (!DeviceInfo->WdpUrl.Contains("://"))
	{
		DeviceInfo->WdpUrl = "http://" + DeviceInfo->WdpUrl;
	}
	if (!DeviceInfo->WdpUrl.EndsWith(TEXT("/")))
	{
		DeviceInfo->WdpUrl += TEXT("/");
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> TestRequest = FHttpModule::Get().CreateRequest();
	TestRequest->SetVerb(TEXT("GET"));
	TestRequest->SetURL(DeviceInfo->WdpUrl + TEXT("api/os/info"));

	if (DeviceInfo->RequiresCredentials)
	{
		FString AuthentificationString = FString::Format(TEXT("auto-{0}:{1}"), { DeviceInfo->Username, DeviceInfo->Password });
		FString AuthHeaderString = FString::Format(TEXT("Basic {0}"), { FBase64::Encode(AuthentificationString) });
		TestRequest->AppendToHeader(TEXT("Authorization"), AuthHeaderString);
	}

	UE_LOG(LogHoloLensTargetPlatform, Log, TEXT("The device %s is being added manually"), *DeviceInfo->WdpUrl);

	TestRequest->OnProcessRequestComplete().BindLambda(
		[=](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		if (!bSucceeded || !HttpResponse.IsValid())
		{
			UE_LOG(LogHoloLensTargetPlatform, Error, TEXT("The device %s didn't respond, the add fails.  Previous log lines may contain more information.  (often ip/port incorrect)"), *DeviceInfo->WdpUrl);
			return;
		}

		if (HttpResponse->GetResponseCode() != EHttpResponseCodes::Ok) //we can accept just 200 OK here
		{
			if (HttpResponse->GetResponseCode() == EHttpResponseCodes::Denied)
			{
				UE_LOG(LogHoloLensTargetPlatform, Error, TEXT("The device %s responded with the http error %d, the add fails.  (this code suggests user or password are incorrect."), *DeviceInfo->WdpUrl, HttpResponse->GetResponseCode());
			}
			else
			{
				UE_LOG(LogHoloLensTargetPlatform, Error, TEXT("The device %s responded with the http error %d, the add fails."), *DeviceInfo->WdpUrl, HttpResponse->GetResponseCode());
			}
			return;
		}

		FString Json = HttpResponse->GetContentAsString();
		auto RootObject = TSharedPtr<FJsonObject>();
		auto Reader = TJsonReaderFactory<TCHAR>::Create(Json);

		if (!FJsonSerializer::Deserialize(Reader, RootObject))
		{
			UE_LOG(LogHoloLensTargetPlatform, Error, TEXT("The device %s responds but the answer couldn't be decoded, the adding fails"), *DeviceInfo->WdpUrl);
			return;
		}

		FString str;
		GetJsonField(str, RootObject, TEXT("OsEdition"));
		if (str != TEXT("Holographic"))
		{
			UE_LOG(LogHoloLensTargetPlatform, Error, TEXT("The device %s responds but its OS isn't Holographic, the adding fails"), *DeviceInfo->WdpUrl);
			return;
		}

		GetJsonField(str, RootObject, TEXT("Platform"));
		if (str.StartsWith(TEXT("HoloLens")))
		{
			DeviceInfo->DeviceTypeName = HoloLensDeviceTypes::HoloLens;
		} 
		else if (str == TEXT("Virtual Machine"))
		{
			DeviceInfo->DeviceTypeName = HoloLensDeviceTypes::HoloLensEmulation;
		}
		else
		{
			UE_LOG(LogHoloLensTargetPlatform, Error, TEXT("The device %s responds but its platform isn't HoloLens/Emulator, the adding fails"), *DeviceInfo->WdpUrl);
			return;
		}

		GetJsonField(str, RootObject, TEXT("OsVersion"));
		{
			auto ptr = GetData(str);
			int idx = 0;
			while (*ptr)
			{
				if (*ptr++ == '.')
				{
					if (++idx == 2)
					{
						break;
					}
				}
			}
			if (*ptr)
			{
				if (_wcsicmp(ptr, TEXT("amd64")) == 0)
				{
					DeviceInfo->Architecture = EHoloLensArchitecture::X64;
				}
				else if (_wcsicmp(ptr, TEXT("x86")) == 0)
				{
					DeviceInfo->Architecture = EHoloLensArchitecture::X86;
				}
				else if (_wcsicmp(ptr, TEXT("arm64")) == 0)
				{
					DeviceInfo->Architecture = EHoloLensArchitecture::ARM64;
				}
			}
		}
		GetJsonField(DeviceInfo->HostName, RootObject, TEXT("ComputerName"));

		AddDevice(*DeviceInfo);
	});
	TestRequest->ProcessRequest();

}



void FHoloLensDeviceDetector::AddDevice(const FHoloLensDeviceInfo& Info)
{
	FScopeLock Lock(&DevicesLock);
	for (const FHoloLensDeviceInfo& Existing : KnownDevices)
	{
		if (Existing.HostName == Info.HostName)
		{
			UE_LOG(LogHoloLensTargetPlatform, Log, TEXT("The device %s already exists"), *Info.WdpUrl);
			return;
		}
	}

	UE_LOG(LogHoloLensTargetPlatform, Display, TEXT("The device %s have been successefully added"), *Info.WdpUrl);

	KnownDevices.Add(Info);
	DeviceDetected.Broadcast(Info);
}

SslCertDisabler::SslCertDisabler()
{
	prevValue = FWindowsPlatformHttp::VerifyPeerSslCertificate(false);
}

SslCertDisabler::~SslCertDisabler()
{
	FWindowsPlatformHttp::VerifyPeerSslCertificate(prevValue);
}

#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"
