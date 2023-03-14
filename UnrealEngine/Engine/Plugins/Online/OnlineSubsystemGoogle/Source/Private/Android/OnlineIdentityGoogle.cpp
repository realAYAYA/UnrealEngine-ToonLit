// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineIdentityGoogle.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "OnlineSubsystemGooglePrivate.h"

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Async/TaskGraphInterfaces.h"

#define GOOGLE_JNI_CPP_ERROR -2
#define GOOGLE_JNI_JAVA_ERROR -1
#define GOOGLE_JNI_OK 0

FOnlineIdentityGoogle::FOnlineIdentityGoogle(FOnlineSubsystemGoogle* InSubsystem)
	: FOnlineIdentityGoogleCommon(InSubsystem)
{
	UE_LOG_ONLINE_IDENTITY(Display, TEXT("FOnlineIdentityGoogle::FOnlineIdentityGoogle()"));

	// Setup permission scope fields
	GConfig->GetArray(TEXT("OnlineSubsystemGoogle.OnlineIdentityGoogle"), TEXT("ScopeFields"), ScopeFields, GEngineIni);
	// always required login access fields
	ScopeFields.AddUnique(TEXT(GOOGLE_PERM_PUBLIC_PROFILE));

	FOnGoogleLoginCompleteDelegate LoginDelegate = FOnGoogleLoginCompleteDelegate::CreateRaw(this, &FOnlineIdentityGoogle::OnLoginComplete);
	OnGoogleLoginCompleteHandle = AddOnGoogleLoginCompleteDelegate_Handle(LoginDelegate);

	FOnGoogleLogoutCompleteDelegate LogoutDelegate = FOnGoogleLogoutCompleteDelegate::CreateRaw(this, &FOnlineIdentityGoogle::OnLogoutComplete);
	OnGoogleLogoutCompleteHandle = AddOnGoogleLogoutCompleteDelegate_Handle(LogoutDelegate);
}

bool FOnlineIdentityGoogle::Init()
{
	int32 ResultCode = GOOGLE_JNI_CPP_ERROR;
	if (ensure(GoogleSubsystem))
	{
		extern int32 AndroidThunkCpp_Google_Init(const FString&, const FString&);
		ResultCode = AndroidThunkCpp_Google_Init(GoogleSubsystem->GetClientId(), GoogleSubsystem->GetServerClientId());
		ensureMsgf(ResultCode == GOOGLE_JNI_OK, TEXT("FOnlineIdentityGoogle::Init AndroidThunkCpp_Google_Init failed"));
	}

	return (ResultCode == GOOGLE_JNI_OK);
}

