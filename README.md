# Headset Wireless

Headset estéreo sem fio feito em casa. O fone leva um ESP32-C3, o dongle USB leva um ESP32-S3, e os dois se falam por ESP-NOW em 2,4 GHz. No Windows o dongle aparece como um headset de verdade, com alto-falante e microfone, e um programa à parte mostra enlace, bateria, volumes e o rádio.

A revisão B é o contrato do rádio. Os dois firmwares e o programa do Windows seguem os mesmos tamanhos de pacote, o mesmo relógio de 5 ms e os mesmos códigos de configuração. Mudar um lado sem o outro quebra o enlace.

Este repositório também traz o manual de montagem e o manual de engenharia da revisão B.

## O que entra e o que sai

```text
PC  --USB áudio-->  ESP32-S3 dongle  --ESP-NOW 48 kHz-->  ESP32-C3 no fone  --> alto-falantes
PC  <--USB áudio--  ESP32-S3 dongle  <--ESP-NOW 16 kHz--  ESP32-C3 no fone  <-- microfone
PC  <--HID--------  estado, bateria, perdas, canal, volume, mudo
```

O Windows não sabe que existe rádio no meio. Ele só vê um dispositivo de áudio chamado Headset Wireless. O programa de gerência usa o mesmo cabo, por HID, para ler o enlace e gravar canal, potência e MAC.

## Peças

| Função | Peça |
| --- | --- |
| Rádio e USB do dongle | ESP32-S3, flash de 16 MB |
| Rádio e áudio do fone | ESP32-C3 SuperMini, flash de 4 MB |
| Microfone | INMP441, I2S |
| DAC dos alto-falantes | PCM5102A |
| Amplificador de fone | TPA6132 |
| Volume local | potenciômetro Odesso T3 |
| Bateria | duas células LiPo de 800 mAh, com chave |

O ESP32-S3 e o ESP32-C3 só têm rádio de 2,4 GHz. Não existe modo 5 GHz nestes chips. Os canais válidos são 1 a 13. O padrão de fábrica é o canal 6, com potência de 15 dBm.

## Pinos do fone (ESP32-C3)

I2S Philips, 48 kHz, slots de 32 bits.

| Sinal | GPIO | Nota |
| --- | --- | --- |
| BCLK | 2 | é pino de strap; o firmware liga o pull-up |
| WS | 3 | |
| DOUT para o DAC | 5 | |
| DIN do microfone | 4 | |
| ADC da bateria | 0 | canal ADC 0; a leitura entra na curva de porcentagem |
| Mudo | 6 | ativo em nível baixo |
| LED | 10 | |

Não use GPIO 8, 9, 18 nem 19 no C3 para estes sinais. No dongle, o USB nativo é GPIO 19 (D−) e GPIO 20 (D+).

Enquanto o C3 estiver no USB para gravar firmware, a chave da bateria fica desligada. O 5 V do USB e o conversor da bateria não podem alimentar o 3,3 V ao mesmo tempo. Depois da gravação, tire o USB antes de ligar a chave.

## Contrato de rádio, revisão B

O S3 é o mestre do tempo. Os dois lados ficam em modo estação, sem economia de energia de Wi-Fi e sem modo de longo alcance. O callback de recepção só enfileira o pacote. Quem trata o áudio é outra tarefa.

Cada bloco de áudio dura 5 ms.

| Sentido | Conteúdo | Tamanho |
| --- | --- | --- |
| Dongle para o fone | estéreo 48 kHz, IMA ADPCM de 4 bits | 250 bytes |
| Fone para o dongle | mono 16 kHz, PCM de 80 amostras | 166 bytes |
| Configuração | canal, potência, ganhos, perfil | tipo 3 |
| Confirmação | o fone devolve canal e perfil | tipo 4 |

No pacote de descida, o nibble baixo é o canal esquerdo e o nibble alto é o direito. O preditor e o índice escritos no pacote são o estado de antes daquele pacote. Se um pacote se perde, o fone repete o último PCM cerca de 3 dB mais baixo. Uma falha de mais de três pacotes zera o estado.

O microfone sobe de 16 kHz para 48 kHz no dongle, em três passos lineares: `a`, `(2a+b)/3`, `(a+2b)/3`. O mudo não para o relógio: as amostras vão zeradas e o bit 0 dos flags fica ligado.

