// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaOutput.h"
#include "Templates/SharedPointer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAvaRundownEditorOutputUtils, Warning, All);

class FJsonObject;

/*
 *	Class designed to deal with Serialization and Editing
 *	of Media Output device data coming from Rundown Server
 *
 *	It is meant to give full control and provide all information to Rundown server
 *	in a form that is usable outside of unreal engine
 */
class FAvaRundownOutputEditorUtils
{
public:
	FAvaRundownOutputEditorUtils();
	virtual ~FAvaRundownOutputEditorUtils();

	static FString SerializeMediaOutput(const UMediaOutput* InMediaOutput);
	static void EditMediaOutput(UMediaOutput* InMediaOutput, const FString& InDeviceData);

private:
	// Serialization helper functions
	static TSharedPtr<FJsonObject> ParsePropertyInfo(TFieldIterator<FProperty> InProperty, const void* InOwnerObject);
	static TSharedPtr<FJsonObject> ParseElementaryPropertyInfo(TFieldIterator<FProperty> InProperty, const void* InOwnerObject);
	static TSharedPtr<FJsonObject> ParseEnumPropertyInfo(TFieldIterator<FProperty> InProperty, const void* InOwnerObject);
	static TSharedPtr<FJsonObject> ParseStructPropertyInfo(TFieldIterator<FProperty> InProperty, const void* InOwnerObject);

	// Helper functions for editing Media Output Devices
	static void SetProperty(void* InOwnerObject, const TSharedPtr<FJsonObject>& InPropertyObject, const TFieldIterator<FProperty>& InProperty);
	static void SetElementaryProperty(void* InOwnerObject, const TSharedPtr<FJsonObject>& InPropertyObject, TFieldIterator<FProperty> InProperty);
	static void SetEnumProperty(void* InOwnerObject, const TSharedPtr<FJsonObject>& InPropertyObject, TFieldIterator<FProperty> InProperty);
	static void SetStructProperties(void* InOwnerObject, const TSharedPtr<FJsonObject>& InPropertyObject, TFieldIterator<FProperty> InProperty);
};
