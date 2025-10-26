#ifndef PERLIN_NOISE_HPP
#define PERLIN_NOISE_HPP

#include <array>
#include <random>
#include <algorithm>
#include <cmath>

class PerlinNoise {
public:
    explicit PerlinNoise(unsigned int seed = 0) {
        // Initialise identity permutation 0..255
        for (int i = 0; i < 256; ++i) perm[i] = i;

        // Shuffle first 256 values using seed
        std::mt19937 engine(seed);
        std::shuffle(perm.begin(), perm.begin() + 256, engine);

        // Duplicate the permutation to avoid overflow in indexing (512-length table)
        for (int i = 0; i < 256; ++i) perm[256 + i] = perm[i];
    }

    double noise(double x, double y) const {
        // Find unit grid cell containing the point
        int X = (int)std::floor(x) & 255;  // lattice X index (mod 256)
        int Y = (int)std::floor(y) & 255;  // lattice Y index (mod 256)

        // Local coordinates within cell (fractional part)
        double fx = x - std::floor(x);
        double fy = y - std::floor(y);

        // Compute fade curves for x and y
        double u = fade(fx);
        double v = fade(fy);

        // Hash corner indices via permutation table
        int A = perm[X] + Y;
        int B = perm[X + 1] + Y;

        // Compute corner contributions (gradient dot product with relative position)
        double n00 = grad(perm[A], fx, fy);
        double n10 = grad(perm[B], fx - 1, fy);
        double n01 = grad(perm[A + 1], fx, fy - 1);
        double n11 = grad(perm[B + 1], fx - 1, fy - 1);

        // Bilinear interpolation of the four corners
        double nx0 = lerp(n00, n10, u);  // interpolate along x for y0
        double nx1 = lerp(n01, n11, u);  // interpolate along x for y1
        double nxy = lerp(nx0, nx1, v);  // interpolate along y

        return nxy;
    }

    double noise(double x, double y, double z) const {
        // Find unit grid cell containing point
        int X = (int)std::floor(x) & 255;  // lattice X index
        int Y = (int)std::floor(y) & 255;  // lattice Y index
        int Z = (int)std::floor(z) & 255;  // lattice Z index

        // Local coordinates within the cell
        double fx = x - std::floor(x);
        double fy = y - std::floor(y);
        double fz = z - std::floor(z);

        // Compute fade curves for x, y, z
        double u = fade(fx);
        double v = fade(fy);
        double w = fade(fz);

        // Hash coordinates of the cube's eight corners
        int A  = perm[X] + Y;
        int AA = perm[A] + Z;
        int AB = perm[A + 1] + Z;
        int B  = perm[X + 1] + Y;
        int BA = perm[B] + Z;
        int BB = perm[B + 1] + Z;

        // Compute corner contributions (gradient dot products)
        double n000 = grad(perm[AA],     fx,     fy,     fz);
        double n100 = grad(perm[BA],     fx - 1, fy,     fz);
        double n010 = grad(perm[AB],     fx,     fy - 1, fz);
        double n110 = grad(perm[BB],     fx - 1, fy - 1, fz);
        double n001 = grad(perm[AA + 1], fx,     fy,     fz - 1);
        double n101 = grad(perm[BA + 1], fx - 1, fy,     fz - 1);
        double n011 = grad(perm[AB + 1], fx,     fy - 1, fz - 1);
        double n111 = grad(perm[BB + 1], fx - 1, fy - 1, fz - 1);

        // Trilinear interpolation of the eight corners
        double nx00 = lerp(n000, n100, u); // interpolate along x (layer z0, y0)
        double nx10 = lerp(n010, n110, u); // interpolate along x (layer z0, y1)
        double nx01 = lerp(n001, n101, u); // interpolate along x (layer z1, y0)
        double nx11 = lerp(n011, n111, u); // interpolate along x (layer z1, y1)
        double nxy0 = lerp(nx00, nx10, v); // interpolate along y (for z0)
        double nxy1 = lerp(nx01, nx11, v); // interpolate along y (for z1)
        double nxyz = lerp(nxy0, nxy1, w); // interpolate along z
        
        return nxyz;
    }

private:
    std::array<int, 512> perm;

    // Pre-defined gradient directions for 2D (8 directions).
    static constexpr double grad2[8][2] = {
        { 1.0,  0.0},  {-1.0,  0.0},  { 0.0,  1.0},  { 0.0, -1.0},
        { 0.70710678,  0.70710678},  {-0.70710678,  0.70710678},
        { 0.70710678, -0.70710678},  {-0.70710678, -0.70710678}
    };

    // Pre-defined gradient directions for 3D (16 directions, each of length sqroot(2)).
    static constexpr double grad3[16][3] = {
        { 1.0,  1.0,  0.0},  {-1.0,  1.0,  0.0},  { 1.0, -1.0,  0.0},  {-1.0, -1.0,  0.0},
        { 1.0,  0.0,  1.0},  {-1.0,  0.0,  1.0},  { 1.0,  0.0, -1.0},  {-1.0,  0.0, -1.0},
        { 0.0,  1.0,  1.0},  { 0.0, -1.0,  1.0},  { 0.0,  1.0, -1.0},  { 0.0, -1.0, -1.0},
        { 1.0,  1.0,  0.0},  {-1.0,  1.0,  0.0},  { 0.0, -1.0,  1.0},  { 0.0, -1.0, -1.0}
    };

    // Quintic fade function
    static inline double fade(double t) {
        return t * t * t * (t * (t * 6 - 15) + 10);
    }

    // Linear interpolation
    static inline double lerp(double a, double b, double t) {
        return a + t * (b - a);
    }

    // Compute dot product of the selected 2D gradient with (x, y)
    static inline double grad(int hash, double x, double y) {
        int h = hash & 7; // only lower 3 bits to get 8 possible directions
        const double *g = grad2[h];
        return g[0] * x + g[1] * y;
    }

    // Compute dot product of the selected 3D gradient with (x, y, z)
    static inline double grad(int hash, double x, double y, double z) {
        int h = hash & 15; // lower 4 bits for 16 directions
        const double *g = grad3[h];
        return g[0] * x + g[1] * y + g[2] * z;
    }
};

#endif // PERLIN_NOISE_HPP