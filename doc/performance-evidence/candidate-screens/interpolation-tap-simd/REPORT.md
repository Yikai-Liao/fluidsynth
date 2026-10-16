# Interpolation tap SIMD reduction

**Decision:** rejected; PCM was exact, but generated SIMD did not produce an end-to-end benefit.

The prototype expresses the four 4th-order interpolation tap products as an OpenMP SIMD reduction. In the ordinary project CMake setup OpenMP is enabled for C but not the C++ translation unit, so the directive is ignored there. An additional C++ `-fopenmp` build generated packed arithmetic, but five paired renders showed a 2.5% median regression in double precision and essentially flat float results. Both precision suites passed 41/41 and PCM hashes matched. The branch retains the measured prototype for review; it is not suitable for merge.
