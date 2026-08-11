# Regras do Projeto para IA

Este arquivo é a fonte oficial de regras para análise, implementação, documentação e tomada de decisão no GCMM-EX.

## Contexto e objetivos

- GCMM-EX é um fork comunitário do GCMM, escrito principalmente em C, para gerenciar saves e imagens RAW de cartões de memória do Nintendo GameCube.
- Preservar compatibilidade com hardware real de Nintendo GameCube e Nintendo Wii é requisito obrigatório.
- Priorizar manutenção, segurança dos dados, compatibilidade com toolchains atuais e baixo uso de memória.
- Preservar funcionalidades existentes, créditos históricos e comportamento compatível com o projeto original, salvo quando a mudança for explicitamente solicitada e documentada.

## Stack e build

- Usar devkitPPC/devkitPro, libogc2, libdvm, PowerPC FreeType (`ppc-freetype`) e zlib.
- Manter o código C compatível com `gnu17`. Não migrar silenciosamente para C23, C++ ou outra linguagem.
- Manter `Makefile.gc` e `Makefile.wii` alinhados quando uma mudança compartilhada afetar compilação, bibliotecas, flags ou assets.
- Manter apenas um tema visual. Ele não precisa ser identificado como “dark” em código, comandos, assets ou documentação.
- Preservar as duas variantes oficiais e seus diretórios intermediários separados:
  - GameCube: `make gc` -> `releases/gcmm_GC.dol`.
  - Wii: `make wii` -> `releases/gcmm_WII.dol`.
- Configurar `DEVKITPRO`, `DEVKITPPC`, `PORTLIBS` e o `PATH` no `.env` local
  antes de compilar. A instalação padrão do devkitPro usa `/opt/devkitpro`.
- Para toolchain instalada no host, compilar após `source .env`:
  - GameCube: `make gc`.
  - Wii: `make wii`.
  - Ambas: `make`.
- Um wrapper Docker local pode ser usado como alternativa de desenvolvimento,
  mas documentação pública não deve depender de repositórios, caminhos ou
  imagens privadas. Documentar sempre caminho público de instalação manual.
- Não versionar artefatos gerados em `build_GC*`, `build_WII*` ou `releases/`.
- A biblioteca de filesystem continua vinculada como `-lfat`, mas deve ser fornecida por `libogc2-libdvm`. Não substituir por `libogc2-libfat`, pois isso remove a configuração atual de detecção de partições e exFAT.

## Arquitetura e plataformas

- Manter código compartilhado em `source/` e código exclusivo do GameCube em `source/aram/`.
- Manter assets compartilhados em `data/`, assets do GameCube em `data-gc/` e assets do Wii em `data-wii/`.
- Isolar comportamento específico por plataforma com os guards existentes: `HW_DOL` para GameCube e `HW_RVL` para Wii.
- Não introduzir variantes, flags ou condicionais de tema. Qualquer mudança visual deve ser verificada nas duas plataformas.
- Não remover suporte atual:
  - Wii: SD frontal, USB e SD Gecko nos slots A/B.
  - GameCube: SD2SP2, SD Gecko nos slots A/B e GC Loader.
  - Filesystems: FAT12, FAT16, FAT32 e exFAT.
- Preservar a montagem por `libdvm`, a detecção do filesystem por partição, a escolha do primeiro volume suportado e a desmontagem de todos os volumes montados pelo dispositivo.

## Regras de código
- Escrever código claro, organizado e de fácil manutenção, respeitando a arquitetura e o estilo legado do arquivo alterado.
- Preferir mudanças pequenas e localizadas. Não introduzir dependências, abstrações ou reescritas amplas sem necessidade comprovada.
- Tratar limites de buffer, tamanhos de arquivo, alinhamento e retorno de APIs explicitamente. Hardware alvo possui memória limitada; o GameCube dispõe de 24 MB de MEM1 e o projeto já mantém um buffer de arquivos de 2 MB.
- Preservar `ATTRIBUTE_ALIGN(32)` e demais requisitos de alinhamento em buffers usados por DMA, CARD, vídeo ou ARAM.
- Não aumentar caches, buffers globais ou uso de memória sem justificar o impacto nas duas plataformas.
- Validar índices, comprimentos, offsets, leituras e escritas antes de acessar buffers ou arquivos. Fechar arquivos e desmontar dispositivos em todos os caminhos relevantes, inclusive erros.
- Não alterar formatos GCI, GCS, SAV, RAW, GCP ou MCI sem verificar compatibilidade binária e comportamento em hardware real.
- Manter textos de interface, documentação pública e comentários novos em inglês dos EUA, seguindo o padrão atual do projeto. Estas regras internas permanecem em português.

