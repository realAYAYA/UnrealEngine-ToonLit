// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEQAUtils.h"

#include "HAL/UnrealMemory.h"
#include "Kismet/GameplayStatics.h"
#include "NNECore.h"
#include "NNECoreAttributeMap.h"
#include "NNECoreModelData.h"
#include "NNECoreModelOptimizerInterface.h"
#include "NNECoreRuntime.h"
#include "NNECoreRuntimeCPU.h"
#include "NNECoreRuntimeRDG.h"
#include "NNEQAModel.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakInterfacePtr.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"

namespace UE::NNEQA::Private 
{
	static void FillTensors(
		TConstArrayView<NNECore::Internal::FTensor> TensorsFromTestSetup,
		TConstArrayView<NNECore::FTensorDesc> TensorDescsFromModel,
		TArray<NNECore::Internal::FTensor>& OutTensors)
	{
		OutTensors = TensorsFromTestSetup;
		if (OutTensors.IsEmpty())
		{
			for (const NNECore::FTensorDesc& SymbolicTensorDesc : TensorDescsFromModel)
			{
				NNECore::Internal::FTensor TensorDesc = NNECore::Internal::FTensor::MakeFromSymbolicDesc(SymbolicTensorDesc);
				OutTensors.Emplace(TensorDesc);
			}
		}
	}
	
	ElementWiseCosTensorInitializer::ElementWiseCosTensorInitializer(ENNETensorDataType InDataType, uint32 InTensorIndex)
	: DataType(InDataType), TensorIndex(InTensorIndex)
	{
	}

	float ElementWiseCosTensorInitializer::operator() (uint32 ElementIndex) const 
	{
		const constexpr uint32 IndexOffsetBetweenTensor = 9;
		switch (DataType)
		{

		case ENNETensorDataType::Boolean:
			return (ElementIndex + IndexOffsetBetweenTensor * TensorIndex) % 2;
		case ENNETensorDataType::Char:
		case ENNETensorDataType::Int8:
		case ENNETensorDataType::Int16:
		case ENNETensorDataType::Int32:
		case ENNETensorDataType::Int64:
			return 10.0f * FMath::Cos((float)(ElementIndex + IndexOffsetBetweenTensor * TensorIndex));
		case ENNETensorDataType::UInt8:
		case ENNETensorDataType::UInt16:
		case ENNETensorDataType::UInt32:
		case ENNETensorDataType::UInt64:
			return 10.0f * FMath::Abs(FMath::Cos((float)(ElementIndex + IndexOffsetBetweenTensor * TensorIndex)));
		default:
			//case ENNETensorDataType::None:
			//case ENNETensorDataType::Half:
			//case ENNETensorDataType::Double:
			//case ENNETensorDataType::Float:
			//case ENNETensorDataType::Complex64:
			//case ENNETensorDataType::Complex128:
			//case ENNETensorDataType::BFloat16:
			return FMath::Cos((float)(ElementIndex + IndexOffsetBetweenTensor * TensorIndex));
		}
	};

	TArray<char> GenerateTensorDataForTest(const NNECore::Internal::FTensor& Tensor, std::function<float(uint32)> ElementInitializer)
	{
		const uint32 NumberOfElements = Tensor.GetVolume();
		const int32 ElementByteSize = Tensor.GetElemByteSize();
		const uint64 BufferSize = Tensor.GetDataSize();
		const ENNETensorDataType DataType = Tensor.GetDataType();
		TArray<char> Buffer;
			
		Buffer.SetNum(BufferSize);
		char* DestinationPtr = Buffer.GetData();

		for (uint32 i = 0; i != NumberOfElements; ++i)
		{
			const float FloatData = ElementInitializer(i);
			const int32 IntData = (int32)FloatData;
			const int64 Int64Data = (int64)FloatData;
			const uint32 UIntData = (uint32)FloatData;
			const char BoolData = FloatData != 0.0f ? 1 : 0;
			switch (DataType)
			{
			case ENNETensorDataType::Float:
				check(sizeof(FloatData) == ElementByteSize);
				FMemory::Memcpy(DestinationPtr, &FloatData, ElementByteSize); break;
			case ENNETensorDataType::Int32:
				check(sizeof(IntData) == ElementByteSize);
				FMemory::Memcpy(DestinationPtr, &IntData, ElementByteSize); break;
			case ENNETensorDataType::Int64:
				check(sizeof(Int64Data) == ElementByteSize);
				FMemory::Memcpy(DestinationPtr, &Int64Data, ElementByteSize); break;
			case ENNETensorDataType::UInt32:
				check(sizeof(UIntData) == ElementByteSize);
				FMemory::Memcpy(DestinationPtr, &UIntData, ElementByteSize); break;
			case ENNETensorDataType::Boolean:
				check(sizeof(BoolData) == ElementByteSize);
				FMemory::Memcpy(DestinationPtr, &BoolData, ElementByteSize); break;
			default:
				FMemory::Memzero(DestinationPtr, ElementByteSize);
			}
			DestinationPtr += ElementByteSize;
		}

		return Buffer;
	}

