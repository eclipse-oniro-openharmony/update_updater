/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd.
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

#ifndef UPDATER_UI_TOOLS_H
#define UPDATER_UI_TOOLS_H

#include <string>

namespace Updater {
class UpdaterUiTools {
public:
    struct BitmapInfoHeader {
        uint32_t bfSize;
        uint16_t bfReserved1;
        uint16_t bfReserved2;
        uint32_t bfOffBits;
        uint32_t biSize;
        int32_t biWidth;
        int32_t biHeight;
        uint16_t biPlanes;
        uint16_t biBitCount;
        uint32_t biCompression;
        uint32_t biSizeImage;
        uint32_t biXPelsPerMeter;
        uint32_t biYPelsPerMeter;
        uint32_t biClrUsed;
        uint32_t biClrImportant;
    };

    UpdaterUiTools() = default;
    virtual ~UpdaterUiTools() = default;
    static void SaveUxBuffToFile(const std::string &filePath);
};
} // namespace Updater
#endif // UPDATER_UI_TOOLS_H
