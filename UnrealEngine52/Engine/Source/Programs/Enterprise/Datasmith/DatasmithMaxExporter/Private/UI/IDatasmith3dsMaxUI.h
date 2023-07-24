// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace DatasmithMaxDirectLink
{
namespace Ui
{

class IMessagesWindow
{
public:

	virtual void OpenWindow() = 0;

	virtual void AddError(const FString& Message) = 0;
	virtual void AddWarning(const FString& Message) = 0;
	virtual void AddInfo(const FString& Message) = 0;
	virtual void AddCompletion(const FString& Message) = 0;
	virtual void ClearMessages() = 0;

	virtual ~IMessagesWindow() = default;
};

IMessagesWindow* CreateMessagesWindow();

}
}
