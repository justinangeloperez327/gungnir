#include <gungnir/language/compiler.hpp>
#include <gungnir/language/cpp_ir.hpp>
#include <cassert>
#include <algorithm>
#include <string>
#include <vector>

using namespace gungnir::language;

static void rejects(const std::string& source, const std::string& code) {
    Compiler compiler;
    const auto full = compiler.compile(source, "invalid.gnr");
    CompilerOptions options;
    options.validate_only = true;
    const auto check = compiler.compile(source, "invalid.gnr", options);
    assert(!full.success() && !check.success() && full.code.empty());
    assert(full.diagnostics.size() == check.diagnostics.size());
    assert(std::any_of(full.diagnostics.begin(), full.diagnostics.end(),
        [&](const auto& diagnostic) { return diagnostic.code == code; }));
    for (std::size_t i = 0; i < full.diagnostics.size(); ++i) {
        assert(full.diagnostics[i].code == check.diagnostics[i].code);
        assert(full.diagnostics[i].location.line == check.diagnostics[i].location.line);
        assert(full.diagnostics[i].span.valid);
    }
}

int main() {
    Compiler compiler;
    const std::string source = R"(
        model User {
            profile() { return hasOne<Profile>(); }
            posts() { return hasMany<Post>(); }
            roles() { return belongsToMany<Role>(); }
            comment() { return hasOneThrough<Comment, Post>(); }
            comments() { return hasManyThrough<Comment, Post>(); }
        }
        model Post {
            fillable = ["user_id"];
            author() { return belongsTo<User>("user_id"); }
            comments() { return hasMany<Comment>(); }
        }
        model Profile {} model Role {} model Comment {}
        function Collection<User> users() { return User::with("posts.comments").orderBy("id", "desc").get(); }
        function Page<User> page() { return User::paginate(1, 20); }
    )";
    const auto result = compiler.compile(source, "relationships.gnr");
    assert(result.success() && result.validated);
    const auto& project = *result.validated;
    assert(project.syntax().declarations[0].relationships.size() == 5);
    assert(project.declarations()[0].relationships.size() == 5);
    const auto& relation = project.declarations()[0].relationships[1];
    assert(relation.keys == std::vector<std::string>({"user_id", "id"}));
    assert(project.types()[project.symbols()[relation.field].type].name == "HasMany");
    const auto& post = project.syntax().declarations[1];
    assert(std::any_of(post.fields.begin(), post.fields.end(), [](const auto& field) {
        return field.name == "user_id" && field.type.name == "int";
    }));
    const auto ir = CppIrLowerer{}.lower(project);
    assert(CppIrVerifier{}.verify(ir).success());
    assert(std::any_of(ir.interface_declarations.begin(), ir.interface_declarations.end(), [](const auto& declaration) {
        return declaration.kind == CppIrDeclarationKind::model_metadata && declaration.name == "User";
    }));
    assert(compiler.compile(source, "relationships.gnr").code == result.code);

    const auto custom = compiler.compile(R"(
        model Account {
            primaryKey = "uuid"; incrementing = false; casts = {"uuid": "string"};
            invoices() { return hasMany<Invoice>(foreignKey: "account_uuid"); }
        }
        model Invoice { account() { return belongsTo<Account>("account_uuid"); } }
        function Collection<Invoice> invoices(string uuid) { return Account::findOrFail(uuid).invoices().get(); }
    )");
    assert(custom.success());
    const auto self = compiler.compile(R"(
        model Node {
            int? parent_id;
            parent() { return belongsTo<Node>(); }
            children() { return hasMany<Node>("parent_id"); }
        }
    )");
    assert(self.success());

    std::vector<SourceFile> files{
        {"user.gnr", "user", "import post as Posts; model User { posts() { return hasMany<Posts::Post>(); } }"},
        {"post.gnr", "post", "model Post {}"}
    };
    const auto modules = compiler.compile_sources(files);
    assert(modules.success());
    std::reverse(files.begin(), files.end());
    assert(compiler.compile_sources(files).code == modules.code);

    rejects("model User { posts() { return hasMany<Unknown>(); } }", "GNR2203");
    rejects("event Post {} model User { posts() { return hasMany<Post>(); } }", "GNR2310");
    rejects("model Post {} model User { posts() { return hasMany<Post?>(); } }", "GNR2310");
    rejects("model Post {} model User { posts() { return hasMany<Post, User>(); } }", "GNR2310");
    rejects("model Post {} model User { posts() { return hasManyThrough<Post>(); } }", "GNR2310");
    rejects("model Post {} model User { posts() { return hasMany<Post>(1); } }", "GNR2310");
    rejects("model Post {} model User { posts() { return hasMany<Post>(\"unsafe-key\"); } }", "GNR2310");
    rejects("model Post {} model User { posts() { return hasMany<Post>(unknownKey: \"user_id\"); } }", "GNR2310");
    rejects("model Post {} model User { posts() { return hasMany<Post>(foreignKey: \"user_id\", foreignKey: \"owner_id\"); } }", "GNR2310");
    rejects("model Post { string user_id; } model User { posts() { return hasMany<Post>(); } }", "GNR2310");
    rejects("model Post {} model User { int posts; posts() { return hasMany<Post>(); } }", "GNR2310");
    rejects("model Post {} model User { async posts() { return hasMany<Post>(); } }", "GNR2310");
    rejects("model Post {} model User { private posts() { return hasMany<Post>(); } }", "GNR2310");
    rejects("model User {} function Collection<User> users() { return User::where(1, true).get(); }", "GNR2201");
    rejects("model User {} function Collection<User> users() { return User::where(\"id\").get(); }", "GNR2209");
    rejects("model User {} function Collection<User> users() { return User::orderBy(\"id\", \"bad\").get(); }", "GNR2311");
    rejects("model User {} function Collection<User> users() { return User::where(\"id\", \"unsafe\", 1).get(); }", "GNR2311");
    rejects("model User {} function User user() { return User::findOrFail(\"wrong-key-type\"); }", "GNR2201");
    rejects("function int invalid(Query<int> query) { return query.count(); }", "GNR2311");
    rejects("model User {} function Page<User?> invalid() { return User::paginate(); }", "GNR2311");
    rejects("model User {} function bool invalid() { return User::save(); }", "GNR2311");
    rejects("model User {} function bool invalid() { const user = User::findOrFail(1); return user.save(); }", "GNR2208");
    rejects("model Post {} model User { posts() { return hasMany<Post>(); } } function int invalid() { return User::posts().count(); }", "GNR2311");
    rejects("model Role { string name; } model User { roles() { return belongsToMany<Role>(relatedKey: \"name\"); } } function int invalid() { let user = User::findOrFail(1); return user.roles.attach(1); }", "GNR2201");
}
