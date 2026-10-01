"""Exercise the actual stdio transport, unsaved modules, and UTF-16 edits."""
import json
import pathlib
import subprocess
import sys
import tempfile


def frame(message):
    body = json.dumps(message, ensure_ascii=False).encode()
    return b"Content-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body


def unpack(data):
    result = []
    while data:
        headers, data = data.split(b"\r\n\r\n", 1)
        length = int(headers.split(b":", 1)[1])
        result.append(json.loads(data[:length]))
        data = data[length:]
    return result


def pos(source, offset):
    before = source[:offset]
    return {"line": before.count("\n"), "character": len(before.rsplit("\n", 1)[-1].encode("utf-16-le")) // 2}


with tempfile.TemporaryDirectory(prefix="gungnir lsp ") as directory:
    root = pathlib.Path(directory)
    math_uri = (root / "math.gnr").as_uri()
    main_uri = (root / "main.gnr").as_uri()
    math = "function int add(int left, int right) { return left + right; }"
    main = "import math;\nfunction int answer() { const label = '😀'; return add(2, 3); }\n"
    messages = []

    def send(method, params=None, ident=None):
        value = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            value["params"] = params
        if ident is not None:
            value["id"] = ident
        messages.append(frame(value))

    send("textDocument/hover", {}, 0)
    send("initialize", {"rootUri": root.as_uri()}, 1)
    send("initialized", {})
    for uri, text in ((math_uri, math), (main_uri, main)):
        send("textDocument/didOpen", {"textDocument": {"uri": uri, "text": text, "version": 1, "languageId": "gungnir"}})
    target = {"textDocument": {"uri": main_uri}, "position": pos(main, main.index("add"))}
    send("textDocument/definition", target, 2)
    send("textDocument/hover", target, 3)
    send("textDocument/documentSymbol", {"textDocument": {"uri": main_uri}}, 4)
    send("textDocument/completion", target, 5)
    send("textDocument/formatting", {"textDocument": {"uri": main_uri}, "options": {"tabSize": 4, "insertSpaces": True}}, 6)
    begin = main.index("3)")
    send("textDocument/didChange", {"textDocument": {"uri": main_uri, "version": 2}, "contentChanges": [{"range": {"start": pos(main, begin), "end": pos(main, begin + 1)}, "text": "'bad'"}]})
    send("textDocument/didChange", {"textDocument": {"uri": main_uri, "version": 3}, "contentChanges": [{"text": main}]})
    send("textDocument/didChange", {"textDocument": {"uri": main_uri, "version": 2}, "contentChanges": [{"text": "invalid"}]})
    send("textDocument/definition", target, 7)
    send("unknown/method", {}, 8)
    messages.append(frame({"jsonrpc": 2, "method": "bad", "id": 10}))
    messages.append(b"Content-Length: 1\r\n\r\n{")
    send("textDocument/didClose", {"textDocument": {"uri": main_uri}})
    send("shutdown", None, 9)
    send("exit")
    process = subprocess.run([sys.argv[1], "lsp"], input=b"".join(messages), capture_output=True, timeout=30)
    assert process.returncode == 0, process.stderr.decode()
    responses = unpack(process.stdout)
    by_id = {value["id"]: value for value in responses if "id" in value}
    assert by_id[0]["error"]["code"] == -32002
    assert by_id[1]["result"]["capabilities"]["positionEncoding"] == "utf-16"
    assert by_id[2]["result"] == {"uri": math_uri, "range": {"start": pos(math, math.index("add")), "end": pos(math, math.index("add") + 3)}}
    assert "int add(int left, int right)" in by_id[3]["result"]["contents"]["value"]
    assert by_id[4]["result"][0]["name"] == "answer"
    assert any(item["label"] == "add" for item in by_id[5]["result"]["items"])
    assert by_id[6]["result"] and "😀" in by_id[6]["result"][0]["newText"]
    assert by_id[7]["result"] == by_id[2]["result"]
    assert by_id[8]["error"]["code"] == -32601
    assert by_id[None]["error"]["code"] == -32700
    assert any(item.get("error", {}).get("code") == -32600 for item in responses)
    diagnostics = [item["params"] for item in responses if item.get("method") == "textDocument/publishDiagnostics" and item["params"]["uri"] == main_uri]
    assert any(item.get("version") == 2 and item["diagnostics"] for item in diagnostics)
    assert any(item.get("version") == 3 and not item["diagnostics"] for item in diagnostics)
    assert diagnostics[-1]["diagnostics"] == []
    assert subprocess.run([sys.argv[1], "lsp"], input=frame({"jsonrpc": "2.0", "method": "exit"}), capture_output=True, timeout=10).returncode == 1
    assert subprocess.run([sys.argv[1], "lsp"], input=b"Content-Length: 999999999\r\n\r\n", capture_output=True, timeout=10).returncode == 1
print("LSP protocol checks passed")
