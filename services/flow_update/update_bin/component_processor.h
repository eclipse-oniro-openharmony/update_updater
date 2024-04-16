/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UPDATER_COMPONENT_PROCESSOR
#define UPDATER_COMPONENT_PROCESSOR

#include <cstdio>
#include <map>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include "applypatch/data_writer.h"
#include "applypatch/update_progress.h"
#include "macros.h"
#include "pkg_manager.h"
#include "script_instruction.h"
#include "script_manager.h"
#include "utils.h"

namespace Updater {
class ComponentProcessor {
public:
    ComponentProcessor(const std::string &name, const size_t len)
        : name_(name), totalSize_(len) {}
    virtual ~ComponentProcessor() {}
    virtual int32_t PreProcess(Uscript::UScriptEnv &env)
    {
        return Uscript::USCRIPT_SUCCESS;
    }
    virtual int32_t DoProcess(Uscript::UScriptEnv &env)
    {
        return Uscript::USCRIPT_SUCCESS;
    }
    virtual int32_t PostProcess(Uscript::UScriptEnv &env)
    {
        return Uscript::USCRIPT_SUCCESS;
    }

    virtual void SetPkgFileSize(const size_t offset, const size_t fileSize)
    {
        readOffset_ = offset;
        pkgFileSize_ = fileSize;
    }

    void SetPkgFileInfo(size_t offset, size_t fileSize, float proportion)
    {
        readOffset_ = offset;
        pkgFileSize_ = fileSize;
        proportion_ = proportion;
    }

    void UpdateProgress(size_t writeSize)
    {
        if (pkgFileSize_ != 0) {
            readOffset_ += writeSize;
            SetUpdateProgress(static_cast<float>(writeSize) / pkgFileSize_ * proportion_);
        }
    }
protected:
    std::string name_;
    size_t totalSize_ {};
    size_t readOffset_ {};
    size_t pkgFileSize_ {};
    float proportion_ {};
};

class ComponentProcessorFactory {
    DISALLOW_COPY_MOVE(ComponentProcessorFactory);
public:
    typedef std::unique_ptr<ComponentProcessor> (*Constructor)(const std::string &name, const uint8_t len);
    using ClassMap = std::map<std::string, Constructor>;
    void RegisterProcessor(Constructor constructor, std::vector<std::string> &nameList);
    std::unique_ptr<ComponentProcessor> GetProcessor(const std::string &name, const uint8_t len) const;
    static ComponentProcessorFactory &GetInstance();

private:
    ComponentProcessorFactory() {}
    ~ComponentProcessorFactory() {}
    ClassMap m_constructorMap;
};

template <typename SubClassName>
std::unique_ptr<ComponentProcessor> NewObject(const std::string &name, const uint8_t len)
{
    return std::make_unique<SubClassName>(name, len);
}

#undef REGISTER_PROCESSOR
#define REGISTER_PROCESSOR(subClassName, ...)                                                  \
    extern "C" __attribute__((constructor)) void subClassName##_RegisterClass()   \
    {                                                                                 \
        std::vector<std::string> nameList {__VA_ARGS__};                                   \
        ComponentProcessorFactory::GetInstance().RegisterProcessor(NewObject<subClassName>,     \
            nameList);                                                 \
    }

class VersionCheckProcessor : public ComponentProcessor {
    DISALLOW_COPY_MOVE(VersionCheckProcessor);
public:
    VersionCheckProcessor(const std::string &name, const size_t len)
        : ComponentProcessor(name, len) {}
    ~VersionCheckProcessor() override {}
    int32_t DoProcess(Uscript::UScriptEnv &env) override;
};

class BoardIdCheckProcessor : public ComponentProcessor {
    DISALLOW_COPY_MOVE(BoardIdCheckProcessor);
public:
    BoardIdCheckProcessor(const std::string &name, const size_t len)
        : ComponentProcessor(name, len) {}
    ~BoardIdCheckProcessor() override {}
    int32_t DoProcess(Uscript::UScriptEnv &env) override;
};

class RawImgProcessor : public ComponentProcessor {
    DISALLOW_COPY_MOVE(RawImgProcessor);
public:
    RawImgProcessor(const std::string &name, const size_t len)
        : ComponentProcessor(name, len) {}
    ~RawImgProcessor() override {}
    int32_t PreProcess(Uscript::UScriptEnv &env) override;
    int32_t DoProcess(Uscript::UScriptEnv &env) override;
    int32_t PostProcess(Uscript::UScriptEnv &env) override;
private:
    int GetWritePathAndOffset(const std::string &partitionName, std::string &writePath, uint64_t &offset,
                              uint64_t &partitionSize);
    virtual int RawImageWriteProcessor(const Hpackage::PkgBuffer &buffer, size_t size, size_t start,
                               bool isFinish, const void* context);
    std::unique_ptr<DataWriter> writer_ = nullptr;
};

class SkipImgProcessor : public ComponentProcessor {
    DISALLOW_COPY_MOVE(SkipImgProcessor);
public:
    SkipImgProcessor(const std::string &name, const size_t len)
        : ComponentProcessor(name, len) {}
    ~SkipImgProcessor() override {}
    int32_t PreProcess(Uscript::UScriptEnv &env) override;
    int32_t DoProcess(Uscript::UScriptEnv &env) override;
    int32_t PostProcess(Uscript::UScriptEnv &env) override;
private:
    int SkipImageWriteProcessor(const Hpackage::PkgBuffer &buffer, size_t size, [[maybe_unused]]size_t start,
                                [[maybe_unused]]bool isFinish, [[maybe_unused]]const void* context);
    std::unique_ptr<DataWriter> writer_ = nullptr;
};
} // namespace Updater
#endif // UPDATER_COMPONENT_PROCESSOR
