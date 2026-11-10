# ==============================================================================
# Script de Compilação Local: Mali-G720 TBDR Vulkan Interceptor
# ==============================================================================

param(
    [string]$NdkPath = ""
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Mali-G720 TBDR Vulkan Interceptor - Build para Poco X8 Pro" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Localização do Android NDK
if (-not $NdkPath) {
    if ($env:ANDROID_NDK_HOME) { $NdkPath = $env:ANDROID_NDK_HOME }
    elseif ($env:ANDROID_NDK_ROOT) { $NdkPath = $env:ANDROID_NDK_ROOT }
    elseif ($env:NDK_HOME) { $NdkPath = $env:NDK_HOME }
    else {
        # Procura em locais comuns de instalacao do Android SDK
        $defaultPaths = @(
            "$env:LOCALAPPDATA\Android\ndk",
            "$env:LOCALAPPDATA\Android\Sdk\ndk",
            "C:\Android\ndk",
            "C:\Program Files\Android\ndk"
        )
        foreach ($p in $defaultPaths) {
            if (Test-Path $p) {
                $found = Get-ChildItem -Path $p -Directory | Sort-Object Name -Descending | Select-Object -First 1
                if ($found) {
                    $NdkPath = $found.FullName
                    break
                }
            }
        }
    }
}

if (-not $NdkPath -or -not (Test-Path $NdkPath)) {
    Write-Host ""
    Write-Host "[!] Android NDK nao encontrado automaticamente." -ForegroundColor Yellow
    Write-Host "    Para compilar localmente no Windows:" -ForegroundColor Gray
    Write-Host "    1. Baixe o NDK (ex: r26d ou r27) de: https://developer.android.com/ndk/downloads" -ForegroundColor Gray
    Write-Host "    2. Execute: .\build.ps1 -NdkPath 'C:\caminho\para\android-ndk'" -ForegroundColor Gray
    Write-Host ""
    Write-Host "[+] DICA RAPIDA (Recomendada): Voce tambem pode subir este projeto para o seu GitHub!" -ForegroundColor Green
    Write-Host "    O arquivo '.github/workflows/build.yml' ira compilar e gerar o .zip pronto" -ForegroundColor Green
    Write-Host "    na aba 'Actions' sem precisar instalar nada no seu computador!" -ForegroundColor Green
    exit 0
}

Write-Host "[+] Utilizando Android NDK: $NdkPath" -ForegroundColor Green

$toolchainFile = Join-Path $NdkPath "build\cmake\android.toolchain.cmake"
if (-not (Test-Path $toolchainFile)) {
    Write-Error "Toolchain CMake do NDK nao encontrado em: $toolchainFile"
}

# 2. Configuracao do CMake
$buildDir = "build_arm64"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

Write-Host "[+] Configurando CMake (arm64-v8a / Android API 29)..." -ForegroundColor Cyan
cmake -B $buildDir -G Ninja `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile" `
    -DANDROID_ABI=arm64-v8a `
    -DANDROID_PLATFORM=android-29 `
    -DCMAKE_BUILD_TYPE=Release `
    -DANDROID_STL=c++_static

# 3. Compilacao
Write-Host "[+] Compilando libvulkan_mali_tbdr.so..." -ForegroundColor Cyan
cmake --build $buildDir --config Release --target vulkan_mali_tbdr

$soFile = Join-Path $buildDir "libvulkan_mali_tbdr.so"
if (-not (Test-Path $soFile)) {
    Write-Error "Arquivo compilado libvulkan_mali_tbdr.so nao foi gerado."
}

# 4. Empacotamento
Write-Host "[+] Empacotando Custom Driver (.zip) para o Eden..." -ForegroundColor Cyan
$distDir = "dist"
if (Test-Path $distDir) { Remove-Item -Recurse -Force $distDir }
New-Item -ItemType Directory -Path $distDir | Out-Null

Copy-Item $soFile -Destination $distDir
Copy-Item "custom-driver-package\meta.json" -Destination $distDir

$zipOutput = "mali-g720-tbdr-interceptor-arm64.zip"
if (Test-Path $zipOutput) { Remove-Item -Force $zipOutput }

Compress-Archive -Path "$distDir\*" -DestinationPath $zipOutput -CompressionLevel Optimal

Write-Host ""
Write-Host "==========================================================" -ForegroundColor Green
Write-Host " [SUCESSO] Pacote gerado com sucesso: $zipOutput" -ForegroundColor Green
Write-Host " Pronto para importar no Eden (Configuracoes > GPU Driver)!" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
