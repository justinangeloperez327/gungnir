# Mail

Gungnir provides structured mail declarations and native transports for composing and sending application email.

## Defining mail

```gnr
mail WelcomeMail {
    string name;
    string email;

    subject() {
        return "Welcome";
    }

    text() {
        return "Welcome, " + name + ".";
    }

    html() {
        return "<h1>Welcome, " + name + "</h1>";
    }
}
```

Mail declarations can carry typed data used during message composition.

## Recipients

Messages support sender, recipients, CC, BCC, and reply-to addresses.

## HTML and text

A message can contain plain-text and HTML bodies. HTML can be produced directly or from a rendered application view.

## Attachments

Messages support file or in-memory attachments with appropriate filenames and content metadata.

## Sending

Mail is sent through the application's configured mailer and transport.

SMTP is provided for network delivery, while an in-memory transport is suitable for tests.

## Queueing

Mail can be dispatched through the queue system when delivery should happen outside the current request.

## Testing mail

Use the in-memory transport to assert recipients, subject, body, headers, and attachments without sending external email.
