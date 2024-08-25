// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DSP/AlignedBuffer.h"
#include "Containers/Array.h"
#include "Misc/CoreMiscDefines.h"

namespace Audio
{
	/* Sets a values to zero if value is denormal. Denormal numbers significantly slow down floating point operations. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferUnderflowClampFast(FAlignedFloatBuffer& InOutBuffer);

	/* Sets a values to zero if value is denormal. Denormal numbers significantly slow down floating point operations. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferUnderflowClampFast(float* RESTRICT InOutBuffer, const int32 InNum);
	
	/* Clamps the values in a buffer between a min and max value.*/
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferRangeClampFast(FAlignedFloatBuffer& InOutBuffer, float InMinValue, float InMaxValue);

	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferRangeClampFast(float* RESTRICT InOutBuffer, const int32 InNum, float InMinValue, float InMaxValue);
	
	/** Multiplies the input aligned float buffer with the given value. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferMultiplyByConstant(const FAlignedFloatBuffer& InFloatBuffer, float InValue, FAlignedFloatBuffer& OutFloatBuffer);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferMultiplyByConstant(const float* RESTRICT InFloatBuffer, float InValue, float* RESTRICT OutFloatBuffer, const int32 InNumSamples);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferMultiplyByConstant(const FAlignedFloatBuffer& InFloatBuffer, float InValue, FAlignedFloatBuffer& OutFloatBuffer);

	/** Similar to BufferMultiplyByConstant, but (a) assumes a buffer length divisible by 4 and (b) performs the multiply in place. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MultiplyBufferByConstantInPlace(FAlignedFloatBuffer& InBuffer, float InGain);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MultiplyBufferByConstantInPlace(float* RESTRICT InBuffer, int32 NumSamples, float InGain);

	/** Adds a constant to a buffer (useful for DC offset removal) */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void AddConstantToBufferInplace(FAlignedFloatBuffer& InBuffer, float Constant);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void AddConstantToBufferInplace(float* RESTRICT InBuffer, int32 NumSamples, float Constant);

	/** Sets a constant to a buffer (useful for DC offset application) */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferSetToConstantInplace(FAlignedFloatBuffer& InBuffer, float Constant);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferSetToConstantInplace(float* RESTRICT InBuffer, int32 NumSamples, float Constant);

	/* Performs an element-wise weighted sum OutputBuffer = (InBuffer1 x InGain1) + (InBuffer2 x InGain2) */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferWeightedSumFast(const FAlignedFloatBuffer& InBuffer1, float InGain1, const FAlignedFloatBuffer& InBuffer2, float InGain2, FAlignedFloatBuffer& OutBuffer);

	/* Performs an element-wise weighted sum OutputBuffer = (InBuffer1 x InGain1) + InBuffer2 */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferWeightedSumFast(const FAlignedFloatBuffer& InBuffer1, float InGain1, const FAlignedFloatBuffer& InBuffer2, FAlignedFloatBuffer& OutBuffer);

	/* Performs an element-wise weighted sum OutputBuffer = (InBuffer1 x InGain1) + (InBuffer2 x InGain2) */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferWeightedSumFast(const float* RESTRICT InBuffer1, float InGain1, const float* RESTRICT InBuffer2, float InGain2, float* RESTRICT OutBuffer, int32 InNum);

	/* Performs an element-wise weighted sum OutputBuffer = (InBuffer1 x InGain1) + InBuffer2 */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferWeightedSumFast(const float* RESTRICT InBuffer1, float InGain1, const float* RESTRICT InBuffer2, float* RESTRICT OutBuffer, int32 InNum);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MultiplyBufferByConstantInPlace(FAlignedFloatBuffer& InBuffer, float InGain);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MultiplyBufferByConstantInPlace(float* RESTRICT InBuffer, int32 NumSamples, float InGain);

