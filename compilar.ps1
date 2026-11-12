# Compila transmissor.exe e receptor.exe usando o GCC + GStreamer do MSYS2 (UCRT64).
# Uso (no terminal PowerShell do VS Code, dentro desta pasta):
#   powershell -ExecutionPolicy Bypass -File .\compilar.ps1

$msys = "C:\msys64\ucrt64\bin"
if (-not (Test-Path "$msys\gcc.exe")) {
    Write-Host "ERRO: nao encontrei $msys\gcc.exe. Instale o MSYS2 e os pacotes do README." -ForegroundColor Red
    exit 1
}
$env:Path = "$msys;$env:Path"

$flags = (pkg-config --cflags --libs gstreamer-1.0) -split '\s+' | Where-Object { $_ }

gcc -Wall transmissor.c -o transmissor.exe @flags
if ($LASTEXITCODE -ne 0) { Write-Host "Falha ao compilar transmissor.c" -ForegroundColor Red; exit 1 }

gcc -Wall receptor.c -o receptor.exe @flags
if ($LASTEXITCODE -ne 0) { Write-Host "Falha ao compilar receptor.c" -ForegroundColor Red; exit 1 }

Write-Host "OK: transmissor.exe e receptor.exe gerados." -ForegroundColor Green
