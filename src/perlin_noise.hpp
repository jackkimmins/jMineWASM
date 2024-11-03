#ifndef PERLIN_NOISE_HPP
#define PERLIN_NOISE_HPP

#include <vector>
#include <numeric>
#include <algorithm>
#include <random>
#include <cmath>

// Inspired by https://stackoverflow.com/questions/29711668/perlin-noise-generation

class PerlinNoise {
public:
    // Constructors
    PerlinNoise();
    PerlinNoise(unsigned int seed);

    // Get a noise value for 2D
    double noise(double x, double y) const;

    // Get a noise value for 3D
    double noise(double x, double y, double z) const;

private:
    std::vector<int> p; // Permutation vector
    double fade(double t) const;
    double lerp(double a, double b, double t) const;
    double grad(int hash, double x, double y, double z) const;
};

////////////////////////////////////////////////////////////////////////////////
// Implementation
////////////////////////////////////////////////////////////////////////////////

PerlinNoise::PerlinNoise() {
    // Initialise permutation vector with values from 0 to 255
    p.resize(256);
    std::iota(p.begin(), p.end(), 0);
    // Shuffle the permutation vector without a seed for default behavior
    std::random_device rd;
    std::mt19937 engine(rd());
    std::shuffle(p.begin(), p.end(), engine);
    // Duplicate the permutation vector
    p.insert(p.end(), p.begin(), p.end());
}

PerlinNoise::PerlinNoise(unsigned int seed) {
    p.resize(256);
    // Fill p with values from 0 to 255
    std::iota(p.begin(), p.end(), 0);
    // Shuffle using the seed
    std::default_random_engine engine(seed);
    std::shuffle(p.begin(), p.end(), engine);
    // Duplicate the permutation vector
    p.insert(p.end(), p.begin(), p.end());
}

double PerlinNoise::noise(double x, double y) const {
    // Find unit grid cell containing point
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;

    // Get relative xy coordinates of point within that cell
    x -= floor(x);
    y -= floor(y);

    // Compute fade curves for x and y
    double u = fade(x);
    double v = fade(y);

    // Hash coordinates of the square corners
    int aa = p[p[X] + Y];
    int ab = p[p[X] + Y + 1];
    int ba = p[p[X + 1] + Y];
    int bb = p[p[X + 1] + Y + 1];

    // Add blended results from the corners
    double res = lerp(v,
                      lerp(u, grad(aa, x, y, 0), grad(ba, x - 1, y, 0)),
                      lerp(u, grad(ab, x, y - 1, 0), grad(bb, x - 1, y - 1, 0))
                     );
    return res;
}

double PerlinNoise::noise(double x, double y, double z) const {
    // Find unit grid cell containing point
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;
    int Z = (int)floor(z) & 255;

    // Get relative xyz coordinates of point within that cell
    x -= floor(x);
    y -= floor(y);
    z -= floor(z);

    // Compute fade curves for x, y, z
    double u = fade(x);
    double v = fade(y);
    double w = fade(z);

    // Hash coordinates of the cube corners
    int aaa = p[p[p[X] + Y] + Z];
    int aba = p[p[p[X] + Y + 1] + Z];
    int aab = p[p[p[X] + Y] + Z + 1];
    int abb = p[p[p[X] + Y + 1] + Z + 1];
    int baa = p[p[p[X + 1] + Y] + Z];
    int bba = p[p[p[X + 1] + Y + 1] + Z];
    int bab = p[p[p[X + 1] + Y] + Z + 1];
    int bbb = p[p[p[X + 1] + Y + 1] + Z + 1];

    // Add blended results from the corners
    double res = lerp(w,
                      lerp(v,
                           lerp(u, grad(aaa, x, y, z), grad(baa, x - 1, y, z)),
                           lerp(u, grad(aba, x, y - 1, z), grad(bba, x - 1, y - 1, z))
                      ),
                      lerp(v,
                           lerp(u, grad(aab, x, y, z - 1), grad(bab, x - 1, y, z - 1)),
                           lerp(u, grad(abb, x, y - 1, z - 1), grad(bbb, x - 1, y - 1, z - 1))
                      )
                     );

    return res;
}

double PerlinNoise::fade(double t) const {
    // 6t^5 - 15t^4 + 10t^3
    return t * t * t * (t * (t * 6 - 15) + 10);
}

double PerlinNoise::lerp(double a, double b, double t) const {
    return a + t * (b - a);
}

double PerlinNoise::grad(int hash, double x, double y, double z) const {
    // Convert low 4 bits of hash code into 12 gradient directions
    int h = hash & 15;  // Take the first 4 bits of the hash
    double u = h < 8 ? x : y;
    double v;

    if (h < 4)
        v = y;
    else if (h == 12 || h == 14)
        v = x;
    else
        v = z;

    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

#endif