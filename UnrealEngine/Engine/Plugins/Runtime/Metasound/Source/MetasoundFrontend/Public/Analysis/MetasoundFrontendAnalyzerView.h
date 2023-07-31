// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Analysis/MetasoundFrontendVertexAnalyzer.h"
#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "MetasoundAssetBase.h"
#include "MetasoundRouter.h"
#include "Templates/UniquePtr.h"


// Forward Declarations
class FMetasoundAssetBase;
class UAudioComponent;

namespace Metasound
{
	// Forward Declarations
	class FOperatorSettings;

	namespace Frontend
	{
		// Pairs an IReceiver with a given AnalyzerAddress, which enables
		// watching a particular analyzer result on any given thread.
		class METASOUNDFRONTEND_API FMetasoundAnalyzerView
		{
			TMap<FName, TSharedPtr<IReceiver>> OutputReceivers;

		public:
			const FAnalyzerAddress AnalyzerAddress = { };

			FMetasoundAnalyzerView() = default;
			FMetasoundAnalyzerView(FAnalyzerAddress&& InAnalyzerAddress);

			void BindToAllOutputs(const FOperatorSettings& InOperatorSettings);
			bool UnbindOutput(FName InOutputName);

			template <typename DataType>
			bool TryGetOutputData(FName InOutputName, DataType& OutValue)
			{
				TSharedPtr<IReceiver>* Receiver = OutputReceivers.Find(InOutputName);
				if (Receiver && Receiver->IsValid())
				{
					TReceiver<DataType>& TypedReceiver = (*Receiver)->GetAs<TReceiver<DataType>>();
					if (TypedReceiver.CanPop())
					{
						TypedReceiver.Pop(OutValue);
						return true;
					}
				}

				return false;
			}
		};
	}
}
