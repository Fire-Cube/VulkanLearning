#define CGLTF_IMPLEMENTATION

#include <cgltf.h>

#include "model.h"

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
            assert(data->meshes[0].primitives[0].attributes[0].type == cgltf_attribute_type_position);

            // Indices
            u64 indexDataSize = data->meshes[0].primitives[0].indices->count * sizeof(u32);
            void* indexData = static_cast<uint8_t*>(data->meshes[0].primitives[0].indices->buffer_view->buffer->data) + data->meshes[0].primitives[0].indices->buffer_view->offset + data->meshes[0].primitives[0].indices->offset;

            createBuffer(context, &resultModel.indexBuffer, indexDataSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
            uploadDataToBuffer(context, &resultModel.indexBuffer, indexData, indexDataSize);
            resultModel.numIndices = data->meshes[0].primitives[0].indices->count;

            // Vertices
            u64 numVertices = data->meshes[0].primitives[0].attributes->data->count;
            u64 vertexDataSize = sizeof(float) * 6 * numVertices;
            std::vector<u8> vertexData(vertexDataSize);

            for (u64 i = 0; i < data->meshes[0].primitives[0].attributes_count; ++i) {
                cgltf_attribute* attribute = data->meshes[0].primitives[0].attributes + i;
                if (attribute->type == cgltf_attribute_type_position) {
                    assert(attribute->data->stride == sizeof(float) * 3);

                    u64 positionDataSize =  attribute->data->count * data->meshes[0].primitives[0].attributes[0].data->stride;
                    void* positionData = static_cast<uint8_t*>(attribute->data->buffer_view->buffer->data) + attribute->data->buffer_view->offset + data->meshes[0].primitives[0].attributes[0].data->offset;
                    fillBuffer(sizeof(float) * 3, positionData, sizeof(float) * 6, vertexData.data(), numVertices, sizeof(float) * 3);
                }
                else if (attribute->type == cgltf_attribute_type_normal) {
                    assert(attribute->data->stride == sizeof(float) * 3);

                    u64 normalDataSize =  attribute->data->count * data->meshes[0].primitives[0].attributes[0].data->stride;
                    void* normalData = static_cast<uint8_t*>(attribute->data->buffer_view->buffer->data) + attribute->data->buffer_view->offset + data->meshes[0].primitives[0].attributes[0].data->offset;
                    fillBuffer(sizeof(float) * 3, normalData, sizeof(float) * 6, vertexData.data() + sizeof(float) * 3, numVertices, sizeof(float) * 3);
                }
            }

            createBuffer(context, &resultModel.vertexBuffer, vertexDataSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
            uploadDataToBuffer(context, &resultModel.vertexBuffer, vertexData.data(), vertexDataSize);
            LOG_INFO("Loaded Model: " + std::string {filename} + " | Indices Count: " + std::to_string(resultModel.numIndices) + " | Vertices Count: " + std::to_string(numVertices) + " | Buffer Size: " + std::to_string(vertexDataSize + indexDataSize));

        }
        cgltf_free(data);
    }

    return resultModel;
}

void destroyModel(VulkanContext* context, Model* model) {
    destroyBuffer(context, &model->vertexBuffer);
    destroyBuffer(context, &model->indexBuffer);

    *model = {};
}
