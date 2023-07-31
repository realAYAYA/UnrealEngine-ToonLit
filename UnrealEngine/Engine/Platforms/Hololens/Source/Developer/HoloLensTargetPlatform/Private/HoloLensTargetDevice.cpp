// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensTargetDevice.h"

#include "Misc/Paths.h"

#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
#include "Windows/AllowWindowsPlatformTypes.h"

#endif
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Misc/Base64.h"
#include "Containers/UnrealString.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ScopeLock.h"

#pragma warning(push)
#pragma warning(disable:4265 4459)

#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#pragma warning(pop)

#include <shlwapi.h>

#if (NTDDI_VERSION < NTDDI_WIN8)
#pragma push_macro("NTDDI_VERSION")
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN8
//this header cannot be directly imported because of current _WIN32_WINNT less then 0x0602 (the value assigned in UEBuildWindows.cs:139)
//the macro code added from couple interface declarations, it doesn't affect to any imported function
#include <shobjidl.h> 
#pragma pop_macro("NTDDI_VERSION")
#else
#include <shobjidl.h> 
#endif

#if APPXPACKAGING_ENABLE 
#include <AppxPackaging.h>
#endif

#include "Microsoft/COMPointer.h"

#if WITH_ENGINE
#include "Engine/World.h"
#include "TimerManager.h"
#endif
#include <functional>


FHoloLensTargetDevice::FHoloLensTargetDevice(const ITargetPlatform& InTargetPlatform, const FHoloLensDeviceInfo& InInfo)
	: HeartBeatRequest(nullptr)
	, TargetPlatform(InTargetPlatform)
	, Info(InInfo)
	, IsDeviceConnected(true)
{
	if (!Info.IsLocal)
	{
#if WITH_ENGINE
		GWorld->GetTimerManager().SetTimer(TimerHandle, std::bind(&FHoloLensTargetDevice::StartHeartBeat, this), 10.f, true, 10.f);
#endif
	}
}

FHoloLensTargetDevice::~FHoloLensTargetDevice()
{
	FScopeLock lock(&CriticalSection);
	if (!Info.IsLocal)
	{
#if WITH_ENGINE
		if ((GWorld != nullptr) && TimerHandle.IsValid())
		{
			GWorld->GetTimerManager().ClearTimer(TimerHandle);
		}
#endif
		if (HeartBeatRequest)
		{
			HeartBeatRequest->OnProcessRequestComplete().Unbind();
			HeartBeatRequest->CancelRequest();
			HeartBeatRequest = nullptr;
		}
	}
}


bool FHoloLensTargetDevice::Connect() 
{
	return true;
}

void FHoloLensTargetDevice::Disconnect() 
{ }

ETargetDeviceTypes FHoloLensTargetDevice::GetDeviceType() const 
{
	return ETargetDeviceTypes::HMD;
}

FTargetDeviceId FHoloLensTargetDevice::GetId() const 
{
	if (Info.IsLocal)
	{
		return FTargetDeviceId(TargetPlatform.PlatformName(), Info.HostName);
	}
	else
	{
		// This is what gets handed off to UAT, so we need to supply the
		// actual Device Portal url instead of just the host name
		return FTargetDeviceId(TargetPlatform.PlatformName(), Info.WdpUrl);
	}
}

FString FHoloLensTargetDevice::GetName() const 
{
	return Info.HostName + TEXT(" (HoloLens)");
}

FString FHoloLensTargetDevice::GetOperatingSystemName() 
{
	return FString::Printf(TEXT("HoloLens (%s)"), *Info.DeviceTypeName.ToString());
}

bool FHoloLensTargetDevice::GetProcessSnapshotAsync(TFunction<void(const TArray<FTargetDeviceProcessInfo>&)> CompleteHandler)
{
	if (Info.IsLocal)
	{
		return false;
	}

	auto HttpRequest = GenerateRequest();
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetURL(Info.WdpUrl + TEXT("api/resourcemanager/processes"));
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[&, CompleteHandler](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		TArray<FTargetDeviceProcessInfo> OutProcessInfos;
		do
		{
			if (!bSucceeded || !HttpResponse.IsValid())
			{
				break;
			}
			FString Json = HttpResponse->GetContentAsString();
			auto RootObject = TSharedPtr<FJsonObject>();
			auto Reader = TJsonReaderFactory<TCHAR>::Create(Json);

			if (!FJsonSerializer::Deserialize(Reader, RootObject))
			{
				break;
			}

			const TArray< TSharedPtr<FJsonValue> > * OutArray;
			if (!RootObject->TryGetArrayField(TEXT("Processes"), OutArray))
			{
				break;
			}

			for (TSharedPtr<FJsonValue> process : *OutArray)
			{
				FTargetDeviceProcessInfo ProcessInfo;
				ProcessInfo.ParentId = 0;
				FString Publisher; //present for HoloLens processes only
				FString ImageName, AppName;
				TSharedPtr<FJsonObject> obj = process->AsObject();
				if (!GetJsonField(Publisher, obj, TEXT("Publisher")))
				{
					continue;
				}
				if (!GetJsonField(ImageName, obj, TEXT("ImageName")))
				{
					continue;
				}
				if (!GetJsonField(AppName, obj, TEXT("AppName")))
				{
					continue;
				}
				if (!GetJsonField(ProcessInfo.UserName, obj, TEXT("UserName")))
				{
					continue;
				}
				if (!GetJsonField(ProcessInfo.Id, obj, TEXT("ProcessId")))
				{
					continue;
				}
				ProcessInfo.Name = FString::Format(TEXT("{0} ({1})"), { ImageName , AppName } );
				OutProcessInfos.Add(ProcessInfo);
			}
		} while (false);

		CompleteHandler(OutProcessInfos);
	});

	HttpRequest->ProcessRequest();
	return true;
}

