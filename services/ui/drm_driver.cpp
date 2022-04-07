/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

namespace updater {
void DrmDriver::FlipBuffer(const void *buf)
{
    if (!buf) {
        LOG(ERROR) << "buf is null";
        return;
    }
    UPDATER_CHECK_ONLY_RETURN(!memcpy_s(buff_.vaddr, buff_.size, buf, buff_.size), return);
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
    bo->vaddr = static_cast<uint8_t*>(mmap(0, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, map.offset));
    const int newColor = 0xff000000;
    uint32_t i = 0;
    uint32_t color = newColor;
    while (i < bo->size) {
        UPDATER_CHECK_ONLY_RETURN(!memcpy_s(&bo->vaddr[i], bo->size, &color, sizeof(color)), return -1);
        i += sizeof(color);
    }
    return 0;
}

bool DrmDriver::GetCrtc(const drmModeRes &res, const int fd, const drmModeConnector &conn, uint32_t &crtcId)
{
    // get possible crtc
    drmModeEncoder *encoder = nullptr;
    for (int i = 0; i < conn.count_encoders; i++) {
        encoder = drmModeGetEncoder(fd, conn.encoders[i]);
        if (encoder == nullptr) {
            continue;
        }

        for (int j = 0; j < res.count_crtcs; j++) {
            if ((encoder->possible_crtcs & (1u << (uint32_t)j)) != 0) {
                crtcId = res.crtcs[j];
                drmModeFreeEncoder(encoder);
                return true;
            }
        }
        drmModeFreeEncoder(encoder);
        encoder = nullptr;
    }
    return false;
}

bool DrmDriver::GetConnector(const drmModeRes &res, const int fd, uint32_t &connId)
{
    // get connected connector
    for (int i = 0; i < res.count_connectors; i++) {
        conn_ = drmModeGetConnector(fd, res.connectors[i]);
        if (conn_ != nullptr &&
            conn_->connection == DRM_MODE_CONNECTED) {
            connId = conn_->connector_id;
            return true;
        }
        drmModeFreeConnector(conn_);
        conn_ = nullptr;
    }

    return false;
}

int DrmDriver::DrmInit(void)
{
    fd_ = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
        LOG(ERROR) << "open failed";
        return -1;
    }

    res_ = drmModeGetResources(fd_);
    if (res_ == nullptr) {
        LOG(ERROR) << "drmModeGetResources";
        return -1;
    }

    uint32_t crtcId;
    uint32_t connId;
    if (!GetConnector(*res_, fd_, connId)) {
        LOG(ERROR) << "DrmInit cannot get drm connector";
        return -1;
    }
    if (GetCrtc(*res_, fd_, *conn_, crtcId)) {
        LOG(ERROR) << "DrmInit cannot get drm crtc";
        return -1;
    }

    buff_.width = conn_->modes[0].hdisplay;
    buff_.height = conn_->modes[0].vdisplay;

    ModesetCreateFb(&buff_);
    drmModeSetCrtc(fd_, crtcId, buff_.fbId, 0, 0, &connId, 1, &conn_->modes[0]);
    LOG(INFO) << " drm init success.";
    return 0;
}

void DrmDriver::LoadDrmDriver()
{
    if (DrmInit() == -1) {
        LOG(ERROR) << "load drm driver fail";
    }
}

void DrmDriver::ModesetDestroyFb(struct BufferObject *bo)
{
    struct drm_mode_destroy_dumb destroy = {};
    drmModeRmFB(fd_, bo->fbId);
    munmap(bo->vaddr, bo->size);
    destroy.handle = bo->handle;
    drmIoctl(fd_, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    drmModeFreeConnector(conn_);
    drmModeFreeResources(res_);
    close(fd_);
}

DrmDriver::~DrmDriver()
{
    ModesetDestroyFb(&buff_);
}
} // namespace updater
