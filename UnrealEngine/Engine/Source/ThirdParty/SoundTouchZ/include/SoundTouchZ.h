// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <stdint.h>

class FSoundTouch
{
public:
	FSoundTouch();
	FSoundTouch(const FSoundTouch&) = delete;
	FSoundTouch& operator=(const FSoundTouch&) = delete;
	~FSoundTouch();

	enum class ESetting
	{
		UseAAFilter,
		AAFilterLength,
		UseQuickseek,
		SequenceMS,
		SeekwindowMS,
		OverlapMS,
		NominalInputSequence,
		NominalOutputSequence,
		InitialLatency
	};

    static const char* GetVersionString();
    static uint32_t GetVersionId();
    void SetRate(double newRate);
    void SetTempo(double newTempo);
    void SetRateChange(double newRate);
    void SetTempoChange(double newTempo);
    void SetPitch(double newPitch);
    void SetPitchOctaves(double newPitch);
    void SetPitchSemiTones(int32_t newPitch);
    void SetPitchSemiTones(double newPitch);
    void SetChannels(uint32_t numChannels);
    void SetSampleRate(uint32_t srate);
    double GetInputOutputSampleRatio();
    void Flush();
    void PutSamples(const float* samples, uint32_t numSamples);
    uint32_t ReceiveSamples(float* output, uint32_t maxSamples);
    uint32_t ReceiveSamples(uint32_t maxSamples);
	void Clear();
    bool SetSetting(ESetting settingId, int32_t value);
    int32_t GetSetting(ESetting settingId) const;
    uint32_t NumUnprocessedSamples() const;
    uint32_t NumChannels() const;
    uint32_t NumSamples() const;
    bool IsEmpty() const;
private:
	class FImpl;
	FImpl* Impl;
};
