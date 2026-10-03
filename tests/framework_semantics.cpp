#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <gungnir/language/compiler.hpp>

namespace {

using namespace gungnir::language;

void reject(
    std::string_view source,
    std::string_view code
) {
    const auto result =
        Compiler{}.compile(
            source,
            "framework-invalid.gnr"
        );

    if (result.success()) {
        std::cerr
            << "unexpected framework semantic success\n";
    }

    assert(!result.success());
    assert(result.code.empty());
    assert(!result.validated);

    const auto found =
        std::find_if(
            result.diagnostics.begin(),
            result.diagnostics.end(),
            [&](const auto& diagnostic) {
                return diagnostic.code == code;
            }
        );

    if (found == result.diagnostics.end()) {
        for (const auto& diagnostic :
             result.diagnostics) {
            std::cerr
                << diagnostic.code
                << ' '
                << diagnostic.message
                << '\n';
        }
    }

    assert(found != result.diagnostics.end());
}

void valid_framework_contracts_compile() {
    constexpr std::string_view source = R"(
model User {
    table = 'users';
    fillable = ['name'];
    timestamps = true;
    softDeletes = true;
}

event Created {
    int id;
}

listener RecordCreated {
    handle(Created event) {
        const copy = event.id;
    }
}

job RebuildIndex {
    int id;

    async handle() {
        await completed();
    }
}

async function void completed() {
    return;
}

middleware PassThrough {
    async handle(Request request, Next next) {
        return await next(request);
    }
}

policy UserPolicy {
    view(User actor, User resource) {
        return actor.id == resource.id;
    }

    private int helper() {
        return 1;
    }
}

mail Welcome {
    string name;

    subject() {
        return 'Welcome ' + name;
    }

    text() {
        return 'Hello ' + name;
    }
}

notification WelcomeNotice {
    via(User recipient) {
        return ['mail', 'database'];
    }

    toMail(User recipient) {
        return Welcome('Ada');
    }

    toDatabase(User recipient) {
        return { id: recipient.id };
    }
}

migration CreateUsers {
    up() {
        Table::create('users', (table) => {
            table.id();
            table.string('name');
            table.timestamps();
            table.softDeletes();
        });
    }

    down() {
        Table::dropIfExists('users');
    }
}

controller HomeController {
    index(Request request) {
        return text(request.path());
    }

    int helper() {
        return 7;
    }
}
)";

    const auto result =
        Compiler{}.compile(
            source,
            "framework-valid.gnr"
        );

    if (!result.success()) {
        for (const auto& diagnostic :
             result.diagnostics) {
            std::cerr
                << diagnostic.code
                << ' '
                << diagnostic.message
                << '\n';
        }
    }

    assert(result.success());
    assert(result.validated);
    assert(!result.code.empty());

    const auto& syntax =
        result.validated->syntax();

    const auto user =
        std::find_if(
            syntax.declarations.begin(),
            syntax.declarations.end(),
            [](const auto& declaration) {
                return
                    declaration.kind ==
                        DeclarationKind::model &&
                    declaration.name == "User";
            }
        );

    assert(user != syntax.declarations.end());

    const auto field =
        [&](std::string_view name) {
            return std::find_if(
                user->fields.begin(),
                user->fields.end(),
                [&](const auto& item) {
                    return item.name == name;
                }
            );
        };

    const auto created_at =
        field("created_at");
    const auto updated_at =
        field("updated_at");
    const auto deleted_at =
        field("deleted_at");

    assert(created_at != user->fields.end());
    assert(updated_at != user->fields.end());
    assert(deleted_at != user->fields.end());

    assert(created_at->type.name == "string");
    assert(created_at->type.optional);
    assert(updated_at->type.optional);
    assert(deleted_at->type.optional);

    assert(
        result.code.find(
            "gungnir::Field<std::optional<gungnir::String>> created_at"
        ) != std::string::npos
    );

    assert(
        result.code.find(
            "gungnir::Field<std::optional<gungnir::String>> deleted_at"
        ) != std::string::npos
    );
}

void invalid_framework_contracts_fail_semantically() {
    reject(
        "middleware Broken { handle(Request request) { "
        "return text('no'); } }",
        "GNR2301"
    );

    reject(
        "middleware Broken { int handle(Request request, Next next) { "
        "return 1; } }",
        "GNR2301"
    );

    reject(
        "migration Broken { private up() {} down() {} }",
        "GNR2302"
    );

    reject(
        "event Created { int id; } "
        "listener Broken { handle(Created? event) {} }",
        "GNR2303"
    );

    reject(
        "job Broken { handle(int value) {} }",
        "GNR2303"
    );

    reject(
        "model User { } "
        "policy Broken { bool view(User actor, User resource) { "
        "return true; } }",
        "GNR2304"
    );

    reject(
        "event Broken { int id; handle() {} }",
        "GNR2305"
    );

    reject(
        "model User { } "
        "notification Broken { private via(User recipient) { "
        "return ['mail']; } }",
        "GNR2306"
    );

    reject(
        "model User { } "
        "notification Broken { via(User recipient) { "
        "return ['mail']; } "
        "toMail(User recipient) { return 1; } }",
        "GNR2306"
    );

    reject(
        "model User { } "
        "notification Broken { via(User recipient) { "
        "return ['database']; } "
        "toDatabase(User recipient) { return 'wrong'; } }",
        "GNR2306"
    );

    reject(
        "mail Broken { "
        "subject() { return 'subject'; } "
        "html() { return '<b>x</b>'; } "
        "content() { return response('x'); } }",
        "GNR2307"
    );

    reject(
        "model Broken { table = ''; }",
        "GNR2308"
    );

    reject(
        "model Broken { "
        "softDeletes = true; "
        "string deleted_at; }",
        "GNR2308"
    );

    reject(
        "model Broken { "
        "primaryKey = 'uuid'; "
        "incrementing = true; "
        "string uuid; }",
        "GNR2308"
    );
}

} // namespace

int main() {
    valid_framework_contracts_compile();
    invalid_framework_contracts_fail_semantically();
}