	static void FillInputTensorBindingsCPU(TConstArrayView<NNECore::Internal::FTensor> Tensors, TConstArrayView<FTests::FTensorData> TensorsData,
		TArray<TArray<char>>& OutMemBuffers, TArray<NNECore::FTensorBindingCPU>& OutBindings)
	{
		OutBindings.Reset();
		OutMemBuffers.Reset();
		check(TensorsData.Num() == 0 || Tensors.Num() == TensorsData.Num());

		for (int Index = 0; Index < Tensors.Num(); ++Index)
		{
			const NNECore::Internal::FTensor& Tensor = Tensors[Index];
			TArray<char>& MemBuffer = OutMemBuffers.Emplace_GetRef();
			if (TensorsData.Num() == 0 || TensorsData[Index].Num() == 0)
			{
				ElementWiseCosTensorInitializer Initializer(Tensor.GetDataType(), Index);
				MemBuffer = GenerateTensorDataForTest(Tensor, Initializer);
			}
			else
			{
				MemBuffer = TensorsData[Index];
			}
			check(MemBuffer.Num() == Tensor.GetDataSize());
			
			OutBindings.Emplace(NNECore::FTensorBindingCPU{(void*)MemBuffer.GetData(),(uint64)MemBuffer.Num()} );
		}
	}

	static void FillOutputTensorBindingsCPU(TConstArrayView<NNECore::Internal::FTensor> Tensors, TArray<TArray<char>>& OutMemBuffers, TArray<NNECore::FTensorBindingCPU>& OutBindings)
	{
		OutBindings.Reset();
		OutMemBuffers.Reset();
		for (int Index = 0; Index < Tensors.Num(); ++Index)
		{
			TArray<char>& MemBuffer = OutMemBuffers.Emplace_GetRef();
			MemBuffer.SetNumUninitialized(Tensors[Index].GetDataSize());
			char constexpr MagicNumber = 0x5b;
			FMemory::Memset(MemBuffer.GetData(), MagicNumber, MemBuffer.Num());
			OutBindings.Emplace(NNECore::FTensorBindingCPU{ (void*)MemBuffer.GetData(),(uint64)MemBuffer.Num() });
		}
	}

	template<typename T> FString ShapeToString(TConstArrayView<T> Shape)
	{
		FString TestSuffix(TEXT("["));
		bool bIsFirstDim = true;
		for (T size : Shape)
		{
			if (!bIsFirstDim) TestSuffix += TEXT(",");
			TestSuffix += FString::Printf(TEXT("%d"), size);
			bIsFirstDim = false;
		}
		TestSuffix += TEXT("]");
		return TestSuffix;
	}
	template FString ShapeToString<int32>(TConstArrayView<int32> Shape);
	template FString ShapeToString<uint32>(TConstArrayView<uint32> Shape);

