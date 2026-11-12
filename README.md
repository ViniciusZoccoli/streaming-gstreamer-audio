# Streaming de áudio RTP/UDP com GStreamer

Atividade prática de **Sistemas Multimídia (CMP 1103)**: aplicação em C com GStreamer para
transmitir e receber áudio pela rede, com controle de jitter/latência e diagnóstico de perda
de pacotes.

- `transmissor.c` — gera um tom de 440 Hz, empacota em RTP (L16) e envia por UDP.
  Opcionalmente simula uma rede ruim (perda e atraso) com o elemento `netsim`.
- `receptor.c` — recebe por UDP, passa pelo `rtpjitterbuffer` (latência configurável),
  desempacota e reproduz; mostra a cada segundo as estatísticas de pacotes entregues,
  perdidos, atrasados e duplicados.

## Estrutura do repositório

```
transmissor.c              código do transmissor
receptor.c                 código do receptor
compilar.ps1               script de compilação para PowerShell (Windows)
pipelines.drawio           diagrama das pipelines (Draw.io)
REGISTRO_EXPERIMENTOS.md   relatório dos experimentos e resultados
CONCLUSAO.md               conclusão sobre o streaming, jitter e perdas
resultados/                CSV (estatísticas) e WAV (áudio reproduzido) de cada teste
```

---

## 1. Dependências

| Dependência | Para quê |
|---|---|
| Compilador C (`gcc`) | Compilar os programas |
| `pkg-config` | Informar ao `gcc` onde estão os cabeçalhos e bibliotecas do GStreamer |
| GStreamer 1.x (núcleo + ferramentas) | Biblioteca principal, `gst-launch-1.0`, `gst-inspect-1.0` |
| `gst-plugins-base` | `audioconvert`, `audioresample`, `audiotestsrc` |
| `gst-plugins-good` | `udpsrc`, `udpsink`, `rtpL16pay`, `rtpL16depay`, `rtpjitterbuffer`, `wavenc`, saída de áudio |
| `gst-plugins-bad` | `netsim` (simulação de perda e atraso) |

Testado com **GStreamer 1.28** no Windows 11 (MSYS2 UCRT64).

### 1.1 Windows (MSYS2)

