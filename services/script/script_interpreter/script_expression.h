/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef USCRIPT_EXPRESSION_H
#define USCRIPT_EXPRESSION_H

#include <memory>
#include "script_context.h"

namespace Uscript {
class UScriptExpression;
using UScriptExpressionPtr = std::shared_ptr<UScriptExpression>;

class ScriptParams;
class ScriptFunction;

class UScriptExpression {
public:
    enum ExpressionType {
        EXPRESSION_TYPE_INTERGER,
        EXPRESSION_TYPE_FLOAT,
        EXPRESSION_TYPE_STRING,
        EXPRESSION_TYPE_IDENTIFIER,
        EXPRESSION_TYPE_BINARY,
        EXPRESSION_TYPE_ASSIGN,
        EXPRESSION_TYPE_INV,
        EXPRESSION_TYPE_FUNC,
        EXPRESSION_TYPE_INVALID,
    };
    enum ExpressionAction {
        ADD_OPERATOR = 1,
        SUB_OPERATOR,
        MUL_OPERATOR,
        DIV_OPERATOR,
        GT_OPERATOR,
        GE_OPERATOR,
        LT_OPERATOR,
        LE_OPERATOR,
        EQ_OPERATOR,
        NE_OPERATOR,
        AND_OPERATOR,
        OR_OPERATOR
    };

public:
    explicit UScriptExpression(ExpressionType expressType);
    virtual ~UScriptExpression();

    virtual UScriptValuePtr Execute(ScriptInterpreter &inter, UScriptContextPtr local);
    static UScriptExpression* CreateExpression(ExpressionType expressType)
    {
        return new UScriptExpression(expressType);
    }
    ExpressionType GetExpressType() const
    {
        return expressType_;
    }

private:
    ExpressionType expressType_;
};

class IntegerExpression : public UScriptExpression {
public:
    explicit IntegerExpression(int v) : UScriptExpression(UScriptExpression::EXPRESSION_TYPE_INTERGER)
    {
        value_ = v;
    }

    ~IntegerExpression() override {}

    UScriptValuePtr Execute(ScriptInterpreter &inter, UScriptContextPtr local) override;

    static UScriptExpression* CreateExpression(int value)
    {
        return new IntegerExpression(value);
    }
private:
    int value_;
};

class FloatExpression : public UScriptExpression {
public:
    explicit FloatExpression(float v) : UScriptExpression(UScriptExpression::EXPRESSION_TYPE_FLOAT)
    {
        value_ = v;
    }

    ~FloatExpression() override {}

    UScriptValuePtr Execute(ScriptInterpreter &inter, UScriptContextPtr local) override;

    static UScriptExpression* CreateExpression(float value)
    {
        return new FloatExpression(value);
    }

private:
    float value_;
};

class StringExpression : public UScriptExpression {
public:
    explicit StringExpression(const std::string str) : UScriptExpression(UScriptExpression::EXPRESSION_TYPE_STRING),
        value_(str) {}

    ~StringExpression() override {}

    UScriptValuePtr Execute(ScriptInterpreter &inter, UScriptContextPtr local) override;

    static UScriptExpression* CreateExpression(const std::string value)
    {
        return new StringExpression(value);
    }
private:
    std::string value_;
};

class IdentifierExpression : public UScriptExpression {
public:
    explicit IdentifierExpression(const std::string str)
        : UScriptExpression(UScriptExpression::EXPRESSION_TYPE_IDENTIFIER), identifier_(str) {}

    ~IdentifierExpression() override {}

    UScriptValuePtr Execute(ScriptInterpreter &inter, UScriptContextPtr local) override;

    static UScriptExpression* CreateExpression(const std::string value)
    {
        return new IdentifierExpression(value);
    }

    const std::string GetIdentifier() const
    {
        return identifier_;
    }

    static int32_t GetIdentifierName(UScriptExpression *expression, std::string &name);
private:
    std::string identifier_;
};

class BinaryExpression : public UScriptExpression {
public:
    BinaryExpression(ExpressionAction action, UScriptExpression *left, UScriptExpression *right)
        : UScriptExpression(UScriptExpression::EXPRESSION_TYPE_BINARY), action_(action), left_(left), right_(right) {}

    ~BinaryExpression() override;

    UScriptValuePtr Execute(ScriptInterpreter &inter, UScriptContextPtr local) override;

    static UScriptExpression* CreateExpression(ExpressionAction action, UScriptExpression *left,
        UScriptExpression *right);
private:
    UScriptExpression::ExpressionAction action_;
    UScriptExpression* left_ = nullptr;
    UScriptExpression* right_ = nullptr;
};

class AssignExpression : public UScriptExpression {
public:
    AssignExpression(const std::string identifier, UScriptExpression *expression)
        : UScriptExpression(UScriptExpression::EXPRESSION_TYPE_ASSIGN), identifier_(identifier),
        expression_(expression) {}

    ~AssignExpression() override;

    UScriptValuePtr Execute(ScriptInterpreter &inter, UScriptContextPtr local) override;

    void AddIdentifier(const std::string &identifier);

    static UScriptExpression* CreateExpression(const std::string identifier, UScriptExpression *expression);
    static UScriptExpression* AddIdentifier(UScriptExpression *expression, const std::string identifier);
private:
    std::string identifier_;
    std::vector<std::string> multipleIdentifiers_; // 最大支持4个参数a,b,v = 1
    UScriptExpression* expression_ = nullptr;
};

class FunctionCallExpression : public UScriptExpression {
public:
    FunctionCallExpression(const std::string identifier, ScriptParams *params)
        : UScriptExpression(UScriptExpression::EXPRESSION_TYPE_FUNC), functionName_(identifier), params_(params) {}

    ~FunctionCallExpression() override;

    UScriptValuePtr Execute(ScriptInterpreter &inter, UScriptContextPtr local) override;

    static UScriptExpression* CreateExpression(const std::string identifier, ScriptParams *params);
private:
    std::string functionName_;
    ScriptParams* params_ = nullptr;
};
} // namespace Uscript
#endif // _HS_EXPRESSION_H
