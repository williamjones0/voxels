#version 460 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

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

struct ChunkDrawCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    uint baseInstance;
    uint chunkIndex;
};

layout (binding = 0) buffer DrawCommands {
    ChunkDrawCommand commands[];
};

layout (binding = 1) readonly buffer Chunks {
    Chunk chunks[];
};

layout (binding = 2) buffer CommandCount {
    uint commandCount;
};

void main() {
    uint index = gl_GlobalInvocationID.x;

    Chunk chunk = chunks[index];

    uint dci = atomicAdd(commandCount, 1);

    commands[dci].count = uint(chunk.numVertices);  // if this is set to some hardcoded value, it works
    commands[dci].instanceCount = 1;
    commands[dci].firstIndex = uint(chunk.firstIndex);  // if this is set to some hardcoded value, it works
    commands[dci].baseInstance = 0;
    commands[dci].chunkIndex = uint(index);
}
