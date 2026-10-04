#include <gungnir/storage/local_disk.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <limits>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winternl.h>
#else
#include <dirent.h>
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
std::string temporary_name(
    std::string_view filename
) {
    const auto sequence =
        temporary_sequence.fetch_add(
            1,
            std::memory_order_relaxed
        );

    return
        "." +
        std::string{filename} +
        ".gungnir-" +
        std::to_string(
            process_id()
        ) +
        "-" +
        std::to_string(sequence) +
        ".tmp";
}

[[nodiscard]]
bool is_temporary_artifact(
    std::string_view name
) noexcept {
    constexpr std::string_view
        marker{".gungnir-"};

    constexpr std::string_view
        suffix{".tmp"};

    return
        !name.empty() &&
        name.front() == '.' &&
        name.find(marker) !=
            std::string_view::npos &&
        name.size() >
            suffix.size() &&
        name.ends_with(suffix);
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

void flush_directory_best_effort(
    HANDLE handle
) {
    if (
        FlushFileBuffers(handle)
    ) {
        return;
    }

    const auto error =
        GetLastError();

    if (
        error ==
            ERROR_INVALID_HANDLE ||
        error ==
            ERROR_ACCESS_DENIED
    ) {
        return;
    }

    throw Error{
        windows_error(
            "Unable to flush storage directory metadata",
            error
        )
    };
}

[[nodiscard]]
std::uint64_t file_index(
    const BY_HANDLE_FILE_INFORMATION& info
) noexcept {
    return
        (
            static_cast<std::uint64_t>(
                info.nFileIndexHigh
            ) <<
            32U
        ) |
        static_cast<std::uint64_t>(
            info.nFileIndexLow
        );
}

[[nodiscard]]
BY_HANDLE_FILE_INFORMATION
file_information(
    HANDLE handle,
    std::string_view operation
) {
    BY_HANDLE_FILE_INFORMATION info{};

    if (
        !GetFileInformationByHandle(
            handle,
            &info
        )
    ) {
        throw Error{
            windows_error(
                operation,
                GetLastError()
            )
        };
    }

    return info;
}

[[nodiscard]]
std::wstring final_path(
    HANDLE handle
) {
    std::vector<wchar_t>
        buffer(1024);

    while (true) {
        const auto length =
            GetFinalPathNameByHandleW(
                handle,
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()
                ),
                FILE_NAME_NORMALIZED |
                    VOLUME_NAME_DOS
            );

        if (length == 0) {
            throw Error{
                windows_error(
                    "Unable to resolve storage handle path",
                    GetLastError()
                )
            };
        }

        if (
            length <
            buffer.size()
        ) {
            return std::wstring{
                buffer.data(),
                length
            };
        }

        buffer.resize(
            static_cast<std::size_t>(
                length
            ) +
            1
        );
    }
}

[[nodiscard]]
std::wstring normalized_native_path(
    std::wstring value
) {
    constexpr std::wstring_view
        unc_prefix{
            L"\\\\?\\UNC\\"
        };

    constexpr std::wstring_view
        device_prefix{
            L"\\\\?\\"
        };

    if (
        value.starts_with(
            unc_prefix
        )
    ) {
        value =
            L"\\" +
            value.substr(
                unc_prefix.size()
            );
    } else if (
        value.starts_with(
            device_prefix
        )
    ) {
        value.erase(
            0,
            device_prefix.size()
        );
    }

    std::replace(
        value.begin(),
        value.end(),
        L'/',
        L'\\'
    );

    while (
        value.size() > 3 &&
        value.back() == L'\\'
    ) {
        value.pop_back();
    }

    for (auto& character : value) {
        character =
            static_cast<wchar_t>(
                std::towlower(
                    character
                )
            );
    }

    return value;
}

[[nodiscard]]
bool native_path_within(
    const std::wstring& root,
    const std::wstring& candidate
) {
    if (
        candidate.size() <
        root.size() ||
        candidate.compare(
            0,
            root.size(),
            root
        ) != 0
    ) {
        return false;
    }

    return
        candidate.size() ==
            root.size() ||
        candidate[root.size()] ==
            L'\\';
}

void reject_reparse(
    HANDLE handle,
    std::string_view path
) {
    const auto info =
        file_information(
            handle,
            "Unable to inspect storage handle"
        );

    if (
        (
            info.dwFileAttributes &
            FILE_ATTRIBUTE_REPARSE_POINT
        ) != 0
    ) {
        throw InvalidPath{
            std::string{path}
        };
    }
}

