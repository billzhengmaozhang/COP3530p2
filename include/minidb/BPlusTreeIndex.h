#pragma once
#include "IIndex.h"
#include <memory>
#include <vector>

namespace minidb {
class BPlusTreeIndex : public IIndex {
public:
    explicit BPlusTreeIndex(std::size_t max_children = 32);
    InsertStatus insert(TradeId key, RowId row) override;
    std::optional<RowId> find(TradeId key) const override;
    std::size_t size() const override;

    std::size_t maxChildren() const;
    std::size_t height() const;
    bool validateStructure() const;

private:
    struct Node {
        bool is_leaf;
        std::vector<TradeId> keys;
        std::vector<RowId> rows; // Leaves only; aligned with keys.
        std::vector<std::unique_ptr<Node>> children; // Internal nodes only.
        Node* next = nullptr; // Non-owning leaf link.
    };

    struct SplitResult {
        TradeId separator;
        std::unique_ptr<Node> right;
    };

    const Node* findLeaf(TradeId key) const;
    std::size_t childFor(const Node& node, TradeId key) const;
    // Called after the public duplicate precheck. Returns a propagated split.
    std::optional<SplitResult> insertRecursive(Node& node, TradeId key, RowId row);
    SplitResult splitLeaf(Node& leaf);
    SplitResult splitInternal(Node& node);
    void createNewRoot(SplitResult split);
    bool validateNode(const Node& node, bool is_root, std::size_t depth,
                      std::optional<TradeId> lower_inclusive,
                      std::optional<TradeId> upper_exclusive,
                      std::optional<std::size_t>& leaf_depth,
                      std::size_t& entry_count) const;
    bool validateLeafLinks() const;

    std::unique_ptr<Node> root_;
    std::size_t size_ = 0;
    std::size_t max_children_;
};
} // namespace minidb
