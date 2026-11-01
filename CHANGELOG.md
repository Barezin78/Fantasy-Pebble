# Changelog

## v1.0.2 — 29/10/2026
- Sync: monta os dados num buffer temporário e só troca/persiste no SYNC=2 (falha no meio não zera a tela).
- ORDEM das ligas: deduplica por `league_id` e limita a 2.
- Buffer do placar dos titulares ampliado (sem aviso de truncamento no build).
- Acentos onde a fonte Gothic aguenta (Última, vitória, pré).
- Teste Node com fixture (não precisa de SLEEPER_USER); `--live` opcional.

## v1.0.1 — 28/10/2026
- Usuário Sleeper configurável nas Configurações (Clay); sem default pessoal.
- Ligas opcionais no config; se vazias, até 2 ligas do usuário.
- Mock/prévia e dados de demonstração com nomes fictícios.
- Autor: Barezin78.

## v1.0.0 — 2026
- Primeira versão: Início, confronto, titulares e tabela.
- PebbleKit JS: Sleeper + scoreboard ESPN; cache de jogadores no celular; persist no relógio.
