//
// SemVer comparator for OTA version handling
//
// Extracted to a header so the native test environment can unit-test it
// without pulling in the Arduino / ESP-IDF dependencies of OTAUpdater.cpp.
// Uses std::string (not Arduino String) so it links cleanly under native.
// OTAUpdater.cpp adapts Arduino String -> std::string at the call boundary.
//

#pragma once

#include <cctype>
#include <optional>
#include <string>

namespace ota {

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease; // empty when no "-rc.1" style suffix
};

// Parse a semver tag like "v1.2.4", "1.2.4-rc.1", "1.0.0+build.7".
// Returns nullopt on malformed input (missing components, non-numeric
// major/minor/patch, etc.).
std::optional<SemVer> parseSemVer(const std::string &tag);

// Three-way comparator per semver.org spec (Section 11):
//   - numeric prerelease identifiers compare as integers
//   - alphanumeric prerelease identifiers compare lexically in ASCII
//   - numeric identifiers have lower precedence than alphanumeric
//   - a version without prerelease has higher precedence than one with
// Returns -1 if a < b, 0 if equal, 1 if a > b.
int compareSemVer(const SemVer &a, const SemVer &b);

// True iff `latest` is strictly newer than `current` per semver.
// Fallback policy when one or both tags can't be parsed:
//   - both unparseable             -> false (refuse)
//   - `current` parseable, other not -> false (refuse; we don't know)
//   - `current` unparseable, other parseable -> true  (assume anything is newer)
bool isNewerVersion(const std::string &latest, const std::string &current);

// Convenience: parse + compare + return tri-state for "is upgrade recommended".
// Returns 1 if latest > current, 0 if equal, -1 if latest < current.
int compareVersions(const std::string &latest, const std::string &current);

} // namespace ota

// ---------------------------------------------------------------------------
// Implementation (header-only; kept here so native tests can link)
// ---------------------------------------------------------------------------

namespace ota {

namespace {

bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool parseIntPrefix(const std::string &s, int &out, size_t &consumed) {
    out = 0;
    consumed = 0;
    if (s.empty() || !isDigit(s[0])) return false;
    long v = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (!isDigit(s[i])) { consumed = i; break; }
        v = v * 10 + (s[i] - '0');
        consumed = i + 1;
        if (v > 999999L) return false; // safety net
    }
    out = static_cast<int>(v);
    return true;
}

// Compare two prerelease identifiers per semver spec (Section 11):
//   1. Identifiers consisting of only digits are compared numerically.
//   2. Identifiers with letters or hyphens are compared lexically in ASCII.
//   3. Numeric identifiers always have lower precedence than alphanumeric.
//   4. A larger set of pre-release fields has higher precedence.
int comparePrereleaseIdentifiers(const std::string &a, const std::string &b) {
    if (a == b) return 0;

    size_t i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        size_t ai = a.size();
        size_t bj = b.size();
        for (size_t k = i; k < a.size(); k++) { if (a[k] == '.') { ai = k; break; } }
        for (size_t k = j; k < b.size(); k++) { if (b[k] == '.') { bj = k; break; } }

        std::string ida = a.substr(i, ai - i);
        std::string idb = b.substr(j, bj - j);

        // Per semver.org spec Section 11, item 4: "A larger set of
        // pre-release fields has higher precedence than a smaller set,
        // if all of the preceding identifiers are equal." So if one
        // identifier is empty (the side ran out) and the other is not,
        // the empty side loses.
        if (ida.empty() && !idb.empty()) return -1;
        if (idb.empty() && !ida.empty()) return 1;
        if (ida.empty() && idb.empty()) {
            // Both ran out at the same boundary — strings are equal up
            // to here. Loop will terminate next iteration.
            i = (ai < a.size()) ? ai + 1 : a.size();
            j = (bj < b.size()) ? bj + 1 : b.size();
            if (i >= a.size() && j >= b.size()) break;
            continue;
        }

        bool aNum = isDigit(ida[0]);
        bool bNum = isDigit(idb[0]);

        if (aNum && bNum) {
            long va = 0, vb = 0;
            for (char c : ida) va = va * 10 + (c - '0');
            for (char c : idb) vb = vb * 10 + (c - '0');
            if (va != vb) return va < vb ? -1 : 1;
        } else if (aNum != bNum) {
            return aNum ? -1 : 1; // numeric < alphanumeric
        } else {
            if (ida < idb) return -1;
            if (ida > idb) return 1;
        }

        i = (ai < a.size()) ? ai + 1 : a.size();
        j = (bj < b.size()) ? bj + 1 : b.size();
        if (i >= a.size() && j >= b.size()) break;
    }

    return 0;
}

} // namespace

inline std::optional<SemVer> parseSemVer(const std::string &tag) {
    if (tag.empty()) return std::nullopt;

    size_t start = 0;
    if (tag[0] == 'v' || tag[0] == 'V') start = 1;
    std::string body = tag.substr(start);
    if (body.empty()) return std::nullopt;

    // Split off prerelease at '-' or build metadata at '+'.
    std::string core = body;
    std::string pre;

    auto plus = body.find('+');
    if (plus != std::string::npos) core = body.substr(0, plus);

    auto dash = core.find('-');
    if (dash != std::string::npos) {
        pre = core.substr(dash + 1);
        core = core.substr(0, dash);
    }

    // core must be exactly three numeric components separated by dots.
    int dots[2] = {-1, -1};
    int dotCount = 0;
    for (size_t i = 0; i < core.size(); i++) {
        if (core[i] == '.') {
            if (dotCount >= 2) return std::nullopt;
            dots[dotCount++] = static_cast<int>(i);
        } else if (!isDigit(core[i])) {
            return std::nullopt;
        }
    }
    if (dotCount != 2) return std::nullopt;

    SemVer v;
    std::string sMajor = core.substr(0, dots[0]);
    std::string sMinor = core.substr(dots[0] + 1, dots[1] - dots[0] - 1);
    std::string sPatch = core.substr(dots[1] + 1);

    size_t consumed = 0;
    if (!parseIntPrefix(sMajor, v.major, consumed) || consumed != sMajor.size()) return std::nullopt;
    if (!parseIntPrefix(sMinor, v.minor, consumed) || consumed != sMinor.size()) return std::nullopt;
    if (!parseIntPrefix(sPatch, v.patch, consumed) || consumed != sPatch.size()) return std::nullopt;

    if (v.major < 0 || v.minor < 0 || v.patch < 0) return std::nullopt;

    v.prerelease = pre;
    return v;
}

inline int compareSemVer(const SemVer &a, const SemVer &b) {
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;

    bool aHasPre = !a.prerelease.empty();
    bool bHasPre = !b.prerelease.empty();
    if (!aHasPre && !bHasPre) return 0;
    if (!aHasPre) return 1;
    if (!bHasPre) return -1;
    return comparePrereleaseIdentifiers(a.prerelease, b.prerelease);
}

inline bool isNewerVersion(const std::string &latest, const std::string &current) {
    auto lp = parseSemVer(latest);
    auto cp = parseSemVer(current);

    if (!lp && !cp) return false;
    if (cp && !lp) return false;
    if (!cp && lp) return true;

    return compareSemVer(*lp, *cp) > 0;
}

inline int compareVersions(const std::string &latest, const std::string &current) {
    auto lp = parseSemVer(latest);
    auto cp = parseSemVer(current);

    if (!lp && !cp) return 0;
    if (cp && !lp) return -1;
    if (!cp && lp) return 1;
    return compareSemVer(*lp, *cp);
}

} // namespace ota
