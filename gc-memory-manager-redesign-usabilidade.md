# Proposta de Redesign de Usabilidade — GC Memory Manager

## Objetivo

Redesenhar o fluxo de uso do programa para que ele deixe de funcionar como uma tela cheia de atalhos e passe a se comportar como um gerenciador de saves simples, previsível e seguro.

A ideia principal é:

1. O usuário abre o software.
2. O software mostra os dispositivos detectados.
3. O usuário escolhe o que deseja fazer.
4. Só então aparecem as ações específicas.
5. Ações perigosas ficam em áreas avançadas e sempre exigem confirmação.

---

# Princípios de usabilidade

## Navegação consistente

Os controles devem seguir sempre a mesma lógica:

- **D-pad / Analógico** → navegar
- **A** → selecionar / confirmar
- **B** → voltar / cancelar
- **X** → abrir ações/contexto
- **Y** → seleção múltipla
- **L/R** → trocar abas ou dispositivo
- **Start** → menu rápido / ajuda

Evitar o modelo atual onde cada botão físico executa diretamente uma função diferente.

Exemplo do que deve ser evitado:

- A = OK
- B = Cancel
- X = Backup
- Y = Delete
- L = Raw Backup
- R = Raw Restore
- Z = Format

Esse modelo exige que o usuário memorize comandos e aumenta o risco de executar uma ação errada.

---

# Fluxo principal

## 1. Inicialização do software

Ao abrir o programa, a primeira tela deve mostrar:

- nome do programa;
- dispositivos detectados;
- quantidade de saves ou espaço disponível;
- opções principais.

### Exemplo

```text
GC Memory Manager

Dispositivos detectados:
- Memory Card A   (27 saves)
- SD Card         (31 GB livres)

O que deseja fazer?

> Gerenciar saves
  Fazer backup
  Restaurar backup
  Configurações

A Selecionar   B Sair
```

### Objetivo

O usuário deve entender imediatamente:

- se o Memory Card foi reconhecido;
- se o SD/USB está disponível;
- quais são as principais tarefas possíveis;
- que não precisa decorar atalhos.

---

# 2. Gerenciar saves

Ao selecionar:

```text
> Gerenciar saves
```

o programa deve perguntar primeiro qual dispositivo será acessado.

## Escolha do dispositivo

```text
Gerenciar saves

Escolha o dispositivo:

> Memory Card A
  Memory Card B
  SD Card

A Continuar   B Voltar
```

O usuário escolhe onde estão os saves que deseja visualizar.

---

# 3. Lista de saves

Depois de escolher o dispositivo, o conteúdo deve ser mostrado.

```text
Memory Card A    27/59 blocos usados

> Zelda Wind Waker         12 blocos
  Mario Kart Double Dash    3 blocos
  F-Zero GX                 8 blocos
  Resident Evil 4           4 blocos

A Abrir   X Opções   Y Seleção múltipla   B Voltar
```

## Ações disponíveis nessa tela

- **A** → abrir detalhes do save;
- **X** → abrir menu de ações;
- **Y** → marcar vários saves;
- **B** → voltar.

---

# 4. Detalhes e ações de um save

Ao abrir um save:

```text
Zelda Wind Waker

Jogo: The Legend of Zelda: The Wind Waker
Tamanho: 12 blocos
Data: 14/05/2025
Slot: Memory Card A

> Copiar
  Mover
  Fazer backup
  Excluir
  Detalhes

A Confirmar   B Voltar
```

As ações só aparecem depois que o usuário já selecionou o save.

---

# 5. Fluxo: copiar um save para o SD

## Passo 1

Abrir o software.

## Passo 2

Selecionar:

```text
> Gerenciar saves
```

## Passo 3

Selecionar:

```text
> Memory Card A
```

## Passo 4

Selecionar o save:

```text
> Zelda Wind Waker
```

## Passo 5

Escolher:

```text
> Copiar
```

