// Copyright Epic Games, Inc.All Rights Reserved.

#include "AvaInteractiveToolsDelegates.h"

FAvaInteractiveToolsRegisterToolsDelegate& FAvaInteractiveToolsDelegates::GetRegisterToolsDelegate()
{
	static FAvaInteractiveToolsRegisterToolsDelegate Delegate;
	return Delegate;
}

FAvaInteractiveToolsRegisterCategoriesDelegate& FAvaInteractiveToolsDelegates::GetRegisterCategoriesDelegate()
{
	static FAvaInteractiveToolsRegisterCategoriesDelegate Delegate;
	return Delegate;
}