[[nodiscard]]
NativeFile open_root(
    const std::filesystem::path& root,
    std::uint64_t identity_a,
    std::uint64_t identity_b
) {
    const auto handle =
        CreateFileW(
            root.c_str(),
            FILE_LIST_DIRECTORY |
                FILE_READ_ATTRIBUTES |
                FILE_TRAVERSE,
            FILE_SHARE_READ |
                FILE_SHARE_WRITE |
                FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS |
                FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr
        );

    if (
        handle ==
        INVALID_HANDLE_VALUE
    ) {
        throw Error{
            windows_error(
                "Unable to open storage root",
                GetLastError()
            )
        };
    }

    NativeFile result{handle};

    reject_reparse(
        result.get(),
        root.generic_string()
    );

    const auto info =
        file_information(
            result.get(),
            "Unable to inspect storage root"
        );

    if (
        static_cast<std::uint64_t>(
            info.dwVolumeSerialNumber
        ) != identity_a ||
        file_index(info) !=
            identity_b
    ) {
        throw InvalidPath{
            root.generic_string()
        };
    }

    return result;
}

[[nodiscard]]
NativeFile open_directory_checked(
    const std::filesystem::path& path,
    const std::wstring& root_final,
    DWORD access =
        FILE_LIST_DIRECTORY |
        FILE_READ_ATTRIBUTES |
        FILE_TRAVERSE
) {
    const auto handle =
        CreateFileW(
            path.c_str(),
            access,
            FILE_SHARE_READ |
                FILE_SHARE_WRITE |
                FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS |
                FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr
        );

    if (
        handle ==
        INVALID_HANDLE_VALUE
    ) {
        throw Error{
            windows_error(
                "Unable to open storage directory",
                GetLastError()
            )
        };
    }

    NativeFile result{handle};

    reject_reparse(
        result.get(),
        path.generic_string()
    );

    const auto actual =
        normalized_native_path(
            final_path(
                result.get()
            )
        );

    if (
        !native_path_within(
            root_final,
            actual
        )
    ) {
        throw InvalidPath{
            path.generic_string()
        };
    }

    return result;
}

[[nodiscard]]
std::optional<NativeFile>
open_regular_checked(
    const std::filesystem::path& path,
    const std::wstring& root_final,
    DWORD access
) {
    const auto handle =
        CreateFileW(
            path.c_str(),
            access |
                FILE_READ_ATTRIBUTES,
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
                "Unable to open storage object",
                error
            )
        };
    }

    NativeFile result{handle};

    reject_reparse(
        result.get(),
        path.generic_string()
    );

    const auto actual =
        normalized_native_path(
            final_path(
                result.get()
            )
        );

    if (
        !native_path_within(
            root_final,
            actual
        )
    ) {
        throw InvalidPath{
            path.generic_string()
        };
    }

    const auto info =
        file_information(
            result.get(),
            "Unable to inspect storage object"
        );

    if (
        (
            info.dwFileAttributes &
            FILE_ATTRIBUTE_DIRECTORY
        ) != 0
    ) {
        return std::nullopt;
    }

    return result;
}

void mark_delete(
    HANDLE handle
) {
    FILE_DISPOSITION_INFO info{};
    info.DeleteFile = TRUE;

    if (
        !SetFileInformationByHandle(
            handle,
            FileDispositionInfo,
            &info,
            sizeof(info)
        )
    ) {
        throw Error{
            windows_error(
                "Unable to remove storage object",
                GetLastError()
            )
        };
    }
}

void rename_handle(
    HANDLE source,
    HANDLE destination_directory,
    std::wstring_view destination_name,
    bool replace
) {
    // Use the native rename service to preserve the checked destination
    // directory handle without resolving a destination pathname again.
    using SetInformation = NTSTATUS (NTAPI*)(
        HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS
    );
    using StatusError = ULONG (NTAPI*)(NTSTATUS);
    static const auto module = GetModuleHandleW(L"ntdll.dll");
    static const auto set_information = module ? reinterpret_cast<SetInformation>(
        GetProcAddress(module, "NtSetInformationFile")
    ) : nullptr;
    static const auto status_error = module ? reinterpret_cast<StatusError>(
        GetProcAddress(module, "RtlNtStatusToDosError")
    ) : nullptr;
    if (!set_information || !status_error) {
        throw Error{"Windows handle-based storage renames are unavailable"};
    }
    if (destination_name.size() >
        (std::numeric_limits<ULONG>::max() - sizeof(FILE_RENAME_INFO)) / sizeof(wchar_t)) {
        throw Error{"Storage destination filename is too long"};
    }
    const auto bytes =
        destination_name.size() *
        sizeof(wchar_t);

    std::vector<std::byte> buffer(
        sizeof(FILE_RENAME_INFO) +
        bytes
    );

    auto* info =
        reinterpret_cast<
            FILE_RENAME_INFO*
        >(buffer.data());

    info->ReplaceIfExists =
        replace
            ? TRUE
            : FALSE;

    info->RootDirectory =
        destination_directory;

    info->FileNameLength =
        static_cast<DWORD>(
            bytes
        );

    std::memcpy(
        info->FileName,
        destination_name.data(),
        bytes
    );

    IO_STATUS_BLOCK completion{};
    // FileRenameInformation is class 10. winternl.h exposes only a subset
    // of FILE_INFORMATION_CLASS; its documented native layout matches
    // FILE_RENAME_INFO for this operation.
    const auto status = set_information(
        source, &completion, info, static_cast<ULONG>(buffer.size()),
        static_cast<FILE_INFORMATION_CLASS>(10)
    );
    if (status < 0) {
        throw Error{
            windows_error(
                "Unable to move storage object",
                status_error(status)
            )
        };
    }
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
                    "Unable to write storage object",
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

    if (
        !FlushFileBuffers(
            file.get()
        )
    ) {
        throw Error{
            windows_error(
                "Unable to flush storage object",
                GetLastError()
            )
        };
    }
}

