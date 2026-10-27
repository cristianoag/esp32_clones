#pragma once
#include <algorithm>
#include <stdint.h>
#include <string.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

constexpr const char *FILE_READ = "r";
constexpr const char *FILE_WRITE = "w";

struct TestNode
{
    std::string name;
    bool directory;
    std::vector<std::shared_ptr<TestNode>> children;
    std::vector<uint8_t> data;
    TestNode(const std::string &value, bool isDirectory) : name(value), directory(isDirectory) {}
};

class File
{
    std::shared_ptr<TestNode> node;
    size_t index = 0;
public:
    File() = default;
    explicit File(std::shared_ptr<TestNode> value) : node(value) {}
    explicit operator bool() const { return !!node; }
    bool isDirectory() const { return node && node->directory; }
    const char *name() const { return node->name.c_str(); }
    File openNextFile()
    {
        return node && index < node->children.size() ? File(node->children[index++]) : File();
    }
    bool seek(size_t offset)
    {
        if (!node || offset > node->data.size()) return false;
        index = offset;
        return true;
    }
    size_t read(uint8_t *output, size_t count)
    {
        if (!node || node->directory) return 0;
        count = std::min(count, node->data.size() - index);
        if (count) memcpy(output, node->data.data() + index, count);
        index += count;
        return count;
    }
    size_t write(const uint8_t *input, size_t count)
    {
        if (!node || node->directory) return 0;
        node->data.resize(std::max(node->data.size(), index + count));
        memcpy(node->data.data() + index, input, count);
        index += count;
        return count;
    }
    void flush() {}
    void close() { node.reset(); }
};

struct TestSd
{
    std::map<std::string, std::shared_ptr<TestNode>> nodes;
    std::vector<std::string> openedPaths;
    std::string failOpenPath;
    File open(const char *path, const char *mode)
    {
        openedPaths.emplace_back(path);
        if (failOpenPath == path) return File();
        const auto found = nodes.find(path);
        if (!strcmp(mode, FILE_WRITE))
        {
            if (found == nodes.end()) add("/", path + 1, false);
            nodes[path]->data.clear();
            return File(nodes[path]);
        }
        return found == nodes.end() ? File() : File(found->second);
    }
    bool exists(const char *path) const { return nodes.count(path) != 0; }
    bool remove(const char *path)
    {
        if (!nodes.count(path)) return false;
        const auto node = nodes[path];
        for (auto &item : nodes)
        {
            auto &children = item.second->children;
            children.erase(std::remove(children.begin(), children.end(), node), children.end());
        }
        nodes.erase(path);
        return true;
    }
    void add(const char *parent, const std::string &name, bool directory)
    {
        const std::string path = std::string(parent) + (std::string(parent) == "/" ? "" : "/") + name;
        const auto child = std::make_shared<TestNode>(name, directory);
        nodes[parent]->children.push_back(child);
        nodes[path] = child;
    }
};
extern TestSd SD_MMC;
