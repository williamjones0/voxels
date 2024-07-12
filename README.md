# Voxels

Current progress:
- Perlin noise terrain generation (slightly optimised)
- Camera
- Chunking
- Compute shader draw command generation

Optimisation progress (featuring very rigorous benchmarking):
| Optimisation                      | FPS average (1024x1024x128 world) |
| ---                               | --- |
| Naive (no optimisation)           | 16-18 |
| Removing triangles between voxels | 40-ish |
| Naive chunking                    | 12-ish |
| Chunking with indirect drawing    | 90-100 |
| Chunking with indirect drawing and <br>single-threaded CPU frustum culling | ~10 |
| Chunking with indirect drawing and <br>GPU frustum culling | ~400 |
