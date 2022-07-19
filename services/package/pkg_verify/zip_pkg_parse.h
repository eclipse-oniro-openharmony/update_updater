/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef PKG_PARSE_H
#define PKG_PARSE_H

#include "pkg_stream.h"
#include "pkg_verify_util.h"

namespace Hpackage {
class ZipPkgParse {
public:
    ZipPkgParse() {};

    ~ZipPkgParse() {};

    int32_t CheckZipPkg(const Hpackage::PkgStreamPtr pkgStream) const;

    int32_t ParseZipPkg(Hpackage::PkgStreamPtr pkgStream, size_t &signatureStart,
        size_t &signatureSize) const;

    int32_t WriteZipSignedData(Hpackage::PkgStreamPtr outStream,
        const Hpackage::PkgBuffer &p7Data, Hpackage::PkgStreamPtr inStream) const;

private:
    int32_t ParsePkgFooter(const uint8_t *footer, size_t length, uint16_t &commentAppendLen,
        uint16_t &commentTotalLen) const;

    int32_t WriteSourcePackageData(Hpackage::PkgStreamPtr outStream, Hpackage::PkgStreamPtr inStream,
        size_t wirteLen) const;

    int32_t WriteFooter(Hpackage::PkgStreamPtr outStream, uint16_t zipCommentLen, size_t &offset) const;

    int32_t CheckZipEocd(const uint8_t *eocd, size_t length, uint16_t commentTotalLen) const;
};
} // namespace Hpackage
#endif
