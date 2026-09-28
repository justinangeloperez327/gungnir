#include <gungnir/storage/local_disk.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <system_error>
#include <thread>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <gungnir/storage/error.hpp>

namespace gungnir::storage {

namespace {

constexpr std::size_t io_buffer_size =
    64 * 1024;

std::atomic<std::uint64_t>
temporary_sequence{0};

[[nodiscard]]
bool is_root_path(
    const std::filesystem::path& value
) {
    return
        value.empty() ||
        value == ".";
}

[[nodiscard]]
bool is_within(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate
) {
    auto root_part =
        root.begin();

    auto candidate_part =
        candidate.begin();

    while (
        root_part != root.end()
    ) {
        if (
            candidate_part ==
                candidate.end() ||
            *candidate_part !=
                *root_part
        ) {
            return false;
        }

        ++root_part;
        ++candidate_part;
    }

    return true;
}

[[nodiscard]]
std::string filesystem_error(
    std::string_view operation,
    const std::error_code& error
) {
    return
        std::string{operation} +
        ": " +
        error.message();
}

[[nodiscard]]
std::uint64_t process_id()
    noexcept {
#ifdef _WIN32
    return static_cast<std::uint64_t>(
        GetCurrentProcessId()
    );
#else
    return static_cast<std::uint64_t>(
        ::getpid()
    );
#endif
}

[[nodiscard]]
std::filesystem::path
temporary_path(
    const std::filesystem::path& target
) {
    const auto sequence =
        temporary_sequence.fetch_add(
            1,
            std::memory_order_relaxed
        );

    auto name =
        "." +
        target.filename().string() +
        ".gungnir-" +
        std::to_string(
            process_id()
        ) +
        "-" +
        std::to_string(sequence) +
        ".tmp";

    return
        target.parent_path() /
        std::move(name);
}

#ifdef _WIN32

class NativeFile {
public:
    explicit NativeFile(
        HANDLE handle =
            INVALID_HANDLE_VALUE
    ) noexcept
        : handle_(handle) {}

    ~NativeFile() {
        close();
    }

    NativeFile(
        const NativeFile&
    ) = delete;

    NativeFile& operator=(
        const NativeFile&
    ) = delete;

    NativeFile(
        NativeFile&& other
    ) noexcept
        : handle_(
            std::exchange(
                other.handle_,
                INVALID_HANDLE_VALUE
            )
          ) {}

    NativeFile& operator=(
        NativeFile&& other
    ) noexcept {
        if (this == &other) {
            return *this;
        }

        close();

        handle_ =
            std::exchange(
                other.handle_,
                INVALID_HANDLE_VALUE
            );

        return *this;
    }

    [[nodiscard]]
    HANDLE get() const noexcept {
        return handle_;
    }

    [[nodiscard]]
    explicit operator bool()
        const noexcept {
        return
            handle_ !=
            INVALID_HANDLE_VALUE;
    }

