/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include "fbdev_driver.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include "log/log.h"
#include "securec.h"
#include "updater_ui_const.h"

namespace Updater {
FbdevDriver::~FbdevDriver()
{
    ReleaseFb(&buff_);
}

void FbdevDriver::FBLog() const
{
    LOG(INFO) << "id=" << finfo_.id;
    LOG(INFO) << "sem_start=" << finfo_.smem_start;
    LOG(INFO) << "smem_len=" << finfo_.smem_len;
    LOG(INFO) << "type=" << finfo_.type;
    LOG(INFO) << "line_length=" << finfo_.line_length;
    LOG(INFO) << "mmio_start=" << finfo_.mmio_start;
    LOG(INFO) << "mmio_len=" << finfo_.mmio_len;
    LOG(INFO) << "visual=" << finfo_.visual;

    LOG(INFO) << "The xres=" << vinfo_.xres;
    LOG(INFO) << "The yres=" << vinfo_.yres;
    LOG(INFO) << "xres_virtual=" << vinfo_.xres_virtual;
    LOG(INFO) << "yres_virtual=" << vinfo_.yres_virtual;
    LOG(INFO) << "xoffset=" << vinfo_.xoffset;
    LOG(INFO) << "yoffset=" << vinfo_.yoffset;
    LOG(INFO) << "bits_per_pixel is :" << vinfo_.bits_per_pixel;
    LOG(INFO) << "red.offset=" << vinfo_.red.offset;
    LOG(INFO) << "red.length=" << vinfo_.red.length;
    LOG(INFO) << "red.msb_right=" << vinfo_.red.msb_right;
    LOG(INFO) << "green.offset=" << vinfo_.green.offset;
    LOG(INFO) << "green.length=" << vinfo_.green.length;
    LOG(INFO) << "green.msb_right=" << vinfo_.green.msb_right;
    LOG(INFO) << "blue.offset=" << vinfo_.blue.offset;
    LOG(INFO) << "blue.length=" << vinfo_.blue.length;
    LOG(INFO) << "blue.msb_right=" << vinfo_.blue.msb_right;
    LOG(INFO) << "transp.offset=" << vinfo_.transp.offset;
    LOG(INFO) << "transp.length=" << vinfo_.transp.length;
    LOG(INFO) << "transp.msb_right=" << vinfo_.transp.msb_right;
    LOG(INFO) << "height=" << vinfo_.height;
}

bool FbdevDriver::Init()
{
    static bool res = [this] () {
        int fd = open(FB_DEV_PATH, O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            LOG(ERROR) << "cannot open fb0";
            return false;
        }

        if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo_) < 0) {
            LOG(ERROR) << "failed to get fb0 info";
            close(fd);
            return false;
        }

        if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo_) < 0) {
            LOG(ERROR) << "failed to get fb0 info";
            close(fd);
            return false;
        }

        FBLog();

        buff_.width = vinfo_.xres;
        buff_.height = vinfo_.yres;
        buff_.size = finfo_.line_length * vinfo_.yres;
        buff_.vaddr = mmap(nullptr, finfo_.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (buff_.vaddr == MAP_FAILED) {
            LOG(ERROR) << "failed to mmap framebuffer";
            close(fd);
            return false;
        }
        (void)memset_s(buff_.vaddr, finfo_.smem_len, 0, finfo_.smem_len);
        fd_ = fd;
        return true;
    } ();
    return res;
}

void FbdevDriver::Flip(const uint8_t *buf)
{
    if (fd_ < 0 || memcpy_s(buff_.vaddr, buff_.size, buf, buff_.size) != EOK) {
        return;
    }
    if (ioctl(fd_, FBIOPAN_DISPLAY, &vinfo_) < 0) {
        LOG(ERROR) << "failed to display fb0!";
    }
}

void FbdevDriver::GetGrSurface(GrSurface &surface)
{
    surface.width = static_cast<int>(vinfo_.xres);
    surface.height = static_cast<int>(vinfo_.yres);
    surface.rowBytes = finfo_.line_length;
    surface.pixelBytes = vinfo_.bits_per_pixel / 8; // 8: byte bit len
}

void FbdevDriver::ReleaseFb(const struct FbBufferObject *fbo)
{
    /*
     * When fd_ isn't less than 0, then fbo->vaddr is valid and can by safely munmap.
     * this can be guaranteed by FbdevDriver::Init.
     */
    if (fd_ < 0) {
        return;
    }
    munmap(fbo->vaddr, fbo->size);
    close(fd_);
    fd_ = -1;
}
} // namespace Updater
