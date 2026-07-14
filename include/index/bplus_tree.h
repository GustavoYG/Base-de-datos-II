#pragma once

#include <string>
#include <vector>

#include "common/types.h"
#include "storage/page_manager.h"
#include "storage/buffer_pool.h"

// Nodo B+ Tree serializado directamente en Page::data.
// Convencion de nodos internos: con k claves hay k+1 hijos.
//   entries[i].child = hijo i            (i en [0, keyCount])
//   entries[i].key   = separador entre hijo i e i+1 (i en [0, keyCount-1])
// En hojas, entries[i].child/pageId y entries[i].slot apuntan al registro.
#pragma pack(push, 1)
struct BTreeNodeHeader {
    int16_t isLeaf;   // 1 = hoja, 0 = interno
    int16_t keyCount;
    int32_t parent;   // pageId del padre, -1 si es la raiz
    int32_t nextLeaf; // solo hojas: pageId de la hoja siguiente, -1 si ninguna
};

struct BTreeEntry {
    int32_t key;
    int32_t child; // hoja: pageId del registro; interno: child pageId
    int32_t slot;  // hoja: slot del registro; interno: 0
};
#pragma pack(pop)

// Orden del arbol: maximo de claves por nodo antes de hacer split.
// Pequeno a proposito: fuerza splits visibles incluso con pocos registros.
static const int BTREE_MAX_KEYS = 8;

class BPlusTree {
public:
    // Crea/abre el indice en 'indexPath'. Usa su propio PageManager + BufferPool
    // (cumple el requisito de interactuar con el Buffer Manager via pin/unpin).
    BPlusTree(const std::string& indexPath, ReplacementPolicy policy = ReplacementPolicy::LRU);
    ~BPlusTree();

    // Inserta key -> (recordPageId, recordSlot). Ignora claves duplicadas.
    void Insert(int32_t key, int32_t recordPageId, int16_t recordSlot);

    // Busca key. Devuelve true y el puntero al registro si lo encuentra.
    bool Find(int32_t key, int32_t& outPageId, int16_t& outSlot);

    // Range scan inclusivo [minKey, maxKey] usando el enlace de hojas.
    void Range(int32_t minKey, int32_t maxKey, std::vector<IndexEntry>& out);

    int GetRootPageId() const { return rootPageId; }

private:
    PageManager pm;
    BufferPool bp;
    std::string metaPath;
    int rootPageId;

    BTreeNodeHeader* NodeHeader(Page* p) { return (BTreeNodeHeader*)p->data; }
    BTreeEntry* NodeEntries(Page* p) { return (BTreeEntry*)(p->data + sizeof(BTreeNodeHeader)); }

    int AllocNode(bool leaf);
    void SetParent(int childPageId, int parentPageId);

    // Retorna true si el nodo hijo hizo split; la clave/hijo promovidos salen en
    // promotedKey / newRightPageId para que el llamador (padre) los inserte.
    bool InsertRecursive(int pageId, int32_t key, int32_t recPage, int16_t recSlot,
                         int32_t& promotedKey, int& newRightPageId);

    bool SplitLeaf(int pageId, Page* p, int32_t& promotedKey, int& newRightPageId);
    bool SplitInternal(int pageId, Page* p, int32_t& promotedKey, int& newRightPageId);

    void SaveMeta();
    void LoadMeta();
};
