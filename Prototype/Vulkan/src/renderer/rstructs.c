#include "rstructs.h"
#include <easyhash.h>

uint64_t hash_edge(Edge edge) {
    return ez_hash_uint64_t(((uint64_t)edge.a << 32) | edge.b);
}

IMPL_ARRLIST(EdgeMeta);
IMPL_ARRLIST(TriangleID);
IMPL_ARRLIST(Triangle);
IMPL_ARR_ARRLIST(vec4);
IMPL_ARR_ARRLIST(vec3);
IMPL_HASHMAP(Edge, size_t, EdgeMap, hash_edge);
IMPL_ARRLIST(EdgeWrite);
IMPL_ARRLIST(FaceWrite);
