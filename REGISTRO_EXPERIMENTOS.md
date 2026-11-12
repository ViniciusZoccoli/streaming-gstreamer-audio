# Registro dos experimentos — Streaming de áudio com GStreamer

## Ambiente de teste

- Sistema operacional: Windows 11 + MSYS2 (UCRT64), GStreamer instalado via `pacman`
- Transmissor e receptor: mesmo computador, endereço 127.0.0.1, porta 5000
- Transmissor executado com: `./transmissor.exe 127.0.0.1 5000 500` (500 buffers ≈ 11,6 s de áudio)
- Simulação de rede ruim: elemento `netsim` no transmissor (`./transmissor.exe 127.0.0.1 5000 500 2 80`)
- Arquivos CSV gerados pelo receptor: `teste_A1.csv` … `teste_B3.csv`

Observação: os 500 buffers de áudio viram **≈1500 pacotes RTP**, porque cada buffer
(1024 amostras × 2 canais × 2 bytes = 4096 bytes) é maior que o tamanho máximo de um
pacote, e o `rtpL16pay` divide cada um em 3.

## Parte A — Rede normal (sem simulação)

| Teste | Latência do jitterbuffer | Como o som saiu | Entregues | Perdidos | Atrasados | Duplicados |
|---|---|---|---|---|---|---|
| A1 | 20 ms   | Começou na hora, limpo | 1500 | 0 | 0 | 0 |
| A2 | 200 ms  | Começou na hora, limpo (pareceu mais baixo¹) | 1500 | 0 | 0 | 0 |
| A3 | 1000 ms | Começou na hora, sem diferença perceptível | 1500 | 0 | 0 | 0 |

Observações:

- Na rede local (127.0.0.1) não há perda nem atraso: os 1500 pacotes chegaram em todas as
  latências testadas. Nesse cenário a latência do buffer só muda **quanto tempo o som demora
  para começar**, não a qualidade.
- Mesmo com 1000 ms o som pareceu começar "na hora": 1 segundo entre apertar Enter no
  transmissor e ouvir o som é difícil de perceber sem uma referência (como voz ou vídeo
  sincronizado). Numa chamada de voz esse atraso seria bem perceptível.

## Parte B — Rede com atraso e perda simulados

Simulação usada: 2% de perda e atraso de até 80 ms em metade dos pacotes
(`./transmissor.exe 127.0.0.1 5000 500 2 80`)

| Teste | Latência do jitterbuffer | Como o som saiu | Entregues | Perdidos | Atrasados | Duplicados |
|---|---|---|---|---|---|---|
| B1 | 20 ms   | Bem picotado, começou na hora | 884  | 614 | 558 | 12 |
| B2 | 200 ms  | Um pouco picotado (pareceu mais baixo¹) | 1457 | 37  | 0   | 0  |
| B3 | 1000 ms | Um pouco picotado (pareceu mais alto¹) | 1460 | 35  | 0   | 0  |

¹ O programa não altera o volume em nenhuma configuração; a diferença percebida não é efeito
da latência (provavelmente volume do sistema ou percepção entre execuções).

Cuidado observado durante os testes: o receptor precisa ser iniciado **antes** do transmissor.
Em uma rodada em que isso não aconteceu, o começo do áudio foi perdido e o jitterbuffer **não**
contou esses pacotes como perdidos (ele só conta a partir do primeiro pacote que recebe).

Os arquivos `teste_*.wav` contêm o áudio exatamente como foi reproduzido em cada teste.

Observações:

- **B1 (20 ms):** o buffer é menor que o atraso da rede (até 80 ms). Por isso quase metade dos
  pacotes não chegou a tempo: 558 chegaram depois do prazo e foram descartados, e o
  jitterbuffer os contou também como perdidos (614). Só 884 dos ~1500 pacotes foram tocados —
  por isso o som saiu bem picotado.
- **B2 (200 ms) e B3 (1000 ms):** o buffer é maior que o atraso máximo, então **nenhum** pacote
  foi descartado por atraso. As perdas que restaram (≈2–3%) são os pacotes que o `netsim`
  jogou fora de propósito — o que explica o som "um pouco picotado" nesses dois testes, contra
  o "bem picotado" do B1.
- De B2 para B3 a qualidade **não melhorou**: depois que o buffer cobre o atraso da rede,
  aumentá-lo só acrescenta demora para o som começar.

## Conclusão

O `rtpjitterbuffer` resolve o problema do **atraso variável**: quando sua latência é maior
que a variação de atraso da rede, os pacotes atrasados são esperados e reordenados, e nenhum é
descartado (B2 e B3). Com latência pequena demais (B1), a maior parte dos pacotes chega depois
do prazo e é descartada, mesmo tendo chegado.

A limitação é que o buffer **não recupera pacotes que realmente se perderam**: os ≈2% descartados
pela rede simulada continuaram perdidos em todas as configurações. Além disso, aumentar a
latência tem custo — o som começa mais tarde — sem ganho de qualidade depois que o atraso da rede
já está coberto. O melhor valor é o menor que ainda cubra a variação de atraso da rede
(aqui, 200 ms).
