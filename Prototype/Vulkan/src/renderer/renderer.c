#include "renderer.h"
#include <easylogger.h>
#include "renderer/vulkan/vutils.h"
#include "renderer/vulkan/vinit.h"
#include "renderer/vulkan/vupdate.h"
#include "renderer/vulkan/vclean.h"
#include "renderer/rmath.h"
#include "renderer/overlay.h"
#include <GLFW/glfw3.h>
#include <easymemory.h>
#include <string.h>
#include <time.h>

Renderer g_renderer = { 0 };
Vector2 g_override_resolution = { 0 };
float g_rft = 0.0f;

PipelineFlags GetPipelineFlags() {
    return g_renderer.config.flags;
}

void SetPipelineFlags(PipelineFlags flags) {
    g_renderer.config.flags = flags;
}

void SetViewportSlice(size_t w, size_t h) {
	float psuedo_w = w * (g_renderer.dimensions.x / (float)GetScreenWidth());
	float psuedo_h = h * (g_renderer.dimensions.y / (float)GetScreenHeight());
    g_renderer.viewport = (Vector2) { ceil(psuedo_w), ceil(psuedo_h) };
}

void OverrideResolution(size_t x, size_t y) {
	g_override_resolution = (Vector2){ x, y };
}

void InitializeRenderer() {
	// init rand
	srand(time(NULL));

    // initialize config
    g_renderer.config.grid = TRUE;
    g_renderer.config.async = TRUE;
    g_renderer.config.flags = PREVIEW_PIPELINE_FLAGS;
    g_renderer.config.orthogonal = FALSE;
    g_renderer.config.depth = 0.0f;

    // initialize min/max BB
    SETVEC3(g_renderer.geometry.bounds.min, FLT_MAX, FLT_MAX, FLT_MAX);
    SETVEC3(g_renderer.geometry.bounds.max, -FLT_MAX, -FLT_MAX, -FLT_MAX);

    // initialize camera
    g_renderer.camera = (SimpleCamera){
        { 0.0f, 2.133f, 2.11f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        90.0f, 0.0f, 0.0f
    };

    // set up dimensions
    g_renderer.dimensions = (Vector2){ 
		g_override_resolution.x == 0 ? GetScreenWidth() : g_override_resolution.x,
		g_override_resolution.y == 0 ? GetScreenHeight() : g_override_resolution.y };

    // initialize vulkan resources
	VUTIL_SetVulkanUtilsContext(&g_renderer);
	VINIT_SetVulkanInitContext(&g_renderer);
	VUPDT_SetVulkanUpdateContext(&g_renderer);
	VCLEAN_SetVulkanCleanContext(&g_renderer);
	BOOL result = VINIT_Vulkan(&(g_renderer.vulkan));
	EZ_ASSERT(result, "Failed to initialize vulkan");

    // set up cpu swap
    for (size_t i = 0; i < CPUSWAP_LENGTH; i++) {
	    g_renderer.swapchain.target[i] = LoadRenderTexture(g_renderer.dimensions.x, g_renderer.dimensions.y);
	    EZ_ASSERT(IsRenderTextureValid(g_renderer.swapchain.target[i]), "Unable to load target texture");
    }

    // configure stat profiler
    ConfigureProfile(&(g_renderer.stats.profile), "Renderer", 10);

    // configure GPU stat cache
    g_renderer.stats.cache.update_interval = 1.0;
    PollGPUCache(TRUE);

    // set overlay context
    SetOverlayContext(&g_renderer);
}

void DestroyRenderer() {
    // clean geometry
    ClearVertices();
    ClearTriangles();

    // destroy vulkan resources
    VCLEAN_Vulkan(&(g_renderer.vulkan));

    // unload cpu swap textures
    for (size_t i = 0; i < CPUSWAP_LENGTH; i++)
	    UnloadRenderTexture(g_renderer.swapchain.target[i]);
}

SimpleCamera GetCamera() {
    return g_renderer.camera;
}

void MoveCamera(SimpleCamera camera) {
    g_renderer.camera = camera;
}

void FitCamera() {
    if (g_renderer.geometry.bounds.min[0] >= g_renderer.geometry.bounds.max[0]) return;
    vec3 l2p;
    glm_vec3_sub(g_renderer.camera.position, g_renderer.camera.look, l2p);
    glm_vec3_normalize(l2p);
    vec3 extend, min2o, newo;
    glm_vec3_sub(g_renderer.geometry.bounds.max, g_renderer.geometry.bounds.min, extend);
    glm_vec3_scale(extend, 0.5f, min2o);
    glm_vec3_add(min2o, g_renderer.geometry.bounds.min, newo);
    float width = glm_vec3_norm(extend);
    glm_vec3_copy(newo, g_renderer.camera.look);
    glm_vec3_scale(l2p, width, l2p);
    glm_vec3_add(l2p, newo, g_renderer.camera.position);
}

void ReorientCamera() {
    for (int i = 0; i < 3; i++) {
        float sign = g_renderer.camera.up[i] > 0 ? 1.0f : (g_renderer.camera.up[i] < 0 ? -1.0f : 0.0f);
        g_renderer.camera.look[i] += sign*1e-6f;
    }
    vec3 desired = { 0, 1, 0 };
    glm_vec3_copy(desired, g_renderer.camera.up);
}

void GetVertex(size_t index, vec3 out) {
    EZ_ASSERT(index < g_renderer.geometry.vertices.size, "Vertex does not exist for requested index");
    glm_vec3_copy(g_renderer.geometry.vertices.data[index], out);
}

float* VertexReference(VertexID vertex) {
    EZ_ASSERT(vertex < g_renderer.geometry.vertices.size, "Vertex reference does not exist for requested index");
    return g_renderer.geometry.vertices.data[vertex];
}

void SubmitVertex(vec3 vertex) {
    g_renderer.geometry.changes.update_vertices = TRUE;
    vec4 v = { 0 };
    glm_vec3_copy(vertex, v);
    ARRLIST_vec4_add(&(g_renderer.geometry.vertices), v);
    if (vertex[0] < g_renderer.geometry.bounds.min[0]) g_renderer.geometry.bounds.min[0] = vertex[0];
    if (vertex[1] < g_renderer.geometry.bounds.min[1]) g_renderer.geometry.bounds.min[1] = vertex[1];
    if (vertex[2] < g_renderer.geometry.bounds.min[2]) g_renderer.geometry.bounds.min[2] = vertex[2];
    if (vertex[0] > g_renderer.geometry.bounds.max[0]) g_renderer.geometry.bounds.max[0] = vertex[0];
    if (vertex[1] > g_renderer.geometry.bounds.max[1]) g_renderer.geometry.bounds.max[1] = vertex[1];
    if (vertex[2] > g_renderer.geometry.bounds.max[2]) g_renderer.geometry.bounds.max[2] = vertex[2];
}

void ClearVertices() {
    if (g_renderer.geometry.vertices.maxsize == 0) return;
    ARRLIST_vec4_clear(&(g_renderer.geometry.vertices));
    ARRLIST_EdgeMeta_clear(&(g_renderer.geometry.edges));
    HASHMAP_EdgeMap_clear(&(g_renderer.geometry.emap));
    g_renderer.geometry.changes.update_vertices = TRUE;
    SETVEC3(g_renderer.geometry.bounds.min, FLT_MAX, FLT_MAX, FLT_MAX);
    SETVEC3(g_renderer.geometry.bounds.max, -FLT_MAX, -FLT_MAX, -FLT_MAX);
}

TriangleID SubmitTriangle(Triangle triangle) {
    g_renderer.geometry.changes.update_triangles = TRUE;
    EZ_ASSERT(triangle.a < g_renderer.geometry.vertices.size &&
              triangle.b < g_renderer.geometry.vertices.size &&
              triangle.c < g_renderer.geometry.vertices.size, "Triangle vertex does not exist");
    TriangleID id = g_renderer.geometry.triangles.size;
    ARRLIST_Triangle_add(&(g_renderer.geometry.triangles), triangle);
    VertexID vs[] = { triangle.a, triangle.b, triangle.c };
    for (size_t i = 0; i < 3; i++) {
        VertexID a = vs[i];
        VertexID b = vs[(i + 1)%3];
        Edge e = { a, b };
        Edge alternate = { b, a };
        Edge primed = HASHMAP_EdgeMap_has(&(g_renderer.geometry.emap), e) ? e : alternate;
        if (!HASHMAP_EdgeMap_has(&(g_renderer.geometry.emap), primed)) {
            EdgeMeta meta = (EdgeMeta) {
                primed.a, primed.b, (EdgeID)id, (EdgeID)-1, (EdgeID)-1,
                (EdgeID)-1, (EdgeID)-1, (EdgeID)-1, (EdgeID)-1, (EdgeID)-1, 0 };
            HASHMAP_EdgeMap_set(&(g_renderer.geometry.emap), primed, g_renderer.geometry.edges.size);
            ARRLIST_EdgeMeta_add(&(g_renderer.geometry.edges), meta);
        } else {
            EdgeMeta* em = &(g_renderer.geometry.edges.data[HASHMAP_EdgeMap_get(&(g_renderer.geometry.emap), primed)]);
            EdgeID* setters[3] = { &(em->f2e1), &(em->f3e1), &(em->f4e1) };
            BOOL found = FALSE;
            for (size_t i = 0; i < 3; i++) {
                if (*setters[i] == (EdgeID)-1) {
                    *setters[i] = (EdgeID)id;
                    found = TRUE;
                    break;
                }
            }
            EZ_ASSERT(found, "Edge metadata has already been primed to degree 4");
        }
    }
    return id;
}

void ClearTriangles() {
    if (g_renderer.geometry.triangles.maxsize == 0) return;
    ARRLIST_Triangle_clear(&(g_renderer.geometry.triangles));
    HASHMAP_EdgeMap_clear(&(g_renderer.geometry.emap));
    ARRLIST_EdgeMeta_clear(&(g_renderer.geometry.edges));
    g_renderer.geometry.changes.update_triangles = TRUE;
}

void Render() {
    static BOOL async_update = TRUE;

    // update render frame time;
    g_rft += GetFrameTime();

    // detect changes in described data
    if (async_update) {
        // profile for stats
        BeginProfile(&(g_renderer.stats.profile));

        BOOL descriptor_changes = 
            g_renderer.geometry.changes.update_triangles |
            g_renderer.geometry.changes.update_vertices;

        // set bvh reconstruction
        if (descriptor_changes) g_renderer.geometry.changes.update_bvh = CPUSWAP_LENGTH;

        // update vertices if needed
        if (g_renderer.geometry.changes.update_vertices) {
            g_renderer.geometry.changes.update_vertices = FALSE;
            if (g_renderer.geometry.changes.max_vertices != g_renderer.geometry.vertices.maxsize) {
                vkDeviceWaitIdle(g_renderer.vulkan.core.general.interface);
                g_renderer.geometry.changes.max_vertices = g_renderer.geometry.vertices.maxsize;
                VCLEAN_Vertices(&(g_renderer.vulkan.core.geometry.vertices));
                VINIT_Vertices(&(g_renderer.vulkan.core.geometry.vertices));
            } else {
                VUPDT_Vertices(&(g_renderer.vulkan.core.geometry.vertices));
            }
        }

        // update triangles if needed
        if (g_renderer.geometry.changes.update_triangles) {
            g_renderer.geometry.changes.update_triangles = FALSE;
            if (g_renderer.geometry.changes.max_triangles != g_renderer.geometry.triangles.maxsize) {
                vkDeviceWaitIdle(g_renderer.vulkan.core.general.interface);
                g_renderer.geometry.changes.max_triangles = g_renderer.geometry.triangles.maxsize;
                VCLEAN_Triangles(&(g_renderer.vulkan.core.geometry.triangles));
                VINIT_Triangles(&(g_renderer.vulkan.core.geometry.triangles));
                VCLEAN_BVH(&(g_renderer.vulkan.core.geometry.bvh));
                VINIT_BVH(&(g_renderer.vulkan.core.geometry.bvh));
            } else {
                VUPDT_Triangles(&(g_renderer.vulkan.core.geometry.triangles));
            }
        }

        // update descriptor sets if needed
        if (descriptor_changes) VUPDT_DescriptorSets(g_renderer.vulkan.core.context.renderdata.descriptors);

        // update uniform buffers
        VUPDT_UniformBuffers(&(g_renderer.vulkan.core.context.renderdata.ubos));

        // reset renderer frame time
        g_rft = 0.0f;

        // reset command buffer and record it
        vkResetCommandBuffer(g_renderer.vulkan.core.scheduler.commands.commands[g_renderer.swapchain.index], 0);
        VUPDT_RecordCommand(g_renderer.vulkan.core.scheduler.commands.commands[g_renderer.swapchain.index]);

        // submit command buffer
        VkSubmitInfo submitInfo = { 0 };
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &(g_renderer.vulkan.core.scheduler.commands.commands[g_renderer.swapchain.index]);
        submitInfo.signalSemaphoreCount = 0;
        VkResult result = vkQueueSubmit(g_renderer.vulkan.core.scheduler.queue, 1, &submitInfo, g_renderer.vulkan.core.scheduler.syncro.fences[g_renderer.swapchain.index]);
        EZ_ASSERT(result == VK_SUCCESS, "failed to submit draw command buffer!");
    }

    // wait for and reset rendering fence
	size_t new_ind = (g_renderer.swapchain.index + 1) % CPUSWAP_LENGTH;
    if (!g_renderer.config.async)
        vkWaitForFences(g_renderer.vulkan.core.general.interface, 1, &(g_renderer.vulkan.core.scheduler.syncro.fences[new_ind]), VK_TRUE, UINT64_MAX);
    if (vkGetFenceStatus(g_renderer.vulkan.core.general.interface, g_renderer.vulkan.core.scheduler.syncro.fences[new_ind]) == VK_SUCCESS) {
        // copy overlay results to host
        memcpy((void*)ExposedOverlaySSBO(), g_renderer.vulkan.core.context.renderdata.overlay_mapped, sizeof(OverlaySSBO));

        // reset fences and update swapchain index
        vkResetFences(g_renderer.vulkan.core.general.interface, 1, &(g_renderer.vulkan.core.scheduler.syncro.fences[new_ind]));
        g_renderer.swapchain.index = new_ind;
        async_update = TRUE;

        // update render target
        glBindTexture(GL_TEXTURE_2D, g_renderer.swapchain.target[new_ind].texture.id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_renderer.dimensions.x, g_renderer.dimensions.y, GL_RGBA, GL_UNSIGNED_BYTE, g_renderer.swapchain.reference);
        glBindTexture(GL_TEXTURE_2D, 0);

        // end profiling
        EndProfile(&(g_renderer.stats.profile));
    } else {
        async_update = FALSE;
    }
}

void DrawHelper(float x, float y, float w, float h, float maxw, float maxh) {
    ClearBackground(BLACK);
    BeginBlendMode(BLEND_ADDITIVE);
	float psuedo_w = w * (g_renderer.dimensions.x / maxw);
	float psuedo_h = h * (g_renderer.dimensions.y / maxh);
    DrawTexturePro(
        g_renderer.swapchain.target[g_renderer.swapchain.index].texture,
        (Rectangle){
            (g_renderer.swapchain.target[g_renderer.swapchain.index].texture.width / 2.0f) - (psuedo_w/2.0f),
            (g_renderer.swapchain.target[g_renderer.swapchain.index].texture.height / 2.0f) - (psuedo_h/2.0f),
            psuedo_w,
            psuedo_h },
        (Rectangle){ x, y, w, h},
        (Vector2){ 0, 0 },
        0.0f,
        WHITE);
    EndBlendMode();
}

void Draw(float x, float y, float w, float h) {
    DrawHelper(x, y, w, h, (float)GetScreenWidth(), (float)GetScreenHeight());
}

float RenderTime() {
    return ProfileResult(&(g_renderer.stats.profile));
}

size_t NumVertices() {
    return g_renderer.geometry.vertices.size;
}

size_t NumTriangles() {
    return g_renderer.geometry.triangles.size;
}

void UpdateVertices() {
    g_renderer.geometry.changes.update_vertices = TRUE;
}

void PrimeEdges() {
    for (size_t i = 0; i < g_renderer.geometry.edges.size; i++) {
        EdgeMeta* em = &(g_renderer.geometry.edges.data[i]);
        EdgeID* faces[4] = { &(em->f1e1), &(em->f2e1), &(em->f3e1), &(em->f4e1) };
        EdgeID* others[4] = { &(em->f1e2), &(em->f2e2), &(em->f3e2), &(em->f4e2) };
        for (size_t j = 0; j < 4; j++) {
            if (*faces[j] == (EdgeID)-1) break;
            Triangle t = g_renderer.geometry.triangles.data[*faces[j]];
            VertexID vertices[3] = { t.a, t.b, t.c };
            VertexID overt = (VertexID)-1;
            for (size_t k = 0; k < 3; k++) {
                if (vertices[k] != em->a && vertices[k] != em->b) {
                    overt = vertices[k];
                    break;
                }
            }
            EZ_ASSERT(overt != (VertexID)-1, "Broken triangle-edge relation detected");
            Edge e = (Edge){ overt, em->a };
            Edge primed = HASHMAP_EdgeMap_has(&(g_renderer.geometry.emap), e) ? e : (Edge){ e.b, e.a };
            *faces[j] = (EdgeID)HASHMAP_EdgeMap_get(&g_renderer.geometry.emap, primed);
            e = (Edge){ overt, em->b };
            primed = HASHMAP_EdgeMap_has(&(g_renderer.geometry.emap), e) ? e : (Edge){ e.b, e.a };
            *others[j] = (EdgeID)HASHMAP_EdgeMap_get(&g_renderer.geometry.emap, primed);
        }
    }
}

Vector2 RenderResolution() {
    return g_renderer.dimensions;
}

RendererConfig* RenderConfig() {
    return &(g_renderer.config);
}

Geometry* RendererGeometry() {
    return &(g_renderer.geometry);
}

float RenderFrameTime() {
    return g_rft;
}

Triangle* TriangleReference(size_t index) {
    EZ_ASSERT(index < g_renderer.geometry.triangles.size, "Invalid triangle index requested");
    return &(g_renderer.geometry.triangles.data[index]);
}

void UpdateTriangles() {
    g_renderer.geometry.changes.update_triangles = TRUE;
}

EdgeMeta* EdgeReference(Edge e) {
    Edge primed = HASHMAP_EdgeMap_has(&(g_renderer.geometry.emap), e) ? e : (Edge){ e.b, e.a };
    EZ_ASSERT(HASHMAP_EdgeMap_has(&(g_renderer.geometry.emap), primed), "Cannot get reference of an edge that does not exist");
    return &(g_renderer.geometry.edges.data[HASHMAP_EdgeMap_get(&(g_renderer.geometry.emap), primed)]);
}

Edge EdgePrimed(Edge e) {
    return HASHMAP_EdgeMap_has(&(g_renderer.geometry.emap), e) ? e : (Edge){ e.b, e.a };
}

void SaveRender(const char* filepath) {
	RenderTexture rt = LoadRenderTexture(g_renderer.dimensions.x, g_renderer.dimensions.y);
    BeginTextureMode(rt);
    DrawHelper(0, 0, g_renderer.dimensions.x, -g_renderer.dimensions.y, g_renderer.dimensions.x, g_renderer.dimensions.y);
    EndTextureMode();
    Image image = LoadImageFromTexture(rt.texture);
    ExportImage(image, filepath);
    UnloadImage(image);
    UnloadRenderTexture(rt);
}

char* GPUModel() {
    return g_renderer.vulkan.core.general.gpuname;
}

void PollGPUCache(BOOL init) {
    if (init || (GetTime() - g_renderer.stats.cache.update_timer) > g_renderer.stats.cache.update_interval) {
		if (init) {
			ARRLIST_StaticString_add(&(g_renderer.vulkan.metadata.extensions.required), "VK_KHR_get_physical_device_properties2");
			if (!VUTIL_CheckGPUExtensionSupport(g_renderer.vulkan.core.general.gpu)) {
				g_renderer.stats.cache.available = FALSE;
			} else {
				g_renderer.stats.cache.available = TRUE;
			}
			ARRLIST_StaticString_remove(&(g_renderer.vulkan.metadata.extensions.required), g_renderer.vulkan.metadata.extensions.required.size - 1);
		}
		if (g_renderer.stats.cache.available) {
			g_renderer.stats.cache.update_timer = GetTime();
			g_renderer.stats.cache.heap_budget = (VkPhysicalDeviceMemoryBudgetPropertiesEXT) { 0 };
	        g_renderer.stats.cache.heap_budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
			g_renderer.stats.cache.heap_budget.pNext = NULL;
			g_renderer.stats.cache.heap_props = (VkPhysicalDeviceMemoryProperties2) { 0 };
			g_renderer.stats.cache.heap_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
			g_renderer.stats.cache.heap_props.pNext = &(g_renderer.stats.cache.heap_budget);
			vkGetPhysicalDeviceMemoryProperties2(g_renderer.vulkan.core.general.gpu, &(g_renderer.stats.cache.heap_props));
		}
    }
}

size_t GPUHeapCount() {
	if (!g_renderer.stats.cache.available) return 0;
    return g_renderer.stats.cache.heap_props.memoryProperties.memoryHeapCount;
}

size_t GPUHeapUsage(size_t i) {
	if (!g_renderer.stats.cache.available) return 0;
    return g_renderer.stats.cache.heap_budget.heapUsage[i];
}

size_t GPUHeapBudget(size_t i) {
	if (!g_renderer.stats.cache.available) return 0;
    return g_renderer.stats.cache.heap_budget.heapBudget[i];
}

const char* GPUHeapType(size_t i) {
	if (!g_renderer.stats.cache.available) return "Unavailable";
    if (g_renderer.stats.cache.heap_props.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        return "LOCAL";
    if (g_renderer.stats.cache.heap_props.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT)
        return "MULTI";
    return "SHARE";
}
