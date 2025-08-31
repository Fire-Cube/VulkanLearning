#define CGLTF_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS

#include <cgltf.h>
#include <stb_image.h>

#include "model.h"

#include <filesystem>

#include "utils.h"

void fillBuffer(u32 inputStride, void* inputData, u32 outputStride, void* outputData, u32 numElements, u32 elementSize) {
    u8* output = static_cast<u8*>(outputData);
    u8* input = static_cast<u8*>(inputData);

    for (u32 i = 0; i < numElements; ++i) {
        for (u32 j = 0; j < elementSize; ++j) {
            output[j] = input[j];
        }
        output += outputStride;
        input += inputStride;
    }
}

Model createModel(VulkanContext* context, const char* filename, const char* modelDir, cgltf_component_type componentType) {
    Model resultModel {};

    cgltf_options options = {};
    cgltf_data* data = 0;
    cgltf_result result = cgltf_parse_file(&options, filename, &data);
    if (result == cgltf_result_success) {
        result = cgltf_load_buffers(&options, data, modelDir);
        if (result == cgltf_result_success) {
            assert(data->meshes_count == 1);
            assert(data->meshes[0].primitives_count == 1);
            assert(data->meshes[0].primitives[0].attributes_count > 0);

            // Indices
            size_t indexStride;
            switch (componentType) {
                case cgltf_component_type_r_16u:
                    indexStride = sizeof(u16);
                    break;

                case cgltf_component_type_r_32u:
                    indexStride = sizeof(u32);
                    break;

                default:
                    assert(false && "unrecognized component type");
            }

            u64 indexDataSize = data->meshes[0].primitives[0].indices->count * indexStride;
            void* indexData = static_cast<uint8_t*>(data->meshes[0].primitives[0].indices->buffer_view->buffer->data) + data->meshes[0].primitives[0].indices->buffer_view->offset + data->meshes[0].primitives[0].indices->offset;

            createBuffer(context, &resultModel.indexBuffer, indexDataSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
            uploadDataToBuffer(context, &resultModel.indexBuffer, indexData, indexDataSize);
            resultModel.numIndices = data->meshes[0].primitives[0].indices->count;

            // Vertices
            u64 outputStride = sizeof(float) * 8;
            u64 numVertices = data->meshes[0].primitives[0].attributes->data->count;
            u64 vertexDataSize = outputStride * numVertices;
            std::vector<u8> vertexData(vertexDataSize);

            for (u64 i = 0; i < data->meshes[0].primitives[0].attributes_count; ++i) {
                cgltf_attribute* attribute = data->meshes[0].primitives[0].attributes + i;
                u8* bufferBase = static_cast<uint8_t*>(attribute->data->buffer_view->buffer->data) + attribute->data->buffer_view->offset + attribute->data->offset;
                u64 inputStride = attribute->data->stride;

                if (attribute->type == cgltf_attribute_type_position) {
                    void* positionData = bufferBase;

                    fillBuffer(inputStride, positionData, outputStride, vertexData.data(), numVertices, sizeof(float) * 3);
                }
                else if (attribute->type == cgltf_attribute_type_normal) {
                    void* normalData = bufferBase;

                    fillBuffer(inputStride, normalData, outputStride, vertexData.data() + sizeof(float) * 3, numVertices, sizeof(float) * 3);
                }
                else if (attribute->type == cgltf_attribute_type_texcoord) {
                    void* texcoordData = bufferBase;

                    fillBuffer(inputStride, texcoordData, outputStride, vertexData.data() + (sizeof(float) * 6), numVertices, sizeof(float) * 2);
                }
            }

            createBuffer(context, &resultModel.vertexBuffer, vertexDataSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
            uploadDataToBuffer(context, &resultModel.vertexBuffer, vertexData.data(), vertexDataSize);

            assert(data->meshes_count == 1);
            cgltf_material* material = &data->materials[0];
            assert(material->has_pbr_metallic_roughness);
            cgltf_texture_view albedoTextureView = material->pbr_metallic_roughness.base_color_texture;
            assert(!albedoTextureView.has_transform);
            assert(albedoTextureView.texcoord == 0);
            assert(albedoTextureView.texture);

            cgltf_texture* albedoTexture = albedoTextureView.texture;

            cgltf_buffer_view* bufferView = albedoTexture->image->buffer_view;
            assert(bufferView->size < INT32_MAX);

            int bpp, width, height;
            u8* textureData = stbi_load_from_memory(static_cast<stbi_uc*>(bufferView->buffer->data), static_cast<int>(bufferView->size), &width, &height, &bpp, 4);
            assert(textureData);
            bpp = 4;

            createImage(context, &resultModel.albedoTexture, width, height, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
            uploadDataToImage(context, &resultModel.albedoTexture, textureData, width * height * bpp, width, height, vk::ImageLayout::eReadOnlyOptimal, vk::AccessFlagBits::eShaderRead);
            stbi_image_free(textureData);

            u64 textureDataSize = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * static_cast<uint64_t>(bpp);

            LOG_INFO("Loaded Model: " + std::string {filename} + " | Indices Count: " + utils::formatNumber(resultModel.numIndices) + " | Vertices Count: " + utils::formatNumber(numVertices) + " | Buffer Size: " + utils::formatBytes(vertexDataSize + indexDataSize + textureDataSize));

        }
        else {
            LOG_ERROR("Could not load additional model buffers");
        }
        cgltf_free(data);
    }
    else {
        std::string additionalInfo;
        if (result == cgltf_result_file_not_found) {
            additionalInfo = " (File not found)";
        }
        LOG_ERROR("Could not load model from file" + additionalInfo);
    }

    return resultModel;
}

void destroyModel(VulkanContext* context, Model* model) {
    destroyBuffer(context, &model->vertexBuffer);
    destroyBuffer(context, &model->indexBuffer);
    destroyImage(context, &model->albedoTexture);

    *model = {};
}
