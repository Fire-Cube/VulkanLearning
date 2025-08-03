#define CGLTF_IMPLEMENTATION

#include "cgltf.h"

#include "model.h"

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
            assert(data->meshes[0].primitives[0].attributes[0].data->stride == sizeof(float) * 3);

            // Indices
            u64 indexDataSize = data->meshes[0].primitives[0].indices->count * sizeof(u32);
            void* indexData = static_cast<uint8_t*>(data->meshes[0].primitives[0].indices->buffer_view->buffer->data) + data->meshes[0].primitives[0].indices->buffer_view->offset + data->meshes[0].primitives[0].indices->offset;

            createBuffer(context, &resultModel.indexBuffer, indexDataSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
            uploadDataToBuffer(context, &resultModel.indexBuffer, indexData, indexDataSize);
            resultModel.numIndices = data->meshes[0].primitives[0].indices->count;

            // Vertices
            u64 vertexDataSize =  data->meshes[0].primitives[0].attributes[0].data->count * data->meshes[0].primitives[0].attributes[0].data->stride;
            void* vertexData = static_cast<uint8_t*>(data->meshes[0].primitives[0].attributes[0].data->buffer_view->buffer->data) + data->meshes[0].primitives[0].attributes[0].data->buffer_view->offset + data->meshes[0].primitives[0].attributes[0].data->offset;

            createBuffer(context, &resultModel.vertexBuffer, vertexDataSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
            uploadDataToBuffer(context, &resultModel.vertexBuffer, vertexData, vertexDataSize);
            LOG_INFO("Loaded Model: " + std::string {filename} + " | Indices Count: " + std::to_string(resultModel.numIndices) + " | Buffer Size: " + std::to_string(vertexDataSize + indexDataSize));

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