[[nodiscard]]
std::string read_all(
    NativeFile& file,
    const CancellationToken& cancellation
) {
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
                ::close(
                    descriptor_
                )
            );

            descriptor_ = -1;
        }
    }

private:
    int descriptor_;
};

void sync_directory(
    int descriptor
) {
    while (
        ::fsync(
            descriptor
        ) != 0
    ) {
        if (errno == EINTR) {
            continue;
        }

        throw Error{
            "Unable to flush storage directory metadata: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }
}

[[nodiscard]]
int directory_flags() noexcept {
    int flags =
        O_RDONLY |
        O_DIRECTORY;

#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    return flags;
}

[[nodiscard]]
int file_read_flags() noexcept {
    int flags =
        O_RDONLY;

#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    return flags;
}

[[nodiscard]]
NativeFile open_root(
    const std::filesystem::path& root,
    std::uint64_t identity_a,
    std::uint64_t identity_b
) {
    const auto descriptor =
        ::open(
            root.c_str(),
            directory_flags()
        );

    if (descriptor < 0) {
        if (errno == ELOOP) {
            throw InvalidPath{
                root.generic_string()
            };
        }

        throw Error{
            "Unable to open storage root: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    NativeFile result{
        descriptor
    };

    struct stat info{};

    if (
        ::fstat(
            result.get(),
            &info
        ) != 0
    ) {
        throw Error{
            "Unable to inspect storage root: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    if (
        !S_ISDIR(
            info.st_mode
        ) ||
        static_cast<std::uint64_t>(
            info.st_dev
        ) != identity_a ||
        static_cast<std::uint64_t>(
            info.st_ino
        ) != identity_b
    ) {
        throw InvalidPath{
            root.generic_string()
        };
    }

    return result;
}

[[nodiscard]]
std::vector<std::string>
components(
    const std::filesystem::path& relative
) {
    std::vector<std::string>
        result;

    for (const auto& part : relative) {
        if (
            part.empty() ||
            part == "."
        ) {
            continue;
        }

        result.push_back(
            part.string()
        );
    }

    return result;
}

[[nodiscard]]
std::optional<NativeFile>
open_child_directory(
    int parent,
    std::string_view name,
    bool create
) {
    auto open_child =
        [&]() {
            return ::openat(
                parent,
                std::string{name}.c_str(),
                directory_flags()
            );
        };

    auto descriptor =
        open_child();

    if (
        descriptor < 0 &&
        errno == ENOENT &&
        create
    ) {
        bool created = false;

        if (
            ::mkdirat(
                parent,
                std::string{name}.c_str(),
                static_cast<mode_t>(
                    0700
                )
            ) == 0
        ) {
            created = true;
        } else if (errno != EEXIST) {
            throw Error{
                "Unable to create storage directory: " +
                std::error_code{
                    errno,
                    std::generic_category()
                }.message()
            };
        }

        if (created) {
            sync_directory(
                parent
            );
        }

        descriptor =
            open_child();
    }

    if (descriptor < 0) {
        const auto open_error =
            errno;

        if (
            open_error == ELOOP ||
            open_error == ENOTDIR
        ) {
            struct stat entry{};

            if (
                ::fstatat(
                    parent,
                    std::string{name}.c_str(),
                    &entry,
                    AT_SYMLINK_NOFOLLOW
                ) == 0
            ) {
                if (
                    S_ISLNK(entry.st_mode) ||
                    !S_ISDIR(entry.st_mode)
                ) {
                    throw InvalidPath{
                        std::string{name}
                    };
                }
            }
        }

        if (open_error == ENOENT) {
            return std::nullopt;
        }

        if (
            open_error == ELOOP ||
            open_error == ENOTDIR
        ) {
            throw InvalidPath{
                std::string{name}
            };
        }

        throw Error{
            "Unable to open storage directory: " +
            std::error_code{
                open_error,
                std::generic_category()
            }.message()
        };
    }

    NativeFile result{
        descriptor
    };

    struct stat info{};

    if (
        ::fstat(
            result.get(),
            &info
        ) != 0
    ) {
        throw Error{
            "Unable to inspect storage directory: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    if (!S_ISDIR(info.st_mode)) {
        throw InvalidPath{
            std::string{name}
        };
    }

    return result;
}

struct ParentHandle {
    NativeFile directory;
    std::string name;
};

[[nodiscard]]
std::optional<ParentHandle>
open_parent(
    NativeFile root,
    const std::filesystem::path& relative,
    bool create
) {
    auto parts =
        components(relative);

    if (parts.empty()) {
        return std::nullopt;
    }

    auto current =
        std::move(root);

    for (
        std::size_t index = 0;
        index + 1 <
            parts.size();
        ++index
    ) {
        auto child =
            open_child_directory(
                current.get(),
                parts[index],
                create
            );

        if (!child) {
            return std::nullopt;
        }

        current =
            std::move(*child);
    }

    return ParentHandle{
        std::move(current),
        std::move(
            parts.back()
        )
    };
}

[[nodiscard]]
std::optional<NativeFile>
open_directory(
    NativeFile root,
    const std::filesystem::path& relative,
    bool create
) {
    auto parts =
        components(relative);

    auto current =
        std::move(root);

    for (const auto& part : parts) {
        auto child =
            open_child_directory(
                current.get(),
                part,
                create
            );

        if (!child) {
            return std::nullopt;
        }

        current =
            std::move(*child);
    }

    return current;
}

[[nodiscard]]
std::optional<NativeFile>
open_regular(
    int parent,
    std::string_view name
) {
    const auto descriptor =
        ::openat(
            parent,
            std::string{name}.c_str(),
            file_read_flags()
        );

    if (descriptor < 0) {
        if (
            errno == ENOENT ||
            errno == ENOTDIR
        ) {
            return std::nullopt;
        }

        if (errno == ELOOP) {
            throw InvalidPath{
                std::string{name}
            };
        }

        throw Error{
            "Unable to open storage object: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    NativeFile result{
        descriptor
    };

    struct stat info{};

    if (
        ::fstat(
            result.get(),
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

    return result;
}

[[nodiscard]]
NativeFile create_exclusive_at(
    int parent,
    std::string_view name
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
        ::openat(
            parent,
            std::string{name}.c_str(),
            flags,
            static_cast<mode_t>(
                0600
            )
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

    return NativeFile{
        descriptor
    };
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
                "Unable to write storage object: " +
                std::error_code{
                    errno,
                    std::generic_category()
                }.message()
            };
        }

        if (written == 0) {
            throw Error{
                "Unable to write storage object"
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
            "Unable to flush storage object: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }
}

[[nodiscard]]
std::string read_all(
    NativeFile& file,
    const CancellationToken& cancellation
) {
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

#ifdef _WIN32
    const auto handle =
        CreateFileW(
            root_.c_str(),
            FILE_LIST_DIRECTORY |
                FILE_READ_ATTRIBUTES |
                FILE_TRAVERSE,
            FILE_SHARE_READ |
                FILE_SHARE_WRITE |
                FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS |
                FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr
        );

    if (
        handle ==
        INVALID_HANDLE_VALUE
    ) {
        throw Error{
            windows_error(
                "Unable to pin storage root",
                GetLastError()
            )
        };
    }

    NativeFile pinned{
        handle
    };

    reject_reparse(
        pinned.get(),
        root_.generic_string()
    );

    const auto info =
        file_information(
            pinned.get(),
            "Unable to inspect storage root"
        );

    root_identity_a_ =
        static_cast<std::uint64_t>(
            info.dwVolumeSerialNumber
        );

    root_identity_b_ =
        file_index(info);
#else
    const auto descriptor =
        ::open(
            root_.c_str(),
            directory_flags()
        );

    if (descriptor < 0) {
        throw Error{
            "Unable to pin storage root: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    NativeFile pinned{
        descriptor
    };

    struct stat info{};

    if (
        ::fstat(
            pinned.get(),
            &info
        ) != 0
    ) {
        throw Error{
            "Unable to inspect storage root: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    root_identity_a_ =
        static_cast<std::uint64_t>(
            info.st_dev
        );

    root_identity_b_ =
        static_cast<std::uint64_t>(
            info.st_ino
        );
#endif
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

    const auto relative =
        validate_relative(path);

    if (is_root_path(relative)) {
        return false;
    }

#ifdef _WIN32
    const auto root_handle =
        open_root(
            root_,
            root_identity_a_,
            root_identity_b_
        );

    const auto root_final =
        normalized_native_path(
            final_path(
                root_handle.get()
            )
        );

    const auto target =
        resolve(path);

    const auto file =
        open_regular_checked(
            target,
            root_final,
            GENERIC_READ
        );

    cancellation.throw_if_cancelled();

    return file.has_value();
#else
    auto parent =
        open_parent(
            open_root(
                root_,
                root_identity_a_,
                root_identity_b_
            ),
            relative,
            false
        );

    if (!parent) {
        return false;
    }

    const auto file =
        open_regular(
            parent->directory.get(),
            parent->name
        );

    cancellation.throw_if_cancelled();

    return file.has_value();
#endif
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

    const auto relative =
        validate_relative(path);

    if (is_root_path(relative)) {
        return std::nullopt;
    }

#ifdef _WIN32
    const auto root_handle =
        open_root(
            root_,
            root_identity_a_,
            root_identity_b_
        );

    const auto root_final =
        normalized_native_path(
            final_path(
                root_handle.get()
            )
        );

    const auto target =
        resolve(path);

    auto file =
        open_regular_checked(
            target,
            root_final,
            GENERIC_READ
        );

    if (!file) {
        return std::nullopt;
    }

    return read_all(
        *file,
        cancellation
    );
#else
    auto parent =
        open_parent(
            open_root(
                root_,
                root_identity_a_,
                root_identity_b_
            ),
            relative,
            false
        );

    if (!parent) {
        return std::nullopt;
    }

    auto file =
        open_regular(
            parent->directory.get(),
            parent->name
        );

    if (!file) {
        return std::nullopt;
    }

    return read_all(
        *file,
        cancellation
    );
#endif
}

void LocalDisk::atomic_put(
    const std::filesystem::path& target,
    std::string_view original_path,
    std::string_view contents,
    const CancellationToken& cancellation
) const {
    cancellation.throw_if_cancelled();

    const auto relative =
        validate_relative(
            original_path
        );

    if (is_root_path(relative)) {
        throw InvalidPath{
            std::string{
                original_path
            }
        };
    }

#ifdef _WIN32
    const auto root_handle =
        open_root(
            root_,
            root_identity_a_,
            root_identity_b_
        );

    const auto root_final =
        normalized_native_path(
            final_path(
                root_handle.get()
            )
        );

    auto parent =
        open_directory_checked(
            target.parent_path(),
            root_final,
            FILE_LIST_DIRECTORY |
                FILE_READ_ATTRIBUTES |
                FILE_TRAVERSE |
                FILE_ADD_FILE
        );

    const auto parent_final =
        normalized_native_path(
            final_path(
                parent.get()
            )
        );

    const auto temp_name =
        temporary_name(
            target.filename()
                .string()
        );

    const auto temporary =
        target.parent_path() /
        temp_name;

    const auto handle =
        CreateFileW(
            temporary.c_str(),
            GENERIC_WRITE |
                DELETE |
                FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ |
                FILE_SHARE_WRITE |
                FILE_SHARE_DELETE,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL |
                FILE_ATTRIBUTE_TEMPORARY |
                FILE_FLAG_OPEN_REPARSE_POINT,
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

    NativeFile output{
        handle
    };

    try {
        reject_reparse(
            output.get(),
            temporary.generic_string()
        );

        const auto actual =
            normalized_native_path(
                final_path(
                    output.get()
                )
            );

        const auto actual_parent =
            normalized_native_path(
                std::filesystem::path{
                    actual
                }.parent_path()
                    .wstring()
            );

        if (
            !native_path_within(
                root_final,
                actual
            ) ||
            actual_parent !=
                parent_final
        ) {
            mark_delete(
                output.get()
            );

            throw InvalidPath{
                std::string{
                    original_path
                }
            };
        }

        write_all(
            output,
            contents,
            cancellation
        );

        cancellation.throw_if_cancelled();

        rename_handle(
            output.get(),
            parent.get(),
            target.filename()
                .wstring(),
            true
        );

        flush_directory_best_effort(
            parent.get()
        );
    } catch (...) {
        try {
            mark_delete(
                output.get()
            );
        } catch (...) {
        }

        throw;
    }
#else
    auto parent =
        open_parent(
            open_root(
                root_,
                root_identity_a_,
                root_identity_b_
            ),
            relative,
            true
        );

    if (!parent) {
        throw InvalidPath{
            std::string{
                original_path
            }
        };
    }

    std::optional<std::string>
        temporary;

    NativeFile output;

    for (
        int attempt = 0;
        attempt < 32;
        ++attempt
    ) {
        const auto candidate =
            temporary_name(
                parent->name
            );

        try {
            output =
                create_exclusive_at(
                    parent->directory.get(),
                    candidate
                );

            temporary =
                candidate;

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
            std::string{
                original_path
            }
        };
    }

    try {
        write_all(
            output,
            contents,
            cancellation
        );

        output.close();

        cancellation.throw_if_cancelled();

        if (
            ::renameat(
                parent->directory.get(),
                temporary->c_str(),
                parent->directory.get(),
                parent->name.c_str()
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

        sync_directory(
            parent->directory.get()
        );

        temporary.reset();
    } catch (...) {
        if (temporary) {
            static_cast<void>(
                ::unlinkat(
                    parent->directory.get(),
                    temporary->c_str(),
                    0
                )
            );
        }

        throw;
    }
#endif
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
#ifdef _WIN32
    const auto target =
        prepare_destination(path);
#else
    const auto relative =
        validate_relative(path);

    if (is_root_path(relative)) {
        throw InvalidPath{
            path
        };
    }

    const auto target =
        root_ / relative;
#endif

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

#ifdef _WIN32
    const auto root_handle =
        open_root(
            root_,
            root_identity_a_,
            root_identity_b_
        );

    const auto root_final =
        normalized_native_path(
            final_path(
                root_handle.get()
            )
        );

    const auto target =
        resolve(path);

    auto file =
        open_regular_checked(
            target,
            root_final,
            DELETE
        );

    if (!file) {
        return false;
    }

    auto parent =
        open_directory_checked(
            target.parent_path(),
            root_final
        );

    cancellation.throw_if_cancelled();

    mark_delete(
        file->get()
    );

    flush_directory_best_effort(
        parent.get()
    );

    return true;
#else
    auto parent =
        open_parent(
            open_root(
                root_,
                root_identity_a_,
                root_identity_b_
            ),
            relative,
            false
        );

    if (!parent) {
        return false;
    }

    struct stat info{};

    if (
        ::fstatat(
            parent->directory.get(),
            parent->name.c_str(),
            &info,
            AT_SYMLINK_NOFOLLOW
        ) != 0
    ) {
        if (errno == ENOENT) {
            return false;
        }

        throw Error{
            "Unable to inspect storage object: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    if (S_ISLNK(info.st_mode)) {
        throw InvalidPath{
            std::string{path}
        };
    }

    if (!S_ISREG(info.st_mode)) {
        return false;
    }

    cancellation.throw_if_cancelled();

    if (
        ::unlinkat(
            parent->directory.get(),
            parent->name.c_str(),
            0
        ) != 0
    ) {
        if (errno == ENOENT) {
            return false;
        }

        throw Error{
            "Unable to remove storage object: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    sync_directory(
        parent->directory.get()
    );

    return true;
#endif
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

    const auto destination_relative =
        validate_relative(to);

    if (
        is_root_path(
            source_relative
        ) ||
        is_root_path(
            destination_relative
        )
    ) {
        throw InvalidPath{
            is_root_path(source_relative)
                ? std::string{from}
                : std::string{to}
        };
    }

#ifdef _WIN32
    const auto destination =
        prepare_destination(to);

    const auto root_handle =
        open_root(
            root_,
            root_identity_a_,
            root_identity_b_
        );

    const auto root_final =
        normalized_native_path(
            final_path(
                root_handle.get()
            )
        );

    auto source =
        open_regular_checked(
            resolve(from),
            root_final,
            DELETE
        );

    if (!source) {
        return false;
    }

    auto destination_parent =
        open_directory_checked(
            destination.parent_path(),
            root_final,
            FILE_LIST_DIRECTORY |
                FILE_READ_ATTRIBUTES |
                FILE_TRAVERSE |
                FILE_ADD_FILE
        );

    cancellation.throw_if_cancelled();

    rename_handle(
        source->get(),
        destination_parent.get(),
        destination.filename()
            .wstring(),
        true
    );

    flush_directory_best_effort(
        destination_parent.get()
    );

    return true;
#else
    auto source_parent =
        open_parent(
            open_root(
                root_,
                root_identity_a_,
                root_identity_b_
            ),
            source_relative,
            false
        );

    if (!source_parent) {
        return false;
    }

    struct stat info{};

    if (
        ::fstatat(
            source_parent
                ->directory
                .get(),
            source_parent
                ->name
                .c_str(),
            &info,
            AT_SYMLINK_NOFOLLOW
        ) != 0
    ) {
        if (errno == ENOENT) {
            return false;
        }

        throw Error{
            "Unable to inspect storage source: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    if (S_ISLNK(info.st_mode)) {
        throw InvalidPath{
            std::string{from}
        };
    }

    if (!S_ISREG(info.st_mode)) {
        return false;
    }

    auto destination_parent =
        open_parent(
            open_root(
                root_,
                root_identity_a_,
                root_identity_b_
            ),
            destination_relative,
            true
        );

    if (!destination_parent) {
        throw InvalidPath{
            std::string{to}
        };
    }

    cancellation.throw_if_cancelled();

    if (
        ::renameat(
            source_parent
                ->directory
                .get(),
            source_parent
                ->name
                .c_str(),
            destination_parent
                ->directory
                .get(),
            destination_parent
                ->name
                .c_str()
        ) != 0
    ) {
        throw Error{
            "Unable to move storage object: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    sync_directory(
        destination_parent
            ->directory
            .get()
    );

    sync_directory(
        source_parent
            ->directory
            .get()
    );

    return true;
#endif
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

    const auto relative =
        validate_relative(path);

    if (is_root_path(relative)) {
        throw NotFound{
            std::string{path}
        };
    }

#ifdef _WIN32
    const auto root_handle =
        open_root(
            root_,
            root_identity_a_,
            root_identity_b_
        );

    const auto root_final =
        normalized_native_path(
            final_path(
                root_handle.get()
            )
        );

    auto file =
        open_regular_checked(
            resolve(path),
            root_final,
            GENERIC_READ
        );

    if (!file) {
        throw NotFound{
            std::string{path}
        };
    }

    LARGE_INTEGER size{};

    if (
        !GetFileSizeEx(
            file->get(),
            &size
        )
    ) {
        throw Error{
            windows_error(
                "Unable to inspect storage object size",
                GetLastError()
            )
        };
    }

    cancellation.throw_if_cancelled();

    return static_cast<
        std::uintmax_t
    >(size.QuadPart);
#else
    auto parent =
        open_parent(
            open_root(
                root_,
                root_identity_a_,
                root_identity_b_
            ),
            relative,
            false
        );

    if (!parent) {
        throw NotFound{
            std::string{path}
        };
    }

    auto file =
        open_regular(
            parent->directory.get(),
            parent->name
        );

    if (!file) {
        throw NotFound{
            std::string{path}
        };
    }

    struct stat info{};

    if (
        ::fstat(
            file->get(),
            &info
        ) != 0
    ) {
        throw Error{
            "Unable to inspect storage object size: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    cancellation.throw_if_cancelled();

    return static_cast<
        std::uintmax_t
    >(info.st_size);
#endif
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

    const auto relative =
        validate_relative(
            directory
        );

    std::vector<std::string>
        result;

#ifdef _WIN32
    const auto root_handle =
        open_root(
            root_,
            root_identity_a_,
            root_identity_b_
        );

    const auto root_final =
        normalized_native_path(
            final_path(
                root_handle.get()
            )
        );

    const auto target =
        resolve(directory);

    NativeFile folder;

    try {
        folder =
            open_directory_checked(
                target,
                root_final
            );
    } catch (const Error&) {
        std::error_code error;

        if (
            !std::filesystem::exists(
                target,
                error
            )
        ) {
            return {};
        }

        throw;
    }

    const auto pattern =
        target /
        std::filesystem::path{L"*"};

    WIN32_FIND_DATAW data{};

    const auto search =
        FindFirstFileW(
            pattern.c_str(),
            &data
        );

    if (
        search ==
        INVALID_HANDLE_VALUE
    ) {
        const auto error =
            GetLastError();

        if (
            error ==
                ERROR_FILE_NOT_FOUND
        ) {
            return {};
        }

        throw Error{
            windows_error(
                "Unable to enumerate storage directory",
                error
            )
        };
    }

    struct FindHandle {
        HANDLE value{
            INVALID_HANDLE_VALUE
        };

        ~FindHandle() {
            if (
                value !=
                INVALID_HANDLE_VALUE
            ) {
                FindClose(value);
            }
        }
    } guard{search};

    while (true) {
        cancellation.throw_if_cancelled();

        const std::wstring name{
            data.cFileName
        };

        if (
            name != L"." &&
            name != L".." &&
            (
                data.dwFileAttributes &
                (
                    FILE_ATTRIBUTE_DIRECTORY |
                    FILE_ATTRIBUTE_REPARSE_POINT
                )
            ) == 0
        ) {
            const auto candidate =
                target /
                std::filesystem::path{
                    name
                };

            try {
                const auto verified =
                    open_regular_checked(
                        candidate,
                        root_final,
                        GENERIC_READ
                    );

                if (verified) {
                    auto output =
                        is_root_path(relative)
                            ? std::filesystem::path{
                                name
                              }
                            : relative /
                                std::filesystem::path{
                                    name
                                };

                    result.push_back(
                        output.generic_string()
                    );
                }
            } catch (
                const InvalidPath&
            ) {
                // The entry or a parent changed after enumeration.
                // Never expose a name that no longer resolves inside
                // the pinned storage root.
            }
        }

        if (
            !FindNextFileW(
                guard.value,
                &data
            )
        ) {
            const auto error =
                GetLastError();

            if (
                error ==
                ERROR_NO_MORE_FILES
            ) {
                break;
            }

            throw Error{
                windows_error(
                    "Unable to enumerate storage directory",
                    error
                )
            };
        }
    }
#else
    auto folder =
        open_directory(
            open_root(
                root_,
                root_identity_a_,
                root_identity_b_
            ),
            relative,
            false
        );

    if (!folder) {
        return {};
    }

    const auto duplicate =
        ::dup(
            folder->get()
        );

    if (duplicate < 0) {
        throw Error{
            "Unable to enumerate storage directory: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    std::unique_ptr<
        DIR,
        decltype(&closedir)
    > stream{
        ::fdopendir(
            duplicate
        ),
        &closedir
    };

    if (!stream) {
        static_cast<void>(
            ::close(
                duplicate
            )
        );

        throw Error{
            "Unable to enumerate storage directory: " +
            std::error_code{
                errno,
                std::generic_category()
            }.message()
        };
    }

    while (true) {
        cancellation.throw_if_cancelled();

        errno = 0;

        auto* entry =
            ::readdir(
                stream.get()
            );

        if (entry == nullptr) {
            if (errno != 0) {
                throw Error{
                    "Unable to enumerate storage directory: " +
                    std::error_code{
                        errno,
                        std::generic_category()
                    }.message()
                };
            }

            break;
        }

        const std::string_view name{
            entry->d_name
        };

        if (
            name == "." ||
            name == ".."
        ) {
            continue;
        }

        struct stat info{};

        if (
            ::fstatat(
                folder->get(),
                entry->d_name,
                &info,
                AT_SYMLINK_NOFOLLOW
            ) != 0
        ) {
            if (errno == ENOENT) {
                continue;
            }

            throw Error{
                "Unable to inspect storage directory entry: " +
                std::error_code{
                    errno,
                    std::generic_category()
                }.message()
            };
        }

        if (!S_ISREG(info.st_mode)) {
            continue;
        }

        auto output =
            is_root_path(relative)
                ? std::filesystem::path{
                    name
                  }
                : relative /
                    std::filesystem::path{
                        name
                    };

        result.push_back(
            output.generic_string()
        );
    }
#endif

    std::sort(
        result.begin(),
        result.end()
    );

    cancellation.throw_if_cancelled();

    return result;
}

std::size_t LocalDisk::cleanup_abandoned(
    std::chrono::seconds older_than
) {
    if (
        older_than <=
        std::chrono::seconds::zero()
    ) {
        throw std::invalid_argument(
            "Storage temporary cleanup age must be greater than zero"
        );
    }

    // Verify that the configured root still refers to the filesystem
    // object pinned when this LocalDisk instance was constructed.
    static_cast<void>(
        open_root(
            root_,
            root_identity_a_,
            root_identity_b_
        )
    );

    const auto cutoff =
        std::filesystem::
            file_time_type::
            clock::now() -
        older_than;

    std::size_t removed = 0;

    std::error_code error;

    std::filesystem::
        recursive_directory_iterator
        iterator{
            root_,
            std::filesystem::
                directory_options::
                    skip_permission_denied,
            error
        };

    if (error) {
        throw Error{
            filesystem_error(
                "Unable to enumerate storage temporary objects",
                error
            )
        };
    }

    const auto end =
        std::filesystem::
            recursive_directory_iterator{};

    while (iterator != end) {
        const auto entry =
            *iterator;

        error.clear();

        const auto status =
            entry.symlink_status(
                error
            );

        if (error) {
            iterator.increment(
                error
            );

            if (error) {
                throw Error{
                    filesystem_error(
                        "Unable to enumerate storage temporary objects",
                        error
                    )
                };
            }

            continue;
        }

        if (
            std::filesystem::
                is_symlink(
                    status
                )
        ) {
            if (
                std::filesystem::
                    is_directory(
                        status
                    )
            ) {
                iterator
                    .disable_recursion_pending();
            }

            iterator.increment(
                error
            );

            if (error) {
                throw Error{
                    filesystem_error(
                        "Unable to enumerate storage temporary objects",
                        error
                    )
                };
            }

            continue;
        }

        const auto name =
            entry.path()
                .filename()
                .string();

        if (
            std::filesystem::
                is_regular_file(
                    status
                ) &&
            is_temporary_artifact(
                name
            )
        ) {
            error.clear();

            const auto modified =
                entry.last_write_time(
                    error
                );

            if (
                !error &&
                modified <= cutoff
            ) {
                const auto relative =
                    entry.path()
                        .lexically_relative(
                            root_
                        )
                        .generic_string();

                try {
                    if (remove(relative)) {
                        ++removed;
                    }
                } catch (
                    const InvalidPath&
                ) {
                    // A concurrent replacement is treated as a skipped
                    // candidate rather than following the replacement.
                }
            }
        }

        iterator.increment(
            error
        );

        if (error) {
            throw Error{
                filesystem_error(
                    "Unable to enumerate storage temporary objects",
                    error
                )
            };
        }
    }

    return removed;
}

} // namespace gungnir::storage
