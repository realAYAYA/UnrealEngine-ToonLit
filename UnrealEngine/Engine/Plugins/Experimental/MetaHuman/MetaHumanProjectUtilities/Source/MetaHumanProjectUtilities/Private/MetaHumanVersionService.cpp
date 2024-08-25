// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanVersionService.h"

#include "Algo/RemoveIf.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "MetaHumanProjectUtilitiesSettings.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanVersionService, Log, All)
namespace UE::MetaHumanVersionService
{
	namespace Private
	{
		class FMetaHumanVersionServiceClient
		{
		public:
			const FString& UEVersionFromMhVersion(const FMetaHumanVersion& Version)
			{
				static const FString UnknownVersion = TEXT("Unknown Version");
				AwaitRequest(VersionInfoRequest);
				if (const FString *UeVersion = VersionMapping.Find(Version))
				{
					return *UeVersion;
				}
				return UnknownVersion;
			}

			TArray<TSharedRef<FReleaseNoteData>> GetReleaseNotesForVersionUpgrade(const FMetaHumanVersion& FromVersion, const FMetaHumanVersion& ToVersion)
			{
				AwaitRequest(ReleaseNotesRequest);
				// Take a copy of the release notes
				TArray<TSharedRef<FReleaseNoteData>> ToReturn = ReleaseNotes;

				// Remove the ones that do not relate to the current upgrade (either before the current version of after the version we are upgrading to)
				ToReturn.SetNum(Algo::RemoveIf(ToReturn, [&FromVersion, &ToVersion](const TSharedRef<FReleaseNoteData>& Item)
				{
					return Item->Version <= FromVersion || Item->Version > ToVersion;
				}));

				// Sort by release version number
				ToReturn.Sort([](const TSharedRef<FReleaseNoteData>& A, const TSharedRef<FReleaseNoteData>& B) { return A->Version > B->Version; });

				return ToReturn;
			}

			static TSharedPtr<FMetaHumanVersionServiceClient> Get()
			{
				if (!MetaHumanVersionServiceClientInst.IsValid())
				{
					MetaHumanVersionServiceClientInst = MakeShareable(new FMetaHumanVersionServiceClient);
				}
				return MetaHumanVersionServiceClientInst;
			}
			
			void OverrideServiceUrl(const FString &OverrideUrl)
			{
				FetchDataFromVersionService(OverrideUrl);
			}
			
			~FMetaHumanVersionServiceClient()
			{
				TerminateRequest(VersionInfoRequest);
				TerminateRequest(ReleaseNotesRequest);
			}

		private:
			FMetaHumanVersionServiceClient()
			{
				const UMetaHumanProjectUtilitiesSettings* Settings = GetDefault<UMetaHumanProjectUtilitiesSettings>();
				FetchDataFromVersionService(Settings->VersionServiceBaseUrl);
			}

			void ParseVersionInfoFromJson(const FJsonValue* Data)
			{
				VersionMapping.Reset();
				for (const TSharedPtr<FJsonValue>& VersionInfoEntry : Data->AsArray())
				{
					FString UEVersion = VersionInfoEntry->AsObject()->GetStringField(TEXT("ueVersion"));
					const TArray<TSharedPtr<FJsonValue>>& MHCVersions = VersionInfoEntry->AsObject()->GetArrayField(TEXT("all"));
					for (const auto& Version : MHCVersions)
					{
						// Values are ordered from most recent UE to least recent. This will mean that newer entries
						// get overwritten by older ones and so we always end up with the earliest UEVersion per
						// MHC version which is what we want.
						VersionMapping.Add(FMetaHumanVersion{Version->AsString()}, UEVersion);
					}
				}
			}

			void ParseReleaseNotesFromJson(const FJsonValue* Data)
			{
				ReleaseNotes.Reset();
				for (const TTuple<FString, TSharedPtr<FJsonValue>>& ReleaseNoteEntry : Data->AsObject()->Values)
				{
					const FString& MHCVersion = ReleaseNoteEntry.Key;
					const TSharedPtr<FJsonObject>& ReleaseNote = ReleaseNoteEntry.Value->AsObject();
					ReleaseNotes.Add(MakeShared<MetaHumanVersionService::FReleaseNoteData>(MetaHumanVersionService::FReleaseNoteData{
						FText::FromString(ReleaseNote->GetStringField(TEXT("title"))),
						FMetaHumanVersion(MHCVersion),
						FText::FromString(ReleaseNote->GetStringField(TEXT("description"))),
						FText::FromString(ReleaseNote->GetStringField(TEXT("details"))),
					}));
				}
			}

			// Initiate an HTTP request and attach an on-completion callback to parse and store the results.
			FHttpRequestPtr InitiateRequest(const FString& RequestUrl, const FString& RequestName, const TFunction<void(const FJsonValue*)>& OnComplete) const
			{
				FHttpRequestPtr HttpRequest = FHttpModule::Get().CreateRequest();
				HttpRequest->OnProcessRequestComplete().BindLambda(
					[this, OnComplete, RequestName](FHttpRequestPtr Unused, FHttpResponsePtr HttpResponse, bool bSucceeded)
					{
						if (!bSucceeded || !HttpResponse.IsValid())
						{
							UE_LOG(LogMetaHumanVersionService, Warning, TEXT("%s: No response"), *RequestName);
							return;
						}

						FString ResponseStr = HttpResponse->GetContentAsString();
						TSharedPtr<FJsonValue> Data;
						if (!EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResponseStr), Data))
						{
							UE_LOG(LogMetaHumanVersionService, Warning, TEXT("%s"), *FString::Format(TEXT("{0}: Invalid response. code={1} response={2}"), {RequestName, HttpResponse->GetResponseCode(), ResponseStr}));
							return;
						}

						OnComplete(Data.Get());
					});

