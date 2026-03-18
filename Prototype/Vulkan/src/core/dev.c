#include "dev.h"

#ifndef PROD_BUILD

#include "renderer/renderer.h"
#include "core/binds.h"

void Screenshot() {
    SaveRender("out.png");
}

void DevInitialize() {
    AddBind("screenshot", Screenshot,
        (BindCommand){ IK_DEV, BIND_KEY_DOWN },
        (BindCommand){ IK_S_OVERRIDE, BIND_KEY_PRESSED });
}

void DevUpdate() {}

#else

void DevInitialize() {}

void DevUpdate() {}

#endif
