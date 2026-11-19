// TEMPORARY diagnostic (not for merge): with MG_DL_LOG=<file> set, every 45th frame's display list is logged there,
// following its calls: color and depth images, scissors, fills, viewports and rectangles.

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
uint8_t* g_rdram;
FILE* g_log;
uint32_t g_frame;

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u));
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

void walk(uint32_t list, int depth, int& budget) {
    if (depth > 8) {
        return;
    }
    for (uint32_t addr = list; budget-- > 0; addr += 8) {
        uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
        uint32_t op = w0 >> 24;
        auto qx = [](uint32_t w) { return ((w >> 12) & 0xFFF) / 4.0f; };
        auto qy = [](uint32_t w) { return (w & 0xFFF) / 4.0f; };
        switch (op) {
        case 0xDF: return;
        case 0xDE: {
            uint32_t t = resolve(w1);
            fprintf(g_log, "%*sDL %08X%s\n", depth * 2, "", w1, ((w0 >> 16) & 0xFF) ? " nopush" : "");
            if (t) {
                walk(t, depth + 1, budget);
            }
            if ((w0 >> 16) & 0xFF) {
                return;
            }
            break;
        }
        case 0xFF: fprintf(g_log, "%*sCIMG w%u %08X\n", depth * 2, "", (w0 & 0xFFF) + 1, w1); break;
        case 0xFE: fprintf(g_log, "%*sZIMG %08X\n", depth * 2, "", w1); break;
        case 0xF7: if(0) fprintf(g_log, "%*sFILLCOLOR %08X\n", depth * 2, "", w1); break;
        case 0xF9: if(0) fprintf(g_log, "%*sBLENDCOLOR %08X\n", depth * 2, "", w1); break;
        case 0xFA: if(0) fprintf(g_log, "%*sPRIMCOLOR %08X\n", depth * 2, "", w1); break;
        case 0xED:
            fprintf(g_log, "%*sSCISSOR %.2f,%.2f %.2f,%.2f\n", depth * 2, "", qx(w0), qy(w0), qx(w1), qy(w1));
            break;
        case 0xF6:
            fprintf(g_log, "%*sFILLRECT %.2f,%.2f %.2f,%.2f\n", depth * 2, "", qx(w1), qy(w1), qx(w0), qy(w0));
            break;
        case 0xFD: fprintf(g_log, "%*sTIMG w%u %08X\n", depth * 2, "", (w0 & 0xFFF) + 1, w1); break;
        case 0xE4: case 0xE5:
            if (((w0 >> 12) & 0xFFF) - ((w1 >> 12) & 0xFFF) < 1000 && !((w1 & 0xFFF) <= 32 || (w0 & 0xFFF) >= 924)) break;
            fprintf(g_log, "%*sTEXRECT %.2f,%.2f %.2f,%.2f\n", depth * 2, "", qx(w1), qy(w1), qx(w0), qy(w0));
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
        case 0xDA: if(0) fprintf(g_log, "%*sMTX %02X %08X\n", depth * 2, "", w0 & 0xFF, w1); break;
        case 0x01: if(0) fprintf(g_log, "%*sVTX %u\n", depth * 2, "", (w0 >> 12) & 0xFF); break;
        default: break;
        }
    }
}
} // namespace

extern "C" void mg_dl_log(uint8_t* rdram, uint32_t list) {
    static const char* path = std::getenv("MG_DL_LOG");
    if (path == nullptr) {
        return;
    }
    g_rdram = rdram;
    if (g_log == nullptr) {
        g_log = fopen(path, "w");
        if (g_log == nullptr) {
            return;
        }
    }
    if ((g_frame++ % 1) != 0) {
        return;
    }
    uint32_t mask = 0;
    for (uint32_t i = 0; i < 17; i++) {
        if (rd_b(0x800B5F58 + i * 0x28) != 0) {
            mask |= 1u << i;
        }
    }
    fprintf(g_log, "=== frame %u overlays %05X list %08X\n", g_frame - 1, mask, list);
    int budget = 400000;
    walk(list | 0x80000000u, 0, budget);
    fflush(g_log);
}
