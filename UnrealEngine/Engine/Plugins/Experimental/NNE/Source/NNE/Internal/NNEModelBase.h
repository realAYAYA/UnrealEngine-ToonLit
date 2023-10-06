// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNE.h"
#include "NNETypes.h"

namespace UE::NNE::Internal
{
	template <class ModelInterface> class FModelInstanceBase : public ModelInterface
	{
	public:

		virtual ~FModelInstanceBase() = default;

		virtual TConstArrayView<NNE::FTensorDesc> GetInputTensorDescs() const override;
		virtual TConstArrayView<NNE::FTensorDesc> GetOutputTensorDescs() const override;
		virtual TConstArrayView<NNE::FTensorShape> GetInputTensorShapes() const override;
		virtual TConstArrayView<NNE::FTensorShape> GetOutputTensorShapes() const override;
		virtual int SetInputTensorShapes(TConstArrayView<NNE::FTensorShape> InInputShapes) override;

	protected:

		FModelInstanceBase() {}

		TArray<FTensorShape>	InputTensorShapes;
		TArray<FTensorShape>	OutputTensorShapes;
		TArray<FTensorDesc>		InputSymbolicTensors;
		TArray<FTensorDesc>		OutputSymbolicTensors;
	};

	template <class T> TConstArrayView<FTensorDesc> FModelInstanceBase<T>::GetInputTensorDescs() const
	{
		return InputSymbolicTensors;
	}

	template <class T> TConstArrayView<FTensorDesc> FModelInstanceBase<T>::GetOutputTensorDescs() const
	{
		return OutputSymbolicTensors;
	}

	template <class T> TConstArrayView<FTensorShape> FModelInstanceBase<T>::GetInputTensorShapes() const
	{
		return InputTensorShapes;
	}

	template <class T> TConstArrayView<FTensorShape> FModelInstanceBase<T>::GetOutputTensorShapes() const
	{
		return OutputTensorShapes;
	}

	template <class T> int FModelInstanceBase<T>::SetInputTensorShapes(TConstArrayView<FTensorShape> InInputShapes)
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

} // namespace UE::NNE::Internal