const class ITargetPlatform& FHoloLensTargetDevice::GetTargetPlatform() const
{
	return TargetPlatform;
}

bool FHoloLensTargetDevice::GetUserCredentials(FString& OutUserName, FString& OutUserPassword) 
{
	if (!Info.RequiresCredentials)
	{
		return false;
	}

	OutUserName = Info.Username;
	OutUserPassword = Info.Password;
	return true;
}

bool FHoloLensTargetDevice::IsConnected()
{
	if (Info.IsLocal)
	{
		return true;
	}

	return IsDeviceConnected;
}

bool FHoloLensTargetDevice::IsDefault() const 
{
	return true;
}

TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> FHoloLensTargetDevice::GenerateRequest() const
{
	if (Info.IsLocal)
	{
		return nullptr;
	}

	SslCertDisabler disabler;
	
	auto HttpRequest = TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>(FPlatformHttp::ConstructRequest());

	if (Info.RequiresCredentials)
	{
		FString AuthentificationString = FString::Format(TEXT("auto-{0}:{1}"), { Info.Username, Info.Password });
		FString AuthHeaderString = FString::Format(TEXT("Basic {0}"), { FBase64::Encode(AuthentificationString) });
		HttpRequest->AppendToHeader(TEXT("Authorization"), AuthHeaderString);
	}

	return HttpRequest;
}


bool FHoloLensTargetDevice::PowerOff(bool Force) 
{
	if (Info.IsLocal)
	{
		return false;
	}

	auto HttpRequest = GenerateRequest();
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(Info.WdpUrl + TEXT("api/control/shutdown"));
	HttpRequest->ProcessRequest();
	return true;
}

bool FHoloLensTargetDevice::PowerOn() 
{
	return false;
}

bool FHoloLensTargetDevice::Reboot(bool bReconnect) 
{
	if (Info.IsLocal)
	{
		return false;
	}

	auto HttpRequest = GenerateRequest();
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(Info.WdpUrl + TEXT("api/control/restart"));
	HttpRequest->ProcessRequest();

	return true;
}

void FHoloLensTargetDevice::SetUserCredentials(const FString& UserName, const FString& UserPassword) 
{ 
	Info.Username = UserName;
	Info.Password = UserPassword;
}

bool FHoloLensTargetDevice::SupportsFeature(ETargetDeviceFeatures Feature) const 
{
	if (Info.IsLocal)
	{
		return false;
	}

	switch (Feature)
	{
	case ETargetDeviceFeatures::PowerOff:
	case ETargetDeviceFeatures::ProcessSnapshot:
	case ETargetDeviceFeatures::Reboot:
		return true;
	default:
		return false;
	}
}

bool FHoloLensTargetDevice::TerminateProcess(const int64 ProcessId) 
{
	if (Info.IsLocal)
	{
		return false;
	}

	auto HttpRequest = GenerateRequest();
	HttpRequest->SetVerb(TEXT("DELETE"));
	HttpRequest->SetURL(Info.WdpUrl + TEXT("api/taskmanager/process?pid=") + FString::FromInt(ProcessId));
	HttpRequest->ProcessRequest();

	return true;
}

bool FHoloLensTargetDevice::TerminateLaunchedProcess(const FString & ProcessIdentifier)
{
	if (Info.IsLocal)
	{
		return false;
	}

	auto HttpRequest = GenerateRequest();
	HttpRequest->SetVerb(TEXT("DELETE"));
	HttpRequest->SetURL(Info.WdpUrl + TEXT("api/taskmanager/app?package=") + FBase64::Encode(ProcessIdentifier));
	HttpRequest->ProcessRequest();

	return true;
}

void FHoloLensTargetDevice::StartHeartBeat()
{
	FScopeLock lock(&CriticalSection);
	if (HeartBeatRequest)
	{
		return;
	}
#if WITH_EDITOR
	if (!TimerHandle.IsValid()) // the app is quiting
	{
		return;
	}
#endif
	HeartBeatRequest = GenerateRequest();
	HeartBeatRequest->SetVerb(TEXT("GET"));
	HeartBeatRequest->SetURL(Info.WdpUrl + TEXT("api/os/info"));

	HeartBeatRequest->OnProcessRequestComplete().BindLambda(
		[=](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		FString HostName;
		bool Success = false;
		do
		{
			if (!bSucceeded || !HttpResponse.IsValid())
			{
				break;
			}
			FString Json = HttpResponse->GetContentAsString();
			auto RootObject = TSharedPtr<FJsonObject>();
			auto Reader = TJsonReaderFactory<TCHAR>::Create(Json);

			if (!FJsonSerializer::Deserialize(Reader, RootObject))
			{
				break;
			}

			if (GetJsonField(HostName, RootObject, TEXT("ComputerName")))
			{
				Success = (HostName == Info.HostName);
			}
		} while (false);

		{
			FScopeLock lock(&CriticalSection);
			IsDeviceConnected = Success;
			HeartBeatRequest = nullptr;
		}
	});

	HeartBeatRequest->ProcessRequest();
}


#include "Windows/HideWindowsPlatformTypes.h"
