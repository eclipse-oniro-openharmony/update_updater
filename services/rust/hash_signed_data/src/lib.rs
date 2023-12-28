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

//! This lib is used by updater to parse and get sig from hash signe data file:

mod hsd;
mod evp;
mod ffi;
mod img_hash_check;
mod macros;

use core::{ffi::{c_char, CStr}, mem::ManuallyDrop, ptr};
use hsd::HashSignedData;
use img_hash_check::ImgHashData;
use img_hash_check::ReadLeBytes;

/// load hash signed data from buffer, then you can verify them by VerifyHashBySignedData
///
/// # Safety
///
/// signed_data must contain a valid nul terminator at the end
#[no_mangle]
pub unsafe extern fn LoadHashSignedData(signed_data: *const c_char)
    -> *const HashSignedData
{
    if signed_data.is_null() {
        updaterlog!(ERROR, "signed data is null");
        return ptr::null();
    }

    let signed_data_str: &CStr = unsafe { CStr::from_ptr(signed_data) };
    let hsd = signed_data_str.to_str();
    if hsd.is_err() {
        updaterlog!(ERROR, "hash signed data str format is invalid {:?}", signed_data_str);
        return ptr::null();
    }

    match HashSignedData::try_from(hsd.unwrap()) {
        Ok(hsd) => {
            updaterlog!(INFO, "hash signed data parse successful!");
            Box::into_raw(Box::new(hsd))
        },
        Err(err) => {
            updaterlog!(ERROR, "hash signed data parse failed, err is {}", err);
            ptr::null()
        }
    }
}

/// Get signature of file from hash signed data
///
/// # Safety
///
/// file_name should be a valid utf8 str, ended with a nul terminator
#[no_mangle]
pub unsafe extern fn GetSigFromHashData(signed_data: *const HashSignedData,
    out: *mut u8, out_len: usize, file_name: *const c_char) -> usize
{
    if out.is_null() || file_name.is_null() || signed_data.is_null() {
        updaterlog!(ERROR, "input invalid, null status hash:{} file_name:{} signed_data:{}",
            out.is_null(), file_name.is_null(), signed_data.is_null());
        return 0;
    }
    let signed_data = ManuallyDrop::new(unsafe { &*signed_data });
    let file_name_c_str: &CStr = unsafe { CStr::from_ptr(file_name) };
    let file_name = match file_name_c_str.to_str() {
        Ok(file_name) => file_name,
        Err(_) => {
            updaterlog!(ERROR, "filename is invalid utf8 str");
            return 0;
        }
    };
    let sig = match signed_data.get_sig_for_file(file_name) {
        Ok(sig) => sig,
        Err(err) => {
            unsafe { ffi::ERR_print_errors_cb(ffi::err_print_cb, ptr::null_mut()); }
            updaterlog!(ERROR, "get sig for file {} failed, err is {}", file_name, err);
            return 0;
        }
    };
    if sig.len() > out_len {
        updaterlog!(ERROR, "out is too small to hold signature");
        return 0;
    }
    unsafe { ptr::copy_nonoverlapping(sig.as_ptr(), out, sig.len()); }
    // hash is owned by a vector in c++, it's memory is allocated in c++, so need to forget it in rust
    updaterlog!(INFO, "get sig succeed for {}", file_name);
    sig.len()
}

/// release hash signed data when you no longer need it
///
/// # Safety
///
/// HashSignedData should be a return value of LoadHashSignedData
#[no_mangle]
pub unsafe extern fn ReleaseHashSignedData(signed_data: *const HashSignedData)
{
    if signed_data.is_null() {
        updaterlog!(ERROR, "signed data is null");
        return;
    }
    unsafe { drop(Box::from_raw(signed_data as *mut HashSignedData)); }
    updaterlog!(INFO, "release hash signed data");
}

/// load hash signed data from buffer, then you can verify them by check_data_hash
///
/// # Safety
///
/// hash_data must contain a valid nul terminator at the end
#[no_mangle]
pub unsafe extern fn LoadImgHashData(hash_data: *const u8, len: usize)
    -> *const ImgHashData<u32>
{
    if hash_data.is_null() {
        updaterlog!(ERROR, "hash data is null");
        return ptr::null();
    }

    let hash_data_vec: Vec<u8> = unsafe {Vec::from_raw_parts(hash_data as *mut u8, len, len)};
    match ImgHashData::load_img_hash_data(&hash_data_vec[..]) {
        Ok(hash_data) => {
            std::mem::forget(hash_data_vec);
            updaterlog!(INFO, "hash data parse successful!");
            Box::into_raw(Box::new(hash_data))
        },
        Err(err) => {
            std::mem::forget(hash_data_vec);
            updaterlog!(ERROR, "hash data parse failed, err is {}", err);
            ptr::null()
        }
    }
}

