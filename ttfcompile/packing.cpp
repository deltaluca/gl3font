#include <map>
#include <vector>
#include <set>
#include <cstdint>

using std::size_t;

// Best fit decreasing.
// Thanks to jakesgordon
// https://github.com/jakesgordon/bin-packing
struct Node {
    size_t x, y, w, h;
    bool used;
    Node* down;
    Node* right;
};
Node* root = nullptr;

Node* node(size_t x, size_t y, size_t w, size_t h)
{
    auto ret = new Node();
    ret->x = x;
    ret->y = y;
    ret->w = w;
    ret->h = h;
    ret->used = false;
    ret->down = ret->right = nullptr;
    return ret;
}

void destroyNode(Node* n)
{
    if (n == nullptr)
    {
        return;
    }

    destroyNode(n->down);
    destroyNode(n->right);
    delete n;
}

Node* findNode(Node* root, size_t w, size_t h)
{
    if (root->used)
    {
        auto a = findNode(root->down, w, h);
        if (a == nullptr)
        {
            a = findNode(root->right, w, h);
        }
        return a;
    }
    else if (w <= root->w && h <= root->h)
    {
        return root;
    }
    else
    {
        return nullptr;
    }
}

Node* splitNode(Node* root, size_t w, size_t h)
{
    root->used = true;
    root->down  = node(root->x,   root->y+h, root->w,   root->h-h);
    root->right = node(root->x+w, root->y,   root->w-w, h);
    return root;
}

Node* growRight(size_t w, size_t h);
Node* growDown(size_t w, size_t h);
Node* growNode(size_t w, size_t h)
{
    bool canGrowDown  = w <= root->w;
    bool canGrowRight = h <= root->h;

    // Attempt to keep squarish.
    bool shouldGrowRight = canGrowRight && (root->h >= (root->w + w));
    bool shouldGrowDown  = canGrowDown  && (root->w >= (root->h + h));

    if (shouldGrowRight)
    {
        return growRight(w,h);
    }
    else if (shouldGrowDown)
    {
        return growDown (w,h);
    }
    else if (canGrowRight)
    {
        return growRight(w,h);
    }
    else if (canGrowDown)
    {
        return growDown (w,h);
    }
    else
    {
        return nullptr;
    }
}

Node* growRight(size_t w, size_t h)
{
    auto newRoot = node(0, 0, root->w+w, root->h);
    newRoot->used = true;
    newRoot->down = root;
    newRoot->right = node(root->w, 0, w, root->h);
    root = newRoot;
    auto node = findNode(root, w, h);
    if (node != nullptr)
    {
         return splitNode(node, w, h);
    }
    else
    {
        return nullptr;
    }
}

Node* growDown(size_t w, size_t h)
{
    auto newRoot = node(0, 0, root->w, root->h+h);
    newRoot->used = true;
    newRoot->down = node(0, root->h, root->w, h);
    newRoot->right = root;
    root = newRoot;
    auto node = findNode(root, w, h);
    if (node != nullptr)
    {
         return splitNode(node, w, h);
    }
    else
    {
        return nullptr;
    }
}

void pack_boxes(const std::vector<std::pair<size_t, size_t>>& sizes, size_t gap,
                std::map<size_t, std::pair<size_t, size_t>>& packing,
                std::pair<size_t, size_t>& size)
{
    size_t N = sizes.size();
    if (N == 0)
    {
        return;
    }

    // Sort element ids by box size. (Simple brute force.)
    std::set<size_t> processed;
    std::vector<size_t> backRef;
    for (size_t i = 0; i < N; ++i)
    {
        size_t max = -1;
        size_t maxSize = 0;
        for (size_t j = 0; j < N; j++)
        {
            if (processed.find(j) != processed.end())
            {
                continue;
            }
            auto& box = sizes[j];
            size_t size = box.first > box.second ? box.first : box.second;
            if (size > maxSize || max == -1)
            {
                max = j;
                maxSize = size;
            }
        }
        backRef.push_back(max);
        processed.insert(max);
    }
    auto getBlock = [&](size_t n)
    {
        return sizes[backRef[n]];
    };

    root = node(0, 0, getBlock(0).first + gap * 2, getBlock(0).second + gap * 2);
    for (size_t i = 0; i < N; ++i)
    {
        const auto& block = getBlock(i);
        Node* node = findNode(root, block.first + gap * 2, block.second + gap * 2);
        Node* fit;
        if (node != nullptr)
        {
            fit = splitNode(node, block.first + gap * 2, block.second + gap * 2);
        }
        else
        {
            fit = growNode(block.first + gap * 2, block.second + gap * 2);
        }
        packing[backRef[i]] = std::pair<size_t,size_t>(fit->x + gap, fit->y + gap);
    }

    size.first = root->w;
    size.second = root->h;

    destroyNode(root);
}