O dongle só muda de canal ou de transporte depois que o fone confirma. Se o fone troca de canal e fica 1,5 s sem ouvir o dongle, ele volta ao canal anterior e grava isso na memória.

### Perfis

A latência abaixo é a conta do firmware, não uma medida com osciloscópio. Ida = quadro + 1 + buffer × quadro + 11. Ida e volta = 2 × ida + 8.

| Perfil | Quadro | Buffer | Ida | Ida e volta |
| --- | --- | --- | --- | --- |
| Jogo | 5 ms | 1 | 22 ms | 52 ms |
| Equilíbrio | 5 ms | 2 | 27 ms | 62 ms |
| Estável | 10 ms | 2 | 42 ms | 92 ms |
| Personalizado | 5 ou 10 ms | 1, 2 ou 3 | pela mesma conta | pela mesma conta |

O buffer maior não diminui a perda de pacotes. Ele só segura o áudio por mais tempo quando um pacote falta, para o corte ser menos seco.

### Perdas

O número `N / 1000` é a fração de pacotes do microfone que não chegaram ao dongle naquele segundo. `100 / 1000` é 10%. `0 / 1000` é um segundo limpo. O contador olha o buraco na sequência dos pacotes de subida.

Com o sinal já forte, perda na casa de 70 a 130 por mil costuma ser interferência em 2,4 GHz, não falta de potência. O canal 1 coincide com o Wi-Fi de muita casa: um dongle transmitindo nele o tempo todo atrapalha a rede enquanto estiver ligado. O canal 6 também é cheio. Vale experimentar 1, 11 e 13 com o fone perto e ficar no que mostrar menos perda. A potência padrão de 15 dBm é o ponto de partida. O ajuste vai de 2 dBm a 20 dBm, em passos de 0,25 dBm.

Os dois rádios transmitem no mesmo canal, a cada 5 ms, um em cada sentido. Parte da perda que sobra num canal limpo é os dois falando ao mesmo tempo. O protocolo não retransmite o pacote perdido.

Se o canal gravado na memória deixar o aparelho mudo ou em cima do Wi-Fi da casa, os gravadores desta pasta apagam a flash e voltam o canal para 6.

## USB do dongle

Dispositivo composto: áudio UAC 1.0 e HID.

| Campo | Valor |
| --- | --- |
| VID / PID | `303A` / `4453` |
| Nome | Headset Wireless |
| Série | HS-DONGLE |
| Reprodução | 48 kHz, estéreo, 16 bits |
| Captura | 48 kHz, mono, 16 bits |
| HID de estado | report ID 1, 32 bytes |
| HID de configuração | report ID 2, 16 bytes |

O conector USB nativo é o do áudio e do HID. O Windows não cria porta COM nesse cabo. A porta COM só aparece no UART do chip CH343, e é essa porta que serve para gravar o firmware do S3. No C3 SuperMini a gravação usa o próprio USB, que no modo boot vira a porta serial do chip.

O console do S3 fica na UART. O USB Serial/JTAG secundário permanece desligado para o USB nativo ficar livre para o áudio.

## Programa no Windows

Aplicação WPF em .NET 8. Ela procura o HID `303A:4453` e os dispositivos de áudio cujo nome contém Headset Wireless.

| Página | O que faz |
| --- | --- |
| Início | dongle, fone no ar, bateria, sinal, potência, pacotes perdidos, ida e volta, barras ou espectro ao vivo |
| Áudio | os mesmos medidores, volume de alto-falante e microfone, retorno da própria voz, avanço do microfone, mudo e tom |
| Transporte | Jogo, Equilíbrio, Estável, ou quadro e buffer manuais |
| Rádio | card do fone C3, MAC personalizado, canal e potência |
| Diagnóstico | histórico de RSSI e de perdas, teste do clique |

O volume principal é o do endpoint do Windows. Com o headset listado no sistema, o ganho de alto-falante e de microfone no firmware fica em 0 dB para não abaixar o som duas vezes. O avanço do microfone, de 0 a 24 dB, só existe no C3.

O tom de 1 kHz é gerado no dongle, por três segundos. O mudo do HID se soma ao botão do GPIO 6.

Na área de notificação ficam o ícone do programa, uma bateria cujo miolo acompanha a carga, e, se a opção estiver ligada, alto-falante esquerdo, alto-falante direito e microfone. A bateria fica vermelha em 15% ou menos. O ícone do executável é o brasão `icone.png`.

