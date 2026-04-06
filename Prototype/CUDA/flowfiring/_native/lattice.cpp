#include <cstdint>

// Z-axis spacing factor
// controls vertical compression of the lattice
static constexpr double Z_SCALE = 0.7;

void build_lattice_3d(int size, double* v0, double* v1,
                      int32_t* types, int32_t* dirs)
{
    int idx = 0;

    for (int z = 0; z < size * 2; ++z) {
        if (z % 2 == 0) {
            double offset = (z % 4 != 0) ? 0.5 : 0.0;
            double zz = z / 2.0 * Z_SCALE;

            for (int x = 0; x < size; ++x) {
                for (int y = 0; y < size * 2; ++y) {
                    int i3 = idx * 3;
                    if (y % 2 == 0) {
                        // Horizontal edge
                        types[idx] = (z % 4) / 2;
                        dirs[idx]  = (y % 4 == 0) ? 1 : -1;
                        double gy = y / 2.0;
                        v0[i3]     = x + offset;
                        v0[i3 + 1] = gy + offset;
                        v0[i3 + 2] = zz;
                        v1[i3]     = x + 1 + offset;
                        v1[i3 + 1] = gy + offset;
                        v1[i3 + 2] = zz;
                    } else {
                        // Vertical edge
                        types[idx] = 2 + (z % 4) / 2;
                        dirs[idx]  = (x % 2 == 0) ? 1 : -1;
                        double gy = (y - 1) / 2.0;
                        v0[i3]     = x + offset;
                        v0[i3 + 1] = gy + offset;
                        v0[i3 + 2] = zz;
                        v1[i3]     = x + offset;
                        v1[i3 + 1] = gy + 1 + offset;
                        v1[i3 + 2] = zz;
                    }
                    ++idx;
                }
            }
        } else {
            for (int x = 0; x < size; ++x) {
                for (int y = 0; y < size; ++y) {
                    double vc_x = x + 0.5;
                    double vc_y = y + 0.5;

                    if ((z - 1) % 4 == 0) {
                        double vc_z = (z + 1) / 2.0 * Z_SCALE;
                        double v0z  = (z - 1) / 2.0 * Z_SCALE;

#define EDGE(T, D, VX, VY) do {           \
    int i3 = idx * 3;                      \
    types[idx] = (T); dirs[idx] = (D);     \
    v0[i3] = (VX); v0[i3+1] = (VY); v0[i3+2] = v0z; \
    v1[i3] = vc_x; v1[i3+1] = vc_y; v1[i3+2] = vc_z; \
    ++idx;                                 \
} while(0)

                        EDGE(4,   1, x,   y);
                        EDGE(5,  -1, x+1, y);
                        EDGE(6,  -1, x,   y+1);
                        EDGE(7,   1, x+1, y+1);

                    } else {
                        double vc_z = (z - 1) / 2.0 * Z_SCALE;
                        double v0z  = (z + 1) / 2.0 * Z_SCALE;

                        EDGE(8,  -1, x,   y);
                        EDGE(9,   1, x+1, y);
                        EDGE(10,  1, x,   y+1);
                        EDGE(11, -1, x+1, y+1);
                    }
#undef EDGE
                }
            }
        }
    }
}
