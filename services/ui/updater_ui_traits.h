/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UPDATER_UI_TRAITS_H
#define UPDATER_UI_TRAITS_H

#include "component/component_factory.h"
#include "json_visitor.h"
#include "macros_updater.h"
#include "traits_util.h"

namespace Updater {
// define struct for load ui strategy in ui config file
DEFINE_STRUCT_TRAIT(ComInfo, "com",
    (std::string, pageId),
    (std::string, comId)
);

DEFINE_STRUCT_TRAIT(ProgressPage, "",
    (std::string, progressPageId),
    (std::string, progressComId),
    (std::string, progressType),
    (std::string, logoComId),
    (std::string, logoType),
    (std::string, warningComId)
);

DEFINE_STRUCT_TRAIT(ResPage, "",
    (std::string, successPageId),
    (std::string, failPageId)
);

DEFINE_STRUCT_TRAIT(UiStrategyCfg, "strategy",
    (std::string, confirmPageId),
    (ComInfo, labelLogId),
    (ComInfo, labelLogResId),
    (ComInfo, labelUpdId),
    (ProgressPage, progressPage),
    (ResPage, resPage)
);

// define struct for load pages' components from page json config, such as menu.json, update.json
DEFINE_TRAIT(UxViewCommonInfo, "Common",
    (int, x),
    (int, y),
    (int, w),
    (int, h),
    (std::string, id),
    (std::string, type),
    (bool, visible)
);

DEFINE_TRAIT(UxBoxProgressInfo, BoxProgressAdapter::COMPONENT_TYPE,
    (uint32_t, defaultValue),
    (std::string, fgColor),
    (std::string, bgColor),
    (std::string, endPoint),
    (bool, hasEp)
);

DEFINE_TRAIT(UxLabelInfo, TextLabelAdapter::COMPONENT_TYPE,
    (uint8_t, fontSize),
    (std::string, text),
    (std::string, align),
    (std::string, fontColor),
    (std::string, bgColor),
    (std::string, style),
    (std::string, focusedFontColor),
    (std::string, focusedBgColor),
    (bool, focusable),
    (std::string, lineBreakMode)
);

DEFINE_TRAIT(UxImageInfo, ImgViewAdapter::COMPONENT_TYPE,
    (std::string, resPath),
    (std::string, filePrefix),
    (uint32_t, imgCnt),
    (uint32_t, updInterval)
);

DEFINE_TRAIT(UxLabelBtnInfo, LabelBtnAdapter::COMPONENT_TYPE,
    (uint8_t, fontSize),
    (std::string, text),
    (std::string, txtColor),
    (std::string, bgColor),
    (std::string, focusedTxtColor),
    (std::string, focusedBgColor),
    (bool, focusable)
);

DEFINE_STRUCT_TRAIT(UxSubPageInfo, "subpages",
    (std::string, id),
    (std::string, bgColor),
    (std::vector<std::string>, coms)
);

DEFINE_STRUCT_TRAIT(PagePath, "",
    (std::string, dir),
    (std::string, entry),
    (std::vector<std::string>, pages)
);

struct UxPageInfo {
    std::string id {};
    std::string bgColor {};
    std::vector<UxViewInfo> viewInfos {};
    std::vector<UxSubPageInfo> subpages {};
};

/**
 * only define trait for uxpageinfo, because viewInfos_ is a variant,
 * can't be parsed by Visit<SETVAL>(...), so it's manually parsed in
 * LayoutParser
 */
DEFINE_TRAIT(UxPageInfo, "",
    (std::string, id),
    (std::string, bgColor),
    (std::vector<UxSubPageInfo>, subpages)
);

// define struct for load language resources in ui config file
DEFINE_STRUCT_TRAIT(Res, "Res",
    (std::string, path),
    (int, level)
);

DEFINE_STRUCT_TRAIT(LangResource, "",
    (std::string, localeFile),
    (std::vector<Res>, res)
);

// define struct for load callback configuration in ui config file
DEFINE_STRUCT_TRAIT(CallbackCfg, "",
    (std::string, pageId),
    (std::string, comId),
    (std::string, type),
    (std::string, func)
);

inline std::ostream &operator<<(std::ostream &os, const ComInfo &com)
{
    os << "pageId: " << com.pageId << " comId: " << com.comId;
    return os;
}
}
#endif
