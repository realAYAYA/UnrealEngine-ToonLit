#pragma once

#include "Modules/ModuleInterface.h"

class IGameServicesModule : public IModuleInterface
{
public:

	virtual void Start() = 0;
	virtual void Shutdown() = 0;

	virtual bool IsRunning() const = 0;
};