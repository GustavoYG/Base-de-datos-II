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

struct RecordPage {
    PageHeader header;
    int32_t recordCount;
    unsigned char data[PAGE_SIZE - sizeof(PageHeader) - sizeof(int32_t)];
};

struct IndexEntry {
    int32_t key;
    int32_t pageId;
    int32_t slot;
};
