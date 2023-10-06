// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/IRemoteControlMaskingFactory.h"

class FVectorMaskingFactory : public IRemoteControlMaskingFactory
{
public:
	static TSharedRef<IRemoteControlMaskingFactory> MakeInstance();

	//~ Begin IRemoteControlMaskingFactory interface
	virtual void ApplyMaskedValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation, bool bIsInteractive) override;
	virtual void CacheRawValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation) override;
	virtual bool SupportsExposedEntity(UScriptStruct* ScriptStruct) const override;
	//~ End IRemoteControlMaskingFactory interface
};

class FVector4MaskingFactory : public IRemoteControlMaskingFactory
{
public:
	static TSharedRef<IRemoteControlMaskingFactory> MakeInstance();

	//~ Begin IRemoteControlMaskingFactory interface
	virtual void ApplyMaskedValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation, bool bIsInteractive) override;
	virtual void CacheRawValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation) override;
	virtual bool SupportsExposedEntity(UScriptStruct* ScriptStruct) const override;
	//~ End IRemoteControlMaskingFactory interface
};

class FIntVectorMaskingFactory : public IRemoteControlMaskingFactory
{
public:
	static TSharedRef<IRemoteControlMaskingFactory> MakeInstance();

	//~ Begin IRemoteControlMaskingFactory interface
	virtual void ApplyMaskedValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation, bool bIsInteractive) override;
	virtual void CacheRawValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation) override;
	virtual bool SupportsExposedEntity(UScriptStruct* ScriptStruct) const override;
	//~ End IRemoteControlMaskingFactory interface
};

class FIntVector4MaskingFactory : public IRemoteControlMaskingFactory
{
public:
	static TSharedRef<IRemoteControlMaskingFactory> MakeInstance();

	//~ Begin IRemoteControlMaskingFactory interface
	virtual void ApplyMaskedValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation, bool bIsInteractive) override;
	virtual void CacheRawValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation) override;
	virtual bool SupportsExposedEntity(UScriptStruct* ScriptStruct) const override;
	//~ End IRemoteControlMaskingFactory interface
};

class FRotatorMaskingFactory : public IRemoteControlMaskingFactory
{
public:
	static TSharedRef<IRemoteControlMaskingFactory> MakeInstance();

	//~ Begin IRemoteControlMaskingFactory interface
	virtual void ApplyMaskedValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation, bool bIsInteractive) override;
	virtual void CacheRawValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation) override;
	virtual bool SupportsExposedEntity(UScriptStruct* ScriptStruct) const override;
	//~ End IRemoteControlMaskingFactory interface
};

class FColorMaskingFactory : public IRemoteControlMaskingFactory
{
public:
	static TSharedRef<IRemoteControlMaskingFactory> MakeInstance();

	//~ Begin IRemoteControlMaskingFactory interface
	virtual void ApplyMaskedValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation, bool bIsInteractive) override;
	virtual void CacheRawValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation) override;
	virtual bool SupportsExposedEntity(UScriptStruct* ScriptStruct) const override;
	//~ End IRemoteControlMaskingFactory interface
};

class FLinearColorMaskingFactory : public IRemoteControlMaskingFactory
{
public:
	static TSharedRef<IRemoteControlMaskingFactory> MakeInstance();

	//~ Begin IRemoteControlMaskingFactory interface
	virtual void ApplyMaskedValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation, bool bIsInteractive) override;
	virtual void CacheRawValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation) override;
	virtual bool SupportsExposedEntity(UScriptStruct* ScriptStruct) const override;
	//~ End IRemoteControlMaskingFactory interface
};
