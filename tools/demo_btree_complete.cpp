#include <iostream>
#include <string>
#include <cstdio>
#include <cstring>
#include "../include/storage/page_manager.h"
#include "../include/storage/buffer_pool.h"
#include "../include/storage/btree.h"

using namespace std;
using namespace storage;

void ClearScreen() {
    system("cls");
}

void PrintMenu() {
    cout << "\n========== B+ TREE DEMO ==========" << endl;
    cout << "1) Insertar clave en el árbol B+" << endl;
    cout << "2) Buscar clave en el árbol B+" << endl;
    cout << "3) Eliminar clave del árbol B+" << endl;
    cout << "4) Mostrar estructura del árbol" << endl;
    cout << "5) Demo automática" << endl;
    cout << "6) Salir" << endl;
    cout << "==================================" << endl;
    cout << "Seleccione una opción: ";
}

void DemoInsert(BPlusTree& tree) {
    cout << "\n--- Insertar Clave ---" << endl;
    int64_t key;
    uint32_t pageId;
    uint16_t slotNum;
    
    cout << "Ingrese la clave a insertar: ";
    cin >> key;
    cout << "Ingrese el ID de página (destino): ";
    cin >> pageId;
    cout << "Ingrese el número de slot: ";
    cin >> slotNum;
    
    if (tree.Insert(key, pageId, slotNum)) {
        cout << "✓ Clave " << key << " insertada exitosamente" << endl;
    } else {
        cout << "✗ Error al insertar la clave" << endl;
    }
}

void DemoSearch(BPlusTree& tree) {
    cout << "\n--- Buscar Clave ---" << endl;
    int64_t key;
    cout << "Ingrese la clave a buscar: ";
    cin >> key;
    
    uint32_t pageId;
    uint16_t slotNum;
    
    if (tree.Search(key, pageId, slotNum)) {
        cout << "✓ Clave " << key << " encontrada" << endl;
        cout << "  - ID de página: " << pageId << endl;
        cout << "  - Número de slot: " << slotNum << endl;
    } else {
        cout << "✗ Clave " << key << " no encontrada" << endl;
    }
}

void DemoDelete(BPlusTree& tree) {
    cout << "\n--- Eliminar Clave ---" << endl;
    int64_t key;
    cout << "Ingrese la clave a eliminar: ";
    cin >> key;
    
    if (tree.Delete(key)) {
        cout << "✓ Clave " << key << " eliminada exitosamente" << endl;
    } else {
        cout << "✗ No se encontró la clave " << key << endl;
    }
}

void DemoShowStructure(BPlusTree& tree) {
    cout << "\n--- Estructura del Árbol B+ ---" << endl;
    cout << "Altura estimada del árbol: " << tree.GetTreeHeight() << endl;
    cout << "Número de nodos: " << tree.GetNodeCount() << endl;
    tree.PrintTree();
}

void DemoAutomatic(BPlusTree& tree) {
    cout << "\n--- Demo Automática ---" << endl;
    cout << "Insertando 20 claves en orden aleatorio..." << endl;
    
    int keys[] = {5, 3, 7, 2, 6, 8, 1, 4, 9, 10, 15, 12, 18, 11, 14, 20, 16, 13, 19, 17};
    
    for (int i = 0; i < 20; i++) {
        int key = keys[i];
        tree.Insert(key, key * 100, key % 5);
        cout << "  Insertado: " << key << endl;
    }
    
    cout << "\nEstructura del árbol después de inserciones:" << endl;
    DemoShowStructure(tree);
    
    cout << "\nBuscando claves específicas..." << endl;
    int searchKeys[] = {5, 15, 20};
    uint32_t pageId;
    uint16_t slotNum;
    
    for (int key : searchKeys) {
        if (tree.Search(key, pageId, slotNum)) {
            cout << "  ✓ Clave " << key << " encontrada (página: " << pageId << ")" << endl;
        }
    }
    
    cout << "\nEliminando claves: 3, 7, 15..." << endl;
    tree.Delete(3);
    tree.Delete(7);
    tree.Delete(15);
    cout << "  Eliminaciones completadas" << endl;
    
    cout << "\nEstructura final del árbol:" << endl;
    DemoShowStructure(tree);
    
    cout << "\nVerificando que claves eliminadas no existen..." << endl;
    int deletedKeys[] = {3, 7, 15};
    for (int key : deletedKeys) {
        if (!tree.Search(key, pageId, slotNum)) {
            cout << "  ✓ Clave " << key << " confirmada como eliminada" << endl;
        }
    }
    
    cout << "\n✓ Demo automática completada exitosamente" << endl;
}

int main() {
    try {
        ClearScreen();
        
        cout << "=== Inicializando B+ Tree ===" << endl;
        
        // Initialize storage
        const char* treeFile = "data/storage/tables/demo_btree.tbl";
        
        // Clean up previous demo files
        std::remove(treeFile);
        
        PageManager pageManager(treeFile);
        cout << "✓ PageManager inicializado" << endl;
        
        BufferPool bufferPool(pageManager, 20);
        cout << "✓ BufferPool inicializado" << endl;
        
        BPlusTree tree(&pageManager, &bufferPool);
        cout << "✓ B+ Tree inicializado" << endl;
        
        cout << "\n=== B+ Tree listo para demostración ===" << endl;
        
        int option = 0;
        while (true) {
            PrintMenu();
            cin >> option;
            cin.ignore();  // Limpiar buffer de entrada
            
            switch (option) {
                case 1:
                    DemoInsert(tree);
                    break;
                case 2:
                    DemoSearch(tree);
                    break;
                case 3:
                    DemoDelete(tree);
                    break;
                case 4:
                    DemoShowStructure(tree);
                    break;
                case 5:
                    DemoAutomatic(tree);
                    break;
                case 6:
                    cout << "\n✓ Saliendo del demo..." << endl;
                    return 0;
                default:
                    cout << "✗ Opción no válida. Intente nuevamente." << endl;
            }
        }
        
        return 0;
    } catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}
