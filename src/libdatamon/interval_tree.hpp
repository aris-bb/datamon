#pragma once

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

namespace datamon {

//! @brief An augmented interval tree built on top of an AVL tree. An AVL tree
//! is more suitable since datamon is lookup-heavy with few insertions and
//! removals. This implementation supports duplicate keys.
//! @tparam TValue The value type to be stored in the tree.
//! @tparam TKey The key type to be used for interval start points.
template <typename TValue, typename TKey = uintptr_t>
class IntervalTree {
 public:
  struct Interval {
    TKey start, end;
    TValue value;
    size_t id;  // unique id for each interval. this allows us to specify
                // which interval we want to delete in case of duplicate
                // interval start points.
                // TODO: we currently use the id only to identify the
                // interval we want to delete. we could probably also use it
                // to delete nodes in O(1) time
  };

  size_t insert(Interval i) {
    i.id = next_id_++;
    id_to_node_[i.id] = i;
    root_ = insert(std::move(root_), i);
    return i.id;
  }

  void erase(size_t id) {
    if (id_to_node_.find(id) != id_to_node_.end()) {
      TKey key = id_to_node_[id].start;
      root_ = erase(std::move(root_), key, id);
    }
  }

  std::vector<Interval> query(TKey point) const { return query(root_, point); }

  bool empty() const { return !root_; }

 private:
  struct Node {
    std::vector<Interval> intervals;
    int height;
    TKey max_end;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;

    Node(Interval i)
        : height(1), max_end(i.end), left(nullptr), right(nullptr) {
      intervals.push_back(i);
    }
  };

  std::unique_ptr<Node> root_;

  int get_balance(const std::unique_ptr<Node>& node) const {
    if (!node) {
      return 0;
    }
    return height(node->left) - height(node->right);
  }

  int height(const std::unique_ptr<Node>& node) const {
    return node ? node->height : 0;
  }

  TKey max_end(const std::unique_ptr<Node>& node) const {
    return node ? node->max_end : 0;
  }

  void update_max_end(const std::unique_ptr<Node>& node) const {
    // update the max_end, taking into account the maximum end point of all
    // intervals stored in the current node
    node->max_end = node->intervals.front().end;
    for (const auto& interval : node->intervals) {
      node->max_end = std::max(node->max_end, interval.end);
    }
    node->max_end =
        std::max({node->max_end, max_end(node->left), max_end(node->right)});
  }

  std::unique_ptr<Node> rotate_left(std::unique_ptr<Node> node) {
    std::unique_ptr<Node> right = std::move(node->right);
    node->right = std::move(right->left);
    right->left = std::move(node);
    right->left->height =
        std::max(height(right->left->left), height(right->left->right)) + 1;
    right->height = std::max(height(right->left), height(right->right)) + 1;
    update_max_end(right);
    return right;
  }

  std::unique_ptr<Node> rotate_right(std::unique_ptr<Node> node) {
    std::unique_ptr<Node> left = std::move(node->left);
    node->left = std::move(left->right);
    left->right = std::move(node);
    left->right->height =
        std::max(height(left->right->left), height(left->right->right)) + 1;
    left->height = std::max(height(left->left), height(left->right)) + 1;
    update_max_end(left);
    return left;
  }

  std::unique_ptr<Node> insert(std::unique_ptr<Node> node, Interval i) {
    // create a new node if we've reached a leaf and return it up the call chain
    if (!node) {
      return std::make_unique<Node>(i);
    }

    if (i.start < node->intervals.front().start) {
      // if the key to be inserted is smaller than the current node, go left
      node->left = insert(std::move(node->left), i);
    } else if (i.start > node->intervals.front().start) {
      // if the key to be inserted is greater than the current node, go right
      node->right = insert(std::move(node->right), i);
    } else {
      // otherwise, we have a duplicate interval start key, so add it to the
      // list of intervals
      node->intervals.push_back(i);
    }

    // update the height of the current node
    node->height = 1 + std::max(height(node->left), height(node->right));

    // update max end
    update_max_end(node);

    // get the balance factor of the current node
    int balance = height(node->left) - height(node->right);

    // if the node is unbalanced, there are 4 cases to consider

    // left left case
    if (balance > 1 && i.start < node->left->intervals.front().start) {
      return rotate_right(std::move(node));
    }

    // right right case
    if (balance < -1 && i.start > node->right->intervals.front().start) {
      return rotate_left(std::move(node));
    }

    // left right case
    if (balance > 1 && i.start > node->left->intervals.front().start) {
      node->left = rotate_left(std::move(node->left));
      return rotate_right(std::move(node));
    }

    // right left case
    if (balance < -1 && i.start < node->right->intervals.front().start) {
      node->right = rotate_right(std::move(node->right));
      return rotate_left(std::move(node));
    }

    return node;
  }

