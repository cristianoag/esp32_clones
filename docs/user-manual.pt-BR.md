# ESP32 Clone Series - Manual do usuário

Este manual aborda o uso diário do computador ESP32-S3 Clone Series com
hardware compartilhado, a preparação do cartão microSD e o carregamento
de programas nos oito firmwares atuais. Para uma lista resumida das
conexões, consulte o [Guia de início rápido](quick-start.pt-BR.md).

O computador executa **um firmware instalado por vez**. Neste guia,
*núcleo* significa o sistema emulado dentro desse firmware. Mudar o perfil
de máquina dentro de um firmware é diferente de instalar outro firmware.

Todos os firmwares estão em desenvolvimento ativo. O CP400 e o MSX
convencional são os mais maduros; os outros núcleos têm compatibilidade
mais limitada, e o MSX TurboR é experimental. O suporte a um formato de
arquivo não garante que todos os jogos funcionem ou rodem na velocidade
original.

## Sumário

- [Conexões e alimentação](#conexões-e-alimentação)
- [O menu F12 comum](#o-menu-f12-comum)
- [Reinicialização, salvamento e desligamento](#reinicialização-salvamento-e-desligamento)
- [Joysticks e mouse](#joysticks-e-mouse)
- [Preparação do cartão microSD](#preparação-do-cartão-microsd)
- [Carregamento de jogos por firmware](#carregamento-de-jogos-por-firmware)
  - [CP400 / CoCo 2](#cp400--coco-2)
  - [MSX / MSX2 / MSX2+](#msx--msx2--msx2)
  - [MSX TurboR / FS-A1ST](#msx-turbor--fs-a1st-experimental)
  - [TK90X / TK95](#tk90x--tk95)
  - [Apple II+ / Apple IIe original](#apple-ii--apple-iie-original)
  - [Macintosh Plus](#macintosh-plus)
  - [Sega Master System](#sega-master-system)
  - [Commodore 64](#commodore-64)
- [Troca e atualização de firmware](#troca-e-atualização-de-firmware)
- [Solução de problemas](#solução-de-problemas)
- [Referências específicas de cada firmware](#referências-específicas-de-cada-firmware)

## Conexões e alimentação

### Localização dos conectores

As orientações abaixo pressupõem que você está **de frente para o lado
com os três conectores USB-A**, com o computador na posição normal de uso.

```text
[Liga/desliga] [Reset]     [Joystick 1] [Joystick 2] [Teclado]
 botão da      botão da      USB da      USB do      USB da
 esquerda      direita      esquerda      meio       direita
```

O **teclado sempre deve ser conectado ao USB-A mais à direita**,
independentemente do firmware instalado. Conecte-o diretamente, sem hub.
Os outros dois conectores são portas de joystick de baixa velocidade;
suas funções em cada firmware estão descritas em
[Joysticks e mouse](#joysticks-e-mouse).

### Conexão de um monitor VGA

1. Com o computador desligado, conecte um cabo VGA ao conector VGA de
   15 pinos e ao monitor.
2. Ligue o monitor e selecione **VGA**, **PC** ou a entrada equivalente.
3. Selecione o modo **4:3** ou **Aspect** do monitor para evitar uma imagem
   esticada. Use o ajuste automático se a imagem estiver deslocada.

A placa gera VGA analógico, não HDMI. Uma tela que só tenha HDMI precisa
de um **conversor ativo com entrada VGA e saída HDMI** compatível; um cabo
passivo ou um conversor HDMI para VGA, no sentido oposto, não serve como
substituto. A conexão VGA direta é o ponto de partida mais simples.

A resolução da imagem varia conforme o firmware. Selecionar *emulação*
PAL/NTSC ou 50/60 Hz não altera necessariamente o sinal VGA enviado ao
monitor. O TurboR também oferece **VGA fit** no F12 caso as bordas da
imagem fiquem cortadas.

### Áudio, cartão e alimentação

- Conecte a **saída de áudio mono de 3,5 mm** a caixas de som amplificadas
  ou a uma entrada de áudio adequada. O VGA não transmite som. Comece com
  um volume moderado nas caixas.
- Insira o **cartão microSD FAT32** preparado antes de iniciar. As ROMs e
  as mídias montadas podem ser lidas durante todo o uso: mantenha o cartão
  inserido.
- Conecte o **cabo de alimentação de 5 V fornecido** ao **conector de
  alimentação cilíndrico de 2,1 mm, com positivo no centro**. Use o
  equipamento de alimentação fornecido, não um adaptador de 9 V/12 V.
- Acione o **botão da esquerda** para ligar ou desligar o computador.
- O **botão da direita reinicia** o computador; ele não seleciona o firmware.

A conexão UART/de programação do módulo ESP32 serve para manutenção e
diagnóstico, não para conectar o teclado durante o uso normal. O uso
rotineiro e a troca de firmware não exigem conexão com um PC.

## O menu F12 comum

**F12 abre o menu do emulador em todos os firmwares.** Ele pausa a emulação
para que você possa gerenciar o computador sem enviar ao jogo as teclas
usadas no menu. Ele é separado do menu ou controle de pausa do próprio
jogo. Alguns teclados de PC exigem **Fn + F12**.

A interface segue convenções comuns, mas **as opções de menu não são
idênticas em todos os firmwares**. Leia as dicas na parte inferior da tela.

| Tecla | Ação usual |
| --- | --- |
| Seta para cima / para baixo | Seleciona uma linha. |
| Enter | Aplica/seleciona ou abre o navegador de arquivos selecionado. |
| Seta para a esquerda / para a direita | Altera uma configuração onde indicado. |
| Esc | Volta uma página; no menu principal, retoma a máquina em execução. |
| F12 | Abre o menu; a maioria dos firmwares também retoma a emulação a partir dos submenus comuns. O CP400 usa Esc / **Exit menu** para sair. |
| Page Up / Page Down | Troca de página no navegador de arquivos, quando disponível. |
| Delete | Ejeta ou remove a associação da mídia selecionada, quando disponível. |
| Y / N | Confirma/cancela ações quando solicitado; Esc/F12 cancela muitas caixas de confirmação. |

As funções comuns incluem seleção de mídia, configuração e **Firmware
update**. Os controles de reinicialização, áudio e entrada dependem do
núcleo. A maioria dos firmwares oferece um breve intervalo para
pressionar F12 antes da inicialização automática; pressioná-lo nesse
momento permite alterar as configurações antes de o programa iniciar.

### Selecionar, aplicar e salvar são ações diferentes

- **Selecionar mídia** escolhe um cartucho, disco, fita ou programa.
  A aplicação imediata depende do núcleo e do tipo de mídia.
- **Inicialização a frio/reinicialização** aplica alterações pendentes
  de ROM, cartucho ou máquina quando necessário. Apenas fechar o F12
  não aplica essas alterações.
- **Save boot configuration** guarda seleções e preferências para a
  próxima inicialização. Isso **não salva o jogo nem cria um snapshot**
  do estado da CPU em execução.
- **Reboot without saving** usa as configurações selecionadas apenas
  naquela sessão. Isso não desfaz gravações já feitas em uma imagem de
  disco gravável.

A maioria dos núcleos mantém configurações e calibração em espaços
internos separados; o CP400 mantém sua configuração no SD. Não espere
que a calibração de controles ou as preferências de inicialização de um
núcleo configurem outro.

## Reinicialização, salvamento e desligamento

| Ação | Efeito |
| --- | --- |
| Botão físico de reinicialização, à direita | Reinicia o ESP32 e o firmware instalado; o estado de execução não salvo é perdido. |
| **Warm reset** no F12, quando disponível | Reinicia a máquina emulada mantendo a RAM; os detalhes variam por núcleo. Não é um recurso de salvamento. |
| **Cold boot** / reinicialização no F12 | Reinicia a máquina emulada e aplica a configuração de inicialização selecionada; considere que o progresso atual será perdido. |
| **Firmware update** no F12 | Instala um aplicativo e reinicia nesse firmware. |

O CP400 tem **Reboot CP400**; o MSX convencional oferece **Boot BIOS +
slots + media (cold reset)** e opções de reinicialização em Media; o SMS
tem **Reset running cartridge**. Nem todos os núcleos têm uma opção de
reinicialização a quente.

Antes de desligar:

1. Salve usando o comando de salvamento do próprio jogo ou sistema
   operacional, quando o núcleo e a mídia permitirem. Conclua qualquer
   procedimento de desligamento do sistema operacional antes de cortar
   a alimentação.
2. Aguarde o fim da atividade de disco. Não reinicie, troque de firmware,
   remova o cartão nem corte a alimentação durante gravações.
3. No TurboR, entre no F12 para gravar nos arquivos os dados pendentes da
   SRAM do firmware/cartucho e do CMOS.
4. Use o **botão de alimentação da esquerda** para desligar. Remova o
   cartão somente depois.

Mantenha backups das imagens graváveis. Os discos de Apple II são somente
leitura, o suporte a D64 do C64 permite apenas extração, e a SRAM dos
cartuchos de SMS dura apenas a sessão; salvar a configuração de
inicialização no F12 não contorna essas limitações.

## Joysticks e mouse

### Compatibilidade USB

Os dois conectores de joystick usam um host USB implementado por software,
restrito a dispositivos **USB HID de baixa velocidade (low-speed)**.
Low-speed é o modo de sinalização USB, **não uma configuração que faz
os jogos rodarem devagar**.

- Use um controle com fio compatível, conectado diretamente a uma porta
  de joystick.
- Gamepads que funcionam apenas em full-speed, hubs, adaptadores Bluetooth
  e controles que aceitam apenas XInput não são compatíveis com essas portas.
- Um controle anunciado como "USB", "retrô" ou "compatível com PC" não é
  automaticamente compatível. Formatos HID/relatórios complexos também
  podem não funcionar.
- O transporte compartilhado dos gamepads tem um limite de oito bytes por
  relatório de entrada. A calibração não faz um dispositivo USB
  incompatível ser enumerado.

Se um controle não for detectado, experimente um gamepad HID low-speed
conhecidamente compatível em vez de tirar o teclado de sua porta dedicada,
mais à direita.

### Calibração de um controle compatível

1. Conecte o gamepad ao conector mais à esquerda ou ao do meio.
2. Abra **F12 > USB joysticks** ou, no CP400, **Joystick setup**.
3. Ative a porta se o firmware oferecer essa configuração e selecione
   a ação de calibração.
4. Siga exatamente a ordem exibida: solte/centralize todos os controles
   e depois registre cada direção solicitada e os dois botões de disparo
   com Enter. **A ordem das direções do CP400 difere da de vários outros
   núcleos.**
5. Mantenha cada controle solicitado acionado até a próxima instrução.
   Verifique a entrada em tempo real se o firmware oferecer esse recurso
   e depois volte ao jogo.
6. Salve as configurações de inicialização separadamente, se necessário,
   após alterar a ativação de portas ou a escolha de interfaces. A
   calibração pode já ter sido salva automaticamente.

Repita em outro firmware conforme necessário. Escolha a porta ou
interface de joystick esperada pelo jogo, não apenas um conector físico
que esteja livre.

| Firmware | Observações sobre entrada |
| --- | --- |
| CP400 / CoCo 2 | Duas portas de joystick USB calibradas; siga os requisitos de controle do jogo. |
| MSX convencional | Duas portas de joystick calibradas; os jogos costumam oferecer controle por joystick ou teclado. |
| MSX TurboR | A integração experimental atual com a placa **não** encaminha os joysticks USB ao núcleo. Use programas compatíveis com teclado. |
| TK90X / TK95 | Escolha Kempston, Sinclair 1, Sinclair 2 ou Cursor conforme o jogo. Fire A é o botão de disparo emulado. Dois gamepads em Kempston têm suas entradas combinadas; Sinclair 1/2 podem atender jogadores separados. |
| Apple II | A porta 1 é mapeada para os paddles 0/1; a porta 2, para os paddles 2/3, com comportamento de botões compartilhado. A entrada direcional fornece posições mínima/central/máxima, não controle analógico contínuo. |
| Macintosh Plus | **O conector do meio / porta de joystick 2 é a porta do mouse**; a porta de joystick 1 fica desativada. |
| Sega Master System | As portas físicas 1/2 controlam os jogadores 1/2, com dois botões de jogo cada. Também há controles pelo teclado. |
| Commodore 64 | As portas físicas são mapeadas para as portas 1/2 do C64. Muitos jogos precisam da porta 2; use **Swap joystick ports** se jogar pela porta física 1. Somente Fire A tem uma linha de disparo no C64. |

### Mouse do Macintosh

Use um **mouse HID low-speed com protocolo boot**, conectado diretamente
ao **conector USB-A do meio**. Mouses que funcionam apenas em full-speed,
hubs, receptores Bluetooth e dispositivos com protocolo específico do
fabricante não são compatíveis. Somente o movimento e o **botão esquerdo**
são convertidos; a roda e os outros botões são ignorados.
O teclado continua no conector mais à direita.

## Preparação do cartão microSD

### Arquivos e pastas

Use **FAT32**. Faça backup do cartão antes de formatá-lo: a formatação apaga
seu conteúdo. Em um PC, copie os arquivos necessários para o cartão e
ejete-o com segurança antes de inseri-lo no computador desligado.

Um único cartão pode armazenar as pastas de todos os núcleos e os pacotes
de firmware. A organização abaixo é um exemplo, não um conjunto de jogos
incluídos:

```text
Raiz do cartão\
  ESP32_CP400-<version>.FLH
  ESP32_MSX-<version>.FLH
  ...outros pacotes de firmware de origem confiável...
  YOURGAME.DSK                  discos de CP400 ficam na raiz
  cp400\
    bios\
  msx\
    bios\
    audio\
    roms\
    disks\
    tapes\
  msxturbor\
    bios\fs-a1st\
    roms\
    disks\
  tk\
    bios\
    tapes\
    snapshots\
  apple2\
    bios\
    disks\
  macplus\
    bios\
    disks\
  sms\
    roms\
  c64\
    bios\
    programs\
```

Copie a pasta de sistema preparada no projeto de firmware para a **raiz
do cartão**, não para dentro de uma pasta `sdcard` adicional. Por exemplo,
o cartão deve conter `cp400\bios`, não `sdcard\cp400\bios`.
Com exceção do navegador de mídia do CP400, que só acessa a raiz, os
navegadores disponíveis geralmente permitem percorrer pastas. Mantenha
nomes de arquivos e caminhos curtos e simples.

Use ROMs de BIOS e programas obtidos legalmente. O projeto não fornece
BIOS, sistemas operacionais ou jogos comerciais. Renomear um ZIP, uma
página HTML, uma gravação de áudio ou um formato de disco não aceito não
o converte em uma imagem compatível: extraia os arquivos compactados no
PC e use o arquivo de mídia propriamente dito.

### Lista de ROMs de inicialização

Estes são os caminhos padrão; alguns firmwares permitem selecionar
alternativas. Prepare apenas os perfis que pretende usar.

| Firmware | Arquivos de inicialização necessários no cartão |
| --- | --- |
| CP400 | `cp400\bios\extbas11.rom` e `bas13.rom`, mais o arquivo selecionado, `cp400dsk.rom` ou `disk11.rom`; cada um tem 8.192 bytes. |
| MSX | Um perfil compatível em `msx\bios`: Omega usa `omega\OMEGA.ROM`; Expert/Hotbit usam `expert\MSX.ROM` / `hotbit\MSX.ROM`; os perfis Panasonic usam seu próprio `PANASONIC.ROM`. Consulte a [referência de configuração do MSX](../software/esp32_msx_emulator/README.md#prepare-the-sd-card) para as importações. |
| MSX TurboR | `msxturbor\bios\fs-a1st\fs-a1st_firmware.rom` (4.194.304 bytes) e `fs-a1st_kanjifont.rom` (262.144 bytes). O firmware do FS-A1GT não é compatível. |
| TK90X / TK95 | `tk\bios\tk90.rom` ou `tk95.rom`, conforme o modelo selecionado, com exatamente 16.384 bytes. O modelo padrão é o TK95. |
| Apple II | `apple2\bios\apple2plus.rom` (12.288 bytes) ou `apple2e.rom` (16.384 bytes), conforme o modelo selecionado, mais `disk2.rom` (256 bytes). Use uma ROM do IIe **original**, não do Enhanced IIe. |
| Macintosh Plus | `macplus\bios\macplus.rom`, uma ROM bruta do Plus de 131.072 bytes, sem modificações, e uma imagem inicializável de disco SCSI completo preparada para o uso normal. |
| Sega Master System | Não requer BIOS. Forneça um arquivo de cartucho de jogo compatível. |
| Commodore 64 | `c64\bios\basic.rom` e `kernal.rom` (8.192 bytes cada), mais `chargen.rom` (4.096 bytes). |

Só a correspondência de nome e tamanho não comprova que uma ROM seja o
dump correto. As ferramentas de importação e os requisitos de validação
específicos de cada firmware estão documentados nas
[referências](#referências-específicas-de-cada-firmware).

## Carregamento de jogos por firmware

Os nomes de opções abaixo correspondem às versões atuais. Substitua os
nomes de arquivos de exemplo pelos nomes reais no cartão; os comandos
mostrados são exemplos, não iniciadores universais para todos os jogos.
Pressione Enter após cada comando BASIC.

### CP400 / CoCo 2

**Mídia:** imagens `.DSK` padrão de 35 trilhas, 18 setores e 256 bytes por
setor (161.280 bytes de dados). Coloque-as **na raiz do cartão**. Use nomes
com extensão `.DSK` no seletor do CP400; arquivos DSK de outras máquinas
não são intercambiáveis.

1. Prepare as ROMs do BASIC e do controlador de disco indicadas na lista.
2. Abra **F12 > Boot Disk ROM** e escolha **CP400** ou **CoCo 2**, conforme
   o programa em disco. Após alterar essa opção, escolha **Reboot CP400**
   para iniciar com a ROM desse controlador.
3. Abra **F12 > Disk drives**, selecione **Drive 0**, pressione Enter e
   escolha o DSK do jogo. As unidades **0-3** estão disponíveis para
   programas com vários discos.
4. Pressione Esc para voltar ao menu principal e depois Esc ou **Exit menu**
   para retornar ao BASIC. Associar um disco não o executa automaticamente.
5. Liste o diretório:

   ```basic
   DIR
   ```

6. Siga as instruções do jogo. Um programa BASIC costuma usar:

   ```basic
   LOAD "GAME"
   RUN
   ```

   Um programa em código de máquina costuma usar:

   ```basic
   LOADM "GAME"
   EXEC
   ```

Alguns programas precisam de um carregador, ROM de disco, número de unidade
ou endereço de entrada específico. Use as instruções originais em vez de
experimentar todos os comandos. Para trocar discos, associe outra imagem
à unidade virtual solicitada pelo F12; Delete ejeta a imagem. As associações
de discos são salvas no SD.

**Salvamento:** as gravações em disco podem modificar o DSK original no
microSD. Mantenha um backup e aguarde o fim das gravações antes de reiniciar
ou desligar. Esse fluxo usa imagens de disquete; não presuma suporte a
fitas cassete ou ao CoCo 3.

### MSX / MSX2 / MSX2+

Selecione um perfil de BIOS adequado à geração de MSX e à RAM exigidas pelo
jogo. Alterações de máquina/perfil exigem uma inicialização a frio.

#### Jogos em cartucho

1. Coloque o `.rom` extraído do jogo em uma pasta como `msx\roms`.
2. Abra **F12 > Media**, selecione **Slot 1** e escolha o cartucho.
   Um segundo cartucho pode ser selecionado no Slot 2.
3. Inicialmente, deixe o mapeador em **Auto**. Se o jogo exigir um mapeador
   manual compatível, altere a linha Mapper abaixo desse slot.
4. Escolha **Reboot and save configuration** para tornar essa configuração
   o padrão da próxima inicialização ou **Reboot without saving** para
   usá-la apenas nesta sessão.
5. O cartucho inicia por meio da BIOS selecionada. Apenas fechar o F12
   retomaria a configuração anterior de cartucho.

O hardware de som é uma configuração separada em **Media > Audio**, não
um mapeador de cartucho. Os requisitos de PSG/SCC/FM variam. Os modos
FM-PAC/MSX-MUSIC precisam de uma BIOS de áudio compatível fornecida pelo
usuário, normalmente `msx\audio\FMPAC.ROM`.
Consulte a [referência de áudio do MSX](../software/esp32_msx_emulator/README.md#audio-profiles-and-fm-pac--msx-music).
Renomear a ROM não torna compatíveis mapeadores especiais sem suporte.

#### Jogos em disco

1. Forneça uma BIOS de disco compatível como **`DISK.ROM` na pasta do perfil
   de BIOS selecionado** e faça uma inicialização a frio. A opção testada
   e documentada é a BIOS de disco de 16 KiB do Philips NMS8250, sem
   modificações. Um DSK de jogo não é uma BIOS de disco.
2. Use imagens brutas de **360 KiB ou 720 KiB**: 80 trilhas, nove setores de
   512 bytes, uma ou duas faces. Uma imagem de 360 KiB com 40 trilhas e
   duas faces não é compatível.
3. Abra **F12 > Media > Drive A** e selecione o DSK do jogo. Use Drive B
   para uma segunda imagem se o programa a exigir.
4. Para um jogo inicializável, escolha uma opção de inicialização a
   frio/reinicialização com o disco associado. Para programas BASIC,
   retome a emulação e liste/carregue o programa pelo nome, por exemplo:

   ```basic
   FILES "A:"
   LOAD "A:GAME.BAS"
   RUN
   ```

5. Quando houver uma solicitação de troca de disco, abra F12, substitua
   a imagem da unidade solicitada e retome sem reiniciar.

Os discos permitem **leitura e gravação**, modificando diretamente seus
arquivos DSK. Use imagens diferentes para A/B e mantenha backups. As ROMs
de disco originais da Panasonic não substituem uma BIOS de disco
compatível com a interface suportada.

#### Jogos em fita cassete

1. Abra **F12 > Media > Tape** e associe uma imagem `.cas` padrão do MSX.
2. Retome o BASIC e use o comando de carregamento do programa. Exemplos
   comuns:

   ```basic
   CLOAD
   RUN
   ```

   Ou `LOAD "CAS:"` seguido de `RUN`; fitas binárias normalmente usam
   `BLOAD "CAS:",R`.
3. Use **Rewind tape** antes de começar novamente após a fita ter avançado.

CAS é somente leitura. Gravações WAV não são compatíveis. Selecionar uma
fita não digita o comando de carregamento nem reinicia o computador.

### MSX TurboR / FS-A1ST (experimental)

Este é um **firmware separado**, não outro perfil de BIOS do MSX
convencional. Use os arquivos completos de firmware/Kanji do FS-A1ST
indicados na lista. O desempenho e o comportamento na placa física ainda
estão em validação; não presuma emulação do TurboR na velocidade original.

#### Jogos em cartucho

1. Coloque um `.rom` compatível em `msxturbor\roms` ou use uma ROM
   compatível em outro local do cartão.
2. Abra **F12 > Media**, selecione **Slot 1** ou **Slot 2** e escolha o arquivo.
3. Comece com o mapeamento **Auto**. O TurboR usa sua própria detecção de
   mapeadores, não o banco de dados de mapeadores do firmware MSX convencional.
4. Selecione **Reboot and save configuration** ou **Reboot without saving**.

Os cartuchos compatíveis têm um cabeçalho `AB`, tamanhos múltiplos de
8 KiB e entre 8 KiB e 2 MiB. As opções manuais incluem mapeadores comuns
Konami/ASCII, GameMaster2 e FM-PAC; hardwares especiais sem suporte
continuam incompatíveis.

#### Jogos em disco

1. Coloque imagens DSK brutas de **360/720 KiB** em `msxturbor\disks`.
2. Mantenha **Built-in firmware: Off** para a inicialização normal no
   BASIC/DOS. Essa opção controla a chave do software integrado original
   da Panasonic, **não** qual emulador ESP32 está instalado.
3. Abra **F12 > Media > Drive A** e selecione o disco do jogo; se desejar,
   associe também o Drive B.
4. Faça uma inicialização a frio para executar programas inicializáveis.
   Para programas BASIC em disco, use os comandos normais de carregamento
   do MSX, como descrito acima.
5. Troque os discos pelo F12 durante a execução quando solicitado; não
   faça uma inicialização a frio apenas para atender a uma troca de disco.

O firmware completo do A1ST fornece a interface de disco; não é necessário
um `DISK.ROM` separado do MSX convencional. As gravações vão diretamente
para o DSK, portanto faça backup. Não associe a mesma imagem gravável às
duas unidades.

**Diferenças importantes:** CAS/fita não é compatível; use o MSX convencional
para fitas cassete. A integração atual com a placa não tem entrada de
joystick USB funcional, portanto use programas controlados pelo teclado.
A pontuação segue a matriz de teclado da máquina japonesa. Entre no F12
antes de desligar para gravar os dados pendentes de SRAM/CMOS, incluindo
a SRAM dos cartuchos compatíveis.

### TK90X / TK95

Escolha TK95 ou **TK90X de 48 KiB** e a ROM correspondente em
**F12 > Configuration / ROM**; depois, faça uma inicialização a frio.

#### Jogos em fita TAP

1. Coloque um `.tap` padrão em uma pasta como `tk\tapes`.
2. Abra **F12 > Media** e associe a fita.
3. Volte ao BASIC e digite:

   ```basic
   LOAD ""
   ```

   Isso usa a matriz original de teclado do TK/Spectrum: na ROM normal
   com entrada por palavras-chave, **J** insere `LOAD`. Para as duas
   aspas duplas, use duas vezes a combinação SYMBOL SHIFT + P da ROM;
   **Ctrl ou Alt** do teclado USB funciona como SYMBOL SHIFT. A tecla
   Shift do PC funciona como CAPS SHIFT.
4. Abra **F12 > Media**, selecione **Play tape** e retome a emulação.
5. Aguarde o carregamento. A fita roda na velocidade real **emulada**,
   não com carregamento instantâneo; uma emulação lenta também aumenta
   o tempo de espera real.

Use **Stop/Rewind** para tentar novamente; depois, repita o comando de
carregamento e inicie a reprodução. Apenas associar a fita não a reproduz.
F12 pausa a posição da fita; uma reinicialização a quente ou a frio a
para e rebobina. Só o caminho da fita é lembrado, não sua posição.

#### Jogos em snapshots

1. Inicie com a ROM do TK desejado.
2. Abra **F12 > Media** e selecione a ação de carregamento de snapshot.
3. Escolha um snapshot **48K `.sna` ou `.z80`** compatível e retome a emulação.

O SNA deve estar no formato 48K de 49.179 bytes; são compatíveis Z80 v1 e
as variantes v2/v3 de 48K simples. Um snapshot restaura o estado do
programa, não uma ROM/modelo ou configuração de joystick.

**Limites:** não há suporte a TZX/WAV, snapshots de 128K, gravação de fita,
salvamento de snapshots nem carregamento certificado de fitas turbo ou
protegidas. As imagens TAP são somente leitura e limitadas a 4 MiB.
Selecione a interface de joystick esperada pelo jogo.

### Apple II+ / Apple IIe original

Prepare a ROM do modelo selecionado e a ROM Disk II do slot 6. Programas
para Enhanced IIe/65C02 não são compatíveis apenas por usarem um nome
de arquivo de IIe.

1. Coloque o disco do jogo em `apple2\disks` como uma **imagem `.nib` de
   35 trilhas**, com exatamente **232.960 bytes**.
2. Se necessário, escolha a máquina e as ROMs em **F12 > Configuration / ROM**.
3. Abra **F12 > Media / Disk II drives**, selecione **Drive 1** e escolha
   o disco de inicialização. Associe o Drive 2 se o programa precisar.
4. Escolha **Reboot and save configuration** ou **Reboot without saving**,
   ou volte ao menu principal e selecione **Cold boot**.
5. Aguarde o carregamento pelo Disk II. Em um prompt BASIC já em execução,
   `PR#6` também inicializa pelo disco da unidade 1.

Apenas inserir um disco de inicialização não reinicia a CPU. Se você
iniciou sem mídia e a ROM Disk II estiver aguardando, **F12 > Warm reset**
pode levar ao BASIC; monte um disco e faça uma inicialização a frio
quando estiver pronto.

Quando um jogo com vários discos solicitar uma troca, substitua no F12
a imagem da unidade indicada e retome sem reiniciar.

**Limites:** ambas as unidades são **somente leitura**. Arquivos
DSK/DO/PO/WOZ não são aceitos; renomeá-los para NIB não os converte.
Não há suporte a salvamento em disco, fita cassete ou snapshots.
O carregamento leva o tempo de disco emulado e pode ser mais lento
que no computador original.

Entre as conversões úteis de teclado estão **Alt esquerdo = Open Apple**,
**Alt direito = Solid Apple** e **Enter = Return**. Use as instruções
do próprio jogo para os controles por teclado ou paddle.

### Macintosh Plus

Os programas de Macintosh são executados a partir de um sistema
operacional Macintosh já iniciado, não pela seleção de uma ROM
individual de jogo.

#### Prepare um disco inicializável em um PC

1. Forneça a ROM do Macintosh Plus de 128 KiB, sem modificações.
2. Prepare uma **imagem bruta de disco SCSI completo** em um emulador
   compatível, como o MAME, com seu mapa de partições, driver SCSI do
   Macintosh, sistema compatível inicializável e seus aplicativos/jogos
   já instalados.
3. Copie a imagem para `macplus\disks\hd.img` ou outro caminho IMG
   selecionável. Mantenha um backup.

Uma imagem de disquete ou um volume HFS isolado usado por alguns outros
emuladores **não** é a imagem de disco SCSI completo exigida. Uma imagem
vazia recém-criada não contém um sistema e não inicializa.

#### Inicialize e jogue

1. Conecte um mouse low-speed com protocolo boot compatível ao
   **conector USB-A do meio**. Mantenha o teclado no conector mais à direita.
2. No F12, selecione a ROM e o `.img` de disco completo; escolha acesso
   somente leitura para a próxima inicialização, se desejar. Salve a
   configuração de inicialização, se quiser.
3. Selecione **Cold boot**. O disco aparece no **SCSI ID 6**.
4. Quando a área de trabalho aparecer, abra o disco/pasta e dê um clique
   duplo no aplicativo do jogo instalado.
5. Para adicionar mais programas, conclua os salvamentos e o desligamento
   do sistema, desligue o computador, faça backup do IMG e modifique-o no
   emulador compatível no PC. Devolva a imagem atualizada ao cartão com
   segurança antes de iniciar novamente.

Alterações de ROM, disco e proteção são aplicadas na **inicialização a
frio**, não ao retomar a emulação. O disco de sistema ativo não pode ser
trocado durante a execução. Leitura e gravação são o padrão, e as
alterações persistem no IMG; o modo somente leitura impede salvamentos
e pode não servir para programas que exigem um disco de sistema gravável.

**Limites:** as imagens devem ter tamanho múltiplo de 512 bytes e não
podem ultrapassar 1 GiB. Não há suporte à inserção de disquetes, rede ou
salvamento de estado. As teclas GUI/Windows correspondem a **Command**,
e Alt a **Option**; F12 continua sendo a tecla do menu do emulador.

### Sega Master System

Não é necessária uma BIOS.

1. Coloque um jogo `.sms` ou `.rom` extraído em `sms\roms`.
2. Abra **F12 > Load ROM** e selecione-o. Confirme a substituição se outro
   jogo já estiver em execução.
3. Um carregamento bem-sucedido inicia o jogo imediatamente.
4. Use **Save boot configuration** se quiser esse jogo e essas configurações
   na próxima inicialização; apenas carregar não altera os padrões salvos.

| Controle pelo teclado | Ação |
| --- | --- |
| Setas | Direções do jogador 1. |
| Z ou Espaço / X | Botão 1 / botão 2 do jogador 1. |
| W / S / A / D | Direções do jogador 2. |
| G / H | Botão 1 / botão 2 do jogador 2. |
| Enter ou Pause | Pause do console SMS, com comportamento que depende do jogo. |
| R | Linha de Reset do console; pode reiniciar o jogo. |
| F12 | Menu do emulador e pausa confiável da emulação. |

As portas de joystick 1/2 calibradas controlam os jogadores 1/2.
Comece com o mapeador em **Auto**; há opções manuais Sega/Codemasters/Korean.
Alterações de temporização/região/mapeador exigem **Configuration >
Reboot selected cartridge** ou um novo carregamento de ROM; uma
reinicialização comum mantém a configuração em execução.

**Limites:** são aceitos 8 KiB ou múltiplos de 16 KiB até 1 MiB de dados;
um cabeçalho opcional de copiador de 512 bytes é removido automaticamente.
Não há carregamento de ZIP, modo Game Gear/SG-1000 ou salvamento de estado.
**A SRAM do cartucho dura apenas a sessão**: não é salva no SD e pode ser
perdida ao reiniciar, substituir o jogo, trocar de firmware ou desligar.

### Commodore 64

Prepare as três ROMs de inicialização, inicie o núcleo e **aguarde
`READY.`** antes de carregar programas pelo F12.

#### Arquivos PRG

1. Coloque o `.prg` do jogo em `c64\programs`.
2. Abra **F12 > Media / PRG and D64 > Load PRG into current machine**.
3. Escolha o arquivo. O carregador o copia para o endereço de carga
   indicado no cabeçalho.
4. Retome a emulação e digite `RUN`, ou escolha **Queue BASIC RUN and resume**
   em Media.
5. Para programas em código de máquina sem um iniciador BASIC, use o
   endereço `SYS` documentado pelo programa; `RUN` não é universal.

#### Programas dentro de imagens D64

1. Abra **F12 > Media / PRG and D64 > Load a PRG from D64**.
2. Selecione um D64 de 35 trilhas compatível e depois a entrada PRG
   desejada, marcada como fechada.
3. Retome a emulação e use `RUN`, a ação que coloca RUN na fila ou o
   comando `SYS` documentado, conforme o caso.

**D64 oferece extração de programas somente para leitura, não uma unidade
de disquete emulada.** Não use `LOAD "*",8,1` esperando uma unidade 1541
conectada. Jogos que buscam arquivos adicionais depois de iniciar
geralmente precisam de uma implementação de carregamento múltiplo/unidade
de disco que esse firmware não fornece.

Os tamanhos D64 aceitos são **174.848 bytes** ou **175.531 bytes** com uma
tabela de erros. Não há implementação de gravações em disco/`SAVE`,
fitas, cartuchos CRT ou snapshots. Carregue no prompt do BASIC, não sobre
um jogo em execução; reinicie antes de trocar de programa.

Muitos jogos exigem a **porta 2** de joystick. Use o conector do meio ou
ative **Swap joystick ports** ao usar o conector mais à esquerda.
**Esc = RUN/STOP**, **F11 = RESTORE**, **Alt/GUI = tecla Commodore**.

## Troca e atualização de firmware

### Prepare uma vez e depois troque pelo F12

O atualizador do F12 instala o **pacote de aplicativo FLH** de outro
emulador. Ele não é um iniciador que mantém os oito firmwares instalados
e em execução. Você pode manter todos os **pacotes e arquivos necessários
no SD** juntos em um único cartão.

1. Obtenha um pacote atual de origem confiável, compilado especificamente
   para esta placa. Se compilar a partir do código-fonte, o comando
   `make firmware` de cada firmware gera o pacote no diretório `dist`
   do respectivo projeto.
2. Com o computador desligado, copie para o cartão as ROMs e mídias
   necessárias ao sistema de destino.
3. Copie o pacote para a **raiz do cartão**, mantendo o nome do arquivo:

   | Destino | Padrão do nome do arquivo do pacote |
   | --- | --- |
   | CP400 / CoCo 2 | `ESP32_CP400-<version>.FLH` |
   | MSX | `ESP32_MSX-<version>.FLH` |
   | MSX TurboR | `ESP32_MSXTURBOR-<version>.FLH` |
   | TK90X / TK95 | `ESP32_TK95-<version>.FLH` |
   | Apple II | `ESP32_APPLE2-<version>.FLH` |
   | Macintosh Plus | `ESP32_MACPLUS-<version>.FLH` |
   | Sega Master System | `ESP32_SMS-<version>.FLH` |
   | Commodore 64 | `ESP32_C64-<version>.FLH` |

4. Ejete o cartão do PC com segurança, insira-o e inicie o computador.
5. Abra **F12 > Firmware update**. No TurboR, a opção se chama
   **Firmware update / switch emulator**.
6. Selecione o FLH de destino, verifique o nome e confirme conforme solicitado.
7. **Não desligue, reinicie nem remova o cartão durante a instalação.**
   Aguarde a validação, a gravação e a reinicialização automática.
8. O firmware de destino inicia. Suas ROMs, configurações de entrada e
   mídias de inicialização ainda precisam ser configuradas conforme
   necessário.

Para voltar a outro sistema, repita o processo com o FLH desse sistema
pelo menu F12 do novo firmware. **Não é necessário renomear arquivos com
os atualizadores atuais.** O FLH de origem permanece no cartão.

Instalar um firmware não é uma operação de salvamento de estado: o
programa em execução não sobrevive à reinicialização. Salve antes os
dados de jogo que tiverem suporte a salvamento.

### Firmware antigo e recuperação

- A troca exige o **esquema compartilhado de partições OTA com duas
  partições de 4 MiB**. A primeira instalação usa uma gravação via UART
  para instalar o bootloader, a tabela de partições e o aplicativo.
- Atualizadores antigos de MSX/TK/Apple que rejeitam nomes de arquivos
  de outros emuladores precisam primeiro atualizar para uma versão atual
  de **seu próprio** firmware.
- Instalações antigas de CP400 com o esquema de partições menor exigem
  uma **gravação única via UART** de uma versão atual. O FLH não pode
  alterar o bootloader nem a tabela de partições.
- Use versões atuais dos firmwares de destino para que a troca continue
  disponível após reiniciar. Se um atualizador rejeitar um pacote,
  registre o erro e verifique a integridade do pacote e o esquema de
  partições em vez de contornar a verificação.

As somas de verificação dos pacotes detectam corrupção, **não
autenticidade**. Instale apenas firmware de origem confiável para este
hardware, não um binário ESP32-S3 qualquer. As verificações de OTA
simulada entre firmwares abrangem as oito versões; elas não certificam
todas as transições na placa real nem garantem recuperação após falta
de energia. Consulte a [referência de configuração](#referências-específicas-de-cada-firmware)
pertinente para manutenção via UART, em vez de experimentar com os
controles de programação/inicialização.

## Solução de problemas

| Sintoma | O que verificar |
| --- | --- |
| Sem imagem | Botão de alimentação da esquerda ligado; cabo de 5 V fornecido conectado; monitor na entrada VGA; cabo VGA bem encaixado. Teste VGA direto antes de adicionar um conversor. |
| Imagem esticada ou deslocada | Use 4:3/Aspect e o ajuste automático do monitor. Para bordas cortadas no TurboR, experimente **VGA fit** no F12. |
| Teclado ou F12 não responde | Teclado no conector **mais à direita**, conectado diretamente; experimente Fn + F12 ou um teclado HID básico com fio. |
| Joystick não detectado | Confirme a compatibilidade HID low-speed, a conexão direta e o formato de relatório aceito. A calibração não corrige hardware que funciona apenas em full-speed. |
| Joystick detectado, mas ignorado pelo jogo | Calibre neste firmware, ative a porta quando aplicável e selecione a porta/interface do jogo. Verifique a porta 2 do C64 e Kempston/Sinclair no TK. Atualmente, o TurboR não tem entrada de joystick USB. |
| Mouse do Macintosh não funciona | Use um mouse HID low-speed com protocolo boot no conector **do meio**, não um joystick nesse conector. |
| SD/mídia não encontrado | FAT32; pastas na raiz e nomes de arquivos corretos; imagem realmente compatível e extraída. Os arquivos DSK do CP400 e os arquivos FLH devem ficar na raiz. |
| Erro de BIOS/ROM ausente | Verifique os arquivos brutos, tamanhos e caminhos exigidos pelo perfil selecionado. Um arquivo com o tamanho correto ainda pode ser a ROM errada. |
| Arquivo selecionado, mas nada inicia | Verifique se o núcleo precisa de inicialização a frio, comando BASIC, **Play tape** para TAP ou `RUN`/`SYS` no C64. |
| Apple aguarda sem disco | Insira um NIB inicializável na unidade 1 e faça uma inicialização a frio, ou use Warm reset para chegar ao BASIC. O carregamento pode levar o tempo de disco emulado. |
| Mac indica mídia de inicialização ausente | Forneça um **disco SCSI completo** inicializável, não um disquete, volume HFS isolado ou IMG vazio. |
| Sem som | Use a saída de áudio separada e caixas de som amplificadas; verifique o volume e a configuração de som do núcleo. F12 pausa/silencia o áudio na maioria dos núcleos; FM no MSX pode exigir uma BIOS de áudio. |
| Configurações desaparecem após reiniciar | Use a ação explícita de salvar configuração do firmware. Salvar configurações não salva o estado do jogo nem a SRAM que dura apenas a sessão. |
| Jogo lento ou com falhas | Verifique a máquina/RAM/mapeador exigidos e as mídias aceitas. Experimente renderizar menos quadros se houver essa opção; isso reduz o trabalho de desenho, não os requisitos de compatibilidade. Alguns núcleos estão em estágio inicial. |
| Pacote de firmware rejeitado | Use um FLH atual de origem confiável para esta placa. Verifique o erro informado e os requisitos de atualizadores antigos/partições descritos acima. |

Se o cartão não tiver sido montado inicialmente, use **Retry microSD** ou
**Rescan SD**, quando disponível. Se for substituir fisicamente um cartão
já montado, **desligue primeiro e reinicie depois**, em vez de depender
de uma nova varredura. Faça backup/recupere as imagens em um PC após
um erro de gravação. Diagnósticos avançados estão disponíveis pelo
console UART.

## Referências específicas de cada firmware

Estes guias de desenvolvimento/configuração fornecem ferramentas de
importação, requisitos exatos de ROM, comandos de compilação/gravação
e limites detalhados de implementação:

- [CP400 / CoCo 2](../software/esp32_cp400_emulator/README.md)
- [MSX / fMSX](../software/esp32_msx_emulator/README.md)
- [MSX TurboR / FS-A1ST](../software/esp32_msxturbor_emulator/README.md)
- [TK90X / TK95](../software/esp32_tk95_emulator/README.md)
- [Apple II+ / Apple IIe original](../software/esp32_apple2_emulator/README.md)
- [Macintosh Plus](../software/esp32_macplus_emulator/README.md)
- [Sega Master System](../software/esp32_sms_emulator/README.md)
- [Commodore 64](../software/esp32_c64_emulator/README.md)
