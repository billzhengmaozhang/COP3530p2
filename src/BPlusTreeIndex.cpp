#include "minidb/BPlusTreeIndex.h"
#include "minidb/Error.h"
#include <algorithm>
#include <iterator>

// Implement leaf lookup, sorted insertion, leaf/internal splits, and root growth.

namespace minidb
{
    BPlusTreeIndex::BPlusTreeIndex(std::size_t max_children)
        : root_(std::make_unique<Node>(Node{true, {}, {}, {}, nullptr})),
            size_(0), max_children_(max_children) {
        if (max_children_ < 3)
        {
            //throw custom error for lt 3 children case
            throw MiniDBError(ErrorCode::InvalidConfiguration, "BPlusTreeIndex max_children must be at least 3.");
        }
    }

    InsertStatus BPlusTreeIndex::insert(TradeId key, RowId row)
    {
        //precheck for dupe
        const Node* leaf = findLeaf(key);
        auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
        if (it != leaf->keys.end() && *it == key)
        {
            return InsertStatus::DuplicateKey;
        }
        auto split = insertRecursive(*root_, key, row);
        if (split.has_value())
        {
            createNewRoot(std::move(*split));
        }
        ++size_;
        return InsertStatus::Inserted;
    }

    std::optional<RowId> BPlusTreeIndex::find(TradeId key) const
    {
        const Node* leaf = findLeaf(key);
        auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
        if (it != leaf->keys.end() && *it == key)
        {
            std::size_t idx = static_cast<std::size_t>(std::distance(leaf->keys.begin(), it));
            return leaf->rows[idx];
        }
        return std::nullopt;
    }

    std::size_t BPlusTreeIndex::size() const
    {
        return size_;
    }

    std::size_t BPlusTreeIndex::maxChildren() const
    {
        return max_children_;
    }

    std::size_t BPlusTreeIndex::height() const
    {
        std::size_t h = 1;
        const Node* curr = root_.get();
        while (curr && !curr->is_leaf)
        {
            ++h;
            curr = curr->children.front().get();
        }
        return h;
    }

    bool BPlusTreeIndex::validateStructure() const
    {
        if (!root_)
        {
            return false;
        }
        std::optional<std::size_t> leaf_depth = std::nullopt;
        std::size_t entry_count = 0;
        if (!validateNode(*root_, true, 0, std::nullopt, std::nullopt, leaf_depth, entry_count))
        {
            return false;
        }
        if (entry_count != size_)
        {
            return false;
        }
        return validateLeafLinks();
    }

    const BPlusTreeIndex::Node* BPlusTreeIndex::findLeaf(TradeId key) const
    {
        const Node* curr = root_.get();
        while (!curr->is_leaf)
        {
            std::size_t idx = childFor(*curr, key);
            curr = curr->children[idx].get();
        }
        return curr;
    }

    std::size_t BPlusTreeIndex::childFor(const Node& node, TradeId key) const
    {
        auto it = std::upper_bound(node.keys.begin(), node.keys.end(), key);
        return static_cast<std::size_t>(std::distance(node.keys.begin(), it));
    }

    std::optional<BPlusTreeIndex::SplitResult> BPlusTreeIndex::insertRecursive(
        Node& node, TradeId key, RowId row)
    {
        if (node.is_leaf)
        {
            auto it = std::lower_bound(node.keys.begin(), node.keys.end(), key);
            std::size_t idx = static_cast<std::size_t>(std::distance(node.keys.begin(), it));
            node.keys.insert(it, key);
            node.rows.insert(node.rows.begin() + static_cast<std::ptrdiff_t>(idx), row);

            // leaf can have m-1 entries, so reaching m will trigger split
            if (node.keys.size() >= max_children_) {
                return splitLeaf(node);
            }
            return std::nullopt;
        }

        std::size_t child_idx = childFor(node, key);
        auto child_split = insertRecursive(*node.children[child_idx], key, row);
        if (!child_split.has_value()) {
            return std::nullopt;
        }

        node.keys.insert(
            node.keys.begin() + static_cast<std::ptrdiff_t>(child_idx),
            child_split->separator);
        node.children.insert(
            node.children.begin() + static_cast<std::ptrdiff_t>(child_idx + 1),
            std::move(child_split->right));

        // Internal node holds at most M children (M - 1 keys); reaching M + 1 children triggers a split
        if (node.children.size() > max_children_) {
            return splitInternal(node);
        }
        return std::nullopt;
    }

    BPlusTreeIndex::SplitResult BPlusTreeIndex::splitLeaf(Node& leaf) {
    auto right = std::make_unique<Node>(Node{true, {}, {}, {}, nullptr});
    std::size_t mid = leaf.keys.size() / 2; // For M entries, left gets M/2, right gets M - M/2

    right->keys.assign(leaf.keys.begin() + static_cast<std::ptrdiff_t>(mid), leaf.keys.end());
    right->rows.assign(leaf.rows.begin() + static_cast<std::ptrdiff_t>(mid), leaf.rows.end());

    leaf.keys.resize(mid);
    leaf.rows.resize(mid);

    // Update linked-leaf pointers
    right->next = leaf.next;
    leaf.next = right.get();

    // Copy right leaf's smallest key into parent as separator
    TradeId separator = right->keys.front();
    return SplitResult{separator, std::move(right)};
}

