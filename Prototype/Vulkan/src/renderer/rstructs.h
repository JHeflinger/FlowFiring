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

#define PREVIEW_PIPELINE_FLAGS   0b11111111110
#define BVH_PIPELINE_FLAGS       0b00011111110
#define SIMULATE_PIPELINE_FLAGS  0b00000000001
#define SIMULATE_SHADER_FLAG     0b1
#define CENTROID_SHADER_FLAG     0b10
#define HISTOGRAM_SHADER_FLAG    0b100
#define HISTORY_SHADER_FLAG      0b1000
#define SCATTER_SHADER_FLAG      0b10000
#define LEAVES_SHADER_FLAG       0b100000
#define BVH_SHADER_FLAG          0b1000000
#define REBIND_SHADER_FLAG       0b10000000
#define DEFAULT_SHADER_FLAG      0b100000000
#define ANALYZE_SHADER_FLAG      0b1000000000
#define OVERLAY_SHADER_FLAG      0b10000000000

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
    size_t max_edges;
    BOOL update_vertices;
    BOOL update_triangles;
    BOOL update_edges;
    BOOL update_holes;
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
    uint32_t offsets[4];
    float lightarea;
    ChangeSet changes;
    AxisAlignedBoundingBox bounds;
} Geometry;

typedef struct {
    VertexID a;
    VertexID b;
    int32_t flow;
} EdgeWrite;
DECLARE_ARRLIST(EdgeWrite);

typedef struct {
    TriangleID id;
    BOOL hole;
} FaceWrite;
DECLARE_ARRLIST(FaceWrite);

typedef struct {
    BOOL grid;
    BOOL async;
    BOOL simulate;
    float depth;
    PipelineFlags flags;
    size_t viewmode;
    size_t geomode;
    size_t edgemode;
    size_t crossmode;
    uint32_t snap;
} RendererConfig;

typedef struct {
    RendererConfig config;
    uint32_t w;
    uint32_t l;
    uint32_t h;
    uint32_t edgewrites;
    uint32_t facewrites;
    SimpleCamera camera;
} SaveConfig;

#endif