	FString TensorToString(const NNECore::Internal::FTensor& desc)
	{
		TArrayView<const uint32> Shape(desc.GetShape().GetData());

		FString TensorDesc;
		TensorDesc.Reserve(50);
		TensorDesc += FString::Printf(TEXT("Name: %s, Shape: "), *desc.GetName());
		TensorDesc += ShapeToString(Shape);
		const FString& DataTypeName = StaticEnum<ENNETensorDataType>()->GetNameStringByValue(static_cast<int64>(desc.GetDataType()));
		TensorDesc += FString::Printf(TEXT(" DataType: %s"), *DataTypeName);

		return TensorDesc;
	}

	FString TensorToString(const NNECore::Internal::FTensor& TensorDesc, TConstArrayView<char> TensorData)
	{
		FString TensorLog;
		TensorLog.Reserve(50);

		TensorLog += TensorToString(TensorDesc);
		TensorLog += TEXT(", Data: ");

		const constexpr uint32 MAX_DATA_TO_LOG = 10;
		const uint32 MaxIndex = FMath::Min(MAX_DATA_TO_LOG, TensorDesc.GetVolume());
        const uint32 ElementByteSize = TensorDesc.GetElemByteSize();
		for (uint32 i = 0; i < MaxIndex; ++i)
		{
			const uint32 ByteOffset = i * ElementByteSize;
			const char* Data = TensorData.GetData() + ByteOffset;
			check((int32)ByteOffset <= TensorData.Num());

			switch (TensorDesc.GetDataType())
			{
				case ENNETensorDataType::Float:
					TensorLog += FString::Printf(TEXT("%0.2f"), *(float*)Data); break;
				case ENNETensorDataType::Int32:
					TensorLog += FString::Printf(TEXT("%d"), *(int32*)Data); break;
				case ENNETensorDataType::UInt32:
					TensorLog += FString::Printf(TEXT("%u"), *(uint32*)Data); break;
				case ENNETensorDataType::Boolean:
					check(ElementByteSize == 1);
					TensorLog += FString::Printf(TEXT("%s"), *(bool*)Data ? "true" : "false"); break;
				default:
					TensorLog += TEXT("?");
			}
			if (i < MaxIndex)
				TensorLog += TEXT(", ");
		}
		if (MaxIndex < TensorDesc.GetVolume())
			TensorLog += TEXT(",...");

		return TensorLog;
	}

