// Copyright (c) Meta Platforms, Inc. and affiliates.
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct AggregateStats {
    size_t origSize;
    size_t compressedSize;
    double ctimeSecs;
    double dtimeSecs;
};

static void processFile(const std::filesystem::path& filepath)
{
    assert(filepath.extension() == ".txt");
    std::map<std::string, AggregateStats> statsMap{};

    std::ifstream in(filepath);
    std::string line;
    std::getline(in, line); // skip header
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }
        assert(fields.size() == 7);
        auto& stats     = statsMap[fields[0]];
        size_t origSize = std::stoull(fields[3]);
        stats.origSize += origSize;
        stats.compressedSize += std::stoull(fields[4]);
        stats.ctimeSecs +=
                (double)origSize / 1024 / 1024 / std::stod(fields[1]);
        stats.dtimeSecs +=
                (double)origSize / 1024 / 1024 / std::stod(fields[2]);
    }
    for (const auto& [name, stats] : statsMap) {
        std::cout << name << ',' << filepath.stem() << ',';
        std::cout << (double)stats.origSize / (double)stats.compressedSize
                  << ',';
        std::cout << (double)stats.origSize / 1024. / 1024.
                        / (double)stats.ctimeSecs
                  << ',';
        std::cout << (double)stats.origSize / 1024. / 1024.
                        / (double)stats.dtimeSecs
                  << std::endl;
    }
}

int main(int argc, char** argv)
{
    std::string dirName = argv[1];
    std::cout << "Analyzing " << dirName << std::endl;
    std::cout
            << "Compressor Name,Dataset,Compression Ratio,Compression Speed MiBps,Decompression Speed MiBps\n";
    std::filesystem::path dirPath(dirName);
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        if (entry.is_regular_file()) {
            processFile(entry.path());
        }
    }
    return 0;
}
