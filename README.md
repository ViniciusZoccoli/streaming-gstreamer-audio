# Streaming de áudio RTP/UDP com GStreamer (versão Windows)

Dois programas em C:

- `transmissor.c` — gera um tom de 440 Hz, empacota em RTP (L16) e envia por UDP.
- `receptor.c` — recebe por UDP, passa pelo `rtpjitterbuffer` (latência configurável),
  desempacota e reproduz; mostra estatísticas de perda a cada segundo.

O código também compila no Linux sem alterações.

## Estrutura da entrega

```
transmissor.c              código do transmissor
receptor.c                 código do receptor
compilar.ps1               script de compilação (PowerShell)
pipelines.drawio           diagrama das pipelines (Draw.io)
REGISTRO_EXPERIMENTOS.md   relatório dos experimentos e resultados
CONCLUSAO.md               conclusão sobre o streaming, jitter e perdas
resultados/                CSV (estatísticas) e WAV (áudio reproduzido) de cada teste
```

## 1. Instalação (uma vez)

1. Instale o **MSYS2** pelo site oficial: https://www.msys2.org (pasta padrão `C:\msys64`).
2. Abra o atalho **"MSYS2 UCRT64"** no menu Iniciar e rode:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-gstreamer mingw-w64-ucrt-x86_64-gst-plugins-base mingw-w64-ucrt-x86_64-gst-plugins-good mingw-w64-ucrt-x86_64-gst-plugins-bad
   ```

   - `plugins-good`: UDP, RTP, jitterbuffer e saída de áudio.
   - `plugins-bad`: elemento `netsim`, usado para simular rede ruim.

## 2. Compilação

No VS Code, abra esta pasta e o terminal (Ctrl+'), que é PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\compilar.ps1
```

## 3. Execução

Em **cada** terminal novo, rode primeiro (para o Windows achar as DLLs do GStreamer):

```powershell
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
```

Abra dois terminais lado a lado (botão "Dividir terminal") e inicie **o receptor primeiro**:

```powershell
.\receptor.exe <porta> <latencia_ms> [arquivo.csv] [arquivo.wav]
.\transmissor.exe <host> <porta> <num_buffers> [perda_%] [atraso_max_ms]
```

Exemplo:

```powershell
.\receptor.exe 5000 200 teste_A2.csv
.\transmissor.exe 127.0.0.1 5000 500
```

Quando o transmissor terminar (~11 s), o receptor percebe que não chegam pacotes novos
há 3 s e **encerra sozinho** (Ctrl+C também funciona onde o terminal o repassa). Ele imprime um
**resumo final** e, se foi passado um arquivo, grava as estatísticas de cada segundo em CSV
(abre direto no Excel). Se for passado também um `arquivo.wav`, o áudio **exatamente como foi
reproduzido** (com as falhas causadas por perdas/atrasos) é gravado para ouvir depois.

Para usar dois computadores: no transmissor, troque `127.0.0.1` pelo IP do receptor
(`ipconfig`). O Firewall do Windows pode pedir permissão para o `receptor.exe` na primeira vez.

## 4. Roteiro dos experimentos

Parte A — rede normal:

| Teste | Receptor | Transmissor |
|---|---|---|
| A1 | `.\receptor.exe 5000 20 teste_A1.csv`   | `.\transmissor.exe 127.0.0.1 5000 500` |
| A2 | `.\receptor.exe 5000 200 teste_A2.csv`  | `.\transmissor.exe 127.0.0.1 5000 500` |
| A3 | `.\receptor.exe 5000 1000 teste_A3.csv` | `.\transmissor.exe 127.0.0.1 5000 500` |

Parte B — rede ruim simulada (2% de perda, até 80 ms de atraso):

| Teste | Receptor | Transmissor |
|---|---|---|
| B1 | `.\receptor.exe 5000 20 teste_B1.csv`   | `.\transmissor.exe 127.0.0.1 5000 500 2 80` |
| B2 | `.\receptor.exe 5000 200 teste_B2.csv`  | `.\transmissor.exe 127.0.0.1 5000 500 2 80` |
| B3 | `.\receptor.exe 5000 1000 teste_B3.csv` | `.\transmissor.exe 127.0.0.1 5000 500 2 80` |

Para também gravar o áudio, acrescente o nome do WAV no receptor
(ex.: `.\receptor.exe 5000 20 teste_A1.csv teste_A1.wav`).

Os resultados obtidos nesta entrega estão em `resultados/` e analisados em `REGISTRO_EXPERIMENTOS.md`.

## 5. Pipelines

Diagrama completo, com cores por tipo de elemento e os parâmetros configurados:
`pipelines.drawio` (abrir em https://app.diagrams.net ou na extensão Draw.io do VS Code).

Transmissor:

```
audiotestsrc wave=sine freq=440 is-live=true num-buffers=N
  ! audioconvert ! audioresample
  ! audio/x-raw,format=S16BE,rate=44100,channels=2
  ! rtpL16pay
  ! [netsim drop-probability=P delay-probability=0.5 max-delay=D]   (opcional)
  ! udpsink host=H port=PORTA
```

Receptor:

```
udpsrc port=PORTA caps="application/x-rtp,media=audio,clock-rate=44100,encoding-name=L16,channels=2,payload=96"
  ! rtpjitterbuffer name=jitbuf latency=L
  ! rtpL16depay ! audioconvert
  ! [tee name=t ! queue]                                    (só com gravação)
  ! audioresample ! autoaudiosink sync=false
  [t. ! queue ! audioconvert ! wavenc ! filesink location=ARQUIVO.wav]   (só com gravação)
```

## 6. O que mudou em relação à versão Linux

| Mudança | Por quê |
|---|---|
| Transmissor aceita `[perda_%] [atraso_max_ms]` e insere `netsim` | `tc netem` não existe no Windows; `netsim` simula perda e atraso dentro do próprio GStreamer |
| Receptor encerra sozinho após 3 s sem pacotes novos | No terminal do MSYS2 o Ctrl+C não chega a programas Windows nativos |
| Receptor aceita `[arquivo.wav]` (tee + wavenc) e envia EOS antes de sair | Ouvir de novo cada teste; o wavenc só fecha o arquivo corretamente ao receber EOS |
| Receptor aceita `[arquivo.csv]` | Registrar os experimentos sem depender de `tee` |
| Receptor imprime o tempo (s) em cada linha e um resumo final ao sair | Facilita preencher o registro |
| Ctrl+C: `g_unix_signal_add` no Linux, `signal()` no Windows | No Linux é a forma segura do GLib; no Windows o tratador roda em outra thread e `g_main_loop_quit` é thread-safe |
| Script `compilar.ps1` | No PowerShell, `$(pkg-config ...)` não separa os argumentos como no bash |