O card **Fone C3** manda ao dongle o MAC `90:DA:72:84:B0:20`, que é o deste fone de referência. Outro C3 precisa do próprio MAC, lido com `esptool chip_id`, no campo **MAC personalizado**. Sem um MAC válido o dongle não endereça o fone e a tela fica em “sem enlace”.

Para compilar e abrir:

```powershell
dotnet build "app\HeadsetManager\HeadsetManager.csproj" -c Release
.\app\HeadsetManager\bin\Release\net8.0-windows\HeadsetManager.exe
```

É preciso o [.NET 8 para Windows](https://dotnet.microsoft.com/download/dotnet/8.0). A mensagem “conecte o dongle no USB nativo” é o estado normal sem o S3 na porta de áudio.

## Firmware

O código comum está em `firmware/components/hs_link`. Cada placa tem o seu projeto ESP-IDF:

- `firmware/dongle_s3` — alvo `esp32s3`, flash 16 MB
- `firmware/headset_c3` — alvo `esp32c3`, flash 4 MB

Foi desenvolvido com ESP-IDF 5.4. O shell do IDF guarda o alvo anterior. Antes de `idf.py set-target`, defina `IDF_TARGET` para a placa certa. Um alvo errado recusa a configuração com a mensagem de que o alvo não combina com o ambiente.

No ambiente do ESP-IDF:

```powershell
cd firmware\dongle_s3
idf.py set-target esp32s3
idf.py build

cd ..\headset_c3
idf.py set-target esp32c3
idf.py build
```

Confira o chip antes de gravar. O S3 deste projeto anunciou, na gravação original, flash de 16 MB. O C3 anunciou flash embutida de 4 MB. Gravar a imagem de uma placa na outra, ou com o tamanho de flash trocado, estraga a memória.

```powershell
cd firmware\dongle_s3
idf.py -p COM4 -b 460800 flash

cd ..\headset_c3
idf.py -p COM5 -b 460800 flash
```

Troque `COM4` e `COM5` pela porta que aparecer em modo boot.

### Gravadores de recuperação

`gravar_dongle.bat` e `gravar_c3.bat` abrem uma janela que apaga a flash e grava de novo o firmware daquela placa. Os dois exigem o Python do ESP-IDF em `H:\espressif\python_env\idf5.4_py3.14_env`. Se o seu IDF estiver em outro lugar, ajuste essa pasta no `.py` ou use o `idf.py flash` acima.

O gravador do dongle aceita S3 ou C3 e escolhe a imagem pelo chip. O gravador do C3 recusa o S3 e não apaga nada se a porta for o dongle.

Modo boot: segure BOOT, toque em RESET e solte BOOT.

- No S3, use o UART do CH343. O USB do áudio não entra em gravação.
- No C3, use o USB da própria placa, com a chave da bateria desligada. Ao terminar, tire o USB antes de ligar a bateria.

Apagar a flash também apaga o MAC do par e o canal. Os dois voltam ao canal 6. O dongle só volta a achar o fone quando recebe o MAC de novo, pelo card do programa ou pelo primeiro pacote que o fone enviar.

## Pastas

```text
firmware/components/hs_link     contrato, ADPCM, ganhos, latência e bateria
firmware/dongle_s3              USB de áudio, ESP-NOW mestre, NVS
firmware/headset_c3             ESP-NOW escravo, I2S, bateria e mudo
firmware/host_test              conferência do ADPCM e da latência em Python
app/HeadsetManager              painel WPF
gravar_dongle.py                recuperação do dongle, ou do chip que estiver em boot
gravar_c3.py                    recuperação só do fone
icone.png                       brasão usado no ícone do programa
Manual de Montagem ….pdf        montagem física
Manual de Engenharia ….pdf      revisão B, o contrato que o código segue
```

As pastas `build` e `managed_components` são produto da compilação. Não precisam ir para o GitHub. O mesmo vale para `__pycache__`.

## Fora deste código

Não há equalizador, áudio espacial, atualização de firmware por dentro do programa, nem instalador de loja. O sidetone é mistura no C3. O teste de enlace olha se o fone responde, se o sinal tem pelo menos duas barras e se a perda ficou abaixo de 50 por mil.

## Licença

Ainda não há arquivo `LICENSE` neste repositório. Para publicar como código aberto, escolha e inclua uma licença antes do primeiro push público. Sem esse arquivo, o padrão é que todos os direitos permanecem com o autor.
