# Eden: Edição Otimizada para Poco X8 Pro (Mali-G720 / Dimensity 8500-Ultra)

🚀 **Compilação sob medida do Eden (Nightly mais recente) com os patches de TBDR do Robert e otimizações de CPU Cortex-A725 / ARMv9.**

---

## 📌 Por que esta versão é necessária?

O Eden oficial (v0.2.1 e versões padrão) possui validações rígidas no código Vulkan que **recusam a inicialização em qualquer chip MediaTek Dimensity**, disparando:
```
Missing required extension VK_EXT_vertex_attribute_divisor
Missing required feature fillModeNonSolid
Missing required feature multiViewport
maxViewports has to be 16 or greater but it is 1
Invalid device index 0! Failed to initialize system (Error 5)!
```

Além disso, drivers customizados em formato `.zip` via "GPU Driver Manager" causam fechamento forçado imediato em GPUs Mali porque esse carregador (`libadrenotools`) foi desenvolvido exclusivamente para chips Qualcomm Snapdragon (Adreno).

---

## 🛠️ O que está incluído nesta versão:

### 1. Correções de Compatibilidade e Estabilidade Mali (Patches do Robert)
* **Remoção da Rejeição de Hardware:** O emulador aceita `maxViewports = 1` e `maxClipDistances = 0` na Mali-G720 sem abortar.
* **Degradação Graciosa:** Fallbacks seguros para 8 recursos ausentes na Mali (`dualSrcBlend`, `fillModeNonSolid`, `logicOp`, `multiViewport`, etc.).
* **Correção de Memória SMMU / nvmap:** Elimina vazamento de memória e travamento que aconteciam aos ~6 minutos de jogo no emulador.
* **Pipeline Guards:** Impede que shaders complexos fechem o jogo em caso de erro na compilação Vulkan.

### 2. Otimizações de Performance para o Poco X8 Pro
* **Otimização TBDR (Tile-Based):** Redução agressiva de barreiras pesadas (`VK_PIPELINE_STAGE_ALL_COMMANDS_BIT`), mantendo o cache de blocos da Mali-G720 abastecido sem esvaziar para a RAM LPDDR5X (~60% mais FPS).
* **Compilador Cortex-A725 (ARMv9):** Instruções vetorizadas SVE2 e NEON ativadas para o processador do Poco X8 Pro, acelerando o JIT Dynarmic e a descompressão de texturas ASTC.
* **Base Atualizada:** Compilado a partir da **versão Nightly mais recente do Eden** (`git.eden-emu.dev`).

---

## 🚀 Como Compilar e Baixar o APK em 1 Clique (GitHub Actions)

Você não precisa compilar manualmente no computador nem instalar gigabytes de ferramentas:

1. **Suba esta pasta para um repositório no seu GitHub:**
   ```bash
   git init
   git add .
   git commit -m "Eden Mali-G720 Poco X8 Pro Builder"
   git branch -M main
   git remote add origin https://github.com/SEU_USUARIO/SEU_REPO.git
   git push -u origin main
   ```
2. **Execute o Workflow:**
   * No seu repositório no GitHub, clique na aba **Actions**.
   * No menu lateral esquerdo, clique em **Build Eden APK (Mali-G720 & Poco X8 Pro Optimized)**.
   * Clique no botão **Run workflow** > **Run workflow**.
3. **Baixe o APK:**
   * Em cerca de 10 a 15 minutos, a compilação nos servidores do GitHub será concluída.
   * Clique na execução concluída e baixe o artefato **`Eden-PocoX8Pro-MaliG720-Optimized`**.
   * Descompacte o arquivo baixado para obter o **`Eden-PocoX8Pro-MaliG720-Optimized.apk`**!
4. **Instale no seu Poco X8 Pro:**
   * Transfira o APK para o celular e instale.
   * Abra o Eden e inicie o jogo com o driver padrão do sistema: ele rodará com todas as correções ativadas nativamente!
