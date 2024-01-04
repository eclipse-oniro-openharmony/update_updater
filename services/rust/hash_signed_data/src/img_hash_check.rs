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

use std::default::Default;
use std::collections::HashMap;
use std::hash::Hash;
use std::cmp::Eq;
use std::cmp::PartialEq;
use std::mem::size_of;
use crate::updaterlog;

const TLV_SIZE: usize = 6;
const HASH_INFO_SIZE: usize = 16;
const IMG_NAME_SIZE: usize = 32;

#[derive(Debug)]
struct HashInfo {
    tlv_type: u16,
    tlv_len: u32,
    algorithm: u16,
    algo_size: u16,
    component_count: u16,
    block_size: u32
}

pub trait ReadLeBytes {
    fn read_le_bytes(buffer: &[u8]) -> Self;
}

impl ReadLeBytes for u32 {
    fn read_le_bytes(buffer: &[u8]) -> Self {
        u32::from_le_bytes(buffer[..].try_into().unwrap())
    }
}

impl ReadLeBytes for u64 {
    fn read_le_bytes(buffer: &[u8]) -> Self {
        u64::from_le_bytes(buffer[..].try_into().unwrap())
    }
}

#[derive(Debug)]
struct HashHeader<T: ReadLeBytes> {
    image_name: String,
    hash_num: u16,
    img_size: T
}

#[repr(C)]
#[derive(Debug)]
struct HashData<T: ReadLeBytes> {
    addr_star: T,
    addr_end: T,
    hash_data: Vec<u8>
}

#[derive(Debug)]
struct HashSign {
    tlv_type: u16,
    tlv_len: u32,
    sign_data: Vec<u8>
}

#[derive(Hash, Eq, PartialEq, Debug)]
struct Tuple<T> (T,  T);

/// PartialEq
#[derive(Debug)]
pub struct ImgHashData<T: Hash + Eq + PartialEq> {
    data: HashMap<String, HashMap<Tuple<T>, Vec<u8>>>
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

impl<T: Default + ReadLeBytes> TLVStruct for HashHeader<T> {
    fn new() -> HashHeader<T> {
        HashHeader {image_name: String::new(), hash_num: 0, img_size: Default::default()}
    }

    fn read_from_le_bytes(&mut self, buffer: &[u8]) -> bool {
        if buffer.len() < TLV_SIZE {
            updaterlog!(ERROR, "{} buffer is too small. {}", line!(), buffer.len());
            return false;
        }

        self.image_name = String::from_utf8(Vec::from(&buffer[0..32])).unwrap().trim_end_matches('\0').to_owned();
        updaterlog!(INFO, "HashHeader  read_from_le_bytes image_name {}", self.image_name);
        self.hash_num = u16::from_le_bytes(buffer[32..34].try_into().unwrap());
        self.img_size =  T::read_le_bytes(&buffer[34..]);
        true
    }
}

impl<T: Default + ReadLeBytes> TLVStruct for HashData<T> {
    fn new() -> HashData<T> {
        HashData {addr_star: Default::default(), addr_end: Default::default(), hash_data: vec![]}
    }

    fn read_from_le_bytes(&mut self, buffer: &[u8]) -> bool {
        if buffer.len() < TLV_SIZE {
            updaterlog!(ERROR, "{} buffer is too small. {}", line!(), buffer.len());
            return false;
        }

        self.addr_star = T::read_le_bytes(&buffer[0..size_of::<T>()]);
        self.addr_end = T::read_le_bytes(&buffer[size_of::<T>()..(2*size_of::<T>())]);
        self.hash_data = Vec::from(&buffer[(2*size_of::<T>())..]);
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
impl<T: Default + ReadLeBytes + Hash + Eq + PartialEq> ImgHashData<T> {
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

        let mut hash_data_map: HashMap<String, HashMap<Tuple<T>, Vec<u8>>> = HashMap::new();
        let hash_header_size: usize = IMG_NAME_SIZE + size_of::<u16>() + size_of::<T>();
        updaterlog!(INFO, "HashHeader  read_from_le_bytes hash_header_size {}", hash_header_size);
        while offset < hash_data_len as usize {
            let mut hash_header: HashHeader<T> = HashHeader::new();
            hash_header.read_from_le_bytes(&buffer[offset..(hash_header_size + offset)]);
            offset += hash_header_size;

            let mut single_data: HashMap<Tuple<T>, Vec<u8>> = HashMap::new();
            for i in 0..hash_header.hash_num {
                let mut hash_data = HashData::new();
                hash_data.read_from_le_bytes(&buffer[offset.. (offset + (2*size_of::<T>()) + hash_info.algo_size as usize)]);
                let mut addr_tuple = Tuple(hash_data.addr_star, hash_data.addr_end);
                single_data.insert(addr_tuple, hash_data.hash_data);
                offset += 2*size_of::<T>() + (hash_info.algo_size) as usize;
            }
            hash_data_map.insert(hash_header.image_name, single_data);
        }
        Ok(ImgHashData { data: hash_data_map })
    }

    pub fn check_img_hash(&self, img_name: String, start: T, end: T, hash_value: &[u8]) -> bool
        where T: std::cmp::Eq, T: std::cmp::PartialEq, T: std::fmt::Display
    {
        let img_hash_map = match self.data.get(&img_name) {
            Some(img_hash_map)=> img_hash_map,
            _ => {
                updaterlog!(ERROR, "nothing found {}", img_name);
                return false;
            }
        };

        let mut addr_tuple = Tuple(start, end);
        let hash_data = match img_hash_map.get(&addr_tuple) {
            Some(hash_data)=> hash_data,
            _ => {
                updaterlog!(ERROR, "nothing found start: {}, end: {}", addr_tuple.0, addr_tuple.1);
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
