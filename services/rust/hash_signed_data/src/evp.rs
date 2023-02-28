/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

//! base64 decode
use crate::ffi;

#[allow(unused)]
pub fn get_b64_decode_len(in_data: &[u8]) -> Result<(usize, usize), String>
{
    let len = in_data.len();
    if len == 0 || len % 4 != 0 { // base64 content len must be a multiple of 4
        return Err("indata is not an valid base64 data".to_string());
    }
    let mut padding = 0usize;
    if in_data[len - 1] == b'=' {
        padding += 1;
    }
    if in_data[len - 2] == b'=' {
        padding += 1;
    }
    Ok(((len >> 2) * 3, padding)) // 3 binary format bytes => 24bits => 4 base64 format character
}

#[allow(unused)]
pub fn evp_decode_block(in_data: &[u8]) -> Result<Vec<u8>, String>
{
    let (out_len, padding) = get_b64_decode_len(in_data)?;
    let mut out_data = Vec::<u8>::new();
    out_data.resize(out_len, 0);
    if unsafe { ffi::EVP_DecodeBlock(out_data.as_mut_ptr(), in_data.as_ptr(), in_data.len() as i32) } <= 0 {
        return Err("evp decode failed".to_string());
    }
    out_data.truncate(out_len - padding);
    Ok(out_data)
}
