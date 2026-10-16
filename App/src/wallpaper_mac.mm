#include "wallpaper.h"

#import <AppKit/AppKit.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace wallpaper {

Source source()
{
    // 动态的背景（随时间变的）是一个 HEIC，里面好几张：取第一张
    NSURL *const url = [NSWorkspace.sharedWorkspace desktopImageURLForScreen:NSScreen.screens.firstObject];
    return {url.isFileURL ? QString::fromNSString(url.path) : QString(), QColor()};
}

QByteArray jpeg(const Source &source)
{
    if (source.file.isEmpty())
        return {};
    // 在线程池的线程上：系统的对象要有自己的释放池
    @autoreleasepool {
        // Qt 读不了 HEIC：用系统的 ImageIO 解码、缩小、编码
        NSURL *const url = [NSURL fileURLWithPath:source.file.toNSString()];
        CGImageSourceRef image = CGImageSourceCreateWithURL(static_cast<CFURLRef>(url), nullptr);
        if (!image)
            return {};
        NSDictionary *const options = @{
            static_cast<NSString *>(kCGImageSourceCreateThumbnailFromImageAlways): @YES,
            static_cast<NSString *>(kCGImageSourceCreateThumbnailWithTransform): @YES,
            static_cast<NSString *>(kCGImageSourceThumbnailMaxPixelSize): @(MaxSide),
        };
        CGImageRef scaled = CGImageSourceCreateThumbnailAtIndex(image, 0, static_cast<CFDictionaryRef>(options));
        CFRelease(image);
        if (!scaled)
            return {};
        NSMutableData *const data = [NSMutableData data];
        CGImageDestinationRef destination = CGImageDestinationCreateWithData(
            static_cast<CFMutableDataRef>(data), static_cast<CFStringRef>(UTTypeJPEG.identifier), 1, nullptr);
        NSDictionary *const quality = @{static_cast<NSString *>(kCGImageDestinationLossyCompressionQuality): @0.85};
        CGImageDestinationAddImage(destination, scaled, static_cast<CFDictionaryRef>(quality));
        const bool written = CGImageDestinationFinalize(destination);
        CFRelease(destination);
        CGImageRelease(scaled);
        return written ? QByteArray::fromNSData(data) : QByteArray();
    }
}

} // namespace wallpaper
