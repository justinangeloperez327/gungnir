# Mail

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../mail.md) before using an API.

A Gungnir `mail` declaration defines an email message.

Mail composition is separate from mail transport.

A mail declaration describes:

- recipients when not supplied externally;
- subject;
- sender/reply-to metadata when required;
- text or HTML/view content;
- attachments;
- message-specific headers where explicitly supported.

SMTP, provider APIs, TLS, retries, connection pooling, and queue workers belong to mail transports/runtime infrastructure.

## Basic mail

```gnr
mail WelcomeMail {
    User user;

    public subject() {
        return 'Welcome to Gungnir';
    }

    public content() {
        return view('mail/welcome', {
            'user': user
        });
    }
}
```

Send it:

```gnr
Mail::to(user.email)
    .send(
        WelcomeMail(user: user)
    );
```

## Mail responsibility

A mail declaration answers:

```text
What is the email subject?
What content should be rendered?
What sender/reply-to metadata is required?
What attachments belong to this message?
```

It does not answer:

```text
Which SMTP socket should be opened?
How should STARTTLS be negotiated?
How should a provider API authenticate?
How should retries be scheduled?
```

Those belong below the application mail layer.

## Mail data

Mail declarations may contain immutable message data:

```gnr
mail InvoiceMail {
    Invoice invoice;
    User customer;

    public subject() {
        return 'Invoice ' + invoice.number;
    }

    public content() {
        return view('mail/invoice', {
            'invoice': invoice,
            'customer': customer
        });
    }
}
```

These fields describe the message context.

They should not be used as arbitrary mutable runtime state.

## Subject

Every normal mail should define a subject:

```gnr
public subject() {
    return 'Welcome';
}
```

Subjects must be validated so control characters cannot create header injection.

## HTML/view content

Use a view:

```gnr
public content() {
    return view('mail/welcome', {
        'user': user
    });
}
```

The view system owns template rendering and escaping.

## Text content

A mail may use plain text:

```gnr
public text() {
    return 'Your order has shipped.';
}
```

## Multipart alternative content

A mail may define both HTML/view and text forms:

```gnr
mail OrderShippedMail {
    Order order;

    public subject() {
        return 'Order shipped';
    }

    public content() {
        return view('mail/order-shipped', {
            'order': order
        });
    }

    public text() {
        return 'Your order ' + order.number + ' has shipped.';
    }
}
```

The transport should encode the alternatives correctly.

## To

Recipients are usually supplied when sending:

```gnr
Mail::to(user.email)
    .send(WelcomeMail(user: user));
```

Multiple recipients:

```gnr
Mail::to([
    first.email,
    second.email
]).send(
    AnnouncementMail()
);
```

## CC

```gnr
Mail::to(customer.email)
    .cc(accountManager.email)
    .send(
        InvoiceMail(invoice: invoice)
    );
```

## BCC

```gnr
Mail::to(customer.email)
    .bcc(auditMailbox)
    .send(
        InvoiceMail(invoice: invoice)
    );
```

BCC recipients must be envelope recipients without appearing in visible BCC headers.

## From

Application-wide sender defaults should normally come from mail configuration.

A message may override the sender when explicitly allowed:

```gnr
public from() {
    return address(
        'billing@example.com',
        'Billing'
    );
}
```

## Reply-to

```gnr
public replyTo() {
    return address(
        'support@example.com',
        'Support'
    );
}
```

## Attachments

A mail may declare attachments:

```gnr
public attachments() {
    return [
        attachment(invoice.pdfPath)
            .name('invoice.pdf')
    ];
}
```

Attachment APIs should validate file access and metadata safely.

Large attachment streaming and storage integration belong to the mail/storage runtime.

## In-memory attachment

Where supported:

```gnr
public attachments() {
    return [
        attachmentData(pdfBytes)
            .name('invoice.pdf')
            .contentType('application/pdf')
    ];
}
```

## Mail headers

Custom message headers should be explicit and validated:

```gnr
public headers() {
    return {
        'X-Application': 'Gungnir'
    };
}
```

Applications should not manually construct standard MIME or transport headers that the framework can generate safely.

## Date and Message-ID

The mail transport/runtime should create valid standard metadata such as:

```text
Date
Message-ID
```

when not explicitly provided.

Application code should not need to generate these manually.

## Sending mail

```gnr
Mail::to(user.email)
    .send(
        WelcomeMail(user: user)
    );
```

