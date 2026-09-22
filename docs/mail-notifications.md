# Mail and Notifications

Gungnir separates message construction from delivery infrastructure.

## Mail

`mail::Message` contains sender, recipients, subject and text/HTML bodies. `mail::Transport` is the delivery boundary and `mail::Mailer` delegates delivery to the configured transport.

`MemoryTransport` captures messages for development and tests. It is not an SMTP implementation.

Production SMTP or provider integrations must implement a real transport with TLS, authentication, timeouts, provider errors and operational retry behavior.

## Notifications

Notifications declare stable names and the channels through which they should be delivered. `notifications::Manager` routes them to configured channel implementations.

The core does not invent SMS, push, chat or mail provider semantics. Each channel is an explicit adapter.

## Queued delivery

Mail and notification delivery is synchronous at this layer. Applications that require deferred delivery should serialize a stable job payload and use the queue subsystem. This keeps transport behavior separate from background execution.

## Security

Header injection, recipient validation, attachment handling and provider credentials belong at transport boundaries. Provider secrets must not be embedded in notification payloads or source code.
