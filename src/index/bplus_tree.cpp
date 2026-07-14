#include "index/bplus_tree.h"

#include <fstream>
#include <cstring>

BPlusTree::BPlusTree(const std::string& indexPath, ReplacementPolicy policy)
    : pm(indexPath), bp(pm, 16, policy), metaPath(indexPath + ".meta"), rootPageId(-1) {
    LoadMeta();
}

BPlusTree::~BPlusTree() {
    // El destructor de bp (miembro) hace FlushAll() y vuelca las paginas sucias.
    SaveMeta();
}

int BPlusTree::AllocNode(bool leaf) {
    int pid = pm.AllocatePage(leaf ? PageType::IndexLeaf : PageType::IndexInternal);
    Page* p = bp.PinPage(pid);
    BTreeNodeHeader* h = NodeHeader(p);
    h->isLeaf = leaf ? 1 : 0;
    h->keyCount = 0;
    h->parent = -1;
    h->nextLeaf = -1;
    // entries ya en cero por AllocatePage
    bp.UnpinPage(pid, true);
    return pid;
}

void BPlusTree::SetParent(int childPageId, int parentPageId) {
    if (childPageId < 0) return;
    Page* cp = bp.PinPage(childPageId);
    NodeHeader(cp)->parent = parentPageId;
    bp.UnpinPage(childPageId, true);
}

void BPlusTree::Insert(int32_t key, int32_t recPage, int16_t recSlot) {
    if (rootPageId < 0) {
        rootPageId = AllocNode(true);
        SaveMeta();
    }

    int32_t promotedKey = 0;
    int newRight = -1;
    bool split = InsertRecursive(rootPageId, key, recPage, recSlot, promotedKey, newRight);

    if (split) {
        // El nodo raiz se dividio: crear un nuevo root interno.
        int newRoot = AllocNode(false);
        Page* rp = bp.PinPage(newRoot);
        BTreeNodeHeader* rh = NodeHeader(rp);
        BTreeEntry* re = NodeEntries(rp);
        rh->keyCount = 1;
        re[0].key = promotedKey;
        re[0].child = rootPageId; // primer hijo = raiz vieja
        re[1].key = 0;
        re[1].child = newRight;   // segundo hijo = nueva hoja/interior derecho
        re[1].slot = 0;
        bp.UnpinPage(newRoot, true);

        SetParent(rootPageId, newRoot);
        SetParent(newRight, newRoot);

        rootPageId = newRoot;
        SaveMeta();
    }
}

bool BPlusTree::InsertRecursive(int pageId, int32_t key, int32_t recPage, int16_t recSlot,
                                int32_t& promotedKey, int& newRightPageId) {
    Page* p = bp.PinPage(pageId);
    BTreeNodeHeader* h = NodeHeader(p);
    BTreeEntry* e = NodeEntries(p);

    if (h->isLeaf) {
        // Clave duplicada: ignorar (indice unico sobre passengerId).
        for (int i = 0; i < h->keyCount; ++i) {
            if (e[i].key == key) {
                bp.UnpinPage(pageId, false);
                return false;
            }
        }
        // Insertar en orden ascendente de clave.
        int pos = h->keyCount;
        for (int i = 0; i < h->keyCount; ++i) {
            if (key < e[i].key) { pos = i; break; }
        }
        for (int i = h->keyCount; i > pos; --i) e[i] = e[i - 1];
        e[pos].key = key;
        e[pos].child = recPage;
        e[pos].slot = recSlot;
        h->keyCount++;

        bool split = false;
        if (h->keyCount > BTREE_MAX_KEYS) {
            split = SplitLeaf(pageId, p, promotedKey, newRightPageId);
        }
        bp.UnpinPage(pageId, true);
        return split;
    }

    // Nodo interno: descender al hijo adecuado.
    int childPos = h->keyCount;
    for (int i = 0; i < h->keyCount; ++i) {
        if (key < e[i].key) { childPos = i; break; }
    }
    int childId = e[childPos].child;

    int32_t childPromotedKey = 0;
    int childNewRight = -1;
    bool childSplit = InsertRecursive(childId, key, recPage, recSlot, childPromotedKey, childNewRight);
    if (!childSplit) {
        bp.UnpinPage(pageId, false);
        return false;
    }

    // Insertar el separador promovido y el nuevo hijo derecho en este interno.
    int pos = h->keyCount;
    for (int i = 0; i < h->keyCount; ++i) {
        if (childPromotedKey < e[i].key) { pos = i; break; }
    }
    // Abrir hueco: desplazar hijos y claves una posicion a la derecha desde pos.
    for (int i = h->keyCount; i > pos; --i) e[i].child = e[i - 1].child;
    for (int i = h->keyCount - 1; i >= pos; --i) e[i + 1].key = e[i].key;
    e[pos].key = childPromotedKey;
    e[pos + 1].child = childNewRight;
    e[pos + 1].slot = 0;
    h->keyCount++;

    SetParent(childNewRight, pageId);

    bool split = false;
    if (h->keyCount > BTREE_MAX_KEYS) {
        split = SplitInternal(pageId, p, promotedKey, newRightPageId);
    }
    bp.UnpinPage(pageId, true);
    return split;
}

