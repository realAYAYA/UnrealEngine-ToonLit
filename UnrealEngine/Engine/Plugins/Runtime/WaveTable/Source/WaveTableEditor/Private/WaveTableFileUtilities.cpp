// Copyright Epic Games, Inc. All Rights Reserved.
#include "WaveTableFileUtilities.h"

#include "Factories/SoundFactory.h"
#include "HAL/Platform.h"
#include "Sound/SoundWave.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "Utils.h"
#include "WaveTableSampler.h"


namespace WaveTable
{
	namespace Editor
	{
		namespace FileUtilities
		{
			void LoadPCMChannel(const FString& InFilePath, int32 InChannelIndex, TArray<float>& OutPCMData)
			{
				OutPCMData.Empty();

				if (!InFilePath.IsEmpty())
				{
					if (USoundFactory* SoundWaveFactory = NewObject<USoundFactory>())
					{
						// Setup sane defaults for importing localized sound waves
						SoundWaveFactory->bAutoCreateCue = false;
						SoundWaveFactory->SuppressImportDialogs();

						UPackage* TempPackage = CreatePackage(TEXT("/Temp/WaveTable"));
						if (USoundWave* SoundWave = ImportObject<USoundWave>(TempPackage, "ImportedWaveTable", RF_Public | RF_Standalone, *InFilePath, nullptr, SoundWaveFactory))
						{
							TArray<uint8> RawPCMData;
							uint32 SampleRate = 0;
							uint16 NumChannels = 0;
							SoundWave->GetImportedSoundWaveData(RawPCMData, SampleRate, NumChannels);

							if (NumChannels > 0)
							{
								const int32 NumFrames = RawPCMData.Num() / NumChannels / sizeof(int16);
								OutPCMData.Empty();
								OutPCMData.AddZeroed(NumFrames);
								const int16* RawDataPtr = (const int16*)(RawPCMData.GetData());
								const int32 ChannelOffset = (InChannelIndex % NumChannels);
								constexpr float Max16BitAsFloat = static_cast<float>(TNumericLimits<int16>::Max());
								for (int32 i = 0; i < OutPCMData.Num(); ++i)
								{
									OutPCMData[i] = (RawDataPtr[(i * NumChannels) + ChannelOffset]) / Max16BitAsFloat;
								}
							}
						}
					}
				}
			}
		} // namespace FileUtilities
	} // namespace Editor
} // namespace WaveTable
