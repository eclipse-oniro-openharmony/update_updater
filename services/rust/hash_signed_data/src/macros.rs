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

/// macro wrapper for Updater::UpdaterLogger in updater
///
/// # intro
/// support 5 level DEBUG, INFO, WARNING, ERROR, FATAL
///
/// # usage:
/// ```
/// let hello = "hello".to_string();
/// updaterlog!(INFO, "this is a info log, {:?}", hello);
/// ```
///
/// # note:
/// InitLogger / SetLevel is done in C++ code. if you need to
/// change log threshold, please add ffi interface in ffi.rs.
///
#[macro_export]
macro_rules! updaterlog {
    ($level:tt, $($arg:tt)* ) => (
        let log_str = format!($($arg)*);
        let file_name_str = match std::path::Path::new(file!()).file_name() {
            Some(name_os_str) => { name_os_str.to_str() },
            None => { None }
        };
        let file_name = file_name_str.unwrap_or("unknown");
        // can use CString::new(...).expect(...) because file_name and log_str can't have internal 0 byte
        unsafe {
            $crate::ffi::Logger(
                $crate::ffi::LogLevel::$level as i32,
                std::ffi::CString::new(file_name).expect("unknown").as_ptr() as *const std::ffi::c_char,
                line!() as i32,
                std::ffi::CString::new(log_str).expect("default log").as_ptr() as *const std::ffi::c_char
            )
        };
    )
}