	/* Takes a float buffer and quickly interpolates it's gain from StartValue to EndValue. */
	/* This operation completely ignores channel counts, so avoid using this function on buffers that are not mono, stereo or quad */
	/* if the buffer needs to fade all channels uniformly. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void FadeBufferFast(FAlignedFloatBuffer& OutFloatBuffer, const float StartValue, const float EndValue);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void FadeBufferFast(float* RESTRICT OutFloatBuffer, int32 NumSamples, const float StartValue, const float EndValue);

	/** Takes buffer InFloatBuffer, optionally multiplies it by Gain, and adds it to BufferToSumTo. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MixInBufferFast(const FAlignedFloatBuffer& InFloatBuffer, FAlignedFloatBuffer& BufferToSumTo, const float Gain);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MixInBufferFast(const float* RESTRICT InFloatBuffer, float* RESTRICT BufferToSumTo, int32 NumSamples, const float Gain);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MixInBufferFast(const FAlignedFloatBuffer& InFloatBuffer, FAlignedFloatBuffer& BufferToSumTo, const float StartGain, const float EndGain);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MixInBufferFast(const float* RESTRICT InFloatBuffer, float* RESTRICT BufferToSumTo, int32 NumSamples, const float StartGain, const float EndGain);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MixInBufferFast(const FAlignedFloatBuffer& InFloatBuffer, FAlignedFloatBuffer& BufferToSumTo);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MixInBufferFast(const float* RESTRICT InFloatBuffer, float* RESTRICT BufferToSumTo, int32 NumSamples);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")

	/* Subtracts two buffers together element-wise. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferSubtractFast(const FAlignedFloatBuffer& InMinuend, const FAlignedFloatBuffer& InSubtrahend, FAlignedFloatBuffer& OutputBuffer);

	/* Subtracts two buffers together element-wise. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferSubtractFast(const float* RESTRICT InMinuend, const float* RESTRICT InSubtrahend, float* RESTRICT OutputBuffer, int32 NumSamples);

	/* Performs element-wise in-place subtraction placing the result in the subtrahend. InOutSubtrahend = InMinuend - InOutSubtrahend */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferSubtractInPlace1Fast(const FAlignedFloatBuffer& InMinuend, FAlignedFloatBuffer& InOutSubtrahend);

	/* Performs element-wise in-place subtraction placing the result in the subtrahend. InOutSubtrahend = InMinuend - InOutSubtrahend */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferSubtractInPlace1Fast(const float* RESTRICT InMinuend, float* RESTRICT InOutSubtrahend, int32 NumSamples);


	/* Performs element-wise in-place subtraction placing the result in the minuend. InOutMinuend = InOutMinuend - InSubtrahend */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferSubtractInPlace2Fast(FAlignedFloatBuffer& InOutMinuend, const FAlignedFloatBuffer& InSubtrahend);

	/* Performs element-wise in-place subtraction placing the result in the minuend. InOutMinuend = InOutMinuend - InSubtrahend */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void BufferSubtractInPlace2Fast(float* RESTRICT InOutMinuend, const float* RESTRICT InSubtrahend, int32 NumSamples);

	/** This version of MixInBufferFast will fade from StartGain to EndGain. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MixInBufferFast(const FAlignedFloatBuffer& InFloatBuffer, FAlignedFloatBuffer& BufferToSumTo, const float StartGain, const float EndGain);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MixInBufferFast(const float* RESTRICT InFloatBuffer, float* RESTRICT BufferToSumTo, int32 NumSamples, const float StartGain, const float EndGain);

	/** Sums two buffers together and places the result in the resulting buffer. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void SumBuffers(const FAlignedFloatBuffer& InFloatBuffer1, const FAlignedFloatBuffer& InFloatBuffer2, FAlignedFloatBuffer& OutputBuffer);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void SumBuffers(const float* RESTRICT InFloatBuffer1, const float* RESTRICT InFloatBuffer2, float* RESTRICT OutputBuffer, int32 NumSamples);

	/** Multiply the second buffer in place by the first buffer. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MultiplyBuffersInPlace(const FAlignedFloatBuffer& InFloatBuffer, FAlignedFloatBuffer& BufferToMultiply);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API void MultiplyBuffersInPlace(const float* RESTRICT InFloatBuffer, float* RESTRICT BufferToMultiply, int32 NumSamples);

	/** CHANNEL-AGNOSTIC ANALYSIS OPERATIONS */

	/** Takes an audio buffer and returns the magnitude across that buffer. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API float GetMagnitude(const FAlignedFloatBuffer& Buffer);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API float GetMagnitude(const float* RESTRICT Buffer, int32 NumSamples);

	/** Takes an audio buffer and gets the average amplitude across that buffer. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API float BufferGetAverageValue(const FAlignedFloatBuffer& Buffer);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API float BufferGetAverageValue(const float* RESTRICT Buffer, int32 NumSamples);

	/** Takes an audio buffer and gets the average absolute amplitude across that buffer. */
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API float BufferGetAverageAbsValue(const FAlignedFloatBuffer& Buffer);
	UE_DEPRECATED(5.1, "Use equivalent function in FloatArrayMath.h")
	SIGNALPROCESSING_API float BufferGetAverageAbsValue(const float* RESTRICT Buffer, int32 NumSamples);