## Passo 6 — Escolher destino

```text
Copiar para:

> SD Card
  Memory Card B

A Confirmar   B Cancelar
```

## Passo 7 — Confirmar operação

```text
Copiar save "Zelda Wind Waker"

Origem: Memory Card A
Destino: SD Card
Tamanho: 12 blocos

> Iniciar cópia
```

## Passo 8 — Progresso

```text
Copiando...

Zelda Wind Waker
███████████████░░░  82%
```

## Passo 9 — Resultado

```text
Cópia concluída com sucesso.

A OK
```

---

# 6. Fluxo: backup completo do Memory Card

Na tela inicial:

```text
> Fazer backup
```

## Escolher origem

```text
Fazer backup

Escolha a origem:

> Memory Card A
  Memory Card B

A Continuar   B Voltar
```

## Escolher destino

```text
Salvar backup em:

> SD Card
  USB Device

Nome do backup:
MCARD_A_2025-05-14

A Continuar
```

## Resumo

```text
Resumo do backup

Origem: Memory Card A
Destino: SD Card
Conteúdo: 27 saves
Espaço estimado: 12.4 MB

> Iniciar backup
```

## Progresso

```text
Criando backup...

Arquivo 12 de 27
███████████░░░░░░  61%
```

## Conclusão

```text
Backup concluído com sucesso.

Nome: MCARD_A_2025-05-14
Local: SD Card

A OK
```

---

# 7. Fluxo: restaurar um backup

Na tela inicial:

```text
> Restaurar backup
```

## Escolher onde está o backup

```text
Escolha onde está o backup:

> SD Card
  USB Device
```

## Selecionar backup

```text
Backups encontrados:

> MCARD_A_2025-05-14
  MCARD_A_2025-04-20
  ZELDA_ONLY_BACKUP
```

## Selecionar destino

```text
Restaurar para:

> Memory Card A
  Memory Card B
```

## Aviso

```text
Atenção:
A restauração pode sobrescrever saves existentes.

Backup: MCARD_A_2025-05-14
Destino: Memory Card A

> Continuar
  Cancelar
```

Depois disso:

1. executar restauração;
2. mostrar progresso;
3. mostrar resultado;
4. informar erros, caso existam.

---

# 8. Fluxo: excluir um save

Caminho:

```text
Gerenciar saves
→ Escolher dispositivo
→ Selecionar save
→ Excluir
```

## Confirmação

```text
Excluir save

Jogo: Resident Evil 4
Tamanho: 4 blocos

Essa ação não pode ser desfeita.

> Excluir
  Cancelar
```

Toda ação destrutiva deve exigir confirmação explícita.

---

# 9. Fluxo: formatar Memory Card

A opção de formatação não deve ficar na tela principal.

Ela deve ficar em:

```text
Configurações
→ Opções avançadas
→ Formatar Memory Card
```

## Primeiro aviso

```text
Formatar Memory Card

Todos os dados serão apagados.

Dispositivo:
Memory Card A

> Continuar
  Cancelar
```

## Confirmação final

```text
Confirmação final

Todos os saves serão perdidos.

> FORMATAR
  Cancelar
```

Idealmente, a formatação deve exigir uma segunda confirmação para evitar acidentes.

---

# 10. Dispositivos como estado, não como ação

Em vez de ter uma opção genérica chamada:

```text
Select Device
```

a interface deve sempre mostrar quais são a origem e o destino atuais.

Exemplo:

```text
SOURCE                         DESTINATION
Memory Card A   27 saves  →    SD Card   31 GB livres
```

Os botões **L/R** podem ser usados para alternar dispositivos.

Exemplo:

```text
L/R Trocar dispositivo
```

Possibilidades:

```text
Memory Card A  →  SD
Memory Card B  →  SD
SD             →  Memory Card A
```

Isso reduz telas desnecessárias e deixa o estado atual sempre visível.

---

# 11. Operações perigosas

