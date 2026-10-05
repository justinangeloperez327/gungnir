# Mail

Gungnir provides structured mail declarations and native transports for composing and sending application email.

The typed delivery APIs below require the current source SDK. Published
v1.0.0 packages retain the native delivery surface described in the
[release scope](release-v1.md).

## Defining mail

```gnr
mail WelcomeMail {
    string name;

    subject() {
        return "Welcome";
    }

    text() {
        return "Welcome, " + name + ".";
    }

    html() {
        return "<h1>Welcome</h1>";
    }
}
```

Mail declarations can carry typed data used during message composition.
`subject()` is required. `text()` and `html()` are optional synchronous methods
returning strings. Use `content()` returning a view response instead of `html()`
when composing HTML from a template:

```gnr
mail ReceiptMail {
    string name;
    subject() { return "Your receipt"; }
    content() { return view("mail/receipt", {name: name}); }
}
```

The existing view engine escapes `{{ name }}`. Direct HTML strings are
application-controlled and are not automatically escaped.

## Recipients

Messages support sender, recipients, CC, BCC, and reply-to addresses.
Inject `Mail`, then select the recipient before sending a mail declaration:

```gnr
controller WelcomeController {
    inject Mail mail;
    store(Request request) {
        const pending = mail.to("ada@example.test", "Ada")
            .cc("copy@example.test")
            .bcc("private@example.test")
            .replyTo("support@example.test", "Support");
        pending.send(WelcomeMail("Ada"));
        return noContent();
    }
}
```

`to`, `cc`, `bcc`, `from`, and `replyTo` accept an email string and an optional
display name. They return an owned `PendingMail` value; extending one value
does not mutate another copy. Configure the default sender in bootstrap, or use
`pending.from(email, name)`. Invalid addresses and header controls are rejected
before transport delivery or queue publication.

## HTML and text

A message can contain plain-text and HTML bodies. HTML can be produced directly or from a rendered application view.

## Attachments

Messages support file or in-memory attachments with appropriate filenames and content metadata.
`attach(filename, bytes, contentType)` adds owned binary-safe contents. The
content type defaults to `application/octet-stream`; filenames are metadata.
Read application-controlled files through `Storage` before attaching them.

```gnr
function void sendAttachment(Mail mail, string bytes) {
    mail.to("ada@example.test")
        .attach("report.csv", bytes, "text/csv")
        .send(WelcomeMail("Ada"));
}
```

## Sending

Mail is sent through the application's configured mailer and transport.

SMTP is provided for network delivery, while an in-memory transport is suitable for tests.
Configure adapters in `bootstrap/app.hpp` before generated controllers resolve:

```cpp
gungnir::ServiceOptions options;
options.mail = std::make_shared<gungnir::mail::MemoryTransport>();
options.sender = {"sender@example.test", "Application"};
options.queue = std::make_shared<gungnir::queue::MemoryDriver>();
app.provider<gungnir::ServicesProvider>(options);
```

For SMTP, build the SDK with `GUNGNIR_WITH_SMTP=ON`, include
`<gungnir/mail/smtp_transport.hpp>`, and supply `SmtpTransport` with your host,
port, TLS, credentials and timeout settings. Installed generated applications
link the exported SMTP adapter when it is available. The core release packages
do not include SMTP. `send` is synchronous and propagates transport errors;
use queued delivery for work that should leave the HTTP request.

## Queueing

Mail can be dispatched through the queue system when delivery should happen outside the current request.

```gnr
controller QueuedWelcomeController {
    inject Mail mail;
    store(Request request) {
        const id = mail.to("ada@example.test").queue(WelcomeMail("Ada"), 3);
        return json({job: id});
    }
}
```

`queue(mail, attempts = 1, afterCommit = true)` returns a job ID. Attempts are
positive integers within the queue driver's range. Composition and recipient
validation happen before publication. An active database transaction defers
publication until commit; rollback discards it. Passing `false` as the third
argument publishes immediately.

The queue stores the composed subject, bodies, addresses and attachment bytes.
Later changes to the source data do not alter queued mail. A worker boots its
own application and uses its configured transport; it does not serialize
service handles. Start it with `gungnir queue:work` or `--once`. Use a shared
queue adapter such as Redis across processes; memory queues are process-local.
Retries use the existing worker backoff and failed-job APIs described in
[Queues and Jobs](queues.md).

## Testing mail

Use the in-memory transport to assert recipients, subject, body, headers, and attachments without sending external email.
Delivery is at least once. A transport may accept a message before an
acknowledgement is lost, so applications must account for possible duplicates.