bool FOnlineIdentityGoogle::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("FOnlineIdentityGoogle::Login"));
	bool bTriggeredLogin = false;
	bool bPendingOp = LoginCompletionDelegate.IsBound() || LogoutCompletionDelegate.IsBound();
	if (!bPendingOp)
	{
		ELoginStatus::Type LoginStatus = GetLoginStatus(LocalUserNum);
		if (LoginStatus == ELoginStatus::NotLoggedIn)
		{
			LoginCompletionDelegate = FOnInternalLoginComplete::CreateLambda(
				[this, LocalUserNum](EGoogleLoginResponse InResponseCode, const FString& InAccessToken)
			{
				UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("FOnInternalLoginComplete %s %s"), ToString(InResponseCode), *InAccessToken);
				if (InResponseCode == EGoogleLoginResponse::RESPONSE_OK)
				{
					FString ErrorStr;

					TSharedPtr<FJsonObject> JsonJavaUserData;
					TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(InAccessToken);

					if (FJsonSerializer::Deserialize(JsonReader, JsonJavaUserData) &&
						JsonJavaUserData.IsValid())
					{
						const TSharedPtr<FJsonObject>* AuthData = nullptr;
						if (JsonJavaUserData->TryGetObjectField(TEXT("auth_data"), AuthData))
						{
							const TSharedPtr<FJsonObject>* UserProfile = nullptr;
							if (JsonJavaUserData->TryGetObjectField(TEXT("user_data"), UserProfile))
							{
								FAuthTokenGoogle AuthToken;
								if (AuthToken.Parse(*AuthData))
								{
									TSharedRef<FUserOnlineAccountGoogle> User = MakeShared<FUserOnlineAccountGoogle>();
									if (User->Parse(AuthToken, *UserProfile))
									{
										// update/add cached entry for user
										UserAccounts.Add(User->GetUserId()->ToString(), User);
										// keep track of user ids for local users
										UserIds.Add(LocalUserNum, User->GetUserId());
									}
									else
									{
										ErrorStr = FString::Printf(TEXT("Error parsing user profile. payload=%s"), *InAccessToken);
									}
								}
								else
								{
									ErrorStr = FString::Printf(TEXT("Error parsing auth token. payload=%s"), *InAccessToken);
								}
							}
							else
							{
								ErrorStr = FString::Printf(TEXT("user_data field missing. payload=%s"), *InAccessToken);
							}
						}
						else
						{
							ErrorStr = FString::Printf(TEXT("auth_data field missing. payload=%s"), *InAccessToken);
						}
					}
					else
					{
						ErrorStr = FString::Printf(TEXT("Failed to deserialize java data. payload=%s"), *InAccessToken);
					}

					OnLoginAttemptComplete(LocalUserNum, ErrorStr);
				}
				else
				{
					FString ErrorStr;
					if (InResponseCode == EGoogleLoginResponse::RESPONSE_CANCELED)
					{
						ErrorStr = LOGIN_CANCELLED;
					}
					else
					{
						ErrorStr = FString::Printf(TEXT("Login failure %s"), ToString(InResponseCode));
					}
					OnLoginAttemptComplete(LocalUserNum, ErrorStr);
				}
			});

			extern int32 AndroidThunkCpp_Google_Login(const TArray<FString>&);
			int32 Result = AndroidThunkCpp_Google_Login(ScopeFields);
			if (!ensure(Result == GOOGLE_JNI_OK))
			{
				// Only if JEnv is wrong
				UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("FOnlineIdentityGoogle::Login AndroidThunkCpp_Google_Login failed"));
				OnLoginComplete(EGoogleLoginResponse::RESPONSE_ERROR, TEXT(""));
			}

			bTriggeredLogin = (Result == GOOGLE_JNI_OK);
		}
		else
		{
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *GetUniquePlayerId(LocalUserNum), TEXT("Already logged in"));
		}
	}
	else
	{
		UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("FOnlineIdentityGoogle::Login Operation already in progress!"));
		FString ErrorStr = FString::Printf(TEXT("Operation already in progress"));
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdGoogle::EmptyId(), ErrorStr);
	}

	return bTriggeredLogin;
}

void FOnlineIdentityGoogle::OnLoginAttemptComplete(int32 LocalUserNum, const FString& ErrorStr)
{
	const FString ErrorStrCopy(ErrorStr);

	if (GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		UE_LOG_ONLINE_IDENTITY(Display, TEXT("Google login was successful."));
		FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
		check(UserId.IsValid());

		GoogleSubsystem->ExecuteNextTick([this, UserId, LocalUserNum, ErrorStrCopy]()
		{
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserId, ErrorStrCopy);
			TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *UserId);
		});
	}
	else
	{
		LogoutCompletionDelegate = FOnInternalLogoutComplete::CreateLambda(
			[this, LocalUserNum, ErrorStrCopy](EGoogleLoginResponse InResponseCode)
		{
			UE_LOG_ONLINE_IDENTITY(Warning, TEXT("Google login failed: %s"), *ErrorStrCopy);

			FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
			if (UserId.IsValid())
			{
				// remove cached user account
				UserAccounts.Remove(UserId->ToString());
			}
			else
			{
				UserId = FUniqueNetIdGoogle::EmptyId();
			}
			// remove cached user id
			UserIds.Remove(LocalUserNum);

			TriggerOnLoginCompleteDelegates(LocalUserNum, false, *UserId, ErrorStrCopy);
		});

		// Clean up anything left behind from cached access tokens
		extern int32 AndroidThunkCpp_Google_Logout();
		int32 Result = AndroidThunkCpp_Google_Logout();
		if (!ensure(Result == GOOGLE_JNI_OK))
		{
			// Only if JEnv is wrong
			UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("FOnlineIdentityGoogle::OnLoginAttemptComplete AndroidThunkCpp_Google_Logout failed"));
			OnLogoutComplete(EGoogleLoginResponse::RESPONSE_ERROR);
		}
	}
}