    BPlusTreeIndex::SplitResult BPlusTreeIndex::splitInternal(Node& node) {
        auto right = std::make_unique<Node>(Node{false, {}, {}, {}, nullptr});
        std::size_t mid = node.keys.size() / 2; // Promote middle separator key

        TradeId separator = node.keys[mid];

        // Move keys after mid to right internal node (separator is removed from both halves)
        right->keys.assign(node.keys.begin() + static_cast<std::ptrdiff_t>(mid + 1), node.keys.end());
        node.keys.resize(mid);

        // Move children after mid to right internal node
        right->children.reserve(node.children.size() - (mid + 1));
        for (std::size_t i = mid + 1; i < node.children.size(); ++i) {
            right->children.push_back(std::move(node.children[i]));
        }
        node.children.resize(mid + 1);

        return SplitResult{separator, std::move(right)};
    }

    void BPlusTreeIndex::createNewRoot(SplitResult split) {
        auto new_root = std::make_unique<Node>(Node{false, {}, {}, {}, nullptr});
        new_root->keys.push_back(split.separator);
        new_root->children.push_back(std::move(root_));
        new_root->children.push_back(std::move(split.right));
        root_ = std::move(new_root);
    }

    bool BPlusTreeIndex::validateNode(
        const Node& node,
        bool is_root,
        std::size_t depth,
        std::optional<TradeId> lower_inclusive,
        std::optional<TradeId> upper_exclusive,
        std::optional<std::size_t>& leaf_depth,
        std::size_t& entry_count) const
    {
        // Verify keys are strictly increasing
        for (std::size_t i = 1; i < node.keys.size(); ++i) {
            if (node.keys[i - 1] >= node.keys[i]) {
                return false;
            }
        }

        // Check key bounds against parent separators
        if (!node.keys.empty()) {
            if (lower_inclusive.has_value() && node.keys.front() < *lower_inclusive) {
                return false;
            }
            if (upper_exclusive.has_value() && node.keys.back() >= *upper_exclusive) {
                return false;
            }
        }

        if (node.is_leaf) {
            if (!node.children.empty()) return false;
            if (node.keys.size() != node.rows.size()) return false;
            if (node.keys.size() > max_children_ - 1) return false;

            if (!is_root) {
                // Non-root leaf has at least ceil((M - 1) / 2) == M / 2 entries
                std::size_t min_entries = max_children_ / 2;
                if (node.keys.size() < min_entries) return false;
            }

            // All leaves must be at the exact same depth
            if (!leaf_depth.has_value()) {
                leaf_depth = depth;
            } else if (*leaf_depth != depth) {
                return false;
            }

            entry_count += node.keys.size();
            return true;
        }

        // Internal node checks
        if (!node.rows.empty() || node.next != nullptr) return false;
        if (node.children.size() != node.keys.size() + 1) return false;
        if (node.children.size() > max_children_) return false;

        if (is_root) {
            if (node.children.size() < 2) return false;
        } else {
            // Non-root internal node has at least ceil(M / 2) == (M + 1) / 2 children
            std::size_t min_children = (max_children_ + 1) / 2;
            if (node.children.size() < min_children) return false;
        }

        for (std::size_t i = 0; i < node.children.size(); ++i) {
            if (!node.children[i]) return false;
            std::optional<TradeId> child_lower = (i == 0) ? lower_inclusive : node.keys[i - 1];
            std::optional<TradeId> child_upper = (i == node.keys.size()) ? upper_exclusive : node.keys[i];

            if (!validateNode(*node.children[i], false, depth + 1,
                              child_lower, child_upper, leaf_depth, entry_count)) {
                return false;
            }

            // Design doc invariant: "Each separator equals the smallest key in its right child subtree."
            if (i > 0) {
                const Node* left_most = node.children[i].get();
                while (!left_most->is_leaf) {
                    left_most = left_most->children.front().get();
                }
                if (left_most->keys.empty() || node.keys[i - 1] != left_most->keys.front()) {
                    return false;
                }
            }
        }

        return true;
    }

    bool BPlusTreeIndex::validateLeafLinks() const {
        const Node* first_leaf = root_.get();
        while (first_leaf && !first_leaf->is_leaf) {
            first_leaf = first_leaf->children.front().get();
        }

        std::size_t linked_entries = 0;
        std::optional<TradeId> prev_key = std::nullopt;
        const Node* curr = first_leaf;
        std::size_t leaf_visits = 0;

        while (curr != nullptr) {
            if (!curr->is_leaf) return false;
            for (TradeId k : curr->keys) {
                if (prev_key.has_value() && k <= *prev_key) {
                    return false;
                }
                prev_key = k;
                ++linked_entries;
            }
            curr = curr->next;
            ++leaf_visits;
            if (leaf_visits > size_ + 1) {
                return false; // Cycle guard
            }
        }

        return linked_entries == size_;
    }

}