Operações de alto risco devem ficar fora do fluxo comum.

## Configurações sugeridas

```text
Configurações
 ├─ Dispositivo padrão
 ├─ Pasta de backups
 ├─ Preferências
 ├─ Informações
 └─ Opções avançadas
      ├─ Formatar Memory Card
      └─ Boot Loader
```

Assim, ações como `Format` e `Boot Loader` não competem visualmente com tarefas comuns como copiar ou fazer backup.

---

# 12. Estrutura completa de navegação

```text
HOME
 │
 ├── Gerenciar saves
 │    ├── Escolher dispositivo
 │    ├── Lista de saves
 │    │    └── Save
 │    │         ├── Copiar
 │    │         ├── Mover
 │    │         ├── Backup
 │    │         ├── Excluir
 │    │         └── Detalhes
 │    │
 │    └── Seleção múltipla
 │
 ├── Fazer backup
 │    ├── Escolher origem
 │    ├── Escolher destino
 │    ├── Confirmar
 │    └── Executar
 │
 ├── Restaurar backup
 │    ├── Escolher origem do backup
 │    ├── Escolher arquivo
 │    ├── Escolher destino
 │    ├── Confirmar
 │    └── Executar
 │
 └── Configurações
      ├── Preferências
      ├── Dispositivos
      └── Avançado
           ├── Formatar
           └── Boot Loader
```

---

# 13. Fluxo resumido do usuário

De forma simples:

1. Abrir o software.
2. Ver quais dispositivos estão conectados.
3. Escolher uma tarefa:
   - Gerenciar saves;
   - Fazer backup;
   - Restaurar backup;
   - Configurações.
4. Se entrar em **Gerenciar saves**:
   - escolher dispositivo;
   - ver lista de saves;
   - selecionar um save;
   - escolher uma ação.
5. Se entrar em **Backup/Restaurar**:
   - escolher origem;
   - escolher destino;
   - revisar a operação;
   - confirmar;
   - acompanhar o progresso;
   - receber mensagem de sucesso ou erro.

---

# 14. Mudança conceitual principal

## Interface antiga

O programa funciona como um painel de comandos:

```text
Move
OK
Cancel
Backup
Restore
Delete
Raw Backup
Raw Restore
Format MC
Select Device
Boot Loader
```

O usuário precisa entender previamente o que cada comando significa e qual botão está associado a cada função.

## Interface proposta

O programa passa a funcionar como uma sequência de decisões:

```text
O que você quer fazer?
        ↓
Onde estão os dados?
        ↓
Qual save?
        ↓
Qual ação?
        ↓
Qual destino?
        ↓
Confirmar
        ↓
Executar
        ↓
Resultado
```

Essa abordagem reduz a carga cognitiva, diminui o risco de erros e facilita o uso por quem nunca utilizou o software antes.

---

# 15. Próximos passos do redesign

Uma possível sequência para implementar o redesign:

1. Criar a nova tela inicial.
2. Criar componente visual reutilizável de menu.
3. Criar tela de seleção de dispositivo.
4. Criar lista de saves.
5. Criar tela de detalhes do save.
6. Criar menu contextual de ações.
7. Criar componente reutilizável de confirmação.
8. Criar componente de progresso.
9. Criar telas de backup e restauração.
10. Mover Format/Boot Loader para opções avançadas.
11. Padronizar os controles A/B/X/Y/L/R em todo o software.
12. Tratar mensagens de erro, ausência de dispositivo e falha de leitura.

---

# Objetivo final

Transformar o GC Memory Manager de uma tela orientada a atalhos em um gerenciador de saves orientado a tarefas.

O usuário não deve precisar saber previamente como o programa funciona.

A própria interface deve conduzi-lo:

```text
abrir
→ escolher tarefa
→ escolher conteúdo
→ escolher ação
→ confirmar
→ executar
→ concluir
```

Esse deve ser o princípio central de toda a nova interface.
