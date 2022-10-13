#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright (c) 2021 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import argparse
import os
import shutil
import subprocess

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', help='output path of yacc')
    parser.add_argument('--bisoninput', help='input path of bison')
    parser.add_argument('--flexinput', help='input path of flex')
    options = parser.parse_args()

    yacc_path = options.output
    parser_file = os.path.join(yacc_path, "parser.cpp")
    lexer_file = os.path.join(yacc_path, "lexer.cpp")
    flexlexer_file = os.path.join(yacc_path, "FlexLexer.h")
    if not os.path.exists(yacc_path):
        os.makedirs(yacc_path)

    bison_cmd = ['bison', '-o', parser_file, options.bisoninput]
    parse_scripts = subprocess.check_call(bison_cmd)
    flex_cmd = ['flex', '-+', '-o', lexer_file, options.flexinput]
    parse_scripts = subprocess.check_call(flex_cmd)

    shutil.copy2("/usr/include/FlexLexer.h", flexlexer_file)
