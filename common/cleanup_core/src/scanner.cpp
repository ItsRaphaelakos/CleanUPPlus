#include "cleanup/scanner.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace cleanup {
namespace {

namespace fs = std::filesystem;

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string toPortableString(const fs::path& path) {
#if defined(_WIN32)
    return path.u8string();
#else
    return path.string();
#endif
}

std::string filenameOf(const fs::path& path) {
    return toPortableString(path.filename());
}

std::string toHex(std::uint64_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

std::uint64_t hashString(const std::string& value) {
    std::uint64_t hash = kFnvOffset;
    for (unsigned char byte : value) {
        hash ^= byte;
        hash *= kFnvPrime;
    }
    return hash;
}

std::string recordIdForPath(const std::string& path) {
    return "item_" + toHex(hashString(path));
}

std::int64_t modifiedMillis(const fs::path& path) {
    std::error_code ec;
    const auto fileTime = fs::last_write_time(path, ec);
    if (ec) {
        return 0;
    }

    const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fileTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               systemTime.time_since_epoch())
        .count();
}

bool hasCategory(const FileRecord& record, Category category) {
    return std::find(record.categories.begin(), record.categories.end(), category) !=
           record.categories.end();
}

void addCategory(FileRecord& record, Category category) {
    if (!hasCategory(record, category)) {
        record.categories.push_back(category);
    }
}

bool isImageExtension(const fs::path& path) {
    const auto ext = toLower(toPortableString(path.extension()));
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp" ||
           ext == ".heic" || ext == ".heif" || ext == ".gif" || ext == ".bmp" ||
           ext == ".tiff" || ext == ".tif";
}

bool containsToken(std::string haystack, const std::vector<std::string>& tokens) {
    haystack = toLower(std::move(haystack));
    for (const auto& token : tokens) {
        if (haystack.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool isScreenshotPath(const fs::path& path) {
    return containsToken(toPortableString(path), {
                                            "screenshot",
                                            "screen_shot",
                                            "screen-shot",
                                            "screen shot",
                                            "screenshots",
                                        });
}

bool isDownloadsPath(const fs::path& path) {
    for (const auto& part : path) {
        const auto value = toLower(toPortableString(part));
        if (value == "download" || value == "downloads") {
            return true;
        }
    }
    return false;
}

std::string hashFileContents(const fs::path& path, std::uint64_t sizeBytes, std::string* error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error) {
            *error = "Unable to open file for hashing";
        }
        return {};
    }

    std::uint64_t hash = kFnvOffset;
    std::array<char, 64 * 1024> buffer{};

    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]);
            hash *= kFnvPrime;
        }
    }

    hash ^= sizeBytes;
    hash *= kFnvPrime;
    return toHex(hash);
}

bool isDangerousDeleteTarget(const fs::path& path) {
    const auto normalized = toPortableString(path.lexically_normal());
    if (normalized.empty() || normalized == "/" || normalized == "\\" ||
        normalized.size() < 6) {
        return true;
    }

    const auto filename = path.filename();
    if (filename.empty() || filename == "." || filename == "..") {
        return true;
    }

    const auto lower = toLower(normalized);
    return lower == "/storage" || lower == "/storage/emulated" ||
           lower == "/storage/emulated/0" || lower == "/sdcard" ||
           lower == "c:\\" || lower == "c:/";
}

void addSpaceStats(const fs::path& root, ScanSummary& summary) {
    std::error_code ec;
    const auto info = fs::space(root, ec);
    if (!ec) {
        summary.totalSpaceBytes += info.capacity;
        summary.availableSpaceBytes += info.available;
    }
}

void addPotentialReclaimableBytes(ScanResult& result) {
    std::unordered_set<std::string> countedPaths;
    std::uint64_t total = 0;

    for (const auto& item : result.items) {
        const bool candidate = item.recommendedForKeepNewest ||
                               hasCategory(item, Category::LargeFile) ||
                               hasCategory(item, Category::Screenshot) ||
                               hasCategory(item, Category::Download) ||
                               hasCategory(item, Category::EmptyFolder);

        if (candidate && countedPaths.insert(item.path).second) {
            total += item.sizeBytes;
        }
    }

    result.summary.reclaimableBytes = total;
}

}  // namespace

