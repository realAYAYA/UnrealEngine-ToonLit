// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveExpressionModule.h"
#include "Modules/ModuleManager.h"

namespace UE::CurveExpression
{

class FModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

}

DEFINE_LOG_CATEGORY(LogCurveExpression);

IMPLEMENT_MODULE(UE::CurveExpression::FModule, CurveExpression)