The send operation resolves the configured mailer/transport.

## Queueing mail

Queued delivery should be explicit once queue support is stable:

```gnr
Mail::to(user.email)
    .queue(
        WelcomeMail(user: user)
    );
```

Queueing is a separate lifecycle from direct send.

The framework should not call something queued if it merely performs asynchronous network I/O in the current request.

## Mailers

Applications may configure multiple mailers:

```text
default
transactional
marketing
```

An explicit mailer may be selected:

```gnr
Mail::mailer('transactional')
    .to(user.email)
    .send(
        WelcomeMail(user: user)
    );
```

Mailer names and transport configuration belong to application configuration.

## SMTP

SMTP is one transport implementation.

The application-facing mail declaration should not contain:

```text
SMTP host
SMTP port
TLS mode
username
password
socket configuration
```

Those values belong in environment/configuration.

Production SMTP should require safe TLS/authentication settings according to the configured adapter.

## Provider APIs

HTTP-provider mail services may implement the same mail transport contract.

Mail declarations should not change because the application switches between SMTP and a provider API.

## Delivery failures

A direct `send()` failure must surface as a mail/delivery error.

The transport must not silently downgrade security settings or report failure as success.

Queued mail failures belong to queue retry/failure handling.

## Mail and notifications

Notifications may choose mail as a channel:

```gnr
notification WelcomeNotification {
    public via(User user) {
        return ['mail'];
    }

    public mail(User user) {
        return WelcomeMail(user: user);
    }
}
```

The notification chooses the channel.

The mail declaration defines the email.

## Mail and events/listeners

A listener may send mail:

```gnr
listener SendInvoiceMail {
    public handle(InvoiceIssued event) {
        Mail::to(event.invoice.customer.email)
            .send(
                InvoiceMail(
                    invoice: event.invoice
                )
            );
    }
}
```

## Dependency injection

Mail declarations should rarely need service dependencies.

If dynamic message composition genuinely requires an application service, `inject` may be supported, but substantial workflow logic should remain outside the mail declaration.

## What does not belong in mail

Mail declarations should not:

- perform business transactions;
- update unrelated models;
- authorize requests;
- parse HTTP requests;
- implement SMTP;
- manage sockets/TLS;
- retry indefinitely;
- act as event listeners;
- contain large application workflows.

## Mail grammar

Conceptually:

```text
mailDeclaration
  := 'mail' Identifier '{'
       mailMember*
     '}'

mailMember
  := fieldDeclaration
   | injectDeclaration
   | mailAction

mailAction
  := 'public' Identifier '(' parameterList? ')' block
```

Recognized composition actions may include:

```text
subject
content
text
from
replyTo
attachments
headers
```

## Mail AST

```text
MailDeclaration
  name
  fields[]
  injections[]
  subject
  content?
  text?
  from?
  replyTo?
  attachments?
  headers?
```

## Semantic validation

The compiler should validate:

- mail names are unique;
- field/injection types resolve;
- subject returns a string-compatible value;
- content returns a view/mail-content-compatible value;
- text returns string-compatible content;
- from/replyTo return address-compatible values;
- attachments return attachment-compatible values;
- headers return a valid structured header map;
- unsupported arbitrary methods are rejected;
- address and header values are validated at runtime where values are dynamic.

## Compiler contract

```gnr
mail WelcomeMail {
    User user;

    public subject() {
        return 'Welcome';
    }

    public content() {
        return view('mail/welcome', {
            'user': user
        });
    }
}
```

should pass through:

```text
source
  -> parser
  -> MailDeclaration AST
  -> field/type resolution
  -> mail action validation
  -> validated mail AST
  -> message composition lowering
  -> mail runtime/transport
  -> C++23 generation
```

The transpiler must not infer mail composition by scanning raw source.

## Generated C++ boundary

The native runtime may use:

```text
mail::Message
mail::Mailer
mail::Transport
SMTP/provider adapters
MIME encoders
attachment streams
```

Those are implementation details.

Normal application code should only describe and send mail through the Gungnir mail API.

## Naming convention

Public mail APIs use camelCase where required:

```text
replyTo
contentType
```

Core APIs remain concise:

```text
to
cc
bcc
from
subject
content
text
attachments
headers
send
queue
mailer
```

## Design rule

```text
mail = email composition
```

Mail declarations describe email content and metadata.

The mail runtime delivers it.

Notifications may select mail as one delivery channel.

