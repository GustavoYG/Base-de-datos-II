$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')

$demoExe = Join-Path $PSScriptRoot 'demo_storage_buffer.exe'

Write-Host 'Compilando demo...'
g++ -std=c++17 -Iinclude tools/demo_storage_buffer.cpp src/common/utils.cpp src/storage/page_manager.cpp src/storage/record_manager.cpp src/storage/atomic_writer.cpp src/storage/wal_log.cpp src/storage/buffer_pool.cpp -o $demoExe

Write-Host 'Ejecutando demo interactiva. Teclea las opciones en la consola.'
& $demoExe
