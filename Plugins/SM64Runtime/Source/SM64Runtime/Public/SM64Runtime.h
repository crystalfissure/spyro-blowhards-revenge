#pragma once

#include "Modules/ModuleManager.h"

class FSM64RuntimeModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