bool FOnlineIdentityGoogle::Logout(int32 LocalUserNum)
{
	bool bTriggeredLogout = false;
	bool bPendingOp = LoginCompletionDelegate.IsBound() || LogoutCompletionDelegate.IsBound();
	if (!bPendingOp)
	{
		ELoginStatus::Type LoginStatus = GetLoginStatus(LocalUserNum);
		if (LoginStatus == ELoginStatus::LoggedIn)
		{
			LogoutCompletionDelegate = FOnInternalLogoutComplete::CreateLambda(
				[this, LocalUserNum](EGoogleLoginResponse InResponseCode)
			{
				UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("FOnInternalLogoutComplete %s"), ToString(InResponseCode));
				FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
				if (UserId.IsValid())
				{
					// remove cached user account
					UserAccounts.Remove(UserId->ToString());
				}
				else
				{
					UserId = FUniqueNetIdGoogle::EmptyId();
				}
				// remove cached user id
				UserIds.Remove(LocalUserNum);

				GoogleSubsystem->ExecuteNextTick([this, UserId, LocalUserNum]()
				{
					TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
					TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::LoggedIn, ELoginStatus::NotLoggedIn, *UserId);
				});
			});

			extern int32 AndroidThunkCpp_Google_Logout();
			int32 Result = AndroidThunkCpp_Google_Logout();
			if (!ensure(Result == GOOGLE_JNI_OK))
			{
				// Only if JEnv is wrong
				UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("FOnlineIdentityGoogle::Logout AndroidThunkCpp_Google_Logout failed"));
				OnLogoutComplete(EGoogleLoginResponse::RESPONSE_ERROR);
			}

			bTriggeredLogout = (Result == GOOGLE_JNI_OK);
		}
		else
		{
			UE_LOG_ONLINE_IDENTITY(Warning, TEXT("No logged in user found for LocalUserNum=%d."), LocalUserNum);
		}
	}
	else
	{
		UE_LOG_ONLINE_IDENTITY(Warning, TEXT("FOnlineIdentityGoogle::Logout - Operation already in progress"));
	}

	if (!bTriggeredLogout)
	{
		UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("FOnlineIdentityGoogle::Logout didn't trigger logout"));
		GoogleSubsystem->ExecuteNextTick([this, LocalUserNum]()
		{
			TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
		});
	}

	return bTriggeredLogout;
}

void FOnlineIdentityGoogle::OnLoginComplete(EGoogleLoginResponse InResponseCode, const FString& InAccessToken)
{
	UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("OnLoginComplete %s %s"), ToString(InResponseCode), *InAccessToken);
	ensure(LoginCompletionDelegate.IsBound());
	LoginCompletionDelegate.ExecuteIfBound(InResponseCode, InAccessToken);
	LoginCompletionDelegate.Unbind();
}

void FOnlineIdentityGoogle::OnLogoutComplete(EGoogleLoginResponse InResponseCode)
{
	UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("OnLogoutComplete %s"), ToString(InResponseCode));
	ensure(LogoutCompletionDelegate.IsBound());
	LogoutCompletionDelegate.ExecuteIfBound(InResponseCode);
	LogoutCompletionDelegate.Unbind();
}


#define CHECK_JNI_METHOD(Id) checkf(Id != nullptr, TEXT("Failed to find " #Id));

int32 AndroidThunkCpp_Google_Init(const FString& InClientId, const FString& InServerId)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("AndroidThunkCpp_Google_Init %s %s"), *InClientId, *InServerId);
	int32 ReturnVal = GOOGLE_JNI_CPP_ERROR;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		const bool bIsOptional = false;
		static jmethodID GoogleInitGoogleMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Google_Init", "(Ljava/lang/String;Ljava/lang/String;)I", bIsOptional);
		CHECK_JNI_METHOD(GoogleInitGoogleMethod);
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("GoogleInitGoogleMethod 0x%08x"), GoogleInitGoogleMethod);

		auto jClientAuthId = FJavaHelper::ToJavaString(Env, InClientId);
		auto jServerAuthId = FJavaHelper::ToJavaString(Env, InServerId);
		
		ReturnVal = FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, GoogleInitGoogleMethod, *jClientAuthId, *jServerAuthId);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
			ReturnVal = GOOGLE_JNI_CPP_ERROR;
		}

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("AndroidThunkJava_Google_Init retval=%d"), ReturnVal);
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("AndroidThunkJava_Google_Init JNI error"));
	}
	return ReturnVal;
}

