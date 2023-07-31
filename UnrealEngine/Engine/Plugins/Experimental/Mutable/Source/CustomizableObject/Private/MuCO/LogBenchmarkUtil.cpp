// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/LogBenchmarkUtil.h"

#include "Containers/UnrealString.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "MuCO/UnrealPortabilityHelpers.h"

bool LogBenchmarkUtil::bLoggingActive = false;

LogBenchmarkUtil::~LogBenchmarkUtil() {}

void LogBenchmarkUtil::startLogging()
{
	bLoggingActive = true;
}

void LogBenchmarkUtil::shutdownAndSaveResults()
{
	if (bLoggingActive)
	{
		FString SaveDirectory = Helper_GetSavedDir() + "/Logs";
		FString FileName = FString("BenchmarkResult.txt");
		FString AbsoluteFilePath = SaveDirectory + "/" + FileName;
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.CreateDirectoryTree(*SaveDirectory))
		{
			FString currentTime = FDateTime::Now().ToString();
			FString DateStamp = LINE_TERMINATOR + FString("[" + currentTime + "]LogBenchmark: (string) ") + FString("benchmark_results_location : ") + SaveDirectory;
			FFileHelper::SaveStringToFile(DateStamp, *AbsoluteFilePath, FFileHelper::EEncodingOptions::ForceAnsi);

			FString keyName;
			int32 maxValue;
			float averageValue;

			// Iterate over the integer stats and log them to a file
			for (TPair<FName, IntStat> ist : getInstance().IntStats)
			{
				currentTime = FDateTime::Now().ToString();

				keyName = FString(ist.Key.ToString());
				maxValue = ist.Value.max;
				averageValue = (long double)ist.Value.total / (long double)ist.Value.totalCalls;

				FString TextToSave = LINE_TERMINATOR;
				TextToSave += FString("[" + currentTime + "]");
				TextToSave += FString("LogBenchmark: (int) peak_");
				TextToSave += FString::Printf(TEXT("%s : %lld"), *keyName, maxValue);
				TextToSave += LINE_TERMINATOR;
				TextToSave += FString("[" + currentTime + "]");
				TextToSave += FString("LogBenchmark: (float) average_");
				TextToSave += FString::Printf(TEXT("%s : %f"), *keyName, averageValue);

				FFileHelper::SaveStringToFile(TextToSave, *AbsoluteFilePath, FFileHelper::EEncodingOptions::ForceAnsi, &IFileManager::Get(), FILEWRITE_Append);
			}

			// Iterate over the Float stats and log them to a file
			for (TPair<FName, FloatStat> dst : getInstance().FloatStats)
			{
				currentTime = FDateTime::Now().ToString();

				keyName = FString(dst.Key.ToString());
				maxValue = dst.Value.max;
				averageValue = dst.Value.total / dst.Value.totalCalls;

				FString TextToSave = LINE_TERMINATOR;
				TextToSave += FString("[" + currentTime + "]");
				TextToSave += FString("LogBenchmark: (int) peak_");
				TextToSave += FString::Printf(TEXT("%s : %lld"), *keyName, maxValue);
				TextToSave += LINE_TERMINATOR;
				TextToSave += FString("[" + currentTime + "]");
				TextToSave += FString("LogBenchmark: (float) average_");
				TextToSave += FString::Printf(TEXT("%s : %f"), *keyName, averageValue);

				FFileHelper::SaveStringToFile(TextToSave, *AbsoluteFilePath, FFileHelper::EEncodingOptions::ForceAnsi, &IFileManager::Get(), FILEWRITE_Append);
			}
		}
		bLoggingActive = false;
	}
}

void LogBenchmarkUtil::updateStat(const FName& stat, int32 value)
{
	if (bLoggingActive)
	{
		if (getInstance().IntStats.Contains(stat))
		{
			getInstance().IntStats[stat] += value;
		}
		else
		{
			getInstance().IntStats.Add(stat) += value;
		}
	}
}

void LogBenchmarkUtil::updateStat(const FName & stat, double value)
{
	updateStat(stat, (long double)value);
}

void LogBenchmarkUtil::updateStat(const FName& stat, long double value)
{
	if (bLoggingActive)
	{
		if (getInstance().FloatStats.Contains(stat))
		{
			getInstance().FloatStats[stat] += value;
		}
		else
		{
			getInstance().FloatStats.Add(stat) += value;
		}
	}
}

bool LogBenchmarkUtil::isLoggingActive()
{
	return bLoggingActive;
}

LogBenchmarkUtil::LogBenchmarkUtil() {}
