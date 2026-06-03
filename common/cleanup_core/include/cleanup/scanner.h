#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cleanup {

enum class Category {
    DuplicatePhoto,
    LargeFile,
    Screenshot,
    Download,
    EmptyFolder,
};

struct ScannerSettings {
    std::uint64_t largeFileThresholdBytes = 100ULL * 1024ULL * 1024ULL;
    bool includeScreenshots = true;
    bool includeDownloads = true;
};

struct FileRecord {
    std::string id;
    std::string path;
    std::string name;
    std::uint64_t sizeBytes = 0;
    std::int64_t modifiedMillis = 0;
    bool isDirectory = false;
    bool isImage = false;
    std::string hash;
    std::string duplicateGroupId;
    bool recommendedForKeepNewest = false;
    std::vector<Category> categories;
};

struct DuplicateGroup {
    std::string id;
    std::string keepPath;
    std::uint64_t reclaimableBytes = 0;
    std::vector<std::string> paths;
};

struct ScanSummary {
    std::uint64_t totalSpaceBytes = 0;
    std::uint64_t availableSpaceBytes = 0;
    std::uint64_t scannedBytes = 0;
    std::uint64_t reclaimableBytes = 0;
    std::uint64_t duplicateBytes = 0;
    std::uint64_t largeFileBytes = 0;
    std::uint64_t screenshotBytes = 0;
    std::uint64_t downloadBytes = 0;
    std::size_t filesScanned = 0;
    std::size_t foldersScanned = 0;
    std::size_t duplicateGroups = 0;
    std::size_t emptyFolders = 0;
};

struct ScanResult {
    ScanSummary summary;
    std::vector<FileRecord> items;
    std::vector<DuplicateGroup> duplicateGroups;
    std::vector<std::string> errors;
};

struct DeleteFailure {
    std::string path;
    std::string reason;
};

struct DeleteResult {
    std::uint64_t deletedBytes = 0;
    std::vector<std::string> deletedPaths;
    std::vector<DeleteFailure> failures;
};

class StorageScanner {
public:
    ScanResult scan(const std::vector<std::string>& rootPaths, const ScannerSettings& settings) const;
    DeleteResult deletePaths(const std::vector<std::string>& paths, bool confirmed) const;
};

std::string categoryToKey(Category category);

}  // namespace cleanup
