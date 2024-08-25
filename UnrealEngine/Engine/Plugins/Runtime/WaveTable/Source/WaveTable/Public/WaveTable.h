// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/ArrayView.h"
#include "WaveTableSampler.h"

#include "WaveTable.generated.h"


namespace WaveTable
{
	// An invalid WaveTable value, used to detect uninitialized final values.
	constexpr float InvalidWaveTableValue = TNumericLimits<float>::Min();

	// Read-only view to a wave table struct
	struct WAVETABLE_API FWaveTableView
	{
		TArrayView<const float> SampleView;
		const float FinalValue = 0.0f;

		explicit FWaveTableView(const TArray<float>& InSamples, const float InFinalValue);
		explicit FWaveTableView(const TArrayView<const float>& InSamples, const float InFinalValue);

		// returns the length of the SampleView array, not including the additional FinalValue
		int32 Num() const;
		bool IsEmpty() const;
	};

	// WaveTable data used at runtime by oscillators, envelopes,
	//  etc.  Always as 32-bit float bit depth for processing optimization,
	// at the expense of memory.
	struct WAVETABLE_API FWaveTable
	{
	private:
		TArray<float> Samples;
		float FinalValue = 0.0f;

	public:
		FWaveTable() = default;
		FWaveTable(const TArray<float>& InSamples, const float InFinalValue = 0.0f);
		FWaveTable(TArray<float>&& InSamples, const float InFinalValue);
		FWaveTable(const ::FWaveTableData& InData);

		float* GetData();
		const float* GetData() const;
		FWaveTableView GetView() const;
		TArrayView<float> GetSamples();
		int32 Num() const;
		void Set(TArray<float>&& InSamples);
		void Set(TArrayView<const float> InSamples);
		void SetFinalValue(const float InValue);
		void SetNum(int32 InSize);
		void Zero();

		FORCEINLINE static void WrapIndexSmooth(int32 InMax, float& InOutIndex)
		{
			InOutIndex = FMath::Abs(InOutIndex); // Avoids fractional offset flip at 0 crossing
			const int32 WrapIndex = FMath::TruncToInt32(InOutIndex) % InMax;
			const float Fractional = FMath::Frac(InOutIndex);
			InOutIndex = WrapIndex + Fractional;
		}
	};
} // namespace WaveTable


UENUM(BlueprintType)
enum class EWaveTableBitDepth : uint8
{
	// Lower resolution and marginal performance cost with
	// conversion overhead (engine operates on 32-bit)
	// with the advantage of half the size in memory.
	PCM_16,

	// Higher precision and faster operative performance
	// (engine operates at 32-bit) at the cost of twice the
	// memory of 16-bit.
	IEEE_Float,

	COUNT UMETA(Hidden)
};


// Serialized WaveTable data, that supports multiple bit depth formats.
USTRUCT(BlueprintType)
struct WAVETABLE_API FWaveTableData
{
	GENERATED_BODY();

	FWaveTableData() = default;
	FWaveTableData(EWaveTableBitDepth InBitDepth);
	FWaveTableData(TArrayView<const float> InSamples, float InFinalValue);
	FWaveTableData(TArrayView<const int16> InSamples, float InFinalValue);

#if WITH_EDITOR
	static FName GetBitDepthPropertyName();
#endif // WITH_EDITOR

private:
	UPROPERTY(EditAnywhere, Category = Options)
	EWaveTableBitDepth BitDepth = EWaveTableBitDepth::PCM_16;

	UPROPERTY()
	TArray<uint8> Data;

	UPROPERTY()
	float FinalValue = ::WaveTable::InvalidWaveTableValue;

	// Returns the size of the underlying data's sample in number of bytes.
	FORCEINLINE int32 GetSampleSize() const;

public:
	// Adds this WaveTableData's internal contents to the given float buffer view,
	// initially performing bit depth conversion to values to IEEE_FLOAT if required.
	// Provided buffer's size must match this WaveTableData's samples count.
	void ArrayMixIn(TArrayView<float> OutputWaveSamples, float Gain = 1.0f) const;

	// Empties the underlying data container, deallocating memory and invalidating the FinalValue.
	void Empty();

	EWaveTableBitDepth GetBitDepth() const;

	// Returns read-only view of byte array that stores sample data.
	const TArray<uint8>& GetRawData() const;

	// Returns true if ArrayView of underlying Data in the proper sample format,
	// setting OutData to a view of the Table's data. Returns false if the bit
	// depth of the provided ArrayView does not match the Table's BitDepth property.
	bool GetDataView(TArrayView<const float>& OutData) const;
	bool GetDataView(TArrayView<float>& OutData);
	bool GetDataView(TArrayView<const int16>& OutData) const;
	bool GetDataView(TArrayView<int16>& OutData);

	// Returns number of samples within the underlying DataView, not including a FinalValue if set.
	// (Should not to be confused with the number of bytes used to represent the given DataView in
	// the internal Data container).
	int32 GetNumSamples() const;

	// Returns the last data value value as a float.  If DataView is empty, returns 0.
	float GetLastValue() const;

	// Returns the value to interpolate to beyond the last value of the DataView.
	// If FinalValue property is set to invalid (default), returns the last value in the 
	// underlying DataView (see GetLastValue).
	float GetFinalValue() const;

	// Whether or not the underlying data is empty.
	bool IsEmpty() const;

	// Resamples the underlying data to a provided new sample rate.
	bool Resample(int32 InCurrentSampleRate, int32 InNewSampleRate, ::WaveTable::FWaveTableSampler::EInterpolationMode InInterpMode = ::WaveTable::FWaveTableSampler::EInterpolationMode::Cubic);

	// Resets the underlying data to the given number of samples. Does not change
	// memory allocation of data unless NumSamples is larger than the current size.
	void Reset(int32 NumSamples);

	// Converts bit depth and sets internal data to the requested bit depth.
	// Returns true if conversion took place, false if not (i.e. data was 
	// already sampled at the given bit depth).
	bool SetBitDepth(EWaveTableBitDepth InBitDepth);
	void SetData(TArrayView<const int16> InData, bool bIsLoop);
	void SetData(TArrayView<const float> InData, bool bIsLoop);
	void SetRawData(EWaveTableBitDepth InBitDepth, TArrayView<const uint8> InData);
	void SetRawData(EWaveTableBitDepth InBitDepth, TArray<uint8>&& InData);
	void SetFinalValue(float InFinalValue);

	// Zeros the underlying data. If InNumSamples is set (optional),
	// allocates space in underlying data array for the given number of samples.
	void Zero(int32 InNumSamples = INDEX_NONE);
};