    void close() noexcept {
        if (*this) {
            CloseHandle(handle_);
            handle_ =
                INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE handle_;
};

[[nodiscard]]
std::string windows_error(
    std::string_view operation,
    DWORD code
) {
    return
        std::string{operation} +
        " (Windows error " +
        std::to_string(code) +
        ")";
}

[[nodiscard]]
NativeFile create_exclusive(
    const std::filesystem::path& path
) {
    const auto handle =
        CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL |
                FILE_ATTRIBUTE_TEMPORARY,
            nullptr
        );

    if (
        handle ==
        INVALID_HANDLE_VALUE
    ) {
        throw Error{
            windows_error(
                "Unable to create temporary storage object",
                GetLastError()
            )
        };
    }

    return NativeFile{handle};
}

void write_all(
    NativeFile& file,
    std::string_view contents,
    const CancellationToken& cancellation
) {
    std::size_t offset = 0;

    while (
        offset <
        contents.size()
    ) {
        cancellation.throw_if_cancelled();

        const auto remaining =
            contents.size() -
            offset;

        const auto chunk =
            static_cast<DWORD>(
                std::min<std::size_t>(
                    remaining,
                    static_cast<std::size_t>(
                        std::numeric_limits<
                            DWORD
                        >::max()
                    )
                )
            );

        DWORD written = 0;

        if (
            !WriteFile(
                file.get(),
                contents.data() +
                    offset,
                chunk,
                &written,
                nullptr
            ) ||
            written == 0
        ) {
            throw Error{
                windows_error(
                    "Unable to write temporary storage object",
                    GetLastError()
                )
            };
        }

        offset +=
            static_cast<std::size_t>(
                written
            );
    }

    cancellation.throw_if_cancelled();

    if (!FlushFileBuffers(file.get())) {
        throw Error{
            windows_error(
                "Unable to flush temporary storage object",
                GetLastError()
            )
        };
    }
}

[[nodiscard]]
std::optional<std::string>
read_file(
    const std::filesystem::path& path,
    const CancellationToken& cancellation
) {
    const auto handle =
        CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ |
                FILE_SHARE_WRITE |
                FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL |
                FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr
        );

    if (
        handle ==
        INVALID_HANDLE_VALUE
    ) {
        const auto error =
            GetLastError();

        if (
            error ==
                ERROR_FILE_NOT_FOUND ||
            error ==
                ERROR_PATH_NOT_FOUND
        ) {
            return std::nullopt;
        }

        throw Error{
            windows_error(
                "Unable to read storage object",
                error
            )
        };
    }

    NativeFile file{handle};

    BY_HANDLE_FILE_INFORMATION info{};

    if (
        !GetFileInformationByHandle(
            file.get(),
            &info
        )
    ) {
        throw Error{
            windows_error(
                "Unable to inspect storage object",
                GetLastError()
            )
        };
    }

    if (
        (
            info.dwFileAttributes &
            FILE_ATTRIBUTE_REPARSE_POINT
        ) != 0
    ) {
        throw InvalidPath{
            path.generic_string()
        };
    }

    std::string result;
    std::array<char, io_buffer_size>
        buffer{};

    while (true) {
        cancellation.throw_if_cancelled();

        DWORD read = 0;

        if (
            !ReadFile(
                file.get(),
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()
                ),
                &read,
                nullptr
            )
        ) {
            throw Error{
                windows_error(
                    "Unable to read storage object",
                    GetLastError()
                )
            };
        }

        if (read == 0) {
            break;
        }

        result.append(
            buffer.data(),
            static_cast<std::size_t>(
                read
            )
        );
    }

    cancellation.throw_if_cancelled();

    return result;
}

void replace_file(
    const std::filesystem::path& temporary,
    const std::filesystem::path& target
) {
    if (
        !MoveFileExW(
            temporary.c_str(),
            target.c_str(),
            MOVEFILE_REPLACE_EXISTING |
                MOVEFILE_WRITE_THROUGH
        )
    ) {
        throw Error{
            windows_error(
                "Unable to replace storage object atomically",
                GetLastError()
            )
        };
    }
}

#else

class NativeFile {
public:
    explicit NativeFile(
        int descriptor = -1
    ) noexcept
        : descriptor_(descriptor) {}

    ~NativeFile() {
        close();
    }

    NativeFile(
        const NativeFile&
    ) = delete;

    NativeFile& operator=(
        const NativeFile&
    ) = delete;

    NativeFile(
        NativeFile&& other
    ) noexcept
        : descriptor_(
            std::exchange(
                other.descriptor_,
                -1
            )
          ) {}

    NativeFile& operator=(
        NativeFile&& other
    ) noexcept {
        if (this == &other) {
            return *this;
        }

        close();

        descriptor_ =
            std::exchange(
                other.descriptor_,
                -1
            );

        return *this;
    }

