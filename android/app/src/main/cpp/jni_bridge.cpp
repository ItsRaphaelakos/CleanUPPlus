#include <jni.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "cleanup/scanner.h"

namespace {

std::string toString(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) {
        return {};
    }
    std::string output(chars);
    env->ReleaseStringUTFChars(value, chars);
    return output;
}

std::vector<std::string> toStringVector(JNIEnv* env, jobjectArray values) {
    std::vector<std::string> output;
    if (values == nullptr) {
        return output;
    }

    const jsize count = env->GetArrayLength(values);
    output.reserve(static_cast<std::size_t>(count));
    for (jsize index = 0; index < count; ++index) {
        auto item = static_cast<jstring>(env->GetObjectArrayElement(values, index));
        output.push_back(toString(env, item));
        env->DeleteLocalRef(item);
    }
    return output;
}

std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char ch : value) {
        switch (ch) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (ch < 0x20) {
                    out << "\\u";
                    const char* hex = "0123456789abcdef";
                    out << '0' << '0' << hex[(ch >> 4) & 0x0F] << hex[ch & 0x0F];
                } else {
                    out << static_cast<char>(ch);
                }
                break;
        }
    }
    return out.str();
}

void appendQuoted(std::ostringstream& out, const std::string& value) {
    out << '"' << jsonEscape(value) << '"';
}

void appendStringArray(std::ostringstream& out, const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            out << ',';
        }
        appendQuoted(out, values[index]);
    }
    out << ']';
}

void appendCategoryArray(std::ostringstream& out, const std::vector<cleanup::Category>& values) {
    out << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            out << ',';
        }
        appendQuoted(out, cleanup::categoryToKey(values[index]));
    }
    out << ']';
}

std::string scanResultToJson(const cleanup::ScanResult& result) {
    std::ostringstream out;
    const auto& summary = result.summary;

    out << "{\"summary\":{";
    out << "\"totalSpaceBytes\":" << summary.totalSpaceBytes << ',';
    out << "\"availableSpaceBytes\":" << summary.availableSpaceBytes << ',';
    out << "\"scannedBytes\":" << summary.scannedBytes << ',';
    out << "\"reclaimableBytes\":" << summary.reclaimableBytes << ',';
    out << "\"duplicateBytes\":" << summary.duplicateBytes << ',';
    out << "\"largeFileBytes\":" << summary.largeFileBytes << ',';
    out << "\"screenshotBytes\":" << summary.screenshotBytes << ',';
    out << "\"downloadBytes\":" << summary.downloadBytes << ',';
    out << "\"filesScanned\":" << summary.filesScanned << ',';
    out << "\"foldersScanned\":" << summary.foldersScanned << ',';
    out << "\"duplicateGroups\":" << summary.duplicateGroups << ',';
    out << "\"emptyFolders\":" << summary.emptyFolders;
    out << "},\"items\":[";

    for (std::size_t index = 0; index < result.items.size(); ++index) {
        if (index > 0) {
            out << ',';
        }

        const auto& item = result.items[index];
        out << '{';
        out << "\"id\":";
        appendQuoted(out, item.id);
        out << ",\"path\":";
        appendQuoted(out, item.path);
        out << ",\"name\":";
        appendQuoted(out, item.name);
        out << ",\"sizeBytes\":" << item.sizeBytes;
        out << ",\"modifiedMillis\":" << item.modifiedMillis;
        out << ",\"isDirectory\":" << (item.isDirectory ? "true" : "false");
        out << ",\"isImage\":" << (item.isImage ? "true" : "false");
        out << ",\"hash\":";
        appendQuoted(out, item.hash);
        out << ",\"duplicateGroupId\":";
        appendQuoted(out, item.duplicateGroupId);
        out << ",\"recommendedForKeepNewest\":"
            << (item.recommendedForKeepNewest ? "true" : "false");
        out << ",\"categories\":";
        appendCategoryArray(out, item.categories);
        out << '}';
    }

    out << "],\"duplicateGroups\":[";
    for (std::size_t index = 0; index < result.duplicateGroups.size(); ++index) {
        if (index > 0) {
            out << ',';
        }

        const auto& group = result.duplicateGroups[index];
        out << "{\"id\":";
        appendQuoted(out, group.id);
        out << ",\"keepPath\":";
        appendQuoted(out, group.keepPath);
        out << ",\"reclaimableBytes\":" << group.reclaimableBytes;
        out << ",\"paths\":";
        appendStringArray(out, group.paths);
        out << '}';
    }

    out << "],\"errors\":";
    appendStringArray(out, result.errors);
    out << '}';
    return out.str();
}

std::string deleteResultToJson(const cleanup::DeleteResult& result) {
    std::ostringstream out;
    out << "{\"deletedBytes\":" << result.deletedBytes;
    out << ",\"deletedPaths\":";
    appendStringArray(out, result.deletedPaths);
    out << ",\"failures\":[";
    for (std::size_t index = 0; index < result.failures.size(); ++index) {
        if (index > 0) {
            out << ',';
        }
        out << "{\"path\":";
        appendQuoted(out, result.failures[index].path);
        out << ",\"reason\":";
        appendQuoted(out, result.failures[index].reason);
        out << '}';
    }
    out << "]}";
    return out.str();
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_cleanup_plus_NativeCleanerBridge_scanStorage(
    JNIEnv* env,
    jobject,
    jobjectArray rootPaths,
    jlong largeFileThresholdBytes,
    jboolean includeScreenshots,
    jboolean includeDownloads) {
    cleanup::ScannerSettings settings;
    settings.largeFileThresholdBytes =
        static_cast<std::uint64_t>(largeFileThresholdBytes < 0 ? 0 : largeFileThresholdBytes);
    settings.includeScreenshots = includeScreenshots == JNI_TRUE;
    settings.includeDownloads = includeDownloads == JNI_TRUE;

    cleanup::StorageScanner scanner;
    const auto result = scanner.scan(toStringVector(env, rootPaths), settings);
    const auto json = scanResultToJson(result);
    return env->NewStringUTF(json.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_cleanup_plus_NativeCleanerBridge_deletePaths(
    JNIEnv* env,
    jobject,
    jobjectArray paths,
    jboolean confirmed) {
    cleanup::StorageScanner scanner;
    const auto result = scanner.deletePaths(toStringVector(env, paths), confirmed == JNI_TRUE);
    const auto json = deleteResultToJson(result);
    return env->NewStringUTF(json.c_str());
}
