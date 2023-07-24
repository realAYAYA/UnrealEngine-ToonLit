// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNECore.h"
#include "NNECoreTypes.h"

namespace UE::NNECore::Internal
{
	template <class ModelInterface> class FModelBase : public ModelInterface
	{
	public:

		virtual ~FModelBase() = default;

		virtual TConstArrayView<NNECore::FTensorDesc> GetInputTensorDescs() const override;
		virtual TConstArrayView<NNECore::FTensorDesc> GetOutputTensorDescs() const override;
		virtual TConstArrayView<NNECore::FTensorShape> GetInputTensorShapes() const override;
		virtual TConstArrayView<NNECore::FTensorShape> GetOutputTensorShapes() const override;
		virtual int SetInputTensorShapes(TConstArrayView<NNECore::FTensorShape> InInputShapes) override;

	protected:

		FModelBase() {}

		TArray<FTensorShape>	InputTensorShapes;
		TArray<FTensorShape>	OutputTensorShapes;
		TArray<FTensorDesc>		InputSymbolicTensors;
		TArray<FTensorDesc>		OutputSymbolicTensors;
	};

	template <class T> TConstArrayView<FTensorDesc> FModelBase<T>::GetInputTensorDescs() const
	{
		return InputSymbolicTensors;
	}

	template <class T> TConstArrayView<FTensorDesc> FModelBase<T>::GetOutputTensorDescs() const
	{
		return OutputSymbolicTensors;
	}

	template <class T> TConstArrayView<FTensorShape> FModelBase<T>::GetInputTensorShapes() const
	{
		return InputTensorShapes;
	}

	template <class T> TConstArrayView<FTensorShape> FModelBase<T>::GetOutputTensorShapes() const
	{
		return OutputTensorShapes;
	}

	template <class T> int FModelBase<T>::SetInputTensorShapes(TConstArrayView<FTensorShape> InInputShapes)
	{
		InputTensorShapes.Reset(InInputShapes.Num());

		if (InInputShapes.Num() != InputSymbolicTensors.Num())
		{
			UE_LOG(LogNNE, Warning, TEXT("Number of input shapes does not match number of input tensors"));
			return -1;
		}

		for (int32 i = 0; i < InInputShapes.Num(); ++i)
		{
			const FTensorDesc SymbolicDesc = InputSymbolicTensors[i];
			if (!InInputShapes[i].IsCompatibleWith(SymbolicDesc.GetShape()))
			{
				UE_LOG(LogNNE, Warning, TEXT("Input shape does not match input tensor %s of index %d"), *SymbolicDesc.GetName(), i);
				return -1;
			}
		}

		InputTensorShapes = InInputShapes;

		//Implementations are responsible to handle output and intermediate tensor shape inference.
		//This base implementation only validate that all inputs are matching what the model can support.
		return 0;
	}

} // namespace UE::NNECore::Internal