bool BPlusTree::SplitLeaf(int pageId, Page* p, int32_t& promotedKey, int& newRightPageId) {
    BTreeNodeHeader* h = NodeHeader(p);
    BTreeEntry* e = NodeEntries(p);
    int total = h->keyCount;
    int rightCount = total / 2;
    int leftCount = total - rightCount;

    int newRight = AllocNode(true);
    Page* rp = bp.PinPage(newRight);
    BTreeNodeHeader* rh = NodeHeader(rp);
    BTreeEntry* re = NodeEntries(rp);
    for (int i = 0; i < rightCount; ++i) re[i] = e[leftCount + i];
    rh->keyCount = rightCount;
    rh->parent = h->parent;
    rh->nextLeaf = h->nextLeaf;

    h->keyCount = leftCount;
    h->nextLeaf = newRight;
    bp.UnpinPage(newRight, true);

    // En B+ Tree la clave separadora se COPIA a la hoja derecha (no se quita de la izq).
    promotedKey = re[0].key;
    newRightPageId = newRight;
    return true;
}

bool BPlusTree::SplitInternal(int pageId, Page* p, int32_t& promotedKey, int& newRightPageId) {
    BTreeNodeHeader* h = NodeHeader(p);
    BTreeEntry* e = NodeEntries(p);
    int total = h->keyCount;
    int mid = total / 2;               // indice de la clave a promover
    int rightKeyCount = total - mid - 1;

    int newRight = AllocNode(false);
    Page* rp = bp.PinPage(newRight);
    BTreeNodeHeader* rh = NodeHeader(rp);
    BTreeEntry* re = NodeEntries(rp);
    for (int i = 0; i < rightKeyCount; ++i) {
        re[i].key = e[mid + 1 + i].key;
        re[i].child = e[mid + 1 + i].child;
        re[i].slot = 0;
    }
    re[rightKeyCount].key = 0;
    re[rightKeyCount].child = e[total].child; // ultimo hijo pasa a la derecha
    re[rightKeyCount].slot = 0;
    rh->keyCount = rightKeyCount;
    rh->parent = h->parent;

    for (int i = 0; i <= rightKeyCount; ++i) SetParent(re[i].child, newRight);
    bp.UnpinPage(newRight, true);

    // Nodo izquierdo queda con claves [0, mid-1] y hijos [0, mid]. La clave en
    // 'mid' se promueve; su hijo izquierdo (e[mid].child) ya es el hijo 'mid'.
    h->keyCount = mid;
    promotedKey = e[mid].key;
    newRightPageId = newRight;
    return true;
}

bool BPlusTree::Find(int32_t key, int32_t& outPageId, int16_t& outSlot) {
    if (rootPageId < 0) return false;

    int pageId = rootPageId;
    while (true) {
        Page* p = bp.PinPage(pageId);
        BTreeNodeHeader* h = NodeHeader(p);
        BTreeEntry* e = NodeEntries(p);

        if (h->isLeaf) {
            for (int i = 0; i < h->keyCount; ++i) {
                if (e[i].key == key) {
                    outPageId = e[i].child;
                    outSlot = (int16_t)e[i].slot;
                    bp.UnpinPage(pageId, false);
                    return true;
                }
            }
            bp.UnpinPage(pageId, false);
            return false;
        }

        int childPos = h->keyCount;
        for (int i = 0; i < h->keyCount; ++i) {
            if (key < e[i].key) { childPos = i; break; }
        }
        int childId = e[childPos].child;
        bp.UnpinPage(pageId, false);
        pageId = childId;
    }
}

void BPlusTree::Range(int32_t minKey, int32_t maxKey, std::vector<IndexEntry>& out) {
    out.clear();
    if (rootPageId < 0) return;

    // 1) Localizar la hoja donde cae minKey.
    int pageId = rootPageId;
    while (true) {
        Page* p = bp.PinPage(pageId);
        BTreeNodeHeader* h = NodeHeader(p);
        if (h->isLeaf) break;
        BTreeEntry* e = NodeEntries(p);
        int childPos = h->keyCount;
        for (int i = 0; i < h->keyCount; ++i) {
            if (minKey < e[i].key) { childPos = i; break; }
        }
        int childId = e[childPos].child;
        bp.UnpinPage(pageId, false);
        pageId = childId;
    }

    // 2) Recorrer hojas enlazadas mientras key <= maxKey.
    while (pageId != -1) {
        Page* p = bp.PinPage(pageId);
        BTreeNodeHeader* h = NodeHeader(p);
        BTreeEntry* e = NodeEntries(p);
        for (int i = 0; i < h->keyCount; ++i) {
            if (e[i].key > maxKey) {
                bp.UnpinPage(pageId, false);
                return;
            }
            if (e[i].key >= minKey) {
                IndexEntry ie;
                ie.key = e[i].key;
                ie.pageId = e[i].child;
                ie.slot = e[i].slot;
                out.push_back(ie);
            }
        }
        int next = h->nextLeaf;
        bp.UnpinPage(pageId, false);
        pageId = next;
    }
}

void BPlusTree::SaveMeta() {
    std::ofstream f(metaPath, std::ios::binary | std::ios::trunc);
    if (f) f.write((char*)&rootPageId, sizeof(int));
}

void BPlusTree::LoadMeta() {
    std::ifstream f(metaPath, std::ios::binary);
    if (f && f.read((char*)&rootPageId, sizeof(int))) {
        // meta valido: reusar el arbol existente en disco
    } else {
        rootPageId = -1;
    }
}