    [[nodiscard]]
    int get() const noexcept {
        return descriptor_;
    }

    [[nodiscard]]
    explicit operator bool()
        const noexcept {
        return descriptor_ >= 0;
    }

    void close() noexcept {
        if (*this) {
            static_cast<void>(
                ::close(descriptor_)
            );

            descriptor_ = -1;
        }
    }

private:
    int descriptor_;
};

[[nodiscard]]
NativeFile create_exclusive(
    const std::filesystem::path& path
) {
    int flags =
        O_WRONLY |
        O_CREAT |
        O_EXCL;

#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    const auto descriptor =
        ::open(
            path.c_str(),
            flags,
            static_cast<mode_t>(0600)
        );

    if (descriptor < 0) {
        throw Error{
            "Unable to create temporary storage object: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    return NativeFile{descriptor};
}

void write_all(
    NativeFile& file,
    std::string_view contents,
    const CancellationToken& cancellation
) {
    std::size_t offset = 0;

    while (
        offset <
        contents.size()
    ) {
        cancellation.throw_if_cancelled();

        const auto written =
            ::write(
                file.get(),
                contents.data() +
                    offset,
                contents.size() -
                    offset
            );

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw Error{
                "Unable to write temporary storage object: " +
                std::error_code{
                    errno,
                    std::generic_category()
                }.message()
            };
        }

        if (written == 0) {
            throw Error{
                "Unable to write temporary storage object"
            };
        }

        offset +=
            static_cast<std::size_t>(
                written
            );
    }

    cancellation.throw_if_cancelled();

    while (
        ::fsync(
            file.get()
        ) != 0
    ) {
        if (errno == EINTR) {
            continue;
        }

        throw Error{
            "Unable to flush temporary storage object: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }
}

[[nodiscard]]
std::optional<std::string>
read_file(
    const std::filesystem::path& path,
    const CancellationToken& cancellation
) {
    int flags = O_RDONLY;

#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    const auto descriptor =
        ::open(
            path.c_str(),
            flags
        );

    if (descriptor < 0) {
        if (
            errno == ENOENT ||
            errno == ENOTDIR
        ) {
            return std::nullopt;
        }

#ifdef ELOOP
        if (errno == ELOOP) {
            throw InvalidPath{
                path.generic_string()
            };
        }
#endif

        throw Error{
            "Unable to read storage object: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    NativeFile file{descriptor};

    struct stat info{};

    if (
        ::fstat(
            file.get(),
            &info
        ) != 0
    ) {
        throw Error{
            "Unable to inspect storage object: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    if (!S_ISREG(info.st_mode)) {
        return std::nullopt;
    }

    std::string result;

    if (info.st_size > 0) {
        result.reserve(
            static_cast<std::size_t>(
                info.st_size
            )
        );
    }

    std::array<char, io_buffer_size>
        buffer{};

    while (true) {
        cancellation.throw_if_cancelled();

        const auto count =
            ::read(
                file.get(),
                buffer.data(),
                buffer.size()
            );

        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw Error{
                "Unable to read storage object: " +
                std::error_code{
                    errno,
                    std::generic_category()
                }.message()
            };
        }

        if (count == 0) {
            break;
        }

        result.append(
            buffer.data(),
            static_cast<std::size_t>(
                count
            )
        );
    }

    cancellation.throw_if_cancelled();

    return result;
}

void replace_file(
    const std::filesystem::path& temporary,
    const std::filesystem::path& target
) {
    if (
        ::rename(
            temporary.c_str(),
            target.c_str()
        ) != 0
    ) {
        throw Error{
            "Unable to replace storage object atomically: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }
}

#endif

} // namespace

LocalDisk::LocalDisk(
    std::filesystem::path root
) {
    if (root.empty()) {
        throw InvalidPath{
            root.generic_string()
        };
    }

    std::error_code error;

    auto absolute =
        std::filesystem::absolute(
            std::move(root),
            error
        );

    if (error) {
        throw Error{
            filesystem_error(
                "Unable to resolve storage root",
                error
            )
        };
    }

    const auto initial_status =
        std::filesystem::symlink_status(
            absolute,
            error
        );

    if (
        !error &&
        std::filesystem::is_symlink(
            initial_status
        )
    ) {
        throw InvalidPath{
            absolute.generic_string()
        };
    }

    error.clear();

    std::filesystem::create_directories(
        absolute,
        error
    );

    if (error) {
        throw Error{
            filesystem_error(
                "Unable to create storage root",
                error
            )
        };
    }

    root_ =
        std::filesystem::weakly_canonical(
            absolute,
            error
        );

    if (error) {
        throw Error{
            filesystem_error(
                "Unable to canonicalize storage root",
                error
            )
        };
    }
}

const std::filesystem::path&
LocalDisk::root() const noexcept {
    return root_;
}

std::filesystem::path
LocalDisk::validate_relative(
    std::string_view value
) const {
    if (
        value.find('\0') !=
        std::string_view::npos
    ) {
        throw InvalidPath{
            std::string{value}
        };
    }

    const std::filesystem::path
        input{
            std::string{value}
        };

    if (
        input.is_absolute() ||
        input.has_root_name() ||
        input.has_root_directory()
    ) {
        throw InvalidPath{
            std::string{value}
        };
    }

    for (
        const auto& part :
        input
    ) {
        if (part == "..") {
            throw InvalidPath{
                std::string{value}
            };
        }
    }

    return input.lexically_normal();
}

void LocalDisk::reject_symlinks(
    const std::filesystem::path& relative
) const {
    auto current = root_;

    for (
        const auto& part :
        relative
    ) {
        if (
            part.empty() ||
            part == "."
        ) {
            continue;
        }

        current /= part;

        std::error_code error;

        const auto status =
            std::filesystem::symlink_status(
                current,
                error
            );

        if (error) {
            if (
                error ==
                std::errc::
                    no_such_file_or_directory
            ) {
                continue;
            }

            throw Error{
                filesystem_error(
                    "Unable to inspect storage path",
                    error
                )
            };
        }

        if (
            std::filesystem::is_symlink(
                status
            )
        ) {
            throw InvalidPath{
                relative.generic_string()
            };
        }
    }
}

std::filesystem::path
LocalDisk::resolve(
    std::string_view value
) const {
    const auto relative =
        validate_relative(value);

    reject_symlinks(relative);

    std::error_code error;

    const auto candidate =
        std::filesystem::weakly_canonical(
            root_ / relative,
            error
        );

    if (error) {
        throw Error{
            filesystem_error(
                "Unable to resolve storage path",
                error
            )
        };
    }

    if (
        !is_within(
            root_,
            candidate
        )
    ) {
        throw InvalidPath{
            std::string{value}
        };
    }

    return candidate;
}

std::filesystem::path
LocalDisk::prepare_destination(
    std::string_view value
) const {
    const auto relative =
        validate_relative(value);

    if (is_root_path(relative)) {
        throw InvalidPath{
            std::string{value}
        };
    }

    reject_symlinks(relative);

    const auto candidate =
        root_ / relative;

    const auto parent =
        candidate.parent_path();

    if (!parent.empty()) {
        std::error_code error;

        std::filesystem::create_directories(
            parent,
            error
        );

        if (error) {
            throw Error{
                filesystem_error(
                    "Unable to create storage directory",
                    error
                )
            };
        }
    }

    reject_symlinks(relative);

    std::error_code error;

    const auto canonical =
        std::filesystem::weakly_canonical(
            candidate,
            error
        );

    if (error) {
        throw Error{
            filesystem_error(
                "Unable to resolve storage destination",
                error
            )
        };
    }

    if (
        !is_within(
            root_,
            canonical
        )
    ) {
        throw InvalidPath{
            std::string{value}
        };
    }

    const auto status =
        std::filesystem::symlink_status(
            canonical,
            error
        );

    if (
        !error &&
        std::filesystem::is_symlink(
            status
        )
    ) {
        throw InvalidPath{
            std::string{value}
        };
    }

    return canonical;
}

bool LocalDisk::exists(
    std::string_view path
) const {
    return exists(
        path,
        CancellationToken{}
    );
}

bool LocalDisk::exists(
    std::string_view path,
    const CancellationToken& cancellation
) const {
    cancellation.throw_if_cancelled();

    const auto target =
        resolve(path);

    std::error_code error;

    const auto result =
        std::filesystem::is_regular_file(
            target,
            error
        );

    if (error) {
        if (
            error ==
            std::errc::
                no_such_file_or_directory
        ) {
            return false;
        }

        throw Error{
            filesystem_error(
                "Unable to inspect storage object",
                error
            )
        };
    }

    cancellation.throw_if_cancelled();

    return result;
}

std::optional<std::string>
LocalDisk::get(
    std::string_view path
) const {
    return get(
        path,
        CancellationToken{}
    );
}

std::optional<std::string>
LocalDisk::get(
    std::string_view path,
    const CancellationToken& cancellation
) const {
    cancellation.throw_if_cancelled();

    return read_file(
        resolve(path),
        cancellation
    );
}

void LocalDisk::atomic_put(
    const std::filesystem::path& target,
    std::string_view original_path,
    std::string_view contents,
    const CancellationToken& cancellation
) const {
    cancellation.throw_if_cancelled();

    std::optional<
        std::filesystem::path
    > temporary;

    try {
        NativeFile output;

        for (
            int attempt = 0;
            attempt < 32;
            ++attempt
        ) {
            const auto candidate =
                temporary_path(target);

            try {
                output =
                    create_exclusive(
                        candidate
                    );

                temporary = candidate;
                break;
            } catch (const Error&) {
                if (attempt == 31) {
                    throw;
                }
            }
        }

        if (
            !temporary ||
            !output
        ) {
            throw Error{
                "Unable to allocate temporary storage object: " +
                std::string{original_path}
            };
        }

        write_all(
            output,
            contents,
            cancellation
        );

        output.close();

        cancellation.throw_if_cancelled();

        replace_file(
            *temporary,
            target
        );

        temporary.reset();
    } catch (...) {
        if (temporary) {
            std::error_code ignored;

            std::filesystem::remove(
                *temporary,
                ignored
            );
        }

        throw;
    }
}

void LocalDisk::put(
    std::string path,
    std::string contents
) {
    put(
        std::move(path),
        std::move(contents),
        CancellationToken{}
    );
}

void LocalDisk::put(
    std::string path,
    std::string contents,
    const CancellationToken& cancellation
) {
    const auto target =
        prepare_destination(path);

    atomic_put(
        target,
        path,
        contents,
        cancellation
    );
}

bool LocalDisk::remove(
    std::string_view path
) {
    return remove(
        path,
        CancellationToken{}
    );
}

bool LocalDisk::remove(
    std::string_view path,
    const CancellationToken& cancellation
) {
    cancellation.throw_if_cancelled();

    const auto relative =
        validate_relative(path);

    if (is_root_path(relative)) {
        throw InvalidPath{
            std::string{path}
        };
    }

    const auto target =
        resolve(path);

    std::error_code error;

    const auto removed =
        std::filesystem::remove(
            target,
            error
        );

    if (error) {
        throw Error{
            filesystem_error(
                "Unable to remove storage object",
                error
            )
        };
    }

    return removed;
}

bool LocalDisk::move(
    std::string_view from,
    std::string_view to
) {
    return move(
        from,
        to,
        CancellationToken{}
    );
}

bool LocalDisk::move(
    std::string_view from,
    std::string_view to,
    const CancellationToken& cancellation
) {
    cancellation.throw_if_cancelled();

    const auto source_relative =
        validate_relative(from);

    if (is_root_path(source_relative)) {
        throw InvalidPath{
            std::string{from}
        };
    }

    const auto source =
        resolve(from);

    std::error_code error;

    if (
        !std::filesystem::is_regular_file(
            source,
            error
        )
    ) {
        if (
            !error ||
            error ==
                std::errc::
                    no_such_file_or_directory
        ) {
            return false;
        }

        throw Error{
            filesystem_error(
                "Unable to inspect storage source",
                error
            )
        };
    }

    cancellation.throw_if_cancelled();

    const auto destination =
        prepare_destination(to);

    std::filesystem::rename(
        source,
        destination,
        error
    );

    if (error) {
        throw Error{
            filesystem_error(
                "Unable to move storage object",
                error
            )
        };
    }

    return true;
}

bool LocalDisk::copy(
    std::string_view from,
    std::string_view to
) {
    return copy(
        from,
        to,
        CancellationToken{}
    );
}

bool LocalDisk::copy(
    std::string_view from,
    std::string_view to,
    const CancellationToken& cancellation
) {
    cancellation.throw_if_cancelled();

    auto contents =
        get(
            from,
            cancellation
        );

    if (!contents) {
        return false;
    }

    put(
        std::string{to},
        std::move(*contents),
        cancellation
    );

    return true;
}

std::uintmax_t LocalDisk::size(
    std::string_view path
) const {
    return size(
        path,
        CancellationToken{}
    );
}

std::uintmax_t LocalDisk::size(
    std::string_view path,
    const CancellationToken& cancellation
) const {
    cancellation.throw_if_cancelled();

    const auto target =
        resolve(path);

    std::error_code error;

    if (
        !std::filesystem::is_regular_file(
            target,
            error
        )
    ) {
        throw NotFound{
            std::string{path}
        };
    }

    const auto result =
        std::filesystem::file_size(
            target,
            error
        );

    if (error) {
        throw Error{
            filesystem_error(
                "Unable to inspect storage object size",
                error
            )
        };
    }

    cancellation.throw_if_cancelled();

    return result;
}

std::vector<std::string>
LocalDisk::files(
    std::string_view directory
) const {
    return files(
        directory,
        CancellationToken{}
    );
}

std::vector<std::string>
LocalDisk::files(
    std::string_view directory,
    const CancellationToken& cancellation
) const {
    cancellation.throw_if_cancelled();

    const auto target =
        resolve(directory);

    std::error_code error;

    if (
        !std::filesystem::exists(
            target,
            error
        )
    ) {
        return {};
    }

    if (
        error ||
        !std::filesystem::is_directory(
            target,
            error
        )
    ) {
        if (error) {
            throw Error{
                filesystem_error(
                    "Unable to inspect storage directory",
                    error
                )
            };
        }

        return {};
    }

    std::vector<std::string> result;

    std::filesystem::directory_iterator
        iterator{
            target,
            std::filesystem::
                directory_options::
                    skip_permission_denied,
            error
        };

    if (error) {
        throw Error{
            filesystem_error(
                "Unable to enumerate storage directory",
                error
            )
        };
    }

    for (
        const auto& entry :
        iterator
    ) {
        cancellation.throw_if_cancelled();

        const auto status =
            entry.symlink_status(
                error
            );

        if (error) {
            throw Error{
                filesystem_error(
                    "Unable to inspect storage directory entry",
                    error
                )
            };
        }

        if (
            std::filesystem::
                is_regular_file(
                    status
                )
        ) {
            result.push_back(
                std::filesystem::relative(
                    entry.path(),
                    root_
                ).generic_string()
            );
        }
    }

    std::sort(
        result.begin(),
        result.end()
    );

    cancellation.throw_if_cancelled();

    return result;
}

} // namespace gungnir::storage
