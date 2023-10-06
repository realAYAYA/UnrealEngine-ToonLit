// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Async/Async.h"
#include "Async/Future.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Math/NumericLimits.h"
#include "Misc/FrameRate.h"
#include "Misc/Timecode.h"
#include "Misc/Timespan.h"
#include "Tests/TestHarnessAdapter.h"


#if WITH_TESTS

/**
 * Run a suite of timecode conversion operations to validate conversion from timecode to timespan/FrameNumber are working
 *
 * Drop Frame drop a frame every minute except every 10th minute
 * 29.97fps
 * 00:58:01:28 ; 00:58:01:29 ; 00:58:02:00 ; 00:58:02:01 (no skip)
 * 01:00:59:28 ; 01:00:59:29 ; 01:01:00:02 ; 01:01:00:03 (every minute, we skip frame 0 and 1)
 * 01:09:59:28 ; 01:09:59:29 ; 01:10:00:00 ; 01:10:00:01 (except every 10th minute, we include frame 0 and 1)
 */
TEST_CASE_NAMED(FTimecodeTest, "System::Core::Misc::Timecode", "[ApplicationContextMask][EngineFilter]")
{
	FFrameRate CommonFrameRates[]{
		FFrameRate(12, 1),
		FFrameRate(15, 1),
		FFrameRate(24, 1),
		FFrameRate(25, 1),
		FFrameRate(30, 1),
		FFrameRate(48, 1),
		FFrameRate(48, 2), // Should give the same result as 24/1
		FFrameRate(50, 1),
		FFrameRate(60, 1),
		FFrameRate(100, 1),
		FFrameRate(120, 1),
		FFrameRate(240, 1),
		FFrameRate(24000, 1001),
		FFrameRate(30000, 1001),
		FFrameRate(48000, 1001),
		FFrameRate(60000, 1001),
	};

	auto ConversionWithFrameRateTest = [](const FFrameRate FrameRate)
	{
		const bool bIsDropFrame = FTimecode::IsDropFormatTimecodeSupported(FrameRate);
		int32 NumberOfErrors = 0;
		FTimecode PreviousTimecodeValue;

		const int64 StartIndex = 0;
		for (int64 FrameIndex = StartIndex; FrameIndex <= MAX_int32; ++FrameIndex)
		{
			const FFrameNumber FrameNumber = static_cast<int32>(FrameIndex);
			const FTimecode TimecodeValue = FTimecode::FromFrameNumber(FrameNumber, FrameRate, bIsDropFrame);
			bool bDoTest = true;

			// Conversion from FrameNumber to Timecode
			if (bDoTest)
			{
				const FFrameNumber ExpectedFrameNumber = TimecodeValue.ToFrameNumber(FrameRate);
				if (FrameNumber != ExpectedFrameNumber)
				{
					FAIL_CHECK(FString::Printf(TEXT("Timecode '%s' didn't convert properly from FrameNumber '%d' for FrameRate '%s'.")
						, *TimecodeValue.ToString()
						, FrameNumber.Value
						, *FrameRate.ToPrettyText().ToString()
					));
					bDoTest = false;
					++NumberOfErrors;
				}
			}
			
			// Conversion from Timespan to Timecode
			if (bDoTest)
			{
				const FTimespan TimespanFromTimecode = TimecodeValue.ToTimespan(FrameRate);
				const FTimecode TimecodeFromTimespanWithRollover = FTimecode::FromTimespan(TimespanFromTimecode, FrameRate, bIsDropFrame, true);
				const FTimecode TimecodeFromTimespanWithoutRollover = FTimecode::FromTimespan(TimespanFromTimecode, FrameRate, bIsDropFrame, false);

				if (TimecodeFromTimespanWithoutRollover != TimecodeValue)
				{
					FAIL_CHECK(FString::Printf(TEXT("Timecode '%s' didn't convert properly from Timespan '%f' with rollover for frame rate '%s'.")
						, *TimecodeValue.ToString()
						, TimespanFromTimecode.GetTotalSeconds()
						, *FrameRate.ToPrettyText().ToString()
					));
					bDoTest = false;
					++NumberOfErrors;
				}
				else if (TimecodeFromTimespanWithoutRollover.Minutes != TimecodeValue.Minutes || TimecodeFromTimespanWithoutRollover.Seconds != TimecodeValue.Seconds || TimecodeFromTimespanWithoutRollover.Frames != TimecodeValue.Frames)
				{
					FAIL_CHECK(FString::Printf(TEXT("Timecode '%s' didn't convert properly from Timespan '%f' without rollover for frame rate '%s'.")
						, *TimecodeValue.ToString()
						, TimespanFromTimecode.GetTotalSeconds()
						, *FrameRate.ToPrettyText().ToString()
					));
					bDoTest = false;
					++NumberOfErrors;
				}
				else if (!bIsDropFrame)
				{
					// Do they have the same hours, minutes, seconds
					// To test this, we start from the number of events (FrameIndex) from which we got our timecode first
					// Timecode is just a label and doesn't necessarily reflect real time especially with 23.976 FrameRate like
					const int32 TotalSeconds = FMath::FloorToInt32((double)FrameIndex * FrameRate.AsInterval());
					const int32 FrameHours = (TotalSeconds / (60 * 60));
					const int32 FrameMinutes = ((TotalSeconds % (60 * 60)) / 60);
					const int32 FrameSeconds = ((TotalSeconds % (60 * 60)) % 60);
					const bool bHoursAreValid = (FrameHours % 24) == TimespanFromTimecode.GetHours() && (FrameHours / 24) == TimespanFromTimecode.GetDays();
					const bool bMinutesAreValid = FrameMinutes == TimespanFromTimecode.GetMinutes();
					const bool bSecondsAreValid = FrameSeconds == TimespanFromTimecode.GetSeconds();
					if (!bHoursAreValid || !bMinutesAreValid || !bSecondsAreValid)
					{
						FAIL_CHECK(FString::Printf(TEXT("Timecode hours/minutes/seconds doesn't matches with Timespan '%s' from frame rate '%s'.")
							, *TimespanFromTimecode.ToString()
							, *FrameRate.ToPrettyText().ToString()
						));
						bDoTest = false;
						++NumberOfErrors;
					}
				}
			}

			// Test if the frame number is incrementing
			bool bIsPreviousTimecodeValid = FrameIndex != StartIndex;
			if (bDoTest && bIsPreviousTimecodeValid)
			{
				bool bWrongFrame = PreviousTimecodeValue.Frames + 1 != TimecodeValue.Frames
					&& TimecodeValue.Frames != 0;
				const bool bWrongSeconds = PreviousTimecodeValue.Seconds != TimecodeValue.Seconds
					&& PreviousTimecodeValue.Seconds + 1 != TimecodeValue.Seconds
					&& TimecodeValue.Seconds != 0;
				const bool bWrongMinutes = PreviousTimecodeValue.Minutes != TimecodeValue.Minutes
					&& PreviousTimecodeValue.Minutes + 1 != TimecodeValue.Minutes
					&& TimecodeValue.Minutes != 0;

				if (bWrongFrame && bIsDropFrame)
				{
					// if new minute but not multiple of 10 mins, 2|4 is expected
					const int32 NumberOfFramesInSecond = FMath::CeilToInt((float)FrameRate.AsDecimal());
					const int32 NumberOfTimecodesToDrop = NumberOfFramesInSecond <= 30 ? 2 : 4;
					bWrongFrame = !(TimecodeValue.Frames == NumberOfTimecodesToDrop && PreviousTimecodeValue.Minutes + 1 == TimecodeValue.Minutes && TimecodeValue.Minutes % 10 != 0);
				}

				if (bWrongFrame || bWrongSeconds || bWrongMinutes)
				{
					FAIL_CHECK(FString::Printf(TEXT("Timecode '%s' is not a continuity of the previous timecode '%s' from frame rate '%s'.")
						, *TimecodeValue.ToString()
						, *PreviousTimecodeValue.ToString()
						, *FrameRate.ToPrettyText().ToString()
					));
					bDoTest = false;
					++NumberOfErrors;
				}
			}

			// Test frame rate that should be equivalent
			if (bDoTest)
			{
				const FFrameRate EquivalentFrameRate = FFrameRate(FrameRate.Numerator * 3, FrameRate.Denominator * 3);
				const FTimecode EquivalentTimecodeValue = FTimecode::FromFrameNumber(FrameNumber, EquivalentFrameRate, bIsDropFrame);
				if (TimecodeValue != EquivalentTimecodeValue)
				{
					FAIL_CHECK(FString::Printf(TEXT("Timecode '%s' didn't convert properly from FrameNumber '%d' when the frame rate is tripled.")
						, *TimecodeValue.ToString()
						, FrameNumber.Value
					));
					bDoTest = false;
					++NumberOfErrors;
				}
			}

			// If we have a lot of errors with this frame rate, there is no need to log them all.
			if (NumberOfErrors > 10)
			{
				WARN(FString::Printf(TEXT("Skip test for frame rate '%s'. Other errors may exists.")
					, *FrameRate.ToPrettyText().ToString()
				));
				break;
			}

			PreviousTimecodeValue = TimecodeValue;

			// LTC timecode support up to 40 hours
			if (TimecodeValue.Hours >= 40)
			{
				break;
			}
		}

		// Conversion from current time to Timecode
		if (NumberOfErrors == 0)
		{
			const FTimespan CurrentTimespan = FTimespan(11694029893428);
			const double CurrentSeconds = 1169402.9893428; //from FPlatformTime::Seconds()

			const FTimecode FromTimespanTimecodeValueWithRollover = FTimecode::FromTimespan(CurrentTimespan, FrameRate, bIsDropFrame, true);
			const FTimecode FromTimespanTimecodeValueWithoutRollover = FTimecode::FromTimespan(CurrentTimespan, FrameRate, bIsDropFrame, false);
			const FTimecode FromSecondsTimecodeValueWithRollover = FTimecode(CurrentSeconds, FrameRate, bIsDropFrame, true);
			const FTimecode FromSecondsTimecodeValueWithoutRollover = FTimecode(CurrentSeconds, FrameRate, bIsDropFrame, false);

			if (FromTimespanTimecodeValueWithRollover != FromSecondsTimecodeValueWithRollover)
			{
				FAIL_CHECK(FString::Printf(TEXT("The timecode '%s' do not match timecode '%s' when converted from the computer clock's time and the frame rate is '%s'")
					, *FromTimespanTimecodeValueWithRollover.ToString()
					, *FromSecondsTimecodeValueWithRollover.ToString()
					, *FrameRate.ToPrettyText().ToString()
				));
				++NumberOfErrors;
			}
			else if (FromTimespanTimecodeValueWithoutRollover != FromSecondsTimecodeValueWithoutRollover)
			{
				FAIL_CHECK(FString::Printf(TEXT("The timecode '%s' do not match timecode '%s' when converted from the computer clock's time and the frame rate is '%s'")
					, *FromTimespanTimecodeValueWithoutRollover.ToString()
					, *FromSecondsTimecodeValueWithoutRollover.ToString()
					, *FrameRate.ToPrettyText().ToString()
				));
				++NumberOfErrors;
			}
			// Can't really test frame number matching between rollver timecode labels. We would need to exclude NDF fractional frame rates
		}

		INFO(FString::Printf(TEXT("Timecode test was completed with frame rate '%s'"), *FrameRate.ToPrettyText().ToString()));

		return NumberOfErrors == 0;
	};

	// Test the conversion for all common frame rate
	TArray<TFuture<bool>> Futures;
	for (const FFrameRate& FrameRate : CommonFrameRates)
	{
		Futures.Add(Async(EAsyncExecution::Thread, [FrameRate, &ConversionWithFrameRateTest](){ return ConversionWithFrameRateTest(FrameRate); }));
	}

	bool bSuccessfully = true;
	for (const TFuture<bool>& Future : Futures)
	{
		Future.Wait();
		bSuccessfully = bSuccessfully && Future.Get();
	}

	REQUIRE(bSuccessfully);
}


#endif //WITH_TESTS
