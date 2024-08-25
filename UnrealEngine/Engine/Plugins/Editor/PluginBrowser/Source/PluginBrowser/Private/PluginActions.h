// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class IPlugin; 
class SWidget; 

namespace PluginActions
{
    void PackagePlugin(TSharedRef<IPlugin> Plugin, TSharedPtr<SWidget> ParentWidget);
}