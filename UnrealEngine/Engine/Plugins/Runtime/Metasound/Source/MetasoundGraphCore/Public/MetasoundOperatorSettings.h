// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Metasound
{
	using FSampleRate = int32;
	static_assert(TIsIntegral<FSampleRate>::Value, "FSampleRate must be integral type.");

	/** FOperatorSettings
	 *
	 * Audio settings for Metasound IOperators including audio sample rate, 
	 * block rate and frames per block.
	 *
	 * The audio SampleRate defines the number of audio samples per a second of
	 * a mono waveform. 
	 *
	 * The BlockRate defines the number of times an IOperator's execution 
	 * function will be triggered per a second of audio content.
	 *
	 * The NumFramesPerBlock defines the number of audio frames written-to 
	 * and/or read-from during a single call to an IOperator's execution 
	 * function.
	 *
	 * GetNumFramesPerBlock() is required to return a value that abides by the
	 * alignment requirements of FAudioBuffer, FAlignedFloatBuffer and the math
	 * routines declared in BufferVectorOperations.h. This enables the vast 
	 * majority of Metasound IOperators to take advantage of SIMD hardware
	 * accelerations.  
	 *
	 * In order to achieve SIMD alignment, desired block rates must be slightly 
	 * adjusted to match the boundaries of the given sample rate and SIMD 
	 * alignment. GetActualBlockRate() returns the true block rate after 
	 * adhering to sample rate and SIMD boundaries. 
	 */
	class METASOUNDGRAPHCORE_API FOperatorSettings
	{
		public:
			/** FOperatorSettings constructor.
			 *
			 * @param InSampleRate      - Audio sample rate in Hz. 
			 * @param InTargetBlockRate - The desired block rate in Hz.
			 */
			FOperatorSettings(FSampleRate InSampleRate, float InTargetBlockRate);

			/** Set the audio sample rate in Hz. */
			void SetSampleRate(FSampleRate InSampleRate);

			/** Get the audio sample rate in Hz. */
			FORCEINLINE float GetSampleRate() const
			{
				return SampleRate;
			}

			/** Set the target block rate in Hz. 
			 *
			 * In order to achieve SIMD alignment, target block rates must be 
			 * adjusted to match the boundaries of the given sample rate and 
			 * SIMD alignment. GetActualBlockRate() returns the true block rate 
			 * after adhering to sample rate and SIMD boundaries. 
			 */
			void SetTargetBlockRate(float InTargetBlockRate);

			/** Get the target block rate in Hz. */
			float GetTargetBlockRate() const;

			/** Get the actual block rate in Hz after adhering to sample rate 
			 * and SIMD alignment.
			 */
			float GetActualBlockRate() const;

			/** Get the number of audio frames in a block. 
			 *
			 * This function is required to return a value that abides by the
			 * alignment requirements of FAudioBuffer, FAlignedFloatBuffer and 
			 * the math routines declared in BufferVectorOperations.h. This 
			 * enables the vast majority of Metasound IOperators to take 
			 * advantage of SIMD hardware accelerations.
			 */
			FORCEINLINE int32 GetNumFramesPerBlock() const
			{
				return NumFramesPerBlock;
			}

		private:
			// Update actual block rate and frames per block.
			void Update();

			int32 RoundToAligned(int32 Alignment, int32 InNum) const;

			FSampleRate SampleRate = 1;
			
			float TargetBlockRate = 1.f;

			float ActualBlockRate = 1.f;

			int32 NumFramesPerBlock = 0;
	};
}
