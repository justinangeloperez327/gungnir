#include <gungnir/language/compiler.hpp>
#include <algorithm>
#include <cassert>
#include <string>

namespace {
void rejects(const std::string& source) {
    for (const bool validation_only : {false, true}) {
        gungnir::language::CompilerOptions options;
        options.validate_only = validation_only;
        const auto result = gungnir::language::Compiler{}.compile(source,"model-contract.gnr",options);
        assert(!result.success());
        assert(std::any_of(result.diagnostics.begin(),result.diagnostics.end(),[](const auto& diagnostic) {
            return diagnostic.code == "GNR2308" || diagnostic.code == "GNR2208";
        }));
    }
}
}

int main() {
    rejects("model User { perPage = 0; }");
    rejects("model User { perPage = -1; }");
    rejects("model User { perPage = 1.5; }");
    rejects("model User { guarded = ['id', 'id']; }");
    rejects("model User { guarded = [1]; }");
    rejects("model User { createdAt = ''; timestamps = true; }");
    rejects("model User { createdAt = 'same'; updatedAt = 'same'; timestamps = true; }");
    rejects("model User { dateFormat = 'unknown'; }");
    rejects("model User { attributes = {'score': 1, 'score': 2}; }");
    rejects("model User { attributes = [1, 2]; }");
    rejects("model User { casts = ['string']; }");
    rejects("model User { int score = 1; attributes = {'score': 2}; }");
    rejects("model User { casts = {'amount': 'decimal'}; attributes = {'amount': 'invalid'}; }");
    rejects("model User { casts = {'owner': 'uint64'}; attributes = {'owner': -1}; }");
    rejects("model User { List<string> tags; }");
    rejects("model User { string save; }");
    rejects("model User { string query; }");
    rejects("model User { string hidden; }");
    rejects("model User { string casts; }");
    rejects("model User { appends = ['missing']; }");
    rejects("model User { appends = ['name']; string name(int input) { return 'name'; } }");
    rejects("model User { string name; appends = ['label']; string label() { name = 'changed'; return name; } }");
    rejects("model User { appends = ['items']; List<string> items() { return ['one']; } }");
    rejects("model User { appends = ['label']; string helper() { return 'ok'; } string label() { return helper(); } }");
    rejects("model User { withCount = ['posts']; int posts_count; posts() { return hasMany<Post>(); } } model Post {}");
    rejects("model User { posts() { return hasMany<Post>(); } } model Post {} function void invalid(User input) { let user = input; user.posts_count = 3; }");
    rejects("model User { with = ['missing']; }");
    rejects("model User { withCount = ['missing']; }");
    rejects("model User { touches = ['missing']; }");
    rejects("model Parent {} model Child { touches = ['parent']; parent() { return belongsTo<Parent>(); } }");
    rejects("model Parent { timestamps = true; } model Child { touches = ['parents']; parents() { return hasMany<Parent>(); } }");
    const auto valid = gungnir::language::Compiler{}.compile(R"(
        model User {
            int id = 7; incrementing = false;
            guarded = ['id']; attributes = {'name': 'Ada'}; perPage = 20;
            timestamps = true; createdAt = 'created_on'; updatedAt = 'modified_on'; dateFormat = 'Y-m-d H:i:s.u';
            appends = ['label']; string label() { return name + '!'; }
        }
        function string show(User user) { return user.label(); }
    )");
    assert(valid.success());
    assert(valid.code.find("with_value") != std::string::npos);
    assert(valid.code.find("label() const") != std::string::npos);
    assert(gungnir::language::Compiler{}.compile("model TimestampKey { primaryKey = 'created_at'; timestamps = false; }").success());
    assert(gungnir::language::Compiler{}.compile("model User { appends = ['label', 'name']; string name() { return 'Ada'; } string label() { return name(); } }").success());
}