	template<typename T> bool CompareTensorData(
		const NNECore::Internal::FTensor& RefTensorDesc,   const TArray<char>& RefRawBuffer,
		const NNECore::Internal::FTensor& OtherTensorDesc, const TArray<char>& OtherRawBuffer,
		float AbsoluteTolerance, float RelativeTolerance)
	{

		const T* RefBuffer = (T*)RefRawBuffer.GetData();
		const T* OtherBuffer = (T*)OtherRawBuffer.GetData();
		
		const uint32 Volume = RefTensorDesc.GetVolume();
		const uint32 ElementByteSize = RefTensorDesc.GetElemByteSize();

		check(Volume == OtherTensorDesc.GetVolume());
		check(Volume * ElementByteSize == RefRawBuffer.Num());
		check(Volume * ElementByteSize == OtherRawBuffer.Num());

		bool bTensorMemMatch = true;

		float WorstAbsoluteError = 0.0f;
		int32 WorstAbsoluteErrorIndex = -1;
		float WorstAbsoluteErrorRef = 0.0f;
		float WorstAbsoluteErrorOther = 0.0f;

		float WorstRelativeError = 0.0f;
		int32 WorstRelativeErrorIndex = -1;
		float WorstRelativeErrorRef = 0.0f;
		float WorstRelativeErrorOther = 0.0f;

		int32 NumExtraNaNsInResults = 0;
		int32 FirstExtraNaNIndex = -1;
		int32 NumMissingNaNsInResults = 0;
		int32 FirstMissingNaNIndex = -1;

		for (uint32 i = 0; i < Volume; ++i)
		{
			//All type are converted to float for comparison purpose
			const float Result = (float)OtherBuffer[i];
			const float Reference = (float)RefBuffer[i];

			
			if (FMath::IsNaN(Result) && !FMath::IsNaN(Reference))
			{
				bTensorMemMatch = false;
				++NumExtraNaNsInResults;
				if (FirstExtraNaNIndex == -1)
				{
					FirstExtraNaNIndex = i;
				}
			}
			if (!FMath::IsNaN(Result) && FMath::IsNaN(Reference))
			{
				bTensorMemMatch = false;
				++NumMissingNaNsInResults;
				if (FirstMissingNaNIndex == -1)
				{
					FirstMissingNaNIndex = i;
				}
			}
			
			if (FMath::IsNaN(Result) || FMath::IsNaN(Reference))
			{
				continue;
			}
			const float AbsoluteError = FMath::Abs<float>(Result - Reference);
			const float AbsoluteRef = FMath::Abs<float>(Reference);
			const float AbsoluteThreshold = AbsoluteTolerance + RelativeTolerance * AbsoluteRef;
			/* from https://numpy.org/doc/stable/reference/generated/numpy.isclose.html */
			if(AbsoluteError > AbsoluteThreshold)
			{
				bTensorMemMatch = false;
				// Depending on dominating term, contribute to different statistics
				if(AbsoluteError > RelativeTolerance * AbsoluteRef)
				{
					WorstRelativeError = AbsoluteError / AbsoluteRef;
					WorstRelativeErrorIndex = i;
					WorstRelativeErrorRef = Reference;
					WorstRelativeErrorOther = Result;
				}
				else
				{
					WorstAbsoluteError = AbsoluteError;
					WorstAbsoluteErrorIndex = i;
					WorstAbsoluteErrorRef = Reference;
					WorstAbsoluteErrorOther = Result;
				}
			}
		}

		if (!bTensorMemMatch)
		{
			FString AbsoluteErrorMessage = WorstAbsoluteErrorIndex == -1 ? FString() : FString::Printf(
				TEXT("LogNNE: Worst absolute tolerance violation %.3e (tol %.3e) at position %d, got %.3e expected %.3e\n"),
				WorstAbsoluteError, AbsoluteTolerance, WorstAbsoluteErrorIndex, WorstAbsoluteErrorOther, WorstAbsoluteErrorRef
			);
			FString RelativeErrorMessage = WorstRelativeErrorIndex == -1 ? FString() : FString::Printf(
				TEXT("LogNNE: Worst relative tolerance violation %.3e (tol %.3e) at position %d, got %.3e expected %.3e\n"), 
				WorstRelativeError, RelativeTolerance, WorstRelativeErrorIndex, WorstRelativeErrorOther, WorstRelativeErrorRef
			); 
			UE_LOG(LogNNE, Error, TEXT("Tensor data do not match.\n"
				"%s"
				"%s"
				"LogNNE: Num unexpected NaNs %d (First at index %d), Num missing NaNs %d (first at index %d)"),
				*AbsoluteErrorMessage,
				*RelativeErrorMessage,
				NumExtraNaNsInResults, FirstExtraNaNIndex, NumMissingNaNsInResults, FirstMissingNaNIndex);
			UE_LOG(LogNNE, Error, TEXT("   Expected : %s"), *TensorToString(RefTensorDesc, RefRawBuffer));
			UE_LOG(LogNNE, Error, TEXT("   But got  : %s"), *TensorToString(OtherTensorDesc, OtherRawBuffer));
			return false;
		}

		return true;
	}