std::string categoryToKey(Category category) {
    switch (category) {
        case Category::DuplicatePhoto:
            return "duplicate_photos";
        case Category::LargeFile:
            return "large_files";
        case Category::Screenshot:
            return "screenshots";
        case Category::Download:
            return "downloads";
        case Category::EmptyFolder:
            return "empty_folders";
    }
    return "unknown";
}

ScanResult StorageScanner::scan(
    const std::vector<std::string>& rootPaths,
    const ScannerSettings& settings) const {
    ScanResult result;
    result.items.reserve(512);

    std::unordered_map<std::uint64_t, std::vector<std::size_t>> imageCandidatesBySize;

    for (const auto& rootValue : rootPaths) {
        fs::path root(rootValue);
        std::error_code ec;

        if (!fs::exists(root, ec)) {
            result.errors.push_back("Root does not exist: " + rootValue);
            continue;
        }

        addSpaceStats(root, result.summary);

        fs::recursive_directory_iterator iterator(
            root,
            fs::directory_options::skip_permission_denied,
            ec);
        fs::recursive_directory_iterator end;
        if (ec) {
            result.errors.push_back("Unable to scan root: " + rootValue);
            continue;
        }

        while (iterator != end) {
            const fs::directory_entry entry = *iterator;
            const fs::path path = entry.path();
            const std::string pathString = toPortableString(path);

            std::error_code statusError;
            const bool isDirectory = entry.is_directory(statusError);
            const bool isRegularFile = entry.is_regular_file(statusError);

            if (statusError) {
                result.errors.push_back("Unable to read metadata: " + pathString);
                iterator.increment(ec);
                if (ec) {
                    result.errors.push_back("Scan skipped unreadable branch: " + pathString);
                    ec.clear();
                }
                continue;
            }

            if (isDirectory) {
                ++result.summary.foldersScanned;

                std::error_code emptyError;
                if (fs::is_empty(path, emptyError) && !emptyError) {
                    FileRecord record;
                    record.id = recordIdForPath(pathString);
                    record.path = pathString;
                    record.name = filenameOf(path);
                    record.modifiedMillis = modifiedMillis(path);
                    record.isDirectory = true;
                    addCategory(record, Category::EmptyFolder);

                    ++result.summary.emptyFolders;
                    result.items.push_back(std::move(record));
                }
            } else if (isRegularFile) {
                std::error_code sizeError;
                const auto size = entry.file_size(sizeError);
                if (sizeError) {
                    result.errors.push_back("Unable to read file size: " + pathString);
                    iterator.increment(ec);
                    if (ec) {
                        result.errors.push_back("Scan skipped unreadable branch: " + pathString);
                        ec.clear();
                    }
                    continue;
                }

                FileRecord record;
                record.id = recordIdForPath(pathString);
                record.path = pathString;
                record.name = filenameOf(path);
                record.sizeBytes = size;
                record.modifiedMillis = modifiedMillis(path);
                record.isImage = isImageExtension(path);

                ++result.summary.filesScanned;
                result.summary.scannedBytes += size;

                if (size > settings.largeFileThresholdBytes) {
                    addCategory(record, Category::LargeFile);
                    result.summary.largeFileBytes += size;
                }

                if (settings.includeScreenshots && isScreenshotPath(path)) {
                    addCategory(record, Category::Screenshot);
                    result.summary.screenshotBytes += size;
                }

                if (settings.includeDownloads && isDownloadsPath(path)) {
                    addCategory(record, Category::Download);
                    result.summary.downloadBytes += size;
                }

                const auto itemIndex = result.items.size();
                if (record.isImage) {
                    imageCandidatesBySize[record.sizeBytes].push_back(itemIndex);
                }
                result.items.push_back(std::move(record));
            }

            iterator.increment(ec);
            if (ec) {
                result.errors.push_back("Scan skipped unreadable branch: " + pathString);
                ec.clear();
            }
        }
    }

    std::size_t groupCounter = 1;
    for (const auto& bySize : imageCandidatesBySize) {
        const auto& candidateIndexes = bySize.second;
        if (candidateIndexes.size() < 2) {
            continue;
        }

        std::unordered_map<std::string, std::vector<std::size_t>> indexesByHash;
        for (const auto index : candidateIndexes) {
            auto& item = result.items[index];
            std::string hashError;
            item.hash = hashFileContents(fs::path(item.path), item.sizeBytes, &hashError);
            if (item.hash.empty()) {
                result.errors.push_back("Hash failed for " + item.path + ": " + hashError);
                continue;
            }
            indexesByHash[item.hash].push_back(index);
        }

        for (auto& byHash : indexesByHash) {
            auto& groupIndexes = byHash.second;
            if (groupIndexes.size() < 2) {
                continue;
            }

            std::sort(groupIndexes.begin(), groupIndexes.end(), [&](std::size_t left, std::size_t right) {
                const auto& a = result.items[left];
                const auto& b = result.items[right];
                if (a.modifiedMillis != b.modifiedMillis) {
                    return a.modifiedMillis > b.modifiedMillis;
                }
                return a.path < b.path;
            });

            DuplicateGroup group;
            group.id = "dup_" + std::to_string(groupCounter++);
            group.keepPath = result.items[groupIndexes.front()].path;

            for (std::size_t position = 0; position < groupIndexes.size(); ++position) {
                auto& item = result.items[groupIndexes[position]];
                addCategory(item, Category::DuplicatePhoto);
                item.duplicateGroupId = group.id;
                group.paths.push_back(item.path);

                if (position > 0) {
                    item.recommendedForKeepNewest = true;
                    group.reclaimableBytes += item.sizeBytes;
                    result.summary.duplicateBytes += item.sizeBytes;
                }
            }

            result.duplicateGroups.push_back(std::move(group));
        }
    }

    result.summary.duplicateGroups = result.duplicateGroups.size();
    addPotentialReclaimableBytes(result);

    std::sort(result.items.begin(), result.items.end(), [](const FileRecord& left, const FileRecord& right) {
        if (left.categories.size() != right.categories.size()) {
            return left.categories.size() > right.categories.size();
        }
        if (left.sizeBytes != right.sizeBytes) {
            return left.sizeBytes > right.sizeBytes;
        }
        return left.path < right.path;
    });

    return result;
}

