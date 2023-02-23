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

use core::ffi::{c_uchar, c_int, c_char, c_ulong, CStr, c_void};

#[allow(unused)]
pub extern fn err_print_cb (err_str: *const c_char, _len: c_ulong, _u: *mut c_void) -> c_int
{
    println!("openssl err: {}", unsafe { CStr::from_ptr(err_str) }.to_string_lossy());
    1 // don't abort program
}

#[link(name = "crypto_static")]
#[allow(unused)]
extern "C" {
    /// evp bindings
    pub fn EVP_DecodeBlock(t: *mut c_uchar, f: *const c_uchar, n: c_int) -> c_int;

    pub fn ERR_print_errors_cb(
        cb: extern fn(
            err_str: *const c_char,
            len: c_ulong,
            u: *mut c_void
        ) -> c_int,
        u: *mut c_void
    ) -> c_void;
}

/// this should be consistent with enum in log/log.h
#[allow(unused)]
pub enum LogLevel {
    DEBUG = 3,
    INFO = 4,
    WARNING = 5,
    ERROR = 6,
    FATAL = 7,
}

#[link(name = "updaterlog")]
extern "C" {
    pub fn Logger(level: c_int, file_name: *const c_char, line: i32, format: *const c_char, ...) -> c_void;
}