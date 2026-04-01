#ifndef RSTRUCTS_H
#define RSTRUCTS_H

#include "data/profile.h"
#include "data/strings.h"
#include "renderer/vulkan/vconfig.h"
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/cglm.h>
#include <vulkan/vulkan.h>
#include <raylib.h>

typedef uint32_t TriangleID;
typedef uint32_t VertexID;
typedef uint32_t EdgeID;

typedef struct {
    VertexID a;
    VertexID b;
} Edge;

typedef struct {
    alignas(4) VertexID a;
    alignas(4) VertexID b;
    alignas(4) EdgeID f1e1;
    alignas(4) EdgeID f1e2;
    alignas(4) EdgeID f2e1;
    alignas(4) EdgeID f2e2;
    alignas(4) EdgeID f3e1;
    alignas(4) EdgeID f3e2;
    alignas(4) EdgeID f4e1;
    alignas(4) EdgeID f4e2;
    alignas(4) int32_t flow;
} EdgeMeta;

DECLARE_ARRLIST(EdgeMeta);
DECLARE_ARRLIST(TriangleID);
DECLARE_ARR_ARRLIST(vec4);
DECLARE_ARR_ARRLIST(vec3);
DECLARE_HASHMAP(Edge, size_t, EdgeMap);

#define PREVIEW_PIPELINE_FLAGS   0b1111111111
#define BVH_PIPELINE_FLAGS       0b0001111111
#define CENTROID_SHADER_FLAG     0b1
#define HISTOGRAM_SHADER_FLAG    0b10
#define HISTORY_SHADER_FLAG      0b100
#define SCATTER_SHADER_FLAG      0b1000
#define LEAVES_SHADER_FLAG       0b10000
#define BVH_SHADER_FLAG          0b100000
#define REBIND_SHADER_FLAG       0b1000000
#define DEFAULT_SHADER_FLAG      0b10000000
#define ANALYZE_SHADER_FLAG      0b100000000
#define OVERLAY_SHADER_FLAG      0b1000000000

typedef uint32_t PipelineFlags;

typedef struct {
    vec3 position;
    vec3 look;
    vec3 up;
	float fov;
    float aperature;
    float focus;
} SimpleCamera;

typedef struct {
    alignas(4) uint32_t hole;
    alignas(4) uint32_t a;
    alignas(4) uint32_t b;
    alignas(4) uint32_t c;
} Triangle;
DECLARE_ARRLIST(Triangle);

typedef struct {
    alignas(16) vec3 min;
    alignas(16) vec3 max;
} AxisAlignedBoundingBox;

typedef struct {
    alignas(16) vec3 min;
    alignas(16) vec3 max;
    alignas(4) uint32_t left;
    alignas(4) uint32_t right;
    alignas(4) uint32_t parent;
    alignas(4) uint32_t counter;
} BVHNode;

typedef struct {
    alignas(4) uint32_t tid;
    alignas(4) float distance;
} RayGenerator;

typedef struct {
    uint32_t value;
    BOOL exists;
} Schrodingnum;

typedef struct {
	RenderTexture2D target[CPUSWAP_LENGTH];
	size_t index;
    void* reference;
} CPUSwap;

typedef struct {
    size_t max_vertices;
    size_t max_triangles;
    BOOL update_vertices;
    BOOL update_triangles;
    size_t update_bvh;
} ChangeSet;

typedef struct {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT heap_budget;
    VkPhysicalDeviceMemoryProperties2 heap_props;
    double update_interval;
    double update_timer;
	BOOL available;
} GPUStatCache;

typedef struct {
    Profiler profile;
    GPUStatCache cache;
} RendererStats;

typedef struct {
    ARRLIST_vec4 vertices;
    ARRLIST_Triangle triangles;
    HASHMAP_EdgeMap emap;
    ARRLIST_EdgeMeta edges;
    float lightarea;
    ChangeSet changes;
    AxisAlignedBoundingBox bounds;
} Geometry;

typedef struct {
    BOOL grid;
    BOOL async;
    BOOL orthogonal;
    float depth;
    PipelineFlags flags;
} RendererConfig;

#endif