				// TODO some more refined retry policy.
				// TODO authentication (if required).
				HttpRequest->SetURL(RequestUrl);
				HttpRequest->SetVerb(TEXT("GET"));
				// HttpRequest.SetHeader("Authorization", AuthorizationHeader);
				HttpRequest->SetTimeout(10);
				HttpRequest->ProcessRequest();
				return HttpRequest;
			}

			// Perform a blocking wait for the request.
			static void AwaitRequest(FHttpRequestPtr& Request)
			{
				// This loop is bounded by the timeout on the request.
				while (Request.IsValid() && !IsFinished(Request->GetStatus()))
				{
					FPlatformProcess::Sleep(0.1);
					FHttpModule::Get().GetHttpManager().Tick(0.1);
				}
				// Request is completed, now clean up.
				TerminateRequest(Request);
			}

			// Clean up any resources associated with the request and cancel it if it is not yet completed
			static void TerminateRequest(FHttpRequestPtr& Request)
			{
				if (Request.IsValid())
				{
					Request->OnProcessRequestComplete().Unbind();
					if (!IsFinished(Request->GetStatus()))
					{
						Request->CancelRequest();
					}
					Request.Reset();
				}
			}

			// These pointers are valid while the initial request is processing and invalid once the data has been retrieved (or if the request fails).
			FHttpRequestPtr VersionInfoRequest;
			FHttpRequestPtr ReleaseNotesRequest;

			// ReleaseNotes are initialised with fall-back data.
			TArray<TSharedRef<FReleaseNoteData>> ReleaseNotes = {
				MakeShared<FReleaseNoteData>(FReleaseNoteData{
					FText::FromString("None Available"),
					FMetaHumanVersion(2, 0, 0),
					FText::FromString("Failed to retrieve release notes."),
					FText::FromString("Release notes.")
				})
			};

			// VersionMappings are initialised with fall-back data.
			TMap<FMetaHumanVersion, FString> VersionMapping = {
				{FMetaHumanVersion(0, 5, 0), TEXT("4.27")},
				{FMetaHumanVersion(0, 5, 1), TEXT("4.27")},
				{FMetaHumanVersion(0, 5, 2), TEXT("4.27")},
				{FMetaHumanVersion(0, 5, 3), TEXT("4.27")},
				{FMetaHumanVersion(1, 0, 0), TEXT("5.0")},
				{FMetaHumanVersion(1, 1, 0), TEXT("5.0")},
				{FMetaHumanVersion(1, 2, 0), TEXT("5.0")},
				{FMetaHumanVersion(1, 2, 1), TEXT("5.0")},
				{FMetaHumanVersion(1, 2, 2), TEXT("5.0")},
				{FMetaHumanVersion(1, 2, 3), TEXT("5.0")},
				{FMetaHumanVersion(1, 3, 0), TEXT("5.0")},
				{FMetaHumanVersion(1, 3, 1), TEXT("5.0")},
				{FMetaHumanVersion(2, 0, 0), TEXT("5.2")}
			};

			// Url Handling
			void FetchDataFromVersionService(const FString &VersionServiceUrl)
			{
				// Cancel any in-flight requests
				TerminateRequest(VersionInfoRequest);
				TerminateRequest(ReleaseNotesRequest);
				
				// Fire off the requests for the live data. These will complete asynchronously. If these requests fail we fall back to bundled data.
				VersionInfoRequest = InitiateRequest(FString::Format(TEXT("{0}/api/v1/versions"), {VersionServiceUrl}), TEXT("Fetch Version Info"), [this](const FJsonValue* Data)
				{
					ParseVersionInfoFromJson(Data);
				});

				ReleaseNotesRequest = InitiateRequest(FString::Format(TEXT("{0}/api/v1/release-notes"), {VersionServiceUrl}), TEXT("Fetch Release Notes"), [this](const FJsonValue* Data)
				{
					ParseReleaseNotesFromJson(Data);
				});
				
			}

			// Singleton Implementation
			static TSharedPtr<FMetaHumanVersionServiceClient> MetaHumanVersionServiceClientInst;
		};

		TSharedPtr<FMetaHumanVersionServiceClient> FMetaHumanVersionServiceClient::MetaHumanVersionServiceClientInst;
	}


	const FString& UEVersionFromMhVersion(const FMetaHumanVersion& Version)
	{
		return Private::FMetaHumanVersionServiceClient::Get()->UEVersionFromMhVersion(Version);
	}

	TArray<TSharedRef<FReleaseNoteData>> GetReleaseNotesForVersionUpgrade(const FMetaHumanVersion& FromVersion, const FMetaHumanVersion& ToVersion)
	{
		return Private::FMetaHumanVersionServiceClient::Get()->GetReleaseNotesForVersionUpgrade(FromVersion, ToVersion);
	}
	
	void SetServiceUrl(const FString &ServiceUrl)
	{
		Private::FMetaHumanVersionServiceClient::Get()->OverrideServiceUrl(ServiceUrl);
	}

	void Init()
	{
		Private::FMetaHumanVersionServiceClient::Get();
	}
}
