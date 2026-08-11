# Regras do Projeto para IA

Este arquivo é fonte oficial para análise, implementação, documentação e
validação do GCMM-EX. Reflete estado atual do repositório: versão `v1.0`,
GameCube e Wii, código em GNU C17.

## Objetivo e escopo

- GCMM-EX gerencia saves e imagens completas de cartões de memória de
  GameCube. Suporta backup, restauração, cópia, movimentação, exclusão e
  formatação.
- Compatibilidade com hardware real de GameCube e Wii é obrigatória. Não
  trocar comportamento seguro por conveniência de emulador.
- Preservar formatos, créditos, licenças, comportamento legado necessário e
  suporte existente, salvo pedido explícito documentado.
- Prioridades: integridade dos dados, compatibilidade, baixo uso de memória,
  manutenção e interface legível em TV SD.

## Plataformas, armazenamento e formatos

- Wii: SD frontal, USB e SD Gecko nos slots A/B.
- GameCube: SD2SP2, SD Gecko nos slots A/B e GC Loader.
- Filesystems: FAT12, FAT16, FAT32 e exFAT via `libdvm`.
- Formatos de save: GCI, GCS e SAV. Imagens completas: RAW, GCP e MCI.
- Manter montagem via `libdvm`: detectar partições, selecionar primeiro volume
  suportado e desmontar todos volumes montados do dispositivo.
- Não alterar layout binário, tamanho, packing ou semântica de GCI/GCS/SAV/
  RAW/GCP/MCI sem validação de compatibilidade binária e teste em hardware.

## Arquitetura

- Código compartilhado fica em `source/`; UI em `source/ui/`; armazenamento e
  cartão em `source/storage/`; código ARAM exclusivo de GameCube em
  `source/aram/`.
- Assets: compartilhados em `data/`, GameCube em `data-gc/`, Wii em
  `data-wii/`.
- Usar guards existentes: `HW_DOL` para GameCube e `HW_RVL` para Wii.
- `main.c` coordena ciclo da aplicação, montagem e fluxos; UI não monta mídia
  nem escreve cartão; módulos de armazenamento não controlam layout da UI.
- `storage/mcard.c` possui `FileBuffer` alinhado de 2 MiB. Preservar
  `ATTRIBUTE_ALIGN(32)` em buffers de CARD, vídeo, DMA ou ARAM.
- Não aumentar buffers globais, caches ou consumo de memória sem justificar
  impacto nas duas plataformas.

## Build e dependências

- Toolchain: devkitPro/devkitPPC, libogc2, libdvm, PowerPC FreeType
  (`ppc-freetype`) e zlib.
- Compilar em GNU C17. Não migrar silenciosamente para C23, C++ ou outra
  linguagem.
- `-lfat` continua linkado, fornecido por `libogc2-libdvm`. Não substituir por
  `libogc2-libfat`, pois removeria suporte a partições e exFAT.
- Configurar `DEVKITPRO`, `DEVKITPPC`, `PORTLIBS` e `PATH` no `.env` local.
  Instalação padrão usa `/opt/devkitpro`.
- Alvos oficiais:
  - `make gc` gera `releases/gcmm_ex_GC.dol` e usa `build_GC/`.
  - `make wii` gera `releases/gcmm_ex_WII.dol` e usa `build_WII/`.
  - `make` gera ambos; `make clean` remove artefatos gerados.
- Alterações compartilhadas de build, flags, bibliotecas ou assets devem manter
  `Makefile.gc` e `Makefile.wii` alinhados.
- Não versionar `build_GC*`, `build_WII*`, `releases/`, `.env` nem dados locais
  de emulador ou cartões de teste.

## Segurança de dados

- Backup, restauração, exclusão, movimentação e formatação são operações
  sensíveis. RAW restore e formatação são destrutivos.
- Nunca remover confirmações, validações, verificação de escrita, mensagens de
  erro ou avisos de fluxos destrutivos.
- Manter `FLASHIDCHECK` habilitado. Desativá-lo pode corromper cartões
  oficiais.
- Antes de restaurar, validar tipo, tamanho, cabeçalho, capacidade, destino e
  identidade do cartão. Preservar tratamento de serial e checksum para saves
  protegidos, incluindo F-Zero GX e Phantasy Star Online.
- Uma movimentação deve copiar, verificar payload de destino e só então apagar
  origem.
- Validar índices, comprimentos, offsets, tamanhos de arquivo, retornos de API
  e componentes de caminho antes de acessar buffers ou mídia. Fechar arquivos
  e desmontar dispositivos também em caminhos de erro.
- Nunca orientar remoção de cartão, SD ou USB durante leitura ou escrita.

## Código, UI e documentação

- Preferir mudanças pequenas, localizadas e compatíveis com estilo do arquivo.
  Não introduzir dependências, abstrações ou reescritas amplas sem necessidade.
- Manter um único tema visual. Não criar flags, variantes ou condicionais de
  tema.
- Reutilizar padrões de UI, fontes, assets e convenções existentes. Preservar
  contraste, estados desabilitados, progresso, dispositivo selecionado, erros
  e confirmações nas duas plataformas.
- Preservar acesso por controle de GameCube e Wii Remote/Classic Controller
  onde aplicável. Considerar overscan e legibilidade em TV SD.
- Textos públicos, textos de UI e novos comentários de código devem ser em
  inglês dos EUA. Estas regras internas ficam em português.
- Atualizar `README.md` quando comandos, dependências, suporte, outputs ou
  estrutura do repositório mudarem. Registrar mudanças relevantes em
  `changelog.md`, seção `Unreleased`.
- Alterar `source/main.c` (`appversion`) e `hbc/meta.xml` juntos somente em
  mudança explícita de versão/release.
- Preservar GPLv3, avisos de licença, autoria e créditos históricos.

## Dolphin e hardware

- `scripts/run-dolphin.sh` usa perfil isolado em `tests/dolphin/user/`; nunca
  apontar testes para perfil normal, cartão, SD ou USB reais.
- Usar apenas cartões virtuais descartáveis em cópia, exclusão, restauração RAW
  e formatação. Dolphin é smoke test de UI e I/O básico, não valida hardware.
- Dolphin não emula SD Gecko/GC2SD no Slot B. Fluxos de armazenamento GC2SD,
  SD Gecko, SD2SP2, GC Loader, Flash ID, remoção física, timing de controle e
  overscan exigem hardware real.
- Para smoke test Wii, usar SD virtual descartável. Para smoke test GameCube,
  tratar cartões virtuais como teste de cartão, não de adaptador SD.

## Validação e guard rails

- Compilar alvos afetados após configurar `.env`; mudanças compartilhadas exigem
  `make gc` e `make wii`. Rodar `make clean` antes de validação final quando
  houver risco de artefatos obsoletos.
- Não afirmar build ou teste sem executar comando correspondente. Informar
  claramente limitações de toolchain, emulador ou hardware.
- Em mudanças de armazenamento/cartão, revisar ao menos: mídia ausente,
  filesystem não suportado, arquivo inválido, espaço insuficiente, falha de
  leitura/escrita e cancelamento.
- Não apagar, sobrescrever ou publicar alterações preexistentes sem autorização
  explícita. Não executar operações em hardware do usuário.
- Não fazer mudanças apenas por preferência. Considerar sempre segurança,
  compatibilidade, desempenho, manutenção e consistência visual.