  Node* min_value_node(Node* node) {
    Node* current = node;

    // loop down to find the leftmost leaf
    while (current->left != nullptr) {
      current = current->left.get();
    }

    return current;
  }

  std::unique_ptr<Node> erase(std::unique_ptr<Node> root, TKey key, size_t id) {
    // standard BST deletion
    if (root == nullptr) {
      return root;
    }

    // if the key to be deleted is smaller than the
    // root's key, then it lies in left subtree
    if (key < root->intervals.front().start) {
      root->left = erase(std::move(root->left), key, id);
    }

    // if the key to be deleted is greater than the
    // root's key, then it lies in right subtree
    else if (key > root->intervals.front().start) {
      root->right = erase(std::move(root->right), key, id);
    }

    // if key is same as root's key, then this is the node
    // to be deleted
    else {
      if (root->intervals.size() > 1) {
        // multiple intervals within the same node, just remove the interval

        // remove the interval with the given id
        for (int i = 0; i < root->intervals.size(); ++i) {
          if (root->intervals[i].id == id) {
            root->intervals.erase(root->intervals.begin() + i);
            id_to_node_.erase(id);
            break;
          }
        }
      } else {
        // the node has only one interval, delete the node itself

        if ((root->left == nullptr) || (root->right == nullptr)) {
          // node with only one child or no child

          std::unique_ptr<Node>& temp = root->left ? root->left : root->right;

          if (temp == nullptr) {
            // no child case
            root = nullptr;
          } else {
            // one child case
            root = std::move(root->left ? root->left : root->right);
          }
        } else {
          // node with two children: get the inorder
          // successor (smallest in the right subtree)
          Node* temp = min_value_node(root->right.get());

          // copy the inorder successor's data to this node
          root->intervals.front() = temp->intervals.front();

          // delete the inorder successor
          root->right =
              erase(std::move(root->right), temp->intervals.front().start, id);
        }
      }
    }

    // if the tree had only one node then return
    if (root == nullptr) {
      return root;
    }

    // update height of the current node
    root->height = 1 + std::max(height(root->left), height(root->right));

    // update max end
    update_max_end(root);

    // get the balance factor
    int balance = get_balance(root);

    // if this node becomes unbalanced, then there are 4 cases

    // left left case
    if (balance > 1 && get_balance(root->left) >= 0) {
      return rotate_right(std::move(root));
    }

    // left right case
    if (balance > 1 && get_balance(root->left) < 0) {
      root->left = rotate_left(std::move(root->left));
      return rotate_right(std::move(root));
    }

    // right right case
    if (balance < -1 && get_balance(root->right) <= 0) {
      return rotate_left(std::move(root));
    }

    // right left case
    if (balance < -1 && get_balance(root->right) > 0) {
      root->right = rotate_right(std::move(root->right));
      return rotate_left(std::move(root));
    }

    return root;
  }

  std::vector<Interval> query(const std::unique_ptr<Node>& node,
                              TKey point) const {
    std::vector<Interval> result;

    if (node == nullptr) {
      return result;
    }

    if (node->intervals.front().start <= point) {
      for (const auto& interval : node->intervals) {
        if (point <= interval.end) {
          result.push_back(interval);
        }
      }
    }

    // check both left and right subtrees instead of checking the right one only
    // if left is unsuitable. this is because otherwise this case will fail:
    //       [25, 35]
    //         /  \
    //        /    \
    //   [15, 45] [30, 40]
    // and we search for point 35. it lies in both left and right subtrees,
    // so it will miss the right subtree if we don't check the right too

    if (node->left != nullptr && node->left->max_end >= point) {
      std::vector<Interval> left_intervals = query(node->left, point);
      result.insert(result.end(), left_intervals.begin(), left_intervals.end());
    }

    if (node->right != nullptr && node->intervals.front().start <= point) {
      std::vector<Interval> right_intervals = query(node->right, point);
      result.insert(result.end(), right_intervals.begin(),
                    right_intervals.end());
    }

    return result;
  }

  // keep track of the unique id for each interval
  size_t next_id_ = 0;
  std::unordered_map<size_t, Interval> id_to_node_;
};

}  // namespace datamon