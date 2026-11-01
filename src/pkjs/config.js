// Clay — engrenagem no app Pebble/Rebble do celular
module.exports = [
  { type: 'heading', defaultValue: 'Fantasy' },
  { type: 'text', defaultValue: 'Configure seu usuário do Sleeper. A API é pública (sem senha).' },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Sleeper' },
      {
        type: 'input',
        messageKey: 'USER',
        label: 'Usuário do Sleeper',
        defaultValue: '',
        attributes: { placeholder: 'seu_usuario' }
      },
      {
        type: 'input',
        messageKey: 'ORDEM',
        label: 'Ligas (opcional, separadas por vírgula)',
        defaultValue: '',
        attributes: { placeholder: 'Liga Alfa, Liga Beta' }
      },
      {
        type: 'text',
        defaultValue: 'Se deixar as ligas em branco, o app mostra até 2 ligas NFL do seu usuário (ordem alfabética).'
      }
    ]
  },
  { type: 'submit', defaultValue: 'Salvar' }
];