1. Instale o **MSYS2**: https://www.msys2.org (mantenha a pasta padrão `C:\msys64`).
2. Abra o atalho **"MSYS2 UCRT64"** no menu Iniciar e instale os pacotes:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-gstreamer mingw-w64-ucrt-x86_64-gst-plugins-base mingw-w64-ucrt-x86_64-gst-plugins-good mingw-w64-ucrt-x86_64-gst-plugins-bad
   ```

### 1.2 Linux (Ubuntu/Debian)

```bash
sudo apt install gcc pkg-config libgstreamer1.0-dev gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-pulseaudio
```

### 1.3 Conferir a instalação

Os comandos abaixo devem mostrar a versão e a descrição de cada elemento (nenhum
"No such element"):

```bash
gst-launch-1.0 --gst-version
gst-inspect-1.0 rtpjitterbuffer
gst-inspect-1.0 rtpL16pay
gst-inspect-1.0 netsim
```

Teste rápido de som (deve tocar um apito; Ctrl+C para parar):

```bash
gst-launch-1.0 audiotestsrc ! autoaudiosink
```

---

## 2. Compilação

**Terminal MSYS2 UCRT64 (Windows) ou Linux:**

```bash
gcc -Wall transmissor.c -o transmissor.exe $(pkg-config --cflags --libs gstreamer-1.0)
gcc -Wall receptor.c -o receptor.exe $(pkg-config --cflags --libs gstreamer-1.0)
```

(No Linux, pode omitir o `.exe`.)

**PowerShell (Windows):** no PowerShell o `$(pkg-config ...)` não funciona como no bash, por
isso existe o script:

```powershell
powershell -ExecutionPolicy Bypass -File .\compilar.ps1
```

---

## 3. Execução

Use **dois terminais**. Inicie **o receptor primeiro** e só depois o transmissor.

```
receptor.exe    <porta> <latencia_ms> [arquivo.csv] [arquivo.wav]
transmissor.exe <host> <porta> <num_buffers> [perda_%] [atraso_max_ms]
```

| Programa | Parâmetro | Padrão | Significado |
|---|---|---|---|
| receptor | `porta` | 5000 | Porta UDP onde escuta (igual à do transmissor) |
| receptor | `latencia_ms` | 200 | Latência do `rtpjitterbuffer` |
| receptor | `arquivo.csv` | — | Grava as estatísticas de cada segundo (separador `;`) |
| receptor | `arquivo.wav` | — | Grava o áudio exatamente como foi reproduzido |
| transmissor | `host` | 127.0.0.1 | Endereço de destino (127.0.0.1 = mesmo computador) |
| transmissor | `porta` | 5000 | Porta de destino |
| transmissor | `num_buffers` | 500 | Duração do áudio (500 ≈ 11,6 s) |
| transmissor | `perda_%` | 0 | Porcentagem de pacotes descartados pelo `netsim` |
| transmissor | `atraso_max_ms` | 0 | Atraso máximo aplicado a metade dos pacotes pelo `netsim` |

Exemplo (terminal MSYS2 ou Linux):

```bash
./receptor.exe 5000 200 teste.csv teste.wav
./transmissor.exe 127.0.0.1 5000 500
```

No **PowerShell**, use `.\receptor.exe` e, antes, rode em cada terminal:
`$env:Path = "C:\msys64\ucrt64\bin;$env:Path"` (para o Windows achar as DLLs do GStreamer).

Quando o transmissor termina, o receptor percebe que não chegam pacotes novos há 3 s e
**encerra sozinho**, imprimindo um resumo final.

**Dois computadores:** no transmissor, troque `127.0.0.1` pelo IP do receptor
(`ipconfig` / `ip addr`). Os dois precisam estar na mesma rede; o firewall pode pedir
permissão para o receptor na primeira vez.

---

## 4. Experimentos

Parte A — rede normal:

| Teste | Receptor | Transmissor |
|---|---|---|
| A1 | `./receptor.exe 5000 20 teste_A1.csv teste_A1.wav`   | `./transmissor.exe 127.0.0.1 5000 500` |
| A2 | `./receptor.exe 5000 200 teste_A2.csv teste_A2.wav`  | `./transmissor.exe 127.0.0.1 5000 500` |
| A3 | `./receptor.exe 5000 1000 teste_A3.csv teste_A3.wav` | `./transmissor.exe 127.0.0.1 5000 500` |

Parte B — rede ruim simulada (2% de perda, até 80 ms de atraso):

| Teste | Receptor | Transmissor |
|---|---|---|
| B1 | `./receptor.exe 5000 20 teste_B1.csv teste_B1.wav`   | `./transmissor.exe 127.0.0.1 5000 500 2 80` |
| B2 | `./receptor.exe 5000 200 teste_B2.csv teste_B2.wav`  | `./transmissor.exe 127.0.0.1 5000 500 2 80` |
| B3 | `./receptor.exe 5000 1000 teste_B3.csv teste_B3.wav` | `./transmissor.exe 127.0.0.1 5000 500 2 80` |

Os resultados obtidos estão em `resultados/`, analisados em
[REGISTRO_EXPERIMENTOS.md](REGISTRO_EXPERIMENTOS.md), com a conclusão em
[CONCLUSAO.md](CONCLUSAO.md).

---

## 5. Pipelines

Diagrama completo, com cores por tipo de elemento e os parâmetros configurados:
[`pipelines.drawio`](pipelines.drawio) (abrir em https://app.diagrams.net →
*Abrir diagrama existente*, ou na extensão Draw.io do VS Code).

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
  ! [tee name=t ! queue]                                                  (só com gravação)
  ! audioresample ! autoaudiosink sync=false
  [t. ! queue ! audioconvert ! wavenc ! filesink location=ARQUIVO.wav]   (só com gravação)
```

---

## 6. Problemas comuns

| Sintoma | Causa / solução |
|---|---|
| `bash: ./transmissor.exe: No such file or directory` | Os programas ainda não foram compilados nesta pasta (seção 2) |
| `bash: $env:Path: command not found` | Comando de PowerShell digitado no terminal MSYS2 (bash); nele não é necessário |
| `no element "netsim"` | Falta o pacote `gst-plugins-bad` (seção 1) |
| Receptor não mostra pacotes | Porta diferente nos dois programas, ou transmissor iniciado antes do receptor |
| Começo do áudio faltando | O transmissor foi iniciado antes do receptor; inicie o receptor primeiro |
| Ctrl+C não encerra o receptor | No terminal MSYS2 o Ctrl+C não chega a programas Windows; o receptor encerra sozinho 3 s após o fim da transmissão |

## 7. Notas de implementação

| Decisão | Por quê |
|---|---|
| Pipelines criadas com `gst_parse_launch` dentro do programa C | Construção programática, equivalente às pipelines validadas com `gst-launch-1.0` |
| Formato `S16BE` antes do `rtpL16pay` | O RTP L16 exige amostras de 16 bits big-endian |
| `sync=false` no `autoaudiosink` | Transmissor e receptor não compartilham o mesmo relógio |
| `netsim` no transmissor | Simula perda e atraso em qualquer sistema (`tc netem` só existe no Linux) |
| Estatísticas lidas da propriedade `stats` do `rtpjitterbuffer` | Diagnóstico de perdas com os recursos do próprio framework |
| Receptor encerra após 3 s sem pacotes novos | Fim automático, independente do terminal repassar o Ctrl+C |
| EOS enviado antes de sair quando há gravação | O `wavenc` só finaliza o cabeçalho do WAV ao receber EOS |
| Ctrl+C: `g_unix_signal_add` no Linux, `signal()` no Windows | Forma segura em cada sistema |
