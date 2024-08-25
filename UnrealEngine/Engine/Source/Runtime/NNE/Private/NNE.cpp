// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNE.h"

#include "EngineAnalytics.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "Misc/SecureHash.h"

DEFINE_LOG_CATEGORY(LogNNE);

namespace UE::NNE
{

	class FRegistry
	{
	public:
		static FRegistry& GetInstance()
		{
			static FRegistry Instance;
			return Instance;
		}

		EResultStatus Add(TWeakInterfacePtr<INNERuntime> Runtime)
		{
			checkf(Runtime.IsValid(), TEXT("Runtime is not valid"));

			const FString RuntimeName = Runtime->GetRuntimeName();
			checkf(!RuntimeName.IsEmpty(), TEXT("Runtime name is empty"));

			if (Runtimes.Contains(RuntimeName))
			{
				UE_LOG(LogNNE, Warning, TEXT("Runtime %s is already registered"), *RuntimeName);
				return EResultStatus::Fail;
			}

			Runtimes.Add(RuntimeName, Runtime);

			return EResultStatus::Ok;
		}

		EResultStatus Remove(TWeakInterfacePtr<INNERuntime> Runtime)
		{
			checkf(Runtime.IsValid(), TEXT("Runtime is not valid"));

			const FString RuntimeName = Runtime->GetRuntimeName();
			checkf(!RuntimeName.IsEmpty(), TEXT("Runtime name is empty"));

			return Runtimes.Remove(RuntimeName) >= 1 ? EResultStatus::Ok : EResultStatus::Fail;
		}

		TWeakInterfacePtr<INNERuntime> Get(const FString& Name) const
		{
			checkf(!Name.IsEmpty(), TEXT("Name is empty"));

			if (Runtimes.Contains(Name))
			{
				TWeakInterfacePtr<INNERuntime> Result = Runtimes.FindChecked(Name);
				ensureMsgf(Result.IsValid(), TEXT("Runtime %s is not valid"), *Name);

				return Result;
			}

			return {};
		}

		TArray<FString> GetAllNames() const
		{
			TArray<FString> Result;
			Runtimes.GenerateKeyArray(Result);

			Result.SetNum(Algo::RemoveIf(Result, [&] (const FString &RuntimeName)
			{
				return !ensureMsgf(Runtimes.FindChecked(RuntimeName).IsValid(), TEXT("Runtime %s is not valid"), *RuntimeName);
			}));

			return Result;
		}

	private:
		TMap<FString, TWeakInterfacePtr<INNERuntime>> Runtimes;
	};

	ERegisterRuntimeStatus RegisterRuntime(TWeakInterfacePtr<INNERuntime> Runtime)
	{
		const ERegisterRuntimeStatus Result = FRegistry::GetInstance().Add(Runtime);

#ifdef WITH_EDITOR
		FModuleManager::Get().LoadModule(TEXT("NNEEditor"));
#endif

		const FString RuntimeName = Runtime->GetRuntimeName();

		FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddLambda([RuntimeName]()
		{
			if (FEngineAnalytics::IsAvailable())
			{
				TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
					TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
					TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*RuntimeName)
				);
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.RegisterRuntime"), Attributes);
			}
		});

		return Result;
	}

	EUnregisterRuntimeStatus UnregisterRuntime(TWeakInterfacePtr<INNERuntime> Runtime)
	{
		return FRegistry::GetInstance().Remove(Runtime);
	}

	TArray<FString> GetAllRuntimeNames()
	{
		return FRegistry::GetInstance().GetAllNames();
	}

	TWeakInterfacePtr<INNERuntime> GetRuntime(const FString& Name)
	{
		return FRegistry::GetInstance().Get(Name);
	}
}