int32 AndroidThunkCpp_Google_Login(const TArray<FString>& InScopeFields)
{
	UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("AndroidThunkCpp_Google_Login"));
	int32 ReturnVal = GOOGLE_JNI_CPP_ERROR;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		const bool bIsOptional = false;
		static jmethodID GoogleLoginMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Google_Login", "([Ljava/lang/String;)I", bIsOptional);
		CHECK_JNI_METHOD(GoogleLoginMethod);
		UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("GoogleLoginMethod 0x%08x"), GoogleLoginMethod);

		// Convert scope array into java fields
		auto ScopeIDArray = NewScopedJavaObject(Env, (jobjectArray)Env->NewObjectArray(InScopeFields.Num(), FJavaWrapper::JavaStringClass, nullptr));
		for (uint32 Param = 0; Param < InScopeFields.Num(); Param++)
		{
			auto StringValue = FJavaHelper::ToJavaString(Env, InScopeFields[Param]);
			Env->SetObjectArrayElement(*ScopeIDArray, Param, *StringValue);
		}

		ReturnVal = FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, GoogleLoginMethod, *ScopeIDArray);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
			ReturnVal = GOOGLE_JNI_CPP_ERROR;
		}

		UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("AndroidThunkCpp_Google_Login retval=%d"), ReturnVal);
	}
	else
	{
		UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("AndroidThunkCpp_Google_Login JNI error"));
	}

	return ReturnVal;
}

JNI_METHOD void Java_com_epicgames_unreal_GoogleLogin_nativeLoginComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jstring javaData)
{
	EGoogleLoginResponse LoginResponse = (EGoogleLoginResponse)responseCode;
	auto JavaData = FJavaHelper::FStringFromParam(jenv, javaData);

	UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("nativeLoginComplete Response: %s Data: %s"), ToString(LoginResponse), *JavaData);

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessGoogleLogin"), STAT_FSimpleDelegateGraphTask_ProcessGoogleLogin, STATGROUP_TaskGraphTasks);
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Google login completed %s"), ToString(LoginResponse));
			if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(GOOGLE_SUBSYSTEM))
			{
				FOnlineIdentityGooglePtr IdentityGoogleInt = StaticCastSharedPtr<FOnlineIdentityGoogle>(OnlineSub->GetIdentityInterface());
				if (IdentityGoogleInt.IsValid())
				{
					IdentityGoogleInt->TriggerOnGoogleLoginCompleteDelegates(LoginResponse, JavaData);
				}
			}
		}),
	GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessGoogleLogin),
	nullptr,
	ENamedThreads::GameThread
	);
}

int32 AndroidThunkCpp_Google_Logout()
{
	UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("AndroidThunkCpp_Google_Logout"));
	int32 ReturnVal = GOOGLE_JNI_CPP_ERROR;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		const bool bIsOptional = false;
		static jmethodID GoogleLogoutMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Google_Logout", "()I", bIsOptional);
		CHECK_JNI_METHOD(GoogleLogoutMethod);
		UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("GoogleLogoutMethod 0x%08x"), GoogleLogoutMethod);
		ReturnVal = FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, GoogleLogoutMethod);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
			ReturnVal = GOOGLE_JNI_CPP_ERROR;
		}
		
		UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("AndroidThunkCpp_Google_Logout retval=%d"), ReturnVal);
	}
	else
	{
		UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("AndroidThunkCpp_Google_Logout JNI error"));
	}
	return ReturnVal;
}

JNI_METHOD void Java_com_epicgames_unreal_GoogleLogin_nativeLogoutComplete(JNIEnv* jenv, jobject thiz, jsize responseCode)
{
	EGoogleLoginResponse LogoutResponse = (EGoogleLoginResponse)responseCode;
	UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("nativeLogoutComplete %s"), ToString(LogoutResponse));

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessGoogleLogout"), STAT_FSimpleDelegateGraphTask_ProcessGoogleLogout, STATGROUP_TaskGraphTasks);
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Google logout completed %s"), ToString(LogoutResponse));
			if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(GOOGLE_SUBSYSTEM))
			{
				FOnlineIdentityGooglePtr IdentityGoogleInt = StaticCastSharedPtr<FOnlineIdentityGoogle>(OnlineSub->GetIdentityInterface());
				if (IdentityGoogleInt.IsValid())
				{
					IdentityGoogleInt->TriggerOnGoogleLogoutCompleteDelegates(LogoutResponse);
				}
			}
		}),
	GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessGoogleLogout),
	nullptr,
	ENamedThreads::GameThread
	);
}





