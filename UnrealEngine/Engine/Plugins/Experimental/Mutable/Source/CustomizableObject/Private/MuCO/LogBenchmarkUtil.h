// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "HAL/Platform.h"
#include "UObject/NameTypes.h"

class LogBenchmarkUtil
{
public:
	~LogBenchmarkUtil();
	static void startLogging();
	static void shutdownAndSaveResults();
	static void updateStat(const FName& stat, int32 value);
	static void updateStat(const FName& stat, double value);
	static void updateStat(const FName& stat, long double value);
	static bool isLoggingActive();

private:
	struct IntStat {
		int32 total;
		int32 max;
		int totalCalls;

		IntStat& operator+=(const int32& rhs)
		{
			total += rhs;
			max = FGenericPlatformMath::Max(max, rhs);
			totalCalls++;
			return *this;
		}
	};

	struct FloatStat {
		long double total;
		long double max;
		int totalCalls;

		FloatStat& operator+=(const long double& rhs)
		{
			total += rhs;
			max = FGenericPlatformMath::Max(max, rhs);
			totalCalls++;
			return *this;
		}
	};

	static bool bLoggingActive;
	TMap<FName, IntStat> IntStats;
	TMap<FName, FloatStat> FloatStats;


	static LogBenchmarkUtil &getInstance() {
		static LogBenchmarkUtil instance;
		return instance;
	}

	LogBenchmarkUtil();
};