	bool VerifyTensorResult(
		const NNECore::Internal::FTensor& RefTensor, const TArray<char>& RefRawBuffer,
		const NNECore::Internal::FTensor& OtherTensor, const TArray<char>& OtherRawBuffer,
		float AbsoluteTolerance, float RelativeTolerance)
	{
		if (RefTensor.GetShape() != OtherTensor.GetShape())
		{
			TArrayView<const uint32> RefShape(RefTensor.GetShape().GetData());
			TArrayView<const uint32> OtherShape(OtherTensor.GetShape().GetData());
			UE_LOG(LogNNE, Error, TEXT("Tensor shape do not match.\nExpected: %s\nGot:      %s"), *ShapeToString(RefShape), *ShapeToString(OtherShape));
			return false;
		}

		if (RefTensor.GetDataType() == ENNETensorDataType::Float)
		{
			return CompareTensorData<float>(RefTensor, RefRawBuffer, OtherTensor, OtherRawBuffer, AbsoluteTolerance, RelativeTolerance);
		}
		else if (RefTensor.GetDataType() == ENNETensorDataType::Boolean)
		{
			check(RefTensor.GetElemByteSize() == 1);
			return CompareTensorData<bool>(RefTensor, RefRawBuffer, OtherTensor, OtherRawBuffer, AbsoluteTolerance, RelativeTolerance);
		}
		else if (RefTensor.GetDataType() == ENNETensorDataType::Int32)
		{
			return CompareTensorData<int32>(RefTensor, RefRawBuffer, OtherTensor, OtherRawBuffer, AbsoluteTolerance, RelativeTolerance);
		}
		else if (RefTensor.GetDataType() == ENNETensorDataType::Int64)
		{
			return CompareTensorData<int64>(RefTensor, RefRawBuffer, OtherTensor, OtherRawBuffer, AbsoluteTolerance, RelativeTolerance);
		}
		else if (RefTensor.GetDataType() == ENNETensorDataType::UInt32)
		{
			return CompareTensorData<uint32>(RefTensor, RefRawBuffer, OtherTensor, OtherRawBuffer, AbsoluteTolerance, RelativeTolerance);
		}
		else
		{
			UE_LOG(LogNNE, Error, TEXT("Tensor comparison for this type of tensor not implemented"));
			return false;
		}

		return true;
	}

	bool RunTestInference(const FNNEModelRaw& ONNXModelData, const FTests::FTestSetup& TestSetup,
		const FString& RuntimeName, TArray<NNECore::Internal::FTensor>& OutOutputTensors, TArray<TArray<char>>& OutOutputMemBuffers)
	{
		OutOutputMemBuffers.Reset();

		TUniquePtr<FModelQA> InferenceModel = FModelQA::MakeModelQA(ONNXModelData, RuntimeName);
		if (!InferenceModel.IsValid())
		{
			UE_LOG(LogNNE, Error, TEXT("Could not create Inference model."));
			return false;
		}

		// If test does not ask for specific input/output, fill with model default converting variable dim to 1 if any.
		TArray<NNECore::Internal::FTensor> InputTensors;

		FillTensors(TestSetup.Inputs, InferenceModel->GetInputTensorDescs(), InputTensors);
		FillTensors(TestSetup.Outputs, InferenceModel->GetOutputTensorDescs(), OutOutputTensors);

		// bind tensors to memory (CPU) and initialize
		TArray<NNECore::FTensorBindingCPU> InputBindings;
		TArray<NNECore::FTensorBindingCPU> OutputBindings;
		TArray<TArray<char>> InputMemBuffers;

		FillInputTensorBindingsCPU(InputTensors, TestSetup.InputsData, InputMemBuffers, InputBindings);
		FillOutputTensorBindingsCPU(OutOutputTensors, OutOutputMemBuffers, OutputBindings);

		// To help for debugging sessions.
#ifdef UE_BUILD_DEBUG
		const constexpr int32 NumTensorPtrForDebug = 3;
		float* InputsAsFloat[NumTensorPtrForDebug];
		float* OutputsAsFloat[NumTensorPtrForDebug];
		FMemory::Memzero(InputsAsFloat, NumTensorPtrForDebug * sizeof(float*));
		FMemory::Memzero(OutputsAsFloat, NumTensorPtrForDebug * sizeof(float*));
		for (int32 i = 0; i < NumTensorPtrForDebug && i < InputMemBuffers.Num(); ++i)
		{
			InputsAsFloat[i] = (float*)InputMemBuffers[i].GetData();
		}
		for (int32 i = 0; i < NumTensorPtrForDebug && i < OutOutputMemBuffers.Num(); ++i)
		{
			OutputsAsFloat[i] = (float*)OutOutputMemBuffers[i].GetData();
		}
#endif //UE_BUILD_DEBUG

		TArray<NNECore::FTensorShape> InputShapes;
		for (const NNECore::Internal::FTensor& Tensor : InputTensors)
		{
			InputShapes.Emplace(Tensor.GetShape());
		}

		// Setup inputs
		if (0 != InferenceModel->SetInputTensorShapes(InputShapes))
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to set input tensor shapes."));
			return false;
		}

