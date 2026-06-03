#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CUPScanSettings : NSObject
@property(nonatomic) unsigned long long largeFileThresholdBytes;
@property(nonatomic) BOOL includeScreenshots;
@property(nonatomic) BOOL includeDownloads;
@end

@interface CUPScannerBridge : NSObject
- (NSDictionary<NSString *, id> *)scanRoots:(NSArray<NSString *> *)roots
                                   settings:(CUPScanSettings *)settings;
- (NSDictionary<NSString *, id> *)deletePaths:(NSArray<NSString *> *)paths
                                    confirmed:(BOOL)confirmed;
@end

NS_ASSUME_NONNULL_END
