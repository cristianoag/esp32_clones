# ESP32 Clone Series - Guia de início rápido

Boas-vindas! Seu computador ESP32 Clone Series pode recriar vários
computadores clássicos e um console de videogame na mesma placa compacta.
Conecte-o a um monitor VGA, adicione um teclado e aproveite a computação retrô.

Para preparar o cartão SD e carregar jogos em cada firmware, consulte o
[Manual do usuário](user-manual.pt-BR.md).

## 1. Conecte o computador

Faça as conexões com o computador desligado.

1. **Monitor:** conecte um cabo VGA entre o conector VGA de 15 pinos do
   computador e um monitor VGA. Ligue o monitor e selecione a entrada VGA/PC.
   Em um monitor widescreen, escolha **4:3** ou **Aspect** em vez de esticar
   a imagem.
2. **Teclado:** conecte um teclado USB com fio diretamente ao **conector
   USB-A mais à direita**, olhando o computador pelo lado com os três
   conectores USB.
3. **Joysticks, se desejar:** os **outros dois conectores USB-A** são portas
   de joystick, não portas adicionais de teclado.
4. **microSD:** insira o cartão FAT32 preparado com as ROMs do firmware atual
   e seus programas. Mantenha-o inserido enquanto usar o computador.
5. **Alimentação:** conecte o **cabo de alimentação de 5 V** fornecido ao
   conector de alimentação de **2,1 mm, com positivo no centro**. Use o
   equipamento de alimentação fornecido; não o substitua por um adaptador
   de 9 V ou 12 V.

Vista do lado com os botões e os três conectores USB-A:

```text
[Liga/desliga] [Reset]     [Joystick 1] [Joystick 2*] [TECLADO]
 botão da      botão da                              sempre
 esquerda      direita                           mais à direita
```

*O Macintosh Plus usa a porta de joystick 2, o conector USB-A do meio, para
um mouse compatível. A porta de joystick 1 fica desativada nesse firmware.
O firmware experimental do MSX TurboR atualmente aceita apenas controles
pelo teclado.*

O VGA transmite **somente vídeo**. Para ouvir o som, conecte a saída de
áudio de 3,5 mm do computador a caixas de som amplificadas ou a uma entrada
de áudio adequada.

## 2. Ligue o computador

Acione o **botão da esquerda** para ligar o computador. Ele inicia o último
firmware instalado, não uma lista de todos os sistemas disponíveis.

Dependendo do firmware e das configurações salvas, você verá um prompt do
BASIC, um jogo, a tela de inicialização do Macintosh ou o menu de
configuração. Pressione **F12** para abrir o menu. Em um teclado cuja fileira
de teclas de função seja compartilhada com outras funções, pode ser
necessário usar **Fn + F12**.

## 3. F12 é sua central de controle

**Todos os firmwares têm um menu F12.** As opções exatas variam conforme o
sistema, mas é nele que você gerencia mídias, configurações e a troca de
firmware. Abri-lo pausa a máquina emulada; isso não reinicia seu jogo.

| Controle | O que faz |
| --- | --- |
| Seta para cima / para baixo | Percorre o menu. |
| Enter | Seleciona uma opção ou abre o navegador de arquivos. |
| Seta para a esquerda / para a direita | Altera configurações onde indicado. |
| Esc | Volta; no menu principal, retorna à máquina em execução. |
| F12 | Abre o menu; a maioria dos firmwares também permite retomar a emulação a partir dos submenus. No CP400, use Esc ou **Exit menu** para voltar. |

Use **Media**, **Disk drives** ou **Load ROM**, dependendo do firmware,
para escolher o programa. Alguns sistemas precisam de um comando de
carregamento no BASIC ou de uma inicialização a frio após a seleção do
arquivo: o [Manual do usuário](user-manual.pt-BR.md#carregamento-de-jogos-por-firmware)
explica cada caso.

## 4. Use joysticks

Use joysticks/gamepads **USB HID de baixa velocidade (low-speed)**,
conectados diretamente. **Low-speed** descreve o padrão da conexão USB,
não a velocidade de execução do jogo. Ter um plugue USB, por si só, não
garante compatibilidade.

- Gamepads que funcionam apenas em full-speed, hubs USB, receptores
  Bluetooth e controles que aceitam apenas XInput não são compatíveis com
  as portas de joystick.
- Quando disponível, abra **F12 > USB joysticks**; no CP400, essa opção se
  chama **Joystick setup**. Calibre o controle seguindo as instruções na
  tela, incluindo a posição centralizada, as direções e os botões de disparo.
- A calibração e as configurações dos controles podem variar entre
  firmwares. Verifique-as novamente após trocar de sistema.
- Os jogos podem exigir uma porta ou interface de joystick específica.
  Muitos jogos de C64 usam a **porta 2**; jogos de TK podem pedir
  **Kempston** ou **Sinclair**.

Use os controles de teclado do jogo se não tiver um joystick compatível.
A [seção sobre controles do manual](user-manual.pt-BR.md#joysticks-e-mouse)
explica as exceções do mouse do Macintosh e do TurboR.

## 5. Reinicie ou troque de sistema

### Comece de novo

Pressione o **botão da direita** uma vez para reiniciar o computador.
Isso reinicia o firmware instalado e perde o estado não salvo do jogo em
execução. Nunca reinicie durante uma atualização de firmware ou gravação
em disco.

Para reiniciar por software, abra F12 e use a opção de reinicialização do
firmware. Quando disponível, **Warm reset** mantém a RAM; **Cold boot**
inicia a máquina emulada do zero e aplica alterações pendentes de
máquina/ROM.

### Troque para outro firmware

1. Com o computador desligado, prepare no cartão as ROMs/mídias necessárias
   ao sistema de destino e copie seu **pacote de firmware `.FLH`** de origem
   confiável para a raiz do cartão.
2. Inicie o computador e abra **F12 > Firmware update**. No TurboR, essa
   opção se chama **Firmware update / switch emulator**.
3. Selecione o pacote de destino e confirme a instalação.
4. **Mantenha a alimentação conectada e não reinicie nem remova o cartão.**
   Aguarde a conclusão da instalação e a reinicialização do computador no
   novo sistema.

Isso instala outro firmware; não é uma troca instantânea entre núcleos já
em execução. Para voltar, instale o FLH do sistema anterior pelo mesmo
menu F12. O pacote permanece no cartão.

Essas etapas exigem firmware atualizado e o esquema de partições
compartilhado. Para instalações antigas, consulte
[Troca e atualização de firmware](user-manual.pt-BR.md#troca-e-atualização-de-firmware).

## 6. Encerre com segurança

Conclua o salvamento dentro do jogo ou sistema operacional e aguarde o
fim da atividade de disco. No TurboR, abra também F12 para gravar nos
arquivos persistentes os dados pendentes de SRAM/CMOS. Depois, use o
**botão da esquerda** para desligar o computador. Remova o cartão microSD
somente depois de desligar a alimentação.

**Tudo pronto para jogar?** Escolha seu sistema no
[guia de carregamento de jogos](user-manual.pt-BR.md#carregamento-de-jogos-por-firmware).
