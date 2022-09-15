#include <cstring>
#include <iostream>
#include "script_manager.h"
#include "script/script_instruction.h"

using namespace std;
using namespace Uscript;

class UserInstruction1 : public UScriptInstruction {
public:
    virtual int32_t Execute(UScriptEnv &env, UScriptContext &context) override
    {
        context.PushParam("user instruction1");
        return USCRIPT_SUCCESS;
    }
};

class UserInstruction2 : public UScriptInstruction {
public:
    virtual int32_t Execute(UScriptEnv &env, UScriptContext &context) override
    {
        context.PushParam("user instruction2");
        return USCRIPT_SUCCESS;
    }
};

class UserInstructionAbort : public UScriptInstruction {
public:
    virtual int32_t Execute(UScriptEnv &env, UScriptContext &context) override
    {
        context.PushParam("user instruction2");
        return USCRIPT_SUCCESS;
    }
};

class UserInstructionFactory : public UScriptInstructionFactory {
public:
    virtual int32_t CreateInstructionInstance(UScriptInstructionPtr& instr, const std::string& name) override
    {
        if (name == "uInstruction1") {
            instr = new (std::nothrow) UserInstruction1(); // 为何不加nothrow？？？
        } else if (name == "uInstruction2") {
            instr = new (std::nothrow) UserInstruction2();
        } else if (name == "uInstruction3") {
            instr = nullptr; // mock new failed scene
        } else if (name == "abort") {
            instr = new (std::nothrow) UserInstructionAbort(); // mock reserved error
        } else {
            return USCRIPT_NOTEXIST_INSTRUCTION;
        }
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

extern "C"  __attribute__((visibility("default"))) Uscript::UScriptInstructionFactoryPtr GetInstructionFactory()
{
    return new (std::nothrow) UserInstructionFactory;
}

extern "C" __attribute__((visibility("default"))) void ReleaseInstructionFactory(Uscript::UScriptInstructionFactoryPtr p)
{
    delete p;
}