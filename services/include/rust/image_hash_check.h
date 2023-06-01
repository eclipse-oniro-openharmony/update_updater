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

#ifndef IMAGE_HASH_DATA_H
#define IMAGE_HASH_DATA_H

#include <cstddef>
#include <cstdint>

extern "C" {
// opaque data type for rust's HashSignedData
struct ImgHashData;
const ImgHashData *LoadImgHashData(uint8_t *hash_data, size_t len);
bool check_data_hash(const ImgHashData *img_hash_data,
    const char *file_name, uint32_t start, uint32_t end, const uint8_t *hash_value, size_t len);
void ReleaseImgHashData(const ImgHashData *hash_data);
}

#endif