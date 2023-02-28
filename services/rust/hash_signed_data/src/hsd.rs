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

//! hsd is abbreviation for hash signed data

use crate::evp;

#[derive(Debug, PartialEq)]
pub struct SingleSignedData {
    name: String,
    signature: String
}

#[derive(Debug, PartialEq)]
pub struct HashSignedData {
    data: Vec<SingleSignedData>
}

impl TryFrom<&str> for SingleSignedData {
    type Error = String;
    fn try_from(value: &str) -> Result<Self, Self::Error> {
        const KEY_STR : &str = "Name:";
        const VALUE_STR : &str = "signed-data:";
        let res: Vec<&str> = value.split('\n').filter(|&s| !s.is_empty()).collect();
        if res.len() != 2 || !res[0].starts_with(KEY_STR) || !res[1].starts_with(VALUE_STR) {
            return Err(format!("format error for single signed data {}", value));
        }
        Ok(SingleSignedData { name: res[0][KEY_STR.len()..].trim().to_owned(),
                signature: res[1][VALUE_STR.len()..].trim().to_owned()})
    }
}

impl TryFrom<&str> for HashSignedData {
    type Error = String;
    fn try_from(value: &str) -> Result<Self, Self::Error> {
        if value.is_empty() {
            return Err("empty value not allowed".to_string());
        }
        let all_hash_data: Vec<&str> = value.split("\n\n").filter(|&s| !s.is_empty()).collect();
        let mut hash_signed_data = Vec::<SingleSignedData>::new();
        for single_hash_data in all_hash_data {
            hash_signed_data.push(SingleSignedData::try_from(single_hash_data)?);
        }
        Ok(HashSignedData { data: hash_signed_data })
    }
}

#[allow(unused)]
impl HashSignedData {
    pub fn get_sig_for_file(&self, file_name: &str) -> Result<Vec<u8>, String>
    {
        match self.data.iter().find(|&single_line| single_line.name == *file_name) {
            Some(single_data) => {
                let single_data = single_data.signature.as_bytes();
                evp::evp_decode_block(single_data)
            },
            None => {
                Err(format!("file name {} invalid, please check your input", file_name))
            }
        }
    }
}
