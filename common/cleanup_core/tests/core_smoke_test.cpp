#include "cleanup/scanner.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

void writeFile(const fs::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary);
    output << content;
}

bool hasCategory(const cleanup::FileRecord& record, cleanup::Category category) {
    for (const auto value : record.categories) {
        if (value == category) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    const auto root = fs::temp_directory_path() /
                      ("cleanup_core_test_" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "Pictures" / "Screenshots");
    fs::create_directories(root / "Download");
    fs::create_directories(root / "EmptyFolder");

    const std::string imageContent = "fake image bytes with duplicate content";
    writeFile(root / "Pictures" / "IMG_001.jpg", imageContent);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    writeFile(root / "Pictures" / "IMG_001_copy.jpg", imageContent);
    writeFile(root / "Pictures" / "Screenshots" / "Screenshot_2026.png", "screen");
    writeFile(root / "Download" / "installer.zip", "download-data");
    writeFile(root / "movie.bin", std::string(128, 'x'));

    cleanup::ScannerSettings settings;
    settings.largeFileThresholdBytes = 64;
    settings.includeDownloads = true;
    settings.includeScreenshots = true;

    cleanup::StorageScanner scanner;
    auto result = scanner.scan({root.string()}, settings);

    assert(result.summary.filesScanned == 5);
    assert(result.summary.duplicateGroups == 1);
    assert(result.summary.emptyFolders == 1);

    bool sawLargeFile = false;
    bool sawScreenshot = false;
    bool sawDownload = false;
    bool sawRecommendedDuplicate = false;

    for (const auto& item : result.items) {
        sawLargeFile = sawLargeFile || hasCategory(item, cleanup::Category::LargeFile);
        sawScreenshot = sawScreenshot || hasCategory(item, cleanup::Category::Screenshot);
        sawDownload = sawDownload || hasCategory(item, cleanup::Category::Download);
        sawRecommendedDuplicate = sawRecommendedDuplicate || item.recommendedForKeepNewest;
    }

    assert(sawLargeFile);
    assert(sawScreenshot);
    assert(sawDownload);
    assert(sawRecommendedDuplicate);

    auto denied = scanner.deletePaths({(root / "movie.bin").string()}, false);
    assert(denied.deletedPaths.empty());
    assert(!denied.failures.empty());
    assert(fs::exists(root / "movie.bin"));

    auto deleted = scanner.deletePaths({(root / "movie.bin").string(), (root / "EmptyFolder").string()}, true);
    assert(deleted.failures.empty());
    assert(deleted.deletedPaths.size() == 2);
    assert(!fs::exists(root / "movie.bin"));
    assert(!fs::exists(root / "EmptyFolder"));

    fs::remove_all(root);
    std::cout << "cleanup_core_smoke_test passed\n";
    return 0;
}
