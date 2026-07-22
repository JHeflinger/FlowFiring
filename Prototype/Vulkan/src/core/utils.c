#include "utils.h"
#include <easylogger.h>
#include <raymath.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif


float clampf(float x, float minVal, float maxVal) { return fminf(fmaxf(x, minVal), maxVal); }
float modf_glsl(float x, float y) { return x - y * floorf(x / y); }
float mixf(float a, float b, float t) { return a * (1.0f - t) + b * t; }

Vector3 hsv2rgb(Vector3 c) {
    Vector3 rgb;
    rgb.x = clampf(fabsf(modf_glsl(c.x * 6.0f + 0.0f, 6.0f) - 3.0f) - 1.0f, 0.0f, 1.0f);
    rgb.y = clampf(fabsf(modf_glsl(c.x * 6.0f + 4.0f, 6.0f) - 3.0f) - 1.0f, 0.0f, 1.0f);
    rgb.z = clampf(fabsf(modf_glsl(c.x * 6.0f + 2.0f, 6.0f) - 3.0f) - 1.0f, 0.0f, 1.0f);
    rgb.x = rgb.x * rgb.x * (3.0f - 2.0f * rgb.x);
    rgb.y = rgb.y * rgb.y * (3.0f - 2.0f * rgb.y);
    rgb.z = rgb.z * rgb.z * (3.0f - 2.0f * rgb.z);
    Vector3 result;
    result.x = c.z * mixf(1.0f, rgb.x, c.y);
    result.y = c.z * mixf(1.0f, rgb.y, c.y);
    result.z = c.z * mixf(1.0f, rgb.z, c.y);
    return result;
}

Color Rainbow(float value) {
    if (value >= 1.0f) return WHITE;
    value = clampf(value, 0.0f, 1.0f);
    Vector3 hsv = { value, 1.0, 1.0 };
    Vector3 rgb = hsv2rgb(hsv);
    return (Color){
        (unsigned char)(rgb.x * 255.0f),
        (unsigned char)(rgb.y * 255.0f),
        (unsigned char)(rgb.z * 255.0f),
        255
    };
}


size_t CurrentRAMUsage() {
#ifdef __APPLE__
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                 (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
        return (size_t)(info.resident_size / (1024 * 1024));
    }
#endif
    return 0;
}

size_t SystemRAMTotal() {
#ifdef __APPLE__
    int64_t total_ram_bytes = 0;
    size_t length = sizeof(total_ram_bytes);
    if (sysctlbyname("hw.memsize", &total_ram_bytes, &length, NULL, 0) == 0) {
        return (size_t)(total_ram_bytes / (1024 * 1024));
    }
#endif
    return 0;
}