DeleteResult StorageScanner::deletePaths(
    const std::vector<std::string>& paths,
    bool confirmed) const {
    DeleteResult result;

    if (!confirmed) {
        for (const auto& path : paths) {
            result.failures.push_back({path, "Deletion requires explicit confirmation"});
        }
        return result;
    }

    for (const auto& value : paths) {
        fs::path path(value);
        std::error_code ec;

        if (isDangerousDeleteTarget(path)) {
            result.failures.push_back({value, "Refusing to delete a broad or unsafe path"});
            continue;
        }

        if (!fs::exists(path, ec)) {
            result.failures.push_back({value, "Path does not exist"});
            continue;
        }

        const bool isDirectory = fs::is_directory(path, ec);
        if (ec) {
            result.failures.push_back({value, "Unable to inspect path"});
            continue;
        }

        std::uint64_t sizeBeforeDelete = 0;
        if (!isDirectory) {
            sizeBeforeDelete = fs::file_size(path, ec);
            if (ec) {
                sizeBeforeDelete = 0;
                ec.clear();
            }
        }

        if (isDirectory) {
            if (!fs::is_empty(path, ec) || ec) {
                result.failures.push_back({value, "Only empty folders can be deleted by the core"});
                continue;
            }

            if (fs::remove(path, ec) && !ec) {
                result.deletedPaths.push_back(value);
            } else {
                result.failures.push_back({value, "Failed to delete empty folder"});
            }
        } else {
            if (fs::remove(path, ec) && !ec) {
                result.deletedBytes += sizeBeforeDelete;
                result.deletedPaths.push_back(value);
            } else {
                result.failures.push_back({value, "Failed to delete file"});
            }
        }
    }

    return result;
}

}  // namespace cleanup
