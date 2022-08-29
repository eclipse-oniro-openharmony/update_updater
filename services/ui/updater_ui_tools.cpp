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

#include "updater_ui_tools.h"
#include "common/screen.h"
#include "draw/draw_utils.h"
#include "gfx_utils/file.h"
#include "gfx_utils/graphic_math.h"
#include "log/log.h"

using namespace OHOS;

namespace Updater {
constexpr uint8_t BITMAP_HEADER_SIZE = 54; /* 54 is bitmap header size */

void UpdaterUiTools::SaveUxBuffToFile(const std::string &filePath)
{
    ImageInfo imageBit {};
    if (!(Screen::GetInstance().GetCurrentScreenBitmap(imageBit))) {
        LOG(ERROR) << "uxtools get bitmap failed";
        return;
    }

    int32_t fd = open(filePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, DEFAULT_FILE_PERMISSION);
    if (fd == -1) {
        LOG(ERROR) << "uxtools open file failed";
        ImageCacheFree(imageBit);
        return;
    }

    uint8_t sizeByColorMode = DrawUtils::GetByteSizeByColorMode(OHOS::ColorMode::ARGB8888);
    uint16_t bfType = 0x4D42;
    struct BitmapInfoHeader bitmapInfo = {
        .bfSize = imageBit.dataSize + BITMAP_HEADER_SIZE,
        .bfOffBits = BITMAP_HEADER_SIZE,
        .biSize = 40, /* 40 is bitmap information header size */
        .biWidth = imageBit.header.width,
        .biHeight = -imageBit.header.height,
        .biPlanes = 1,
        .biBitCount = sizeByColorMode * 8, /* 8 is uint8_t bit */
        .biSizeImage = imageBit.dataSize,
    };
    if (write(fd, &bfType, sizeof(bfType)) > 0 && write(fd, &bitmapInfo, sizeof(bitmapInfo)) > 0 &&
        write(fd, imageBit.data, imageBit.dataSize) > 0) {
        LOG(INFO) << "uxtools save file done";
    } else {
        LOG(ERROR) << "uxtools save file failed";
    }
    ImageCacheFree(imageBit);
    close(fd);

    return;
}
} // namespace Updater