	/** CHANNEL-SPECIFIC OPERATIONS */

	/** Takes a 2 channel interleaved buffer and applies Gains to it. Gains is expected to point to a 2 float long buffer.
	 *  StereoBuffer must have an even number of frames.
	 *  If StartGains and EndGains are provided, this function will interpolate between the two across the buffer.
	*/
	SIGNALPROCESSING_API void Apply2ChannelGain(FAlignedFloatBuffer& StereoBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Apply2ChannelGain(float* RESTRICT StereoBuffer, int32 NumSamples, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Apply2ChannelGain(FAlignedFloatBuffer& StereoBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void Apply2ChannelGain(float* RESTRICT StereoBuffer, int32 NumSamples, const float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/** Takes a 1 channel buffer and mixes it to a stereo buffer using Gains. Gains is expected to point to a 2 float long buffer.
	 *  these buffers must have an even number of frames.
	 *  If StartGains and EndGains are provided, this function will interpolate between the two across the buffer.
	*/
	SIGNALPROCESSING_API void MixMonoTo2ChannelsFast(const FAlignedFloatBuffer& MonoBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void MixMonoTo2ChannelsFast(const float* RESTRICT MonoBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void MixMonoTo2ChannelsFast(const FAlignedFloatBuffer& MonoBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void MixMonoTo2ChannelsFast(const float* RESTRICT MonoBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void MixMonoTo2ChannelsFast(const FAlignedFloatBuffer& MonoBuffer, FAlignedFloatBuffer& DestinationBuffer);
	SIGNALPROCESSING_API void MixMonoTo2ChannelsFast(const float* RESTRICT MonoBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames);

	/** Takes a 2 channel buffer and mixes it to an 2 channel interleaved buffer using Gains. Gains is expected to point to a 16 float long buffer.
	*  Output gains for the left input channel should be the first 8 values in Gains, and Output gains for the right input channel should be rest.
	*  NumFrames must be a multiple of 4.
	*  If StartGains and EndGains are provided, this function will interpolate between the two across the buffer.
	*/
	SIGNALPROCESSING_API void Mix2ChannelsTo2ChannelsFast(const FAlignedFloatBuffer& SourceBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Mix2ChannelsTo2ChannelsFast(const float* RESTRICT SourceBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Mix2ChannelsTo2ChannelsFast(const FAlignedFloatBuffer& SourceBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void Mix2ChannelsTo2ChannelsFast(const float* RESTRICT SourceBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/** Takes a 4 channel interleaved buffer and applies Gains to it. Gains is expected to point to a 2 float long buffer.
	 *  If StartGains and EndGains are provided, this function will interpolate between the two across the buffer.
	*/
	SIGNALPROCESSING_API void Apply4ChannelGain(FAlignedFloatBuffer& InterleavedBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Apply4ChannelGain(float* RESTRICT InterleavedBuffer, int32 NumSamples, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Apply4ChannelGain(FAlignedFloatBuffer& InterleavedBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void Apply4ChannelGain(float* RESTRICT InterleavedBuffer, int32 NumSamples, const float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/** Takes a 1 channel buffer and mixes it to an 8 channel interleaved buffer using Gains. Gains is expected to point to a 8 float long buffer.
	*  these buffers must have an even number of frames.
	*  If StartGains and EndGains are provided, this function will interpolate between the two across the buffer.
	*/
	SIGNALPROCESSING_API void MixMonoTo4ChannelsFast(const FAlignedFloatBuffer& MonoBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void MixMonoTo4ChannelsFast(const float* RESTRICT MonoBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void MixMonoTo4ChannelsFast(const FAlignedFloatBuffer& MonoBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void MixMonoTo4ChannelsFast(const float* RESTRICT MonoBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/** Takes a 2 channel buffer and mixes it to an 8 channel interleaved buffer using Gains. Gains is expected to point to a 16 float long buffer.
	*  Output gains for the left input channel should be the first 8 values in Gains, and Output gains for the right input channel should be rest.
	*  NumFrames must be a multiple of 4.
	*  If StartGains and EndGains are provided, this function will interpolate between the two across the buffer.
	*/
	SIGNALPROCESSING_API void Mix2ChannelsTo4ChannelsFast(const FAlignedFloatBuffer& SourceBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Mix2ChannelsTo4ChannelsFast(const float* RESTRICT SourceBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Mix2ChannelsTo4ChannelsFast(const FAlignedFloatBuffer& SourceBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void Mix2ChannelsTo4ChannelsFast(const float* RESTRICT SourceBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/** Takes a 6 channel interleaved buffer and applies Gains to it. Gains is expected to point to a 2 float long buffer.
	 *  InterleavedBuffer must have an even number of frames.
	*/
	SIGNALPROCESSING_API void Apply6ChannelGain(FAlignedFloatBuffer& InterleavedBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Apply6ChannelGain(float* RESTRICT InterleavedBuffer, int32 NumSamples, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Apply6ChannelGain(FAlignedFloatBuffer& InterleavedBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void Apply6ChannelGain(float* RESTRICT InterleavedBuffer, int32 NumSamples, const float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/** Takes a 1 channel buffer and mixes it to an 8 channel interleaved buffer using Gains. Gains is expected to point to a 8 float long buffer.
	 *  these buffers must have an even number of frames.
	 *  If StartGains and EndGains are provided, this function will interpolate between the two across the buffer.
	*/
	SIGNALPROCESSING_API void MixMonoTo6ChannelsFast(const FAlignedFloatBuffer& MonoBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void MixMonoTo6ChannelsFast(const float* RESTRICT MonoBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void MixMonoTo6ChannelsFast(const FAlignedFloatBuffer& MonoBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void MixMonoTo6ChannelsFast(const float* RESTRICT MonoBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/** Takes a 2 channel buffer and mixes it to an 8 channel interleaved buffer using Gains. Gains is expected to point to a 16 float long buffer.
	 *  Output gains for the left input channel should be the first 8 values in Gains, and Output gains for the right input channel should be rest.
	 *  NumFrames must be a multiple of 4.
	 *  If StartGains and EndGains are provided, this function will interpolate between the two across the buffer.
	*/
	SIGNALPROCESSING_API void Mix2ChannelsTo6ChannelsFast(const FAlignedFloatBuffer& SourceBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Mix2ChannelsTo6ChannelsFast(const float* RESTRICT SourceBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Mix2ChannelsTo6ChannelsFast(const FAlignedFloatBuffer& SourceBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void Mix2ChannelsTo6ChannelsFast(const float* RESTRICT SourceBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/** Takes an 8 channel interleaved buffer and applies Gains to it. Gains is expected to point to an 8 float long buffer. */
	SIGNALPROCESSING_API void Apply8ChannelGain(FAlignedFloatBuffer& InterleavedBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Apply8ChannelGain(float* RESTRICT InterleavedBuffer, int32 NumSamples, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Apply8ChannelGain(FAlignedFloatBuffer& InterleavedBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void Apply8ChannelGain(float* RESTRICT InterleavedBuffer, int32 NumSamples, const float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/** Takes a 1 channel buffer and mixes it to an 8 channel interleaved buffer using Gains. Gains is expected to point to a 8 float long buffer.
	 *  these buffers must have an even number of frames.
	 *  If StartGains and EndGains are provided, this function will interpolate between the two across the buffer.
	*/
	SIGNALPROCESSING_API void MixMonoTo8ChannelsFast(const FAlignedFloatBuffer& MonoBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void MixMonoTo8ChannelsFast(const float* RESTRICT MonoBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void MixMonoTo8ChannelsFast(const FAlignedFloatBuffer& MonoBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void MixMonoTo8ChannelsFast(const float* RESTRICT MonoBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/** Takes a 2 channel buffer and mixes it to an 8 channel interleaved buffer using Gains. Gains is expected to point to a 16 float long buffer.
	 *  Output gains for the left input channel should be the first 8 values in Gains, and Output gains for the right input channel should be rest.
	 *  these buffers must have an even number of frames.
	 *  If StartGains and EndGains are provided, this function will interpolate between the two across the buffer.
	*/
	SIGNALPROCESSING_API void Mix2ChannelsTo8ChannelsFast(const FAlignedFloatBuffer& SourceBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Mix2ChannelsTo8ChannelsFast(const float* RESTRICT SourceBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void Mix2ChannelsTo8ChannelsFast(const FAlignedFloatBuffer& SourceBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void Mix2ChannelsTo8ChannelsFast(const float* RESTRICT SourceBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/** This is a generalized operation that uses the channel gain matrix provided in Gains to mix an interleaved source buffer to the interleaved downmix buffer.
	 *  This operation is not explicitly vectorized and will almost always be slower than using one of the functions above.
	*/
	SIGNALPROCESSING_API void DownmixBuffer(int32 NumSourceChannels, int32 NumDestinationChannels, const FAlignedFloatBuffer& SourceBuffer, FAlignedFloatBuffer& DestinationBuffer, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void DownmixBuffer(int32 NumSourceChannels, int32 NumDestinationChannels, const float* RESTRICT SourceBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void DownmixBuffer(int32 NumSourceChannels, int32 NumDestinationChannels, const FAlignedFloatBuffer& SourceBuffer, FAlignedFloatBuffer& DestinationBuffer, float* RESTRICT StartGains, const float* RESTRICT EndGains);
	SIGNALPROCESSING_API void DownmixBuffer(int32 NumSourceChannels, int32 NumDestinationChannels, const float* RESTRICT SourceBuffer, float* RESTRICT DestinationBuffer, int32 NumFrames, float* RESTRICT StartGains, const float* RESTRICT EndGains);

	/**
	 * This is similar to DownmixBuffer, except that it sums into DestinationBuffer rather than overwriting it.
	 */
	SIGNALPROCESSING_API void DownmixAndSumIntoBuffer(int32 NumSourceChannels, int32 NumDestinationChannels, const FAlignedFloatBuffer& SourceBuffer, FAlignedFloatBuffer& BufferToSumTo, const float* RESTRICT Gains);
	SIGNALPROCESSING_API void DownmixAndSumIntoBuffer(int32 NumSourceChannels, int32 NumDestinationChannels, const float* RESTRICT SourceBuffer, float* RESTRICT BufferToSumTo, int32 NumFrames, const float* RESTRICT Gains);

	/** Interleaves samples from two input buffers */
	SIGNALPROCESSING_API void BufferInterleave2ChannelFast(const FAlignedFloatBuffer& InBuffer1, const FAlignedFloatBuffer& InBuffer2, FAlignedFloatBuffer& OutBuffer);

	/** Interleaves samples from two input buffers */
	SIGNALPROCESSING_API void BufferInterleave2ChannelFast(const float* RESTRICT InBuffer1, const float* RESTRICT InBuffer2, float* RESTRICT OutBuffer, const int32 InNum);

	/** Deinterleaves samples from a 2 channel input buffer */
	SIGNALPROCESSING_API void BufferDeinterleave2ChannelFast(const FAlignedFloatBuffer& InBuffer, FAlignedFloatBuffer& OutBuffer1, FAlignedFloatBuffer& OutBuffer2);

	/** Deinterleaves samples from a 2 channel input buffer */
	SIGNALPROCESSING_API void BufferDeinterleave2ChannelFast(const float* RESTRICT InBuffer, float* RESTRICT OutBuffer1, float* RESTRICT OutBuffer2, const int32 InNum);

	/** Sums 2 channel interleaved input samples. OutSamples[n] = InSamples[2n] + InSamples[2n + 1] */
	SIGNALPROCESSING_API void BufferSum2ChannelToMonoFast(const FAlignedFloatBuffer& InSamples, FAlignedFloatBuffer& OutSamples);

	/** Sums 2 channel interleaved input samples. OutSamples[n] = InSamples[2n] + InSamples[2n + 1] */
	SIGNALPROCESSING_API void BufferSum2ChannelToMonoFast(const float* RESTRICT InSamples, float* RESTRICT OutSamples, const int32 InNumFrames);

	/** Class which handles a vectorized interpolation of an entire buffer to the values of a target buffer */
	class FBufferLinearEase
	{
	public:
		SIGNALPROCESSING_API FBufferLinearEase();
		SIGNALPROCESSING_API FBufferLinearEase(const FAlignedFloatBuffer& InSourceValues, const FAlignedFloatBuffer& InTargetValues, int32 InLerpLength);
		SIGNALPROCESSING_API ~FBufferLinearEase();

		/** will cache SourceValues ptr and manually update SourceValues on Update() */
		SIGNALPROCESSING_API void Init(const FAlignedFloatBuffer& InSourceValues, const FAlignedFloatBuffer& InTargetValues, int32 InLerpLength);

		/** Performs Vectorized update of SourceValues float buffer. Returns true if interpolation is complete */
		SIGNALPROCESSING_API bool Update(FAlignedFloatBuffer& InSourceValues);

		/** Update overloaded to let you jump forward more than a single time-step */
		SIGNALPROCESSING_API bool Update(uint32 StepsToJumpForward, FAlignedFloatBuffer& InSourceValues);

		/** returns const reference to the deltas buffer for doing interpolation elsewhere */
		SIGNALPROCESSING_API const FAlignedFloatBuffer& GetDeltaBuffer();

	private:
		int32 BufferLength {0};
		int32 LerpLength {0};
		int32 CurrentLerpStep{0};
		FAlignedFloatBuffer DeltaBuffer;

	}; // class BufferLerper
}
