#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include <log/log.h>

#include <iostream>
using namespace std;

#include "gralloc_priv.h"


void init_kms(struct private_module_t *m) {
  if (m->kms_fd == 0) {
    int fd = open("/dev/dri/card0", O_RDWR);
    if (fd < 0) {
        ALOGE("gralloc_device_open() failed to open kms_fd");
    } else {
        ALOGI("gralloc_device_open() kms_fd open sucess %d", fd);
        m->kms_fd = fd;
    }
    
  }
}


intptr_t mmapDrmBuffer(struct private_module_t *m, uint32_t handle, size_t size) {
    struct drm_mode_map_dumb arg;
    memset (&arg, 0, sizeof (arg));
    arg.handle = handle;
    int ret = drmIoctl(m->kms_fd, DRM_IOCTL_MODE_MAP_DUMB, &arg);
    if (ret != 0) {
        ALOGE("failed MAP_DUMB : %s", strerror(errno));
        return 0;
    }
    
    void *map = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, m->kms_fd, arg.offset);
    if (map == MAP_FAILED) {
        ALOGE("mmap() failed");
        return 0;
    }

    return (intptr_t)map;
}

intptr_t mapDrmBufferLocked(struct private_module_t *m, uint32_t *handle) {
    ALOGI("mapDrmBufferLocked()");
    *handle = 0;

    struct drm_mode_create_dumb arg;
    memset (&arg, 0, sizeof (arg));
    arg.bpp = 32;
    arg.width = 720;
    arg.height = 480;

    int ret = drmIoctl(m->kms_fd, DRM_IOCTL_MODE_CREATE_DUMB, &arg);
    if (ret != 0) {
        ALOGE("failed CREATE_DUMB : %s", strerror(errno));
    	return 0;
    } 
    ALOGI("CREATE_DUMB size: %lld , handle: %x ", arg.size, arg.handle);
    
    *handle = arg.handle;
    return mmapDrmBuffer(m, *handle, arg.size);
}

int getPrimeFd(struct private_module_t *m, uint32_t handle) {
    int prime_fd = 0;
    int ret = drmPrimeHandleToFD(m->kms_fd, handle, O_CLOEXEC, &prime_fd);
    if (ret != 0) {
        ALOGE("failed drmPrimeHandleToFd() : %s", strerror(errno));
	return errno;
    } else {
        return prime_fd;
    }
}

uint32_t getHandle(struct private_module_t *m, int prime_fd) {
    uint32_t handle = 0;
    int ret = drmPrimeFDToHandle(m->kms_fd, prime_fd, &handle);
    if (ret != 0) {
        ALOGE("failed drmPrimeFdToHandle() : %s", strerror(errno));
	return errno;
    } else {
        return handle;
    }
}


void unMapDrmDestroyBuffer(struct private_module_t *m, uint32_t handle, intptr_t map) {
    int ret = munmap((void *)map, 720 * 480 * 4);
    if (ret != 0) {
        ALOGE("failed unmap() : %s", strerror(errno));
    } else {
      ALOGI("unMapDrmBuffer() drm_handle %d, base %lx",
	    handle, map);
    }

    struct drm_mode_destroy_dumb arg;
    memset (&arg, 0, sizeof (arg));
    arg.handle = handle;

    ret = drmIoctl(m->kms_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &arg);
    if (ret != 0) {
      ALOGE("failed DESTROY_DUMB %s", strerror(errno));
      }
}