/// load hash signed data from buffer, then you can verify them by check_data_hash
///
/// # Safety
///
/// hash_data must contain a valid nul terminator at the end
#[no_mangle]
pub unsafe extern fn LoadImgHashDataNew(hash_data: *const u8, len: usize)
    -> *const ImgHashData<u64>
{
    if hash_data.is_null() {
        updaterlog!(ERROR, "hash data is null");
        return ptr::null();
    }

    let hash_data_vec: Vec<u8> = unsafe {Vec::from_raw_parts(hash_data as *mut u8, len, len)};
    match ImgHashData::load_img_hash_data(&hash_data_vec[..]) {
        Ok(hash_data) => {
            std::mem::forget(hash_data_vec);
            updaterlog!(INFO, "hash data parse successful!");
            Box::into_raw(Box::new(hash_data))
        },
        Err(err) => {
            std::mem::forget(hash_data_vec);
            updaterlog!(ERROR, "hash data parse failed, err is {}", err);
            ptr::null()
        }
    }
}

/// check hash data from buffer
///
/// # Safety
///
/// signed_data must contain a valid nul terminator at the end
// #[no_mangle]
pub unsafe extern fn check_data_hash_template<T>(img_hash_data: *const ImgHashData<T>,
    img_name: *const c_char, start: T, end: T, hash_value: *const u8,  len: usize) -> bool
    where T: ReadLeBytes + std::hash::Hash + std::cmp::Eq + std::fmt::Display + std::default::Default
{
    if img_hash_data.is_null() || img_name.is_null() || hash_value.is_null() {
        updaterlog!(ERROR, "input invalid, null status img_hash_data:{} img_name:{} hash_value:{}",
        img_hash_data.is_null(), img_name.is_null(), hash_value.is_null());
        return false;
    }

    let hash_data = ManuallyDrop::new( unsafe { &*img_hash_data });
    let img_name_c_str: &CStr = unsafe { CStr::from_ptr(img_name) };
    let img_name = match img_name_c_str.to_str() {
        Ok(img_name) => img_name.to_owned(),
        Err(_) => {
            updaterlog!(ERROR, "img_name is invalid utf8 str");
            return false;
        }
    };

    let hash_value_vec: Vec<u8> = unsafe {Vec::from_raw_parts(hash_value as *mut u8, len, len)};
    updaterlog!(INFO, "check_data_hash, img_name: {}, start: {}, hash_value_vec: {:?}", img_name, start, hash_value_vec);
    let is_valid = hash_data.check_img_hash(img_name, start, end, &hash_value_vec[..]);
    std::mem::forget(hash_value_vec);
    is_valid
}

/// check hash data from buffer
///
/// # Safety
///
/// signed_data must contain a valid nul terminator at the end
#[no_mangle]
pub unsafe extern fn check_data_hash(img_hash_data: *const ImgHashData<u32>,
    img_name: *const c_char, start: u32, end: u32, hash_value: *const u8,  len: usize) -> bool
{
    check_data_hash_template(img_hash_data, img_name, start, end, hash_value, len)
}

/// check hash data from buffer
///
/// # Safety
///
/// signed_data must contain a valid nul terminator at the end
#[no_mangle]
pub unsafe extern fn CheckDataHashNew(img_hash_data: *const ImgHashData<u64>,
    img_name: *const c_char, start: u64, end: u64, hash_value: *const u8,  len: usize) -> bool
{
    check_data_hash_template(img_hash_data, img_name, start, end, hash_value, len)
}

/// release hash signed data when you no longer need it
///
/// # Safety
///
/// HashSignedData should be a return value of LoadHashSignedData
#[no_mangle]
pub unsafe extern fn ReleaseImgHashData(hash_data: *const ImgHashData<u32>)
{
    if hash_data.is_null() {
        updaterlog!(ERROR, "image hash data is null");
        return;
    }
    unsafe { drop(Box::from_raw(hash_data as *mut ImgHashData<u32>)); }
    updaterlog!(INFO, "release image hash data");
}

/// release hash signed data when you no longer need it
///
/// # Safety
///
/// HashSignedData should be a return value of LoadHashSignedData
#[no_mangle]
pub unsafe extern fn ReleaseImgHashDataNew(hash_data: *const ImgHashData<u64>)
{
    if hash_data.is_null() {
        updaterlog!(ERROR, "image hash data is null");
        return;
    }
    unsafe { drop(Box::from_raw(hash_data as *mut ImgHashData<u64>)); }
    updaterlog!(INFO, "release image hash data");
}