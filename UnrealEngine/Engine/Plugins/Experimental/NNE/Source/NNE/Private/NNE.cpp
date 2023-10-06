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

		static FRegistry* Get()
		{
			static FRegistry Inst;

			return &Inst;
		}

		bool Add(TWeakInterfacePtr<INNERuntime> Runtime)
		{
			if (!Runtime.IsValid())
			{
				return false;
			}
			
			if (FindByName(Runtime->GetRuntimeName()) >= 0)
			{
				UE_LOG(LogNNE, Warning, TEXT("Runtime %s is already registered"), *Runtime->GetRuntimeName());
				return false;
			}

			Runtimes.Add(Runtime);

			return true;
		}

		bool Remove(TWeakInterfacePtr<INNERuntime> Runtime)
		{
			if (!Runtime.IsValid())
			{
				return false;
			}
			
			int Index = FindByName(Runtime->GetRuntimeName());
			if (Index >= 0)
			{
				Runtimes.RemoveAtSwap(Index);
				return true;
			}

			return false;
		}


		TArrayView<TWeakInterfacePtr<INNERuntime>> GetAllRuntimes()
		{
			RemoveInvalidPtrs();
			return Runtimes;
		}

	private:

		void RemoveInvalidPtrs()
		{
			Runtimes.RemoveAllSwap([](TWeakInterfacePtr<INNERuntime> Runtime) -> bool {return !Runtime.IsValid();}, false);
		}

		int FindByName(const FString& Name)
		{
			RemoveInvalidPtrs();
			
			int Index = -1;

			for (int Idx = 0; Idx < Runtimes.Num(); ++Idx)
			{
				if (Runtimes[Idx]->GetRuntimeName() == Name)
				{
					Index = Idx;
					break;
				}
			}

			return Index;
		}

		TArray<TWeakInterfacePtr<INNERuntime>>	Runtimes;
	};

	bool RegisterRuntime(TWeakInterfacePtr<INNERuntime> Runtime)
	{
		FString RuntimeName = Runtime->GetRuntimeName();
		FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddLambda([RuntimeName]() {
			if (FEngineAnalytics::IsAvailable())
			{
				TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
					TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
					TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*RuntimeName)
				);
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.RegisterRuntime"), Attributes);
			}
		});

		return FRegistry::Get()->Add(Runtime);
	}

	bool UnregisterRuntime(TWeakInterfacePtr<INNERuntime> Runtime)
	{
		return FRegistry::Get()->Remove(Runtime);
	}

	TArrayView<TWeakInterfacePtr<INNERuntime>> GetAllRuntimes()
	{
		return FRegistry::Get()->GetAllRuntimes();
	}
}
