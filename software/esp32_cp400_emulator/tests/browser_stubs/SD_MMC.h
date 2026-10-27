#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

constexpr const char *FILE_READ = "r";

struct TestNode
{
    std::string name;
    bool directory;
    std::vector<std::shared_ptr<TestNode>> children;
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
};

struct TestSd
{
    std::map<std::string, std::shared_ptr<TestNode>> nodes;
    File open(const char *path, const char *)
    {
        const auto found = nodes.find(path);
        return found == nodes.end() ? File() : File(found->second);
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
