# Hello Gungnir

This is the canonical minimal Gungnir 1.0 application example.

Create a generated project first:

```sh
gungnir new hello
cd hello
```

Then replace the generated controller and route files with:

- [HomeController.gnr](app/controllers/HomeController.gnr)
- [web.gnr](routes/web.gnr)

Validate and run:

```sh
gungnirc app/controllers/HomeController.gnr --check
gungnir build
gungnir run
```

The default generated environment listens on `127.0.0.1:8000`. A request to `/` should return:

```text
Hello from Gungnir
```

The documentation workflow validates the controller with the current structured compiler so this example cannot silently drift away from the supported 1.0 language contract.
