#version 460 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

const int CHUNK_SIZE = 16;

struct Chunk {
    mat4 model;
    int cx;
    int cz;
    int minY;
    int maxY;
    uint numVertices;
    uint firstIndex;
    uint _pad0;
    uint _pad1;
};

struct ChunkDrawArraysCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    uint baseInstance;
    uint chunkIndex;
};

struct ChunkDrawElementsCommand {
	uint count;
	uint instanceCount;
	uint firstIndex;
	uint baseVertex;
	uint baseInstance;
	uint chunkIndex;
};

layout (binding = 0) writeonly buffer DrawCommands {
    ChunkDrawElementsCommand drawCommands[];
};

layout (binding = 1) readonly buffer Chunks {
    Chunk chunks[];
};

layout (binding = 2) buffer CommandCount {
    uint commandCount;
};

uniform vec4 frustum[6];

bool isVisible(int cx, int cz, int minY, int maxY) {
    float minX = cx * CHUNK_SIZE;
    float maxX = minX + CHUNK_SIZE;
    float minZ = cz * CHUNK_SIZE;
    float maxZ = minZ + CHUNK_SIZE;

    // Check AABB outside/inside of frustum
    for (int i = 0; i < 6; ++i) {
        int res = 0;
        res += ((dot(frustum[i], vec4(minX, minY, minZ, 1.0)) < 0.0) ? 1 : 0);
        res += ((dot(frustum[i], vec4(maxX, minY, minZ, 1.0)) < 0.0) ? 1 : 0);
        res += ((dot(frustum[i], vec4(minX, maxY, minZ, 1.0)) < 0.0) ? 1 : 0);
        res += ((dot(frustum[i], vec4(maxX, maxY, minZ, 1.0)) < 0.0) ? 1 : 0);
        res += ((dot(frustum[i], vec4(minX, minY, maxZ, 1.0)) < 0.0) ? 1 : 0);
        res += ((dot(frustum[i], vec4(maxX, minY, maxZ, 1.0)) < 0.0) ? 1 : 0);
        res += ((dot(frustum[i], vec4(minX, maxY, maxZ, 1.0)) < 0.0) ? 1 : 0);
        res += ((dot(frustum[i], vec4(maxX, maxY, maxZ, 1.0)) < 0.0) ? 1 : 0);
        if (res == 8) {
            return false;
        }
    }

    return true;
}

void main() {
    uint index = gl_GlobalInvocationID.x;
    if (index >= chunks.length()) {
        return;
    }

    Chunk chunk = chunks[index];
    bool visible = isVisible(chunk.cx, chunk.cz, chunk.minY, chunk.maxY);

    if (true) {
        uint dci = atomicAdd(commandCount, 1);

        drawCommands[dci].count = chunk.numVertices;
        drawCommands[dci].instanceCount = 1;
        drawCommands[dci].firstIndex = chunk.firstIndex;
        drawCommands[dci].baseVertex = 0;
        drawCommands[dci].baseInstance = 0;
        drawCommands[dci].chunkIndex = index;
    }
}
