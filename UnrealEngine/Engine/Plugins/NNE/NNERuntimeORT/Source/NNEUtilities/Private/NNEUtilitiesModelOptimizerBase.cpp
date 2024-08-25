// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEUtilitiesModelOptimizerBase.h"

#include "NNE.h"
#include "NNERuntimeFormat.h"

#include "NNEUtilitiesThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT

#include "onnx/common/common.h"
#include "onnx/checker.h"
#include "onnx/proto_utils.h"

NNE_THIRD_PARTY_INCLUDES_END



namespace UE::NNEUtilities::Internal
{

FString FModelValidatorONNX::GetName() const
{
	return TEXT("ONNX Model validator");
}

bool FModelValidatorONNX::ValidateModel(const FNNEModelRaw& InputModel, const FOptimizerOptionsMap& Options) const
{
	if (InputModel.Format != ENNEInferenceFormat::ONNX)
	{
		return true;
	}

	onnx::ModelProto Model;
	onnx::ParseProtoFromBytes(&Model, reinterpret_cast<const char*>(InputModel.Data.GetData()), static_cast<size_t>(InputModel.Data.Num()));

#ifdef ONNX_NO_EXCEPTIONS
	static_assert(false, "ONNX_NO_EXCEPTIONS is defined meaning onnx check_model would abort the program in case of validation failure.");
#else
	try
	{
		onnx::checker::check_model(Model);
	}
	catch (onnx::checker::ValidationError& e)
	{
		UE_LOG(LogNNE, Warning, TEXT("Input model is invalid : %s."), ANSI_TO_TCHAR(e.what()));
		return false;
	}
#endif

	return true;
}

bool FModelOptimizerBase::IsModelValid(const FNNEModelRaw& ModelToValidate, const FOptimizerOptionsMap& Options)
{
	bool bIsModelValid = true;

	for (TSharedPtr<NNE::Internal::IModelValidator>& Validator : Validators)
	{
		check(Validator.IsValid());
		if (!Validator->ValidateModel(ModelToValidate, Options))
		{
			UE_LOG(LogNNE, Warning, TEXT("Model validator '%s' detected an error."), *(Validator->GetName()));
			bIsModelValid = false;
		}
	}
	return bIsModelValid;
}

bool FModelOptimizerBase::ApplyAllPassesAndValidations(FNNEModelRaw& OptimizedModel, const FOptimizerOptionsMap& Options)
{
	if (!IsModelValid(OptimizedModel, Options))
	{
		UE_LOG(LogNNE, Warning, TEXT("Model is not valid."));
		return false;
	}
		
	for (TSharedPtr<NNE::Internal::IModelOptimizerPass>& Pass : OptimizationPasses)
	{
		check(Pass.IsValid());

		//Note: Usefull to enable for debug purpose
		//FFileHelper::SaveArrayToFile(OptimizedModel.Data, TEXT("D:\\OnnxBeforePass.onnx"));
			
		if (!Pass->ApplyPass(OptimizedModel, Options))
		{
			UE_LOG(LogNNE, Warning, TEXT("Error while executing model optimisation pass '%s'."), *(Pass->GetName()));
			return false;
		}

		//Note: Usefull to enable for debug purpose
		//FFileHelper::SaveArrayToFile(OptimizedModel.Data, TEXT("D:\\OnnxAfterPass.onnx"));

		if (!IsModelValid(OptimizedModel, Options))
		{
			UE_LOG(LogNNE, Warning, TEXT("Model validation failed after optimisation pass '%s'."), *(Pass->GetName()));
			return false;
		}
	}

	return true;
}

void FModelOptimizerBase::AddOptimizationPass(TSharedPtr<NNE::Internal::IModelOptimizerPass> ModelOptimizerPass)
{
	if (ModelOptimizerPass.IsValid())
	{
		OptimizationPasses.Add(ModelOptimizerPass);
	}
}

void FModelOptimizerBase::AddValidator(TSharedPtr<NNE::Internal::IModelValidator> ModelValidator)
{
	if (ModelValidator.IsValid())
	{
		Validators.Add(ModelValidator);
	}
}

bool FModelOptimizerBase::Optimize(const FNNEModelRaw& InputModel, FNNEModelRaw& OptimizedModel, const FOptimizerOptionsMap& Options)
{
	OptimizedModel = InputModel;
	return ApplyAllPassesAndValidations(OptimizedModel, Options);
}

} // namespace UE::NNEUtilities::Internal
