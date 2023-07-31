// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "IAudioProxyInitializer.h"
#include "MetasoundDataReference.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "WaveTable.h"
#include "WaveTableBank.h"


namespace Metasound
{
	class FWaveTableBankAsset;
	using FWaveTableBankAssetReadRef = TDataReadReference<FWaveTableBankAsset>;
	using FWaveTableReadRef = TDataReadReference<WaveTable::FWaveTable>;
	using FWaveTableWriteRef = TDataWriteReference<WaveTable::FWaveTable>;

	class METASOUNDENGINE_API FWaveTableBankAsset
	{
		FWaveTableBankAssetProxyPtr Proxy;

	public:
		FWaveTableBankAsset()
			: Proxy(nullptr)
		{
		}

		FWaveTableBankAsset(const FWaveTableBankAsset& InOther)
			: Proxy(InOther.Proxy)
		{
		}

		FWaveTableBankAsset& operator=(const FWaveTableBankAsset& InOther)
		{
			Proxy = InOther.Proxy;
			return *this;
		}

		FWaveTableBankAsset& operator=(FWaveTableBankAsset&& InOther)
		{
			Proxy = MoveTemp(InOther.Proxy);
			return *this;
		}

		FWaveTableBankAsset(const Audio::IProxyDataPtr& InInitData)
		{
			if (InInitData.IsValid())
			{
				if (InInitData->CheckTypeCast<FWaveTableBankAssetProxy>())
				{
					Proxy = MakeShared<FWaveTableBankAssetProxy, ESPMode::ThreadSafe>(InInitData->GetAs<FWaveTableBankAssetProxy>());
				}
			}
		}

		bool IsValid() const
		{
			return Proxy.IsValid();
		}

		const FWaveTableBankAssetProxyPtr& GetProxy() const
		{
			return Proxy;
		}

		const FWaveTableBankAssetProxy* operator->() const
		{
			return Proxy.Get();
		}

		FWaveTableBankAssetProxy* operator->()
		{
			return Proxy.Get();
		}
	};

	// Disable arrays of WaveTables (as of addition, not supported by UX). Must be defined prior to DataType declaration macro below.
	template<>
	struct TEnableAutoArrayTypeRegistration<WaveTable::FWaveTable>
	{
		static constexpr bool Value = false;
	};

	DECLARE_METASOUND_DATA_REFERENCE_TYPES(WaveTable::FWaveTable, METASOUNDENGINE_API, FWaveTableTypeInfo, FWaveTableReadRef, FWaveTableWriteRef)
	DECLARE_METASOUND_DATA_REFERENCE_TYPES(FWaveTableBankAsset, METASOUNDENGINE_API, FWaveTableBankAssetTypeInfo, FWaveTableBankAssetReadRef, FWaveTableBankAssetWriteRef)
} // namespace Metasound

