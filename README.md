# Fantasy para Pebble 🏈 (v1.0.2)

App de fantasy NFL (Sleeper) para **Pebble Time / Time 2** (e basalt/diorite). Mostra até duas ligas, confronto da semana, titulares e tabela. Os dados vêm do celular via PebbleKit JS (API pública do Sleeper) e ficam em cache no relógio.

**Versão:** 1.0.2 · autor: Barezin78

## Configurar
1. Instale o `.pbw` pelo app Pebble/Rebble no celular.
2. Abra as **Configurações** (engrenagem) do app Fantasy.
3. Informe o **usuário do Sleeper** (obrigatório).
4. Opcional: liste as ligas separadas por vírgula. Se deixar em branco, o app pega até 2 ligas NFL do usuário (ordem alfabética).

Sem usuário configurado, o relógio mostra um erro pedindo a configuração.

## Telas
- Início: cartões das ligas
- Selecionar: confronto → titulares (rolar) → tabela

## Compilar
```
npm install
pebble build
```
O pacote versionado sai em `dist/Fantasy-v1.0.2.pbw`.

## Privacidade
O repositório e as prévias usam nomes **fictícios**. Seu usuário Sleeper fica só nas Configurações do celular (localStorage), não no código.