		// Run inference
		if (0 != InferenceModel->RunSync(InputBindings, OutputBindings))
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to run the model."));
			return false;
		}

		// Verify output shapes are as expected
		TConstArrayView<NNECore::FTensorShape> OutputShapes = InferenceModel->GetOutputTensorShapes();
		if (OutputShapes.Num() != OutOutputTensors.Num())
		{
			UE_LOG(LogNNE, Error, TEXT("Expected %d output tensors, got %d."), OutOutputTensors.Num(), OutputShapes.Num());
			return false;
		}
		for (int i = 0; i < OutOutputTensors.Num(); ++i)
		{
			const NNECore::FTensorShape& RefTensorShape = OutOutputTensors[i].GetShape();
			const NNECore::FTensorShape& OtherTensorShape = OutputShapes[i];
			if (RefTensorShape != OtherTensorShape)
			{
				TArrayView<const uint32> RefShape(RefTensorShape.GetData());
				TArrayView<const uint32> OtherShape(OtherTensorShape.GetData());
				UE_LOG(LogNNE, Error, TEXT("Output shape do not match at index %d.\nExpected: %s\nGot:      %s"), i, *ShapeToString(RefShape), *ShapeToString(OtherShape));
				return false;
			}
		}

		return true;
	}

	bool RunTestInferenceAndCompareToRef(const FTests::FTestSetup& TestSetup, const FString& RuntimeName, const FNNEModelRaw& ONNXModel,
		TArrayView<TArray<char>> RefOutputMemBuffers, TArrayView<NNECore::Internal::FTensor> RefOutputTensors)
	{
		TArray<TArray<char>> OutputMemBuffers;
		TArray<NNECore::Internal::FTensor> OutputTensors;

		const float AbsoluteTolerance = TestSetup.GetAbsoluteToleranceForRuntime(RuntimeName);
		const float RelativeTolerance = TestSetup.GetRelativeToleranceForRuntime(RuntimeName);
		bool bTestSuceeded = true;

		if (!RunTestInference(ONNXModel, TestSetup, RuntimeName, OutputTensors, OutputMemBuffers))
		{
			UE_LOG(LogNNE, Error, TEXT("Error running inference for runtime %s."), *RuntimeName);
			return false;
		}

		if (OutputTensors.Num() == RefOutputTensors.Num())
		{
			for (int i = 0; i < OutputTensors.Num(); ++i)
			{
				bTestSuceeded &= VerifyTensorResult(
					RefOutputTensors[i], RefOutputMemBuffers[i],
					OutputTensors[i], OutputMemBuffers[i],
					AbsoluteTolerance, RelativeTolerance);
			}
		}
		else
		{
			UE_LOG(LogNNE, Error, TEXT("Expecting %d output tensor(s), got %d."), RefOutputTensors.Num(), OutputTensors.Num());
			bTestSuceeded = false;
		}
		return bTestSuceeded;
	}

	bool CompareONNXModelInferenceAcrossRuntimes(const FNNEModelRaw& ONNXModel, const FNNEModelRaw& ONNXModelVariadic, const FTests::FTestSetup& TestSetup, const FString& RuntimeFilter)
	{
		FString CurrentPlatform = UGameplayStatics::GetPlatformName();
		if (TestSetup.AutomationExcludedPlatform.Contains(CurrentPlatform))
		{
			UE_LOG(LogNNE, Display, TEXT("Skipping test of '%s' for platform %s (by config)"), *TestSetup.TargetName, *CurrentPlatform);
			return true;
		}
		UE_LOG(LogNNE, Display, TEXT("Starting tests of '%s'"), *TestSetup.TargetName);

		// Reference runtime
		const FString& RefName = TEXT("NNERuntimeORTCpu");
		const float AbsoluteTolerance = TestSetup.GetAbsoluteToleranceForRuntime(RefName);
		const float RelativeTolerance = TestSetup.GetRelativeToleranceForRuntime(RefName);
		TArray<TArray<char>> RefOutputMemBuffers;
		TArray<NNECore::Internal::FTensor> RefOutputTensors;
		bool bAllTestsSucceeded = true;

		if (!RunTestInference(ONNXModel, TestSetup, RefName, RefOutputTensors, RefOutputMemBuffers))
		{
			UE_LOG(LogNNE, Error, TEXT("Error running reference inference with runtime %s."), *RefName);
			return false;
		}

		// If outputs data have been defined by test setup we check that reference match it
		for (int i = 0; i < TestSetup.OutputsData.Num(); ++i)
		{
			if (!TestSetup.OutputsData[i].IsEmpty())
			{
				bAllTestsSucceeded &= VerifyTensorResult(
					TestSetup.Outputs[i], TestSetup.OutputsData[i],
					RefOutputTensors[i], RefOutputMemBuffers[i],
					AbsoluteTolerance, RelativeTolerance);
			}
		}
		if (!bAllTestsSucceeded)
		{
			UE_LOG(LogNNE, Error, TEXT("Expecting output from test setup are no matched by reference runtime."));
		}

		for (auto Runtime : NNECore::GetAllRuntimes())
		{
			const FString& RuntimeName = Runtime->GetRuntimeName();
			if (RuntimeName == RefName)
			{
				continue;
			}
			if (!RuntimeFilter.IsEmpty() && !RuntimeFilter.Contains(RuntimeName))
			{
				continue;
			}
			FString TestResult;
			if (TestSetup.AutomationExcludedRuntime.Contains(RuntimeName) ||
				TestSetup.AutomationExcludedPlatformRuntimeCombination.Contains(TPair<FString, FString>(CurrentPlatform, RuntimeName)) ||
				(TestSetup.SkipStaticTestForRuntime.Contains(RuntimeName) && TestSetup.SkipVariadicTestForRuntime.Contains(RuntimeName)))
			{
				TestResult = TEXT("skipped (by config)");
			}
			else
			{
				bool bTestSuceeded = true;
				bool bShouldRunStaticTest = !TestSetup.SkipStaticTestForRuntime.Contains(RuntimeName);
				bool bShouldRunVariadicTest = !TestSetup.SkipVariadicTestForRuntime.Contains(RuntimeName);

				bShouldRunVariadicTest &= (ONNXModelVariadic.Format != ENNEInferenceFormat::Invalid);
				bShouldRunVariadicTest &= !(RuntimeName == "NNERuntimeRDGDml");
				
				if (bShouldRunStaticTest)
				{
					bool bTestResult = RunTestInferenceAndCompareToRef(TestSetup, Runtime->GetRuntimeName(), ONNXModel, RefOutputMemBuffers, RefOutputTensors);
					if (!bTestResult)
					{
						UE_LOG(LogNNE, Error, TEXT("Failed running static test."));
						bTestSuceeded = false;
					}
				}
				if (bShouldRunVariadicTest)
				{
					bool bTestResult = RunTestInferenceAndCompareToRef(TestSetup, Runtime->GetRuntimeName(), ONNXModelVariadic, RefOutputMemBuffers, RefOutputTensors);
					if (!bTestResult)
					{
						UE_LOG(LogNNE, Error, TEXT("Failed running variadic test."));
						bTestSuceeded = false;
					}
				}
				
				TestResult = bTestSuceeded ? TEXT("SUCCESS") : TEXT("FAILED");
				bAllTestsSucceeded &= bTestSuceeded;
			}

			UE_LOG(LogNNE, Display, TEXT("  %s tests: %s"), *RuntimeName, *TestResult);
		}
		
		return bAllTestsSucceeded;
	}
	FTests::FTestSetup& FTests::AddTest(const FString & Category, const FString & ModelOrOperatorName, const FString & TestSuffix)
	{
		FString TestName = Category + ModelOrOperatorName + TestSuffix;
		//Test name should be unique
		check(nullptr == TestSetups.FindByPredicate([TestName](const FTestSetup& Other) { return Other.TestName == TestName; }));
		TestSetups.Emplace(Category, ModelOrOperatorName, TestSuffix);
		return TestSetups.Last(0);
	}

} // namespace UE::NNEQA::Private