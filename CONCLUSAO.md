# Conclusão

Foram implementados um transmissor e um receptor de áudio em C com GStreamer. O áudio é
encapsulado em pacotes RTP (L16), enviado por UDP e reconstruído no receptor, que usa o
`rtpjitterbuffer` para compensar o jitter e consulta suas estatísticas para diagnosticar perdas.

## Comportamento do streaming

- Em rede local sem interferência (127.0.0.1), todos os ~1500 pacotes chegaram nas três
  latências testadas (20, 200 e 1000 ms). Nesse cenário a latência só muda o tempo até o som
  começar, não a qualidade.
- Com rede ruim simulada pelo elemento `netsim` (2% de perda e até 80 ms de atraso), a latência
  passou a fazer diferença:
  - **20 ms:** menor que o atraso da rede; cerca de metade dos pacotes chegou depois do prazo e
    foi descartada (558 atrasados, 614 perdidos) — som bem picotado;
  - **200 ms e 1000 ms:** maiores que o atraso da rede; nenhum pacote descartado por atraso,
    restando apenas os ≈2% realmente perdidos — som apenas levemente picotado.

## Mecanismos de controle de jitter e perda

- O `rtpjitterbuffer` resolve o **atraso variável**: quando sua latência é maior que a variação
  de atraso da rede, os pacotes são esperados, reordenados pelo número de sequência e entregues
  em ordem.
- Sua limitação é que ele **não recupera pacotes que realmente se perderam**: apenas os detecta
  (buraco na numeração RTP) e os contabiliza em `num-lost`. A recuperação exigiria mecanismos
  adicionais (retransmissão, FEC), fora do escopo da atividade.
- Aumentar a latência tem custo: o áudio começa mais tarde, sem ganho de qualidade depois que o
  atraso da rede já está coberto. O melhor valor é o **menor que ainda cubra a variação de
  atraso da rede** — neste experimento, 200 ms.
- As estatísticas do próprio `rtpjitterbuffer` (`num-pushed`, `num-lost`, `num-late`,
  `num-duplicates`) foram suficientes para diagnosticar e comparar o comportamento das
  configurações, sem implementar um algoritmo próprio.

Detalhes dos testes em [REGISTRO_EXPERIMENTOS.md](REGISTRO_EXPERIMENTOS.md).
