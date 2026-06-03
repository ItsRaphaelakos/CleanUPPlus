#import "CleanUpCoreBridge.h"

#include <string>
#include <vector>

#include "../common/cleanup_core/include/cleanup/scanner.h"

@implementation CUPScanSettings

- (instancetype)init {
    self = [super init];
    if (self) {
        _largeFileThresholdBytes = 100ULL * 1024ULL * 1024ULL;
        _includeScreenshots = YES;
        _includeDownloads = YES;
    }
    return self;
}

@end

namespace {

NSArray<NSString *> *ToNSStringArray(const std::vector<std::string>& values) {
    NSMutableArray<NSString *> *array = [NSMutableArray arrayWithCapacity:values.size()];
    for (const auto& value : values) {
        [array addObject:[NSString stringWithUTF8String:value.c_str()]];
    }
    return array;
}

NSArray<NSString *> *ToCategoryArray(const std::vector<cleanup::Category>& values) {
    NSMutableArray<NSString *> *array = [NSMutableArray arrayWithCapacity:values.size()];
    for (const auto& value : values) {
        const auto key = cleanup::categoryToKey(value);
        [array addObject:[NSString stringWithUTF8String:key.c_str()]];
    }
    return array;
}

std::vector<std::string> ToStdVector(NSArray<NSString *> *values) {
    std::vector<std::string> output;
    output.reserve(values.count);
    for (NSString *value in values) {
        output.emplace_back(value.UTF8String ?: "");
    }
    return output;
}

NSDictionary<NSString *, id> *ToDictionary(const cleanup::FileRecord& item) {
    return @{
        @"id": [NSString stringWithUTF8String:item.id.c_str()],
        @"path": [NSString stringWithUTF8String:item.path.c_str()],
        @"name": [NSString stringWithUTF8String:item.name.c_str()],
        @"sizeBytes": @(item.sizeBytes),
        @"modifiedMillis": @(item.modifiedMillis),
        @"isDirectory": @(item.isDirectory),
        @"isImage": @(item.isImage),
        @"hash": [NSString stringWithUTF8String:item.hash.c_str()],
        @"duplicateGroupId": [NSString stringWithUTF8String:item.duplicateGroupId.c_str()],
        @"recommendedForKeepNewest": @(item.recommendedForKeepNewest),
        @"categories": ToCategoryArray(item.categories),
    };
}

}  // namespace

@implementation CUPScannerBridge

- (NSDictionary<NSString *, id> *)scanRoots:(NSArray<NSString *> *)roots
                                   settings:(CUPScanSettings *)settings {
    cleanup::ScannerSettings nativeSettings;
    nativeSettings.largeFileThresholdBytes = settings.largeFileThresholdBytes;
    nativeSettings.includeScreenshots = settings.includeScreenshots;
    nativeSettings.includeDownloads = settings.includeDownloads;

    cleanup::StorageScanner scanner;
    const auto result = scanner.scan(ToStdVector(roots), nativeSettings);

    NSMutableArray<NSDictionary<NSString *, id> *> *items =
        [NSMutableArray arrayWithCapacity:result.items.size()];
    for (const auto& item : result.items) {
        [items addObject:ToDictionary(item)];
    }

    NSMutableArray<NSDictionary<NSString *, id> *> *groups =
        [NSMutableArray arrayWithCapacity:result.duplicateGroups.size()];
    for (const auto& group : result.duplicateGroups) {
        [groups addObject:@{
            @"id": [NSString stringWithUTF8String:group.id.c_str()],
            @"keepPath": [NSString stringWithUTF8String:group.keepPath.c_str()],
            @"reclaimableBytes": @(group.reclaimableBytes),
            @"paths": ToNSStringArray(group.paths),
        }];
    }

    const auto& summary = result.summary;
    return @{
        @"summary": @{
            @"totalSpaceBytes": @(summary.totalSpaceBytes),
            @"availableSpaceBytes": @(summary.availableSpaceBytes),
            @"scannedBytes": @(summary.scannedBytes),
            @"reclaimableBytes": @(summary.reclaimableBytes),
            @"duplicateBytes": @(summary.duplicateBytes),
            @"largeFileBytes": @(summary.largeFileBytes),
            @"screenshotBytes": @(summary.screenshotBytes),
            @"downloadBytes": @(summary.downloadBytes),
            @"filesScanned": @(summary.filesScanned),
            @"foldersScanned": @(summary.foldersScanned),
            @"duplicateGroups": @(summary.duplicateGroups),
            @"emptyFolders": @(summary.emptyFolders),
        },
        @"items": items,
        @"duplicateGroups": groups,
        @"errors": ToNSStringArray(result.errors),
    };
}

- (NSDictionary<NSString *, id> *)deletePaths:(NSArray<NSString *> *)paths
                                    confirmed:(BOOL)confirmed {
    cleanup::StorageScanner scanner;
    const auto result = scanner.deletePaths(ToStdVector(paths), confirmed);

    NSMutableArray<NSDictionary<NSString *, NSString *> *> *failures =
        [NSMutableArray arrayWithCapacity:result.failures.size()];
    for (const auto& failure : result.failures) {
        [failures addObject:@{
            @"path": [NSString stringWithUTF8String:failure.path.c_str()],
            @"reason": [NSString stringWithUTF8String:failure.reason.c_str()],
        }];
    }

    return @{
        @"deletedBytes": @(result.deletedBytes),
        @"deletedPaths": ToNSStringArray(result.deletedPaths),
        @"failures": failures,
    };
}

@end
