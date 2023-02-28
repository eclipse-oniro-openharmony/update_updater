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
mod macros;

use core::{ffi::{c_char, CStr}, mem::ManuallyDrop, ptr};
use hsd::HashSignedData;

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