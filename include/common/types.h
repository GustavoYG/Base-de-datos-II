#pragma once

#include <cstdint>

const int PAGE_SIZE = 4096;

struct PassengerRecord {
    int32_t passengerId;
    int32_t survived;
    int32_t pclass;
    char name[64];
    char sex[8];
    float age;
    int32_t sibSp;
    int32_t parch;
    char ticket[32];
    float fare;
    char cabin[16];
    char embarked[4];
};

struct PageHeader {
    int32_t pageId;
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
    int32_t key;
    int32_t pageId;
    int32_t slot;
};
