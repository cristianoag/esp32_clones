#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstring>

constexpr const char *FILE_READ = "r";
constexpr int SDMMC_FREQ_DEFAULT = 20000;
struct ProfileTestNode
{
    std::string name, data;
    size_t size = 0;
    bool directory = false;
    std::vector<std::shared_ptr<ProfileTestNode>> children;
};
class File
{
    std::shared_ptr<ProfileTestNode> node;
    size_t cursor = 0;
public:
    File() = default;
    explicit File(std::shared_ptr<ProfileTestNode> value) : node(value) {}
    explicit operator bool() const { return !!node; }
    bool isDirectory() const { return node && node->directory; }
    size_t size() const { return node ? node->size : 0; }
    const char *name() const { return node->name.c_str(); }
    size_t readBytes(char *output, size_t count)
    {
        if (!node || count > node->data.size()) return 0;
        memcpy(output, node->data.data(), count);
        return count;
    }
    File openNextFile()
    {
        return node && cursor < node->children.size() ? File(node->children[cursor++]) : File();
    }
};
struct ProfileTestSd
{
    std::map<std::string, std::shared_ptr<ProfileTestNode>> nodes;
    bool mounted = false;
    void end() { mounted = false; }
    bool setPins(int clk, int cmd, int d0) { return clk == 39 && cmd == 38 && d0 == 40; }
    bool begin(const char *, bool, bool, int, unsigned) { mounted = true; return true; }
    File open(const char *path, const char * = FILE_READ)
    {
        const auto found = nodes.find(path);
        return mounted && found != nodes.end() ? File(found->second) : File();
    }
    bool exists(const char *path) { return mounted && nodes.count(path); }
    void add(const std::string &path, size_t size, const std::string &data = "", bool directory = false)
    {
        auto node = std::make_shared<ProfileTestNode>();
        node->name = path.substr(path.find_last_of('/') + 1);
        node->size = size;
        node->data = data;
        node->directory = directory;
        nodes[path] = node;
        const auto parent = nodes.find(path.substr(0, path.find_last_of('/')));
        if (parent != nodes.end()) parent->second->children.push_back(node);
    }
};
extern ProfileTestSd SD_MMC;
