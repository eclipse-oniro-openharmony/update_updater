#include <cstring>
#include "script_manager.h"
#include "script/script_instruction.h"

using namespace std;
using namespace Uscript;

class UserInstruction1 : public UScriptInstruction {
public:
    virtual int32_t Execute(UScriptEnv &env, UScriptContext &context) override
    {
        return USCRIPT_SUCCESS;
    }
};

class UserInstruction2 : public UScriptInstruction {
public:
    virtual int32_t Execute(UScriptEnv &env, UScriptContext &context) override
    {
        return USCRIPT_SUCCESS;
    }
};

class UserInstructionFactory : public UScriptInstructionFactory {
public:
    virtual int32_t CreateInstructionInstance(UScriptInstructionPtr& instr, const std::string& name) override
    {
        return USCRIPT_SUCCESS;
    }
    UserInstructionFactory()
    {
        std::cout << "UserInstructionFactory" << std::endl << std::flush;
    }
    ~UserInstructionFactory()
    {
        std::cout << "~UserInstructionFactory" << std::endl << std::flush;
    }
};

extern "C" __attribute__((visibility("default"))) Uscript::UScriptInstructionFactoryPtr GetInstructionFactory()
{
    return new (std::nothrow) UserInstructionFactory;
}

extern "C" __attribute__((visibility("default"))) void ReleaseInstructionFactory(Uscript::UScriptInstructionFactoryPtr p)
{
    delete p;
}