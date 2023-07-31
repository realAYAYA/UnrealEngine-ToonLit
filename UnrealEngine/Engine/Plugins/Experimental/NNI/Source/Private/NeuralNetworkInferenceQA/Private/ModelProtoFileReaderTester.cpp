// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelProtoFileReaderTester.h"
#include "ModelProtoFileReader.h"
#include "NeuralNetworkInferenceQAUtils.h"



/* FModelProtoFileReaderTester public functions
 *****************************************************************************/

bool FModelProtoFileReaderTester::Test(const FString& InONNXModelFileName)
{
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("-------------------- Read ONNX Network And Test"));

#if WITH_EDITOR
	FModelProto ModelProto;
	FModelProtoFileReader::ReadModelProtoFromFile(ModelProto, InONNXModelFileName);
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("ModelProto:\n%s"), *ModelProto.ToString());
	ensureMsgf(ModelProto.IsLoaded(), TEXT("FModelProtoFileReaderTester::ONNXReadNetworkTest() failed, FModelProto could not be read from InONNXModelFileName: %s."), *InONNXModelFileName);
	return ModelProto.IsLoaded();
#else //WITH_EDITOR
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("ONNXReadNetworkTest test skipped (only if WITH_EDITOR enabled)."));
	return true;
#endif //WITH_EDITOR
}
