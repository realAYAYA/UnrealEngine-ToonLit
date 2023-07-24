// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEHlslShadersInstanceNormalizationCS.h"
#include "NNECore.h"
#include "NNECoreTensor.h"

namespace UE::NNEHlslShaders::Internal
{
	namespace InstNormUtils
	{
		FIntVector GetGroupSize(EInstanceNormalizationAlgorithm Algorithm)
		{
			switch (Algorithm)
			{
			case EInstanceNormalizationAlgorithm::Simple1x265: 			return {    1, 256, 1 };
			case EInstanceNormalizationAlgorithm::SharedMemory256x1:	return {  256,   1, 1 };
			case EInstanceNormalizationAlgorithm::SharedMemory512x1:	return {  512,   1, 1 };

			// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-attributes-numthreads
			case EInstanceNormalizationAlgorithm::SharedMemory768x1:	return {  768,   1, 1 }; // Compute shader version 4.X
			case EInstanceNormalizationAlgorithm::SharedMemory1024x1:	return { 1024,   1, 1 }; // Compute shader version 5.0
			}

			check(false);
			return {1, 256, 1};
		}
	}

	void TInstanceNormalizationCS::FillInParameters(float Epsilon, const  UE::NNECore::Internal::FTensor& Input, TInstanceNormalizationCS::FParameters& Parameters)
	{
		check(Input.GetShape().Rank() >= 3);
		
		Parameters.Epsilon = Epsilon;
		const int32 N = Input.GetShape().GetData()[0];
		Parameters.C = Input.GetShape().GetData()[1];
		Parameters.NxC = N * Parameters.C;
		Parameters.W = Input.GetShape().Volume() / (Parameters.NxC);
	}

	FIntVector TInstanceNormalizationCS::GetGroupCount(const TInstanceNormalizationCS::FParameters& Parameters, EInstanceNormalizationAlgorithm Algorithm)
	{
		int ThreadGroupCountValueY = FMath::DivideAndRoundUp((int32)Parameters.NxC, InstNormUtils::GetGroupSize(Algorithm).Y);

		return {1, ThreadGroupCountValueY, 1};
	}

	EInstanceNormalizationAlgorithm TInstanceNormalizationCS::GetAlgorithm(const TInstanceNormalizationCS::FParameters& Parameters)
	{
		return EInstanceNormalizationAlgorithm::SharedMemory768x1; // note: not all platforms support higher number of threads (yet)
	}

	void TInstanceNormalizationCS::LexFromString(EInstanceNormalizationAlgorithm& OutValue, const TCHAR* StringVal)
	{
		OutValue = EInstanceNormalizationAlgorithm::MAX;
		if (FCString::Stricmp(StringVal, TEXT("Simple1x265")) == 0) OutValue = EInstanceNormalizationAlgorithm::Simple1x265;
		else if (FCString::Stricmp(StringVal, TEXT("SharedMemory256x1")) == 0) OutValue = EInstanceNormalizationAlgorithm::SharedMemory256x1;
		else if (FCString::Stricmp(StringVal, TEXT("SharedMemory512x1")) == 0) OutValue = EInstanceNormalizationAlgorithm::SharedMemory512x1;
		else if (FCString::Stricmp(StringVal, TEXT("SharedMemory768x1")) == 0) OutValue = EInstanceNormalizationAlgorithm::SharedMemory768x1;
		else if (FCString::Stricmp(StringVal, TEXT("SharedMemory1024x1")) == 0) OutValue = EInstanceNormalizationAlgorithm::SharedMemory1024x1;
	}

	IMPLEMENT_GLOBAL_SHADER(TInstanceNormalizationCS, "/NNE/NNEHlslShadersInstanceNormalization.usf", "InstanceNormalization", SF_Compute);
} // UE::NNEHlslShaders::Internal