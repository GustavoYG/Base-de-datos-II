#pragma once

#include <cstdint>

const int PAGE_SIZE = 4096;

// Largo fijo de una clave de indice. Cualquier columna indizada se codifica en
// un BTreeKey de este tamano (entero/flotante ocupan 4 B, texto se rellena).
const int BTREE_KEY_SIZE = 64;

struct BTreeKey {
    unsigned char data[BTREE_KEY_SIZE];
};

enum class PageType : int16_t {
    Data = 0,
    IndexLeaf = 1,
    IndexInternal = 2
};

struct PageHeader {
    int32_t pageId;
    int16_t pageType;
    int32_t checksum;
    int32_t freeBytes;
};

struct Page {
    PageHeader header;
    unsigned char data[PAGE_SIZE - sizeof(PageHeader)];
};

struct SlotEntry {
    int16_t offset;
    int16_t length;
};

struct RecordPage {
    PageHeader header;
    int16_t slotCount;
    int16_t freeSpaceOffset;
    int16_t freeSlotHead; // index of first free slot (-1 if none)
    unsigned char data[PAGE_SIZE - sizeof(PageHeader) - sizeof(int16_t) * 3];
};

struct IndexEntry {
    BTreeKey key;
    int32_t pageId;
    int32_t slot;
};
