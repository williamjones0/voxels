# Voxels

Current progress:
- Perlin noise terrain generation (slightly optimised)
- Camera

Optimisation progress (featuring very rigorous benchmarking):
| Optimisation                      | FPS average (1024x1024 world) |
| ---                               | --- |
| Naive (no optimisation)           | 16-18 |
| Removing triangles between voxels | 40-ish |
