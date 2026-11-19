// TEMPORARY diagnostic (not for merge): with MG_DBG_LOG=<file> set, every 6th frame's display list, as border.cpp
// leaves it, is logged there: color images, scissors, fills, full-width or bottom-reaching rectangles with their
// texture coordinates, projection matrices, and the sky's matrix, vertices and triangles.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
uint8_t* g_rdram;
FILE* g_log;
uint32_t g_frame;
uint32_t g_last_mtx;
uint32_t g_timg;
std::chrono::steady_clock::time_point g_t0;

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u));
}
uint16_t rd_h(uint32_t addr) {
    return *reinterpret_cast<uint16_t*>(g_rdram + (((addr & 0x7FFFFF) ^ 2)));
}
uint8_t rd_b(uint32_t addr) {
    return g_rdram[(addr & 0x7FFFFF) ^ 3];
}
uint32_t resolve(uint32_t a) {
    if ((a & 0x0F000000) != 0) {
        return 0;
    }
    return 0x80000000u | (a & 0x7FFFFF);
}

void matrix(int depth, const char* what, uint32_t flags, uint32_t addr) {
    uint32_t m = resolve(addr);
    fprintf(g_log, "%*s%s %02X %08X", depth * 2, "", what, flags, addr);
    if (m) {
        for (int i = 0; i < 16; i++) {
            int32_t whole = int16_t(rd_h(m + i * 2));
            uint32_t frac = rd_h(m + 32 + i * 2);
            fprintf(g_log, " %.4f", whole + frac / 65536.0);
        }
    }
    fprintf(g_log, "\n");
}

void walk(uint32_t list, int depth, int& budget) {
    if (depth > 10) {
        return;
    }
    bool sky = false;
    for (uint32_t addr = list; budget-- > 0; addr += 8) {
        uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
        uint32_t op = w0 >> 24;
        auto qx = [](uint32_t w) { return ((w >> 12) & 0xFFF) / 4.0f; };
        auto qy = [](uint32_t w) { return (w & 0xFFF) / 4.0f; };
        if (sky && op != 0x05 && op != 0x06 && op != 0x07 && op != 0xE7) {
            sky = false;
        }
        switch (op) {
        case 0xDF: return;
        case 0xDE: {
            uint32_t t = resolve(w1);
            if (t) {
                walk(t, depth + 1, budget);
            }
            if ((w0 >> 16) & 0xFF) {
                return;
            }
            break;
        }
        case 0xFF: fprintf(g_log, "%*sCIMG w%u %08X\n", depth * 2, "", (w0 & 0xFFF) + 1, w1); break;
        case 0xF7: fprintf(g_log, "%*sFILLCOLOR %08X\n", depth * 2, "", w1); break;
        case 0xFD: g_timg = w1; break;
        case 0xED:
            fprintf(g_log, "%*sSCISSOR %.2f,%.2f %.2f,%.2f\n", depth * 2, "", qx(w0), qy(w0), qx(w1), qy(w1));
            break;
        case 0xF6:
            fprintf(g_log, "%*sFILLRECT %.2f,%.2f %.2f,%.2f\n", depth * 2, "", qx(w1), qy(w1), qx(w0), qy(w0));
            break;
        case 0xE4: case 0xE5: {
            float ulx = qx(w1), uly = qy(w1), lrx = qx(w0), lry = qy(w0);
            uint32_t h1 = rd_w(addr + 12), h2 = rd_w(addr + 20);
            if (lrx - ulx >= 300 || lry >= 232) {
                fprintf(g_log, "%*sTEXRECT %.2f,%.2f %.2f,%.2f s %d t %d dsdx %d dtdy %d timg %08X\n", depth * 2, "",
                        ulx, uly, lrx, lry, int16_t(h1 >> 16), int16_t(h1), int16_t(h2 >> 16), int16_t(h2),
                        g_timg);
            }
            addr += 16;
            break;
        }
        case 0xDA:
            if (w0 & 0x04) {
                matrix(depth, "PROJ", w0 & 0xFF, w1);
            }
            else {
                g_last_mtx = addr;
            }
            break;
        case 0x01: {
            uint32_t a = w1 & 0x7FFFFF;
            if (a == (0x800C69C0 & 0x7FFFFF) || a == (0x800C6A60 & 0x7FFFFF)) {
                if (g_last_mtx) {
                    matrix(depth, "SKYMTX", rd_w(g_last_mtx) & 0xFF, rd_w(g_last_mtx + 4));
                }
                uint32_t n = (w0 >> 12) & 0xFF;
                fprintf(g_log, "%*sSKYVTX %u:", depth * 2, "", n);
                for (uint32_t i = 0; i < n && i < 16; i++) {
                    uint32_t v = 0x80000000u | (a + i * 16);
                    fprintf(g_log, " (%d,%d,%d s%d t%d %08X)", int16_t(rd_h(v)), int16_t(rd_h(v + 2)),
                            int16_t(rd_h(v + 4)), int16_t(rd_h(v + 8)), int16_t(rd_h(v + 10)), rd_w(v + 12));
                }
                fprintf(g_log, "\n");
                sky = true;
            }
            break;
        }
        case 0x05: case 0x06: case 0x07:
            if (sky) {
                fprintf(g_log, "%*sSKYTRI %08X %08X\n", depth * 2, "", w0, w1);
            }
            break;
        case 0xDC:
            if ((w0 & 0xFF) == 8) {
                uint32_t vp = resolve(w1);
                if (vp) {
                    uint32_t a = rd_w(vp), b = rd_w(vp + 8);
                    fprintf(g_log, "%*sVIEWPORT s %d,%d t %d,%d\n", depth * 2, "", int16_t(a >> 16), int16_t(a),
                            int16_t(b >> 16), int16_t(b));
                }
            }
            break;
        default: break;
        }
    }
}
} // namespace

extern "C" void mg_dbg_log(uint8_t* rdram, uint32_t list) {
    static const char* path = std::getenv("MG_DBG_LOG");
    if (path == nullptr) {
        return;
    }
    g_rdram = rdram;
    if (g_log == nullptr) {
        g_log = fopen(path, "w");
        if (g_log == nullptr) {
            return;
        }
        g_t0 = std::chrono::steady_clock::now();
    }
    if ((g_frame++ % 6) != 0) {
        return;
    }
    uint32_t mask = 0;
    for (uint32_t i = 0; i < 17; i++) {
        if (rd_b(0x800B5F58 + i * 0x28) != 0) {
            mask |= 1u << i;
        }
    }
    double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_t0).count();
    fprintf(g_log, "=== frame %u t %.2f count %u overlays %05X\n", g_frame - 1, t, rd_w(0x800B67F0), mask);
    int budget = 40000;
    g_last_mtx = 0;
    walk(list | 0x80000000u, 0, budget);
    fflush(g_log);
}
