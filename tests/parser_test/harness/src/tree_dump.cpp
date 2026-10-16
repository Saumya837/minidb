#include "printer.hpp"
#include "tree_dump.hpp"
#include <iostream>

namespace {

    void dumpNode(const ASTNode& node, size_t depth, std::vector<std::string>& out) {
        std::string label = tagToString(node.type);
        if (!node.value.empty()) label += ": " + node.value;

        out.push_back(std::string(depth * 2, ' ') + label);

        for (const auto& child : node.children)
            dumpNode(*child, depth + 1, out);
    }

}  // namespace

std::vector<std::string> dumpTree(const ASTNode& root) {
    std::vector<std::string> lines;
    dumpNode(root, 0, lines);
    return lines;
}



