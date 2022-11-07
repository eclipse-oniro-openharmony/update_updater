/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include "drm_driver.h"
#include <cstdio>
#include <unistd.h>
#include "log/log.h"
#include "securec.h"
#include "updater_ui_const.h"

namespace Updater {
void DrmDriver::Flip(const uint8_t *buf)
{
    if (buff_.vaddr != MAP_FAILED) {
        if (memcpy_s(buff_.vaddr, buff_.size, buf, buff_.size) != EOK) {
            LOG(ERROR) << "DrmDriver::Flip memcpy_s fail";
        }
    }
}

void DrmDriver::GetGrSurface(GrSurface &surface)
{
    surface.width = static_cast<int>(buff_.width);
    surface.height = static_cast<int>(buff_.height);
    surface.rowBytes = 0;
    surface.pixelBytes = 0;
}

int DrmDriver::ModesetCreateFb(struct BufferObject *bo)
{
    struct drm_mode_create_dumb create = {};
    struct drm_mode_map_dumb map = {};
    const int offsetNumber = 4;
    uint32_t handles[offsetNumber] = {0};
    uint32_t pitches[offsetNumber] = {0};
    uint32_t offsets[offsetNumber] = {0};

    /* create a dumb-buffer, the pixel format is XRGB888 */
    const int pixelDepth = 32;
    create.width = bo->width;
    create.height = bo->height;
    create.bpp = pixelDepth;
    drmIoctl(fd_, DRM_IOCTL_MODE_CREATE_DUMB, &create);

    /* bind the dumb-buffer to an FB object */
    bo->pitch = create.pitch;
    bo->size = create.size;
    bo->handle = create.handle;

    handles[0] = bo->handle;
    pitches[0] = bo->pitch;
    offsets[0] = 0;
    int ret = drmModeAddFB2(fd_, bo->width, bo->height, DRM_FORMAT_ARGB8888, handles, pitches, offsets, &bo->fbId, 0);
    if (ret) {
        LOG(ERROR) << "[fbtest]failed to add fb (" << bo->width << "x" << bo->height << "): " << strerror(errno);
        return -1;
    }

    /* map the dumb-buffer to userspace */
    map.handle = create.handle;
    drmIoctl(fd_, DRM_IOCTL_MODE_MAP_DUMB, &map);
    bo->vaddr = static_cast<uint8_t *>(mmap(0, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, map.offset));
    if (bo->vaddr == MAP_FAILED) {
        LOG(ERROR) << "failed to mmap framebuffer";
        return -1;
    }
    const uint32_t newColor = 0xff000000;
    uint32_t i = 0;
    uint32_t color = newColor;
    while (i < bo->size) {
        if (memcpy_s(&bo->vaddr[i], bo->size, &color, sizeof(color)) != EOK) {
            return -1;
        }
        i += sizeof(color);
    }
    return 0;
}

drmModeCrtc *DrmDriver::GetCrtc(const drmModeRes &res, const int fd, const drmModeConnector &conn) const
{
    // if connector has one encoder, use it
    drmModeEncoder *encoder = nullptr;
    if (conn.encoder_id != 0) {
        encoder = drmModeGetEncoder(fd, conn.encoder_id);
    }
    if (encoder != nullptr && encoder->crtc_id != 0) {
        uint32_t crtcId = encoder->crtc_id;
        drmModeFreeEncoder(encoder);
        return drmModeGetCrtc(fd, crtcId);
    }

    if (encoder != nullptr) {
        drmModeFreeEncoder(encoder);
    }

    // try get a vaild encoder and crtc
    for (int i = 0; i < conn.count_encoders; i++) {
        encoder = drmModeGetEncoder(fd, conn.encoders[i]);
        if (encoder == nullptr) {
            continue;
        }

        for (int j = 0; j < res.count_crtcs; j++) {
            if ((encoder->possible_crtcs & (1u << static_cast<uint32_t>(j))) != 0) {
                drmModeFreeEncoder(encoder);
                return drmModeGetCrtc(fd, res.crtcs[j]);
            }
        }
        drmModeFreeEncoder(encoder);
    }
    return nullptr;
}

drmModeConnector *DrmDriver::GetFirstConnector(const drmModeRes &res, const int fd) const
{
    // get connected connector
    for (int i = 0; i < res.count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(fd, res.connectors[i]);
        if (conn == nullptr) {
            continue;
        }
        if (conn->count_modes > 0 &&
            conn->connection == DRM_MODE_CONNECTED) {
            return conn;
        }
        drmModeFreeConnector(conn);
    }
    return nullptr;
}

drmModeConnector *DrmDriver::GetConnectorByType(const drmModeRes &res, const int fd, const uint32_t type) const
{
    // get connected connector
    for (int i = 0; i < res.count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(fd, res.connectors[i]);
        if (conn == nullptr) {
            continue;
        }
        if (conn->connector_type == type &&
            conn->count_modes > 0 &&
            conn->connection == DRM_MODE_CONNECTED) {
            return conn;
        }
        drmModeFreeConnector(conn);
    }
    return nullptr;
}


drmModeConnector *DrmDriver::GetConnector(const drmModeRes &res, const int fd, uint32_t &modeId) const
{
    // get main connector : lvds edp and dsi
    uint32_t mainConnector[] = {
        DRM_MODE_CONNECTOR_LVDS,
        DRM_MODE_CONNECTOR_eDP,
        DRM_MODE_CONNECTOR_DSI,
    };

    drmModeConnector *conn = nullptr;
    for (uint32_t i = 0; i < sizeof(mainConnector) / sizeof(mainConnector[0]); i++) {
        conn = GetConnectorByType(res, fd, mainConnector[i]);
        if (conn != nullptr) {
            break;
        }
    }

    if (conn == nullptr) {
        conn = GetFirstConnector(res, fd);
    }

    if (conn == nullptr) {
        LOG(ERROR) << "DrmDriver cannot get vaild connector";
        return nullptr;
    }

    // get preferred mode index
    modeId = 0;
    for (int i = 0; i < conn->count_modes; i++) {
        if ((conn->modes[i].type & DRM_MODE_TYPE_PREFERRED) != 0) {
            modeId = static_cast<uint32_t>(i);
            break;
        }
    }

    return conn;
}

drmModeRes *DrmDriver::GetResources(int &fd) const
{
    // 1: open drm resource
    drmModeRes *res = nullptr;
    for (int i = 0; i < DRM_MAX_MINOR; i++) {
        res = GetOneResources(i, fd);
        if (res != nullptr) {
            break;
        }
    }
    return res;
}

drmModeRes *DrmDriver::GetOneResources(const int devIndex, int &fd) const
{
    // 1: open drm device
    fd = -1;
    std::string devName = std::string("/dev/dri/card") + std::to_string(devIndex);
    int tmpFd = open(devName.c_str(), O_RDWR | O_CLOEXEC);
    if (tmpFd < 0) {
        LOG(ERROR) << "open failed " << devName;
        return nullptr;
    }
    // 2: check drm capacity
    uint64_t cap = 0;
    int ret = drmGetCap(tmpFd, DRM_CAP_DUMB_BUFFER, &cap);
    if (ret != 0 || cap == 0) {
        LOG(ERROR) << "drmGetCap failed";
        close(tmpFd);
        return nullptr;
    }

    // 3: get drm resources
    drmModeRes *res = drmModeGetResources(tmpFd);
    if (res == nullptr) {
        LOG(ERROR) << "drmModeGetResources failed";
        close(tmpFd);
        return nullptr;
    }

    // 4: check it has connected connector and crtc
    if (res->count_crtcs > 0 && res->count_connectors > 0 && res->count_encoders > 0) {
        drmModeConnector *conn = GetFirstConnector(*res, tmpFd);
        if (conn != nullptr) {
            // don't close fd
            LOG(INFO) << "drm dev:" << devName;
            drmModeFreeConnector(conn);
            fd = tmpFd;
            return res;
        }
    }
    close(tmpFd);
    drmModeFreeResources(res);
    return nullptr;
}

int DrmDriver::DrmInit(void)
{
    // 1: open drm resource
    res_ = GetResources(fd_);
    if (fd_ < 0 || res_ == nullptr) {
        LOG(ERROR) << "DrmInit: GetResources failed";
        return -1;
    }

    // 2 : get connected connector
    uint32_t modeId;
    conn_ = GetConnector(*res_, fd_, modeId);
    if (conn_ == nullptr) {
        LOG(ERROR) << "DrmInit: GetConnector failed";
        return -1;
    }

    // 3: get vaild encoder and crtc
    crtc_ = GetCrtc(*res_, fd_, *conn_);
    if (crtc_ == nullptr) {
        LOG(ERROR) << "DrmInit: GetCrtc failed";
        return -1;
    }

    // 4: create userspace buffer
    buff_.width = conn_->modes[modeId].hdisplay;
    buff_.height = conn_->modes[modeId].vdisplay;
    ModesetCreateFb(&buff_);

    // 5: bind ctrc and connector
    drmModeSetCrtc(fd_, crtc_->crtc_id, buff_.fbId, 0, 0, &conn_->connector_id, 1, &conn_->modes[modeId]);
    LOG(INFO) << "DrmInit: buff_.width:" << buff_.width << " buff_.height:" << buff_.height;
    LOG(INFO) << "DrmInit: crtc_id:" << crtc_->crtc_id << " connector_id:" << conn_->connector_id;
    LOG(INFO) << " drm init success.";
    return 0;
}

bool DrmDriver::Init()
{
    // this static variable can guarantee Init be called only once
    static bool res = [this] () {
        if (DrmInit() == -1) {
            LOG(ERROR) << "load drm driver fail";
            return false;
        }
        return true;
    } ();
    return res;
}

void DrmDriver::ModesetDestroyFb(struct BufferObject *bo)
{
    if (fd_ > 0 && bo->fbId != 0) {
        drmModeRmFB(fd_, bo->fbId);
    }
    if (bo->vaddr != MAP_FAILED) {
        munmap(bo->vaddr, bo->size);
    }
    if (fd_ > 0) {
        struct drm_mode_destroy_dumb destroy = {};
        destroy.handle = bo->handle;
        drmIoctl(fd_, DRM_IOCTL_GEM_CLOSE, &destroy);
    }
    if (crtc_ != nullptr) {
        drmModeFreeCrtc(crtc_);
    }
    if (conn_ != nullptr) {
        drmModeFreeConnector(conn_);
    }
    if (res_ != nullptr) {
        drmModeFreeResources(res_);
    }
    if (fd_ > 0) {
        close(fd_);
        fd_ = -1;
    }
}

DrmDriver::~DrmDriver()
{
    ModesetDestroyFb(&buff_);
}
} // namespace Updater