## Segurança de dados

- Tratar backup, restauração, exclusão e formatação como operações sensíveis; restauração RAW e formatação são destrutivas.
- Nunca reduzir confirmações, validações, verificações de escrita ou avisos existentes em fluxos destrutivos.
- Manter `FLASHIDCHECK` habilitado. Desativá-lo pode corromper cartões oficiais.
- Exigir validação de tipo, tamanho, cabeçalho, capacidade e destino antes de restaurar saves ou imagens completas.
- Preservar os patches de serial e checksum usados por saves protegidos, incluindo F-Zero GX e Phantasy Star Online.
- Não orientar remoção de cartão de memória, SD ou USB durante leitura ou escrita.

## Layout e interação

- Seguir os padrões visuais existentes antes de criar novas abordagens.
- Reutilizar funções, componentes gráficos, fontes, backgrounds e convenções de posicionamento atuais.
- Manter consistência de tipografia, cores, espaçamento, contraste, estados e hierarquia nas duas plataformas.
- Preservar acesso equivalente pelas entradas suportadas: controle de GameCube e Wii Remote quando aplicável.
- Não ocultar informações críticas, prompts de confirmação, erros, dispositivo selecionado ou progresso de operações.
- Considerar overscan e legibilidade em telas de definição padrão; não assumir layout web, mouse, teclado, alta resolução ou touch.

## Documentação e versão

- Atualizar `README.md` quando comandos, dependências, plataformas, dispositivos, filesystems, outputs ou estrutura do repositório mudarem.
- Registrar mudanças relevantes em `changelog.md`, na seção `Unreleased`.
- Manter `source/main.c` (`appversion`) e `hbc/meta.xml` sincronizados somente em uma alteração explícita de versão/release.
- Não alterar número de versão ou data de release como efeito colateral de outra tarefa.
- Usar os artefatos em `openspec/` quando a tarefa possuir uma especificação ativa; não contradizer decisões registradas nela sem atualizar a especificação.
- Preservar GPLv3, avisos de licença, autoria, créditos herdados e marcações exigidas em fontes modificadas.

## Validação

- Compilar todas as variantes afetadas com o toolchain configurado em `.env`.
  Para mudanças compartilhadas, validar os alvos GameCube e Wii.
- Para mudanças exclusivas de plataforma, validar ao menos todas as variantes que usam o trecho ou asset alterado.
- Executar `make clean` antes de uma validação final quando houver risco de objetos ou assets obsoletos.
- Não afirmar que uma build ou teste passou sem executar o comando correspondente. Se o toolchain ou hardware não estiver disponível, registrar claramente a limitação.
- Como não há suíte automatizada no repositório, complementar a compilação com revisão dos fluxos afetados e, quando possível, teste em hardware real ou emulador.
- Em mudanças de armazenamento ou cartão de memória, testar também falhas previsíveis: mídia ausente, filesystem sem suporte, arquivo inválido, espaço insuficiente, erro de leitura/escrita e cancelamento do usuário.

## Guard rails

- Não executar comandos diretamente em ambiente de produção ou em hardware do usuário. Fornecer instruções para execução manual quando necessário.
- Não realizar ações destrutivas ou irreversíveis sem confirmação explícita e descrição do impacto.
- Não apagar nem sobrescrever alterações preexistentes do usuário.
- Não introduzir mudanças apenas por preferência pessoal.
- Não ignorar impactos em segurança, desempenho, usabilidade, manutenção, compatibilidade ou consistência visual.
