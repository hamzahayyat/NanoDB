#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>

// ─────────────────────────────────────────────
//  AVL Tree – self-balancing BST
//  Key: int (e.g. c_custkey)
//  Value: uint32_t page id + int row offset
// ─────────────────────────────────────────────

struct AVLRecord {
    uint32_t pageId;
    int      rowOffset;
};

struct AVLNode {
    int       key;
    AVLRecord record;
    AVLNode*  left;
    AVLNode*  right;
    int       height;

    AVLNode(int k, AVLRecord r)
        : key(k), record(r), left(nullptr), right(nullptr), height(1) {}
};

class AVLTree {
public:
    AVLTree() : root(nullptr), nodeCount(0) {}
    ~AVLTree() { destroyAll(root); }

    void    insert(int key, AVLRecord rec) { root = insertNode(root, key, rec); ++nodeCount; }
    void    remove(int key)                { root = deleteNode(root, key); }

    // Returns nullptr if not found
    AVLRecord* search(int key) {
        AVLNode* n = find(root, key);
        return n ? &n->record : nullptr;
    }

    // Range scan [lo, hi] – calls cb(key, record) for each match
    template <typename Callback>
    void rangeScan(int lo, int hi, Callback cb) {
        rangeHelper(root, lo, hi, cb);
    }

    int  count() const { return nodeCount; }

    void printInOrder() const { inOrder(root); }

private:
    AVLNode* root;
    int      nodeCount;

    int height(AVLNode* n) { return n ? n->height : 0; }

    int balanceFactor(AVLNode* n) {
        return n ? height(n->left) - height(n->right) : 0;
    }

    void updateHeight(AVLNode* n) {
        if (!n) return;
        int lh = height(n->left), rh = height(n->right);
        n->height = (lh > rh ? lh : rh) + 1;
    }

    AVLNode* rotateRight(AVLNode* y) {
        AVLNode* x  = y->left;
        AVLNode* T2 = x->right;
        x->right = y;
        y->left  = T2;
        updateHeight(y);
        updateHeight(x);
        return x;
    }

    AVLNode* rotateLeft(AVLNode* x) {
        AVLNode* y  = x->right;
        AVLNode* T2 = y->left;
        y->left  = x;
        x->right = T2;
        updateHeight(x);
        updateHeight(y);
        return y;
    }

    AVLNode* balance(AVLNode* n) {
        updateHeight(n);
        int bf = balanceFactor(n);

        // Left-Left
        if (bf > 1 && balanceFactor(n->left) >= 0)
            return rotateRight(n);
        // Left-Right
        if (bf > 1 && balanceFactor(n->left) < 0) {
            n->left = rotateLeft(n->left);
            return rotateRight(n);
        }
        // Right-Right
        if (bf < -1 && balanceFactor(n->right) <= 0)
            return rotateLeft(n);
        // Right-Left
        if (bf < -1 && balanceFactor(n->right) > 0) {
            n->right = rotateRight(n->right);
            return rotateLeft(n);
        }
        return n;
    }

    AVLNode* insertNode(AVLNode* n, int key, AVLRecord rec) {
        if (!n) return new AVLNode(key, rec);
        if (key < n->key)      n->left  = insertNode(n->left,  key, rec);
        else if (key > n->key) n->right = insertNode(n->right, key, rec);
        else { n->record = rec; --nodeCount; return n; }  // update
        return balance(n);
    }

    AVLNode* minNode(AVLNode* n) {
        while (n->left) n = n->left;
        return n;
    }

    AVLNode* deleteNode(AVLNode* n, int key) {
        if (!n) return nullptr;
        if (key < n->key)      n->left  = deleteNode(n->left,  key);
        else if (key > n->key) n->right = deleteNode(n->right, key);
        else {
            if (!n->left || !n->right) {
                AVLNode* child = n->left ? n->left : n->right;
                delete n;
                --nodeCount;
                return child;
            }
            AVLNode* succ = minNode(n->right);
            n->key    = succ->key;
            n->record = succ->record;
            n->right  = deleteNode(n->right, succ->key);
        }
        return balance(n);
    }

    AVLNode* find(AVLNode* n, int key) {
        if (!n) return nullptr;
        if (key == n->key) return n;
        if (key < n->key)  return find(n->left, key);
        return find(n->right, key);
    }

    template <typename Callback>
    void rangeHelper(AVLNode* n, int lo, int hi, Callback cb) {
        if (!n) return;
        if (n->key > lo) rangeHelper(n->left, lo, hi, cb);
        if (n->key >= lo && n->key <= hi) cb(n->key, n->record);
        if (n->key < hi) rangeHelper(n->right, lo, hi, cb);
    }

    void inOrder(AVLNode* n) const {
        if (!n) return;
        inOrder(n->left);
        std::printf("  key=%d  page=%u  offset=%d\n", n->key, n->record.pageId, n->record.rowOffset);
        inOrder(n->right);
    }

    void destroyAll(AVLNode* n) {
        if (!n) return;
        destroyAll(n->left);
        destroyAll(n->right);
        delete n;
    }
};
