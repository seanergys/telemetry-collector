#ifndef TELEMETRY_COLLECTOR_SEMVER_H
#define TELEMETRY_COLLECTOR_SEMVER_H

#ifdef __cplusplus
#include <iostream>
#include <ostream>
#include <string>
#endif

typedef struct version version;
typedef version version_t;

struct version
{
    int major;
    int minor;
    int patch;

#ifdef __cplusplus
    version(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}

    bool operator==(const version &other) const
    {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    bool operator!=(const version &other) const
    {
        return !(*this == other);
    }

    bool operator<(const version &other) const
    {
        if (major != other.major)
            return major < other.major;
        if (minor != other.minor)
            return minor < other.minor;
        return patch < other.patch;
    }

    bool operator>(const version &other) const
    {
        return other < *this;
    }

    bool operator<=(const version &other) const
    {
        return !(*this > other);
    }

    bool operator>=(const version &other) const
    {
        return !(*this < other);
    }

    std::string to_string() const
    {
        return std::to_string(major) + "." +
               std::to_string(minor) + "." +
               std::to_string(patch);
    }

    void print(std::ostream &output) const
    {
        output << *this;
    }

    void print() const
    {
        print(std::cout);
    }

    friend std::ostream &operator<<(std::ostream &output, const version &v)
    {
        return output << v.major << "." << v.minor << "." << v.patch;
    }
#endif
};

#endif
