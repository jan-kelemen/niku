#ifndef VKGLSL_GLSLANG_ADAPTER_INCLUDED
#define VKGLSL_GLSLANG_ADAPTER_INCLUDED

#include <glslang/Public/ShaderLang.h>

#include <volk.h>

#include <cassert>

namespace vkglsl
{
    [[nodiscard]] constexpr EShLanguage to_glslang(
        VkShaderStageFlagBits const stage)
    {
        switch (stage)
        {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return EShLangVertex;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return EShLangTessControl;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return EShLangTessEvaluation;
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            return EShLangGeometry;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return EShLangFragment;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return EShLangCompute;
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            return EShLangRayGen;
        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
            return EShLangAnyHit;
        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
            return EShLangClosestHit;
        case VK_SHADER_STAGE_MISS_BIT_KHR:
            return EShLangMiss;
        case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
            return EShLangIntersect;
        case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
            return EShLangCallable;
        case VK_SHADER_STAGE_TASK_BIT_EXT:
            return EShLangTask;
        case VK_SHADER_STAGE_MESH_BIT_EXT:
            return EShLangMesh;
        default:
            assert(false);
            return EShLangVertex;
        }
    }

    [[nodiscard]] constexpr VkShaderStageFlagBits to_vulkan(
        EShLanguage const language)
    {
        switch (language)
        {
        case EShLangVertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case EShLangTessControl:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case EShLangTessEvaluation:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case EShLangGeometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case EShLangFragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case EShLangCompute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case EShLangRayGen:
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case EShLangAnyHit:
            return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case EShLangClosestHit:
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case EShLangMiss:
            return VK_SHADER_STAGE_MISS_BIT_KHR;
        case EShLangIntersect:
            return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case EShLangCallable:
            return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        case EShLangTask:
            return VK_SHADER_STAGE_TASK_BIT_EXT;
        case EShLangMesh:
            return VK_SHADER_STAGE_MESH_BIT_EXT;
        default:
            assert(false);
            return VK_SHADER_STAGE_VERTEX_BIT;
        }
    }
} // namespace vkglsl
#endif
