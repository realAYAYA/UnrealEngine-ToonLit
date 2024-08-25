// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Device/Mem/Device_Mem.h"

class TEXTUREGRAPHENGINE_API Device_Disk : public Device_Mem
{
protected:
	FString							BaseDir;				/// The base directory for 

public:
									Device_Disk();
	virtual							~Device_Disk() override;

	virtual FString					Name() const override { return "Device_Disk"; }
	virtual void					Update(float Delta) override;
	virtual FString					SetBaseDirectory(const FString& InBaseDir, bool MigrateExisting = true);
	FString							GetCacheFilename(HashType HashValue) const;

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static Device_Disk*				Get();

	//////////////////////////////////////////////////////////////////////////
	/// Inline function
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE const FString&		GetBaseDirectory() const { return BaseDir; }
};
