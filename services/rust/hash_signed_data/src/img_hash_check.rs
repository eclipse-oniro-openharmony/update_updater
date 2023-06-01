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

use std::collections::HashMap;
use crate::updaterlog;

const TLV_SIZE:usize = 6;
const HASH_INFO_SIZE: usize = 16;
const HASH_HEADER_SIZE: usize = 38;

#[derive(Debug)]
struct HashInfo {
    tlv_type: u16,
    tlv_len: u32,
    algorithm: u16,
    algo_size: u16,
    component_count: u16,
    block_size: u32
}

#[derive(Debug)]
struct HashHeader {
    image_name: String,
    hash_num: u16,
    img_size: u32
}

#[repr(C)]
#[derive(Debug)]
struct HashData {
    addr_star: u32,
    addr_end: u32,
    hash_data: Vec<u8>
}

#[derive(Debug)]
struct HashSign {
    tlv_type: u16,
    tlv_len: u32,
    sign_data: Vec<u8>
}

#[derive(Debug, PartialEq)]
pub struct ImgHashData {
    data: HashMap<String, HashMap<(u32, u32), Vec<u8>>>
}

trait TLVStruct {
    fn new() -> Self;

    fn read_from_le_bytes(&mut self, buffer: &[u8]) -> bool;
}

impl TLVStruct for HashInfo {
    fn new() -> HashInfo {
        HashInfo {tlv_type: 0, tlv_len: 0, algorithm: 0, algo_size: 0, component_count: 0, block_size: 0}
    }

    fn read_from_le_bytes(&mut self, buffer: &[u8]) -> bool {
        if buffer.len() < HASH_INFO_SIZE {
            updaterlog!(ERROR, "{} buffer is too small. {}", line!(), buffer.len());
            return false;
        }

        self.tlv_type = u16::from_le_bytes(buffer[0..2].try_into().unwrap());
        self.tlv_len = u32::from_le_bytes(buffer[2..6].try_into().unwrap());
        self.algorithm = u16::from_le_bytes(buffer[6..8].try_into().unwrap());
        self.algo_size = u16::from_le_bytes(buffer[8..10].try_into().unwrap());
        self.component_count = u16::from_le_bytes(buffer[10..12].try_into().unwrap());
        self.block_size = u32::from_le_bytes(buffer[12..16].try_into().unwrap());
        true
    }
}

impl TLVStruct for HashHeader {
    fn new() -> HashHeader {
        HashHeader {image_name: String::new(), hash_num: 0, img_size: 0}
    }

    fn read_from_le_bytes(&mut self, buffer: &[u8]) -> bool {
        if buffer.len() < TLV_SIZE {
            updaterlog!(ERROR, "{} buffer is too small. {}", line!(), buffer.len());
            return false;
        }

        self.image_name = String::from_utf8(Vec::from(&buffer[0..32])).unwrap().trim_end_matches('\0').to_owned();
        updaterlog!(INFO, "HashHeader  read_from_le_bytes image_name {}", self.image_name);
        self.hash_num = u16::from_le_bytes(buffer[32..34].try_into().unwrap());
        self.img_size = u32::from_le_bytes(buffer[34..].try_into().unwrap());
        true
    }
}

impl TLVStruct for HashData {
    fn new() -> HashData {
        HashData {addr_star: 0, addr_end: 0, hash_data: vec![]}
    }

    fn read_from_le_bytes(&mut self, buffer: &[u8]) -> bool {
        if buffer.len() < TLV_SIZE {
            updaterlog!(ERROR, "{} buffer is too small. {}", line!(), buffer.len());
            return false;
        }

        self.addr_star = u32::from_le_bytes(buffer[0..4].try_into().unwrap());
        self.addr_end = u32::from_le_bytes(buffer[4..8].try_into().unwrap());
        self.hash_data = Vec::from(&buffer[8..]);
        true
    }
}

impl TLVStruct for HashSign {
    fn new() -> HashSign {
        HashSign {tlv_type: 0, tlv_len: 0, sign_data: vec![]}
    }

    fn read_from_le_bytes(&mut self, buffer: &[u8]) -> bool {
        if buffer.len() < TLV_SIZE {
            updaterlog!(ERROR, "{} buffer is too small. {}", line!(), buffer.len());
            return false;
        }

        self.tlv_type = u16::from_le_bytes(buffer[0..2].try_into().unwrap());
        self.tlv_len = u32::from_le_bytes(buffer[2..6].try_into().unwrap());
        self.sign_data = Vec::from(&buffer[6..]);
        true
    }
}

#[allow(unused)]
impl ImgHashData {
    pub fn load_img_hash_data(buffer: &[u8]) -> Result<Self, String> {
        let mut offset = 0usize;
        let mut hash_info = HashInfo::new();
        hash_info.read_from_le_bytes( &buffer[..HASH_INFO_SIZE]);

        offset = HASH_INFO_SIZE + 2;
        let hash_data_len = u32::from_le_bytes(buffer[offset..4 + offset].try_into().unwrap());

        offset += 4;
        if buffer.len() < hash_data_len as usize {
            return Err(format!("{} buffer is too small. {}", line!(), buffer.len()));
        }

        let mut hash_data_map: HashMap<String, HashMap<(u32, u32), Vec<u8>>> = HashMap::new();
        while offset < hash_data_len as usize {
            let mut hash_header = HashHeader::new();
            hash_header.read_from_le_bytes(&buffer[offset..(HASH_HEADER_SIZE + offset)]);
            offset += HASH_HEADER_SIZE;

            let mut single_data: HashMap<(u32, u32), Vec<u8>> = HashMap::new();
            for i in 0..hash_header.hash_num {
                let mut hash_data = HashData::new();
                hash_data.read_from_le_bytes(&buffer[offset.. (offset + 8 + hash_info.algo_size as usize)]);
                single_data.insert((hash_data.addr_star, hash_data.addr_end), hash_data.hash_data);
                offset += (8 + hash_info.algo_size) as usize;
            }
            hash_data_map.insert(hash_header.image_name, single_data);
        }
        Ok(ImgHashData { data: hash_data_map })
    }

    pub fn check_img_hash(&self, img_name: String, start: u32, end: u32, hash_value: &[u8]) -> bool
    {
        let img_hash_map = match self.data.get(&img_name) {
            Some(img_hash_map)=> img_hash_map,
            _ => {
                updaterlog!(ERROR, "nothing found {}", img_name);
                return false;
            }
        };

        let hash_data = match img_hash_map.get(&(start, end)) {
            Some(hash_data)=> hash_data,
            _ => {
                updaterlog!(ERROR, "nothing found start: {}, end: {}", start, end);
                return false;
            }
        };

        if hash_data.len() != hash_value.len() {
            updaterlog!(ERROR, "hash value len is invalid {}", hash_value.len());
            return false;
        }
    
        for i in 0..hash_data.len() {
            if hash_data[i] != hash_value[i] {
                updaterlog!(ERROR, "hash value check fail");
                return false;
            }
        }
        true
    }
}
