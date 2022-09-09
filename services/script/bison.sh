#!/bin/bash

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

set -e
if [ ! -d ./yacc ]; then
   mkdir -p ./yacc
fi

export PATH=/usr/share/bison:$PATH

if [ -f "/usr/include/FlexLexer.h" ] && [ -f "./yacc/FlexLexer.h" ];then
  src_file_md5=$(md5sum "/usr/include/FlexLexer.h" | awk '{print $1}')
  dest_file_md5=$(md5sum "./yacc/FlexLexer.h" | awk '{print $1}')
  if [ "${src_file_md5}"x == "${dest_file_md5}"x ];then
    echo "md5 check successful."
  else
    echo "md5 check failed."
    cp /usr/include/FlexLexer.h ./yacc
  fi
else
  echo "/usr/include/FlexLexer.h or ./yacc/FlexLexer.h doesn't exist."
  cp /usr/include/FlexLexer.h ./yacc
fi
