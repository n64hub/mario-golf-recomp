// The TLB. The game maps one 128 KB window at 0x02000000 (func_8005342C, 32 4 KB pages as 16 entries of an even and
// an odd page, each mapping from index 0), each time to another object's buffer in RDRAM: it calls osVirtualToPhysical
// on the buffer, maps it, and reads and writes it through the window and through osVirtualToPhysical of window
// addresses. So one virtual range is a different stretch of RDRAM from one mapping to the next, and backing the window
// with RDRAM at a fixed place (Paper Mario's effects) would give every object the same memory.
//
// The recompiled code reads KUSEG (below 0x80000000) at rdram + RECOMP_RDRAM_OFFSET(address) (recomp.h), memory of
// its own that the runtime keeps for it: natively 2 GB past rdram, opened with recomp::make_kuseg_accessible, and on the
// web the 16 MB window RECOMP_KUSEG_WINDOW places right after RDRAM, which 0x02000000 wraps to the start of. A mapping
// copies its pages' RDRAM there (osMapTLB), and what the game then writes through the window goes back to that RDRAM
// when the entry is mapped again or unmapped (osUnmapTLB), and before each RSP task (the runtime's before_rsp_task),
// since the RSP reads the buffer by its physical address. Only the words that changed through the window go back, so
// what the game writes to the buffer through KSEG0 or DMA while it is mapped, beside the window's, stays.
//
// All of this runs on the game's threads, which run one at a time.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "recomp.h"
#include "librecomp/addresses.hpp"
#include "ultramodern/ultramodern.hpp"

#include "mg.h"

namespace {
    constexpr int num_entries = 32; // the R4300's TLB

    struct Page {
        uint32_t vaddr;
        uint32_t paddr;
        std::vector<uint8_t> snapshot; // the RDRAM copied in, to tell what the game changed through the window
    };

    struct Entry {
        bool valid = false;
        uint32_t page_size = 0;
        Page pages[2]{};
        bool has_page[2]{};
    };

    Entry entries[num_entries];

    uint8_t* kuseg_host(uint8_t* rdram, uint32_t vaddr) {
        return rdram + RECOMP_RDRAM_OFFSET((gpr)(int32_t)vaddr);
    }

    // Opens the KUSEG memory behind [vaddr, vaddr + size), natively. On the web it is always there (the window).
    bool open_kuseg(uint8_t* rdram, uint32_t vaddr, uint32_t size) {
#if defined(__EMSCRIPTEN__)
        (void)rdram;
        (void)vaddr;
        (void)size;
        return true;
#else
        return recomp::make_kuseg_accessible(rdram, vaddr, size);
#endif
    }

    // Puts what the game wrote through page's window back in its RDRAM, and takes that as the page's new snapshot.
    void write_back(uint8_t* rdram, Page& page) {
        uint32_t* window = reinterpret_cast<uint32_t*>(kuseg_host(rdram, page.vaddr));
        uint32_t* physical = reinterpret_cast<uint32_t*>(rdram + page.paddr);
        uint32_t* snapshot = reinterpret_cast<uint32_t*>(page.snapshot.data());
        size_t words = page.snapshot.size() / sizeof(uint32_t);
        for (size_t i = 0; i < words; i++) {
            if (window[i] != snapshot[i]) {
                physical[i] = window[i];
                snapshot[i] = window[i];
            }
        }
    }

    void unmap(uint8_t* rdram, int index) {
        Entry& entry = entries[index];
        if (!entry.valid) {
            return;
        }
        for (int i = 0; i < 2; i++) {
            if (entry.has_page[i]) {
                write_back(rdram, entry.pages[i]);
            }
        }
        entry.valid = false;
    }

    uint32_t virtual_to_physical(uint32_t vaddr) {
        for (const Entry& entry : entries) {
            if (!entry.valid) {
                continue;
            }
            for (int i = 0; i < 2; i++) {
                const Page& page = entry.pages[i];
                if (entry.has_page[i] && vaddr - page.vaddr < entry.page_size) {
                    return page.paddr + (vaddr - page.vaddr);
                }
            }
        }
        return 0xFFFFFFFFu;
    }

    void before_rsp_task(uint8_t* rdram) {
        for (Entry& entry : entries) {
            if (!entry.valid) {
                continue;
            }
            for (int i = 0; i < 2; i++) {
                if (entry.has_page[i]) {
                    write_back(rdram, entry.pages[i]);
                }
            }
        }
    }
}

void mg::init_tlb(uint8_t* rdram) {
    ultramodern::set_tlb_callbacks({
        .virtual_to_physical = virtual_to_physical,
        .before_rsp_task = before_rsp_task,
    });
}

// osMapTLB(index, pagemask, vaddr, evenpaddr, oddpaddr, asid): a pair of pages, each half of what pagemask spans, the
// even at vaddr and the odd after it; a paddr of -1 leaves its page unmapped.
extern "C" void osMapTLB_recomp(uint8_t* rdram, recomp_context* ctx) {
    int32_t index = (int32_t)ctx->r4;
    uint32_t pagemask = (uint32_t)ctx->r5;
    uint32_t vaddr = (uint32_t)ctx->r6;
    uint32_t paddr[2] = { (uint32_t)ctx->r7, MEM_W(0x10, ctx->r29) };
    if (index < 0 || index >= num_entries) {
        return;
    }
    unmap(rdram, index);

    Entry& entry = entries[index];
    entry.page_size = ((pagemask | 0x1FFF) + 1) >> 1;
    for (int i = 0; i < 2; i++) {
        Page& page = entry.pages[i];
        page.vaddr = vaddr + i * entry.page_size;
        page.paddr = paddr[i] & 0x1FFFFFFF;
        entry.has_page[i] = (int32_t)paddr[i] != -1 && page.paddr + entry.page_size <= recomp::mem_size &&
            page.vaddr < 0x80000000u;
        if (!entry.has_page[i]) {
            continue;
        }
        if (!open_kuseg(rdram, page.vaddr, entry.page_size)) {
            fprintf(stderr, "[tlb] can't back 0x%08X-0x%08X\n", page.vaddr, page.vaddr + entry.page_size);
            entry.has_page[i] = false;
            continue;
        }
        page.snapshot.assign(rdram + page.paddr, rdram + page.paddr + entry.page_size);
        memcpy(kuseg_host(rdram, page.vaddr), page.snapshot.data(), entry.page_size);
    }
    entry.valid = entry.has_page[0] || entry.has_page[1];
}

extern "C" void osUnmapTLB_recomp(uint8_t* rdram, recomp_context* ctx) {
    int32_t index = (int32_t)ctx->r4;
    if (index >= 0 && index < num_entries) {
        unmap(rdram, index);
    }
}
