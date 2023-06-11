#ifndef __KDTREE_HPP__
#define __KDTREE_HPP__

#include <vector>
#include <list>

#include "scene/Scene.hpp"
#include "Ray.hpp"

#define SPHERE 1
#define TRIANGLE 2

struct Block {
    int type;
    std::vector<float> begin;
    std::vector<float> end;
    union
    {
        NRenderer::Sphere* sphere;
        NRenderer::Triangle* triangle;
    };
    Block(NRenderer::Sphere* sp) {
        type = SPHERE;
        this->sphere = sp;
        begin = std::vector<float>{ sp->position.x - sp->radius, sp->position.y - sp->radius, sp->position.z - sp->radius };
        end = std::vector<float>{ sp->position.x + sp->radius, sp->position.y + sp->radius, sp->position.z + sp->radius };
    }
    Block(NRenderer::Triangle* tr) {
        type = TRIANGLE;
        this->triangle = tr;
        begin = std::vector<float>{ std::min(tr->v1.x, std::min(tr->v2.x, tr->v3.x)),
            std::min(tr->v1.y, std::min(tr->v2.y, tr->v3.y)),
            std::min(tr->v1.z, std::min(tr->v2.z, tr->v3.z)) };

        end = std::vector<float>{ std::max(tr->v1.x, std::max(tr->v2.x, tr->v3.x)),
            std::max(tr->v1.y, std::max(tr->v2.y, tr->v3.y)),
            std::max(tr->v1.z, std::max(tr->v2.z, tr->v3.z)) };
    }
};

struct Node {
    std::vector<float> begin;
    std::vector<float> end;
    std::list<Block*> block_list;
    Node* left, * right;
    bool is_leaf_node = true;

    Node() {
        begin = std::vector<float>(3);
        end = std::vector<float>(3);
        left = right = nullptr;
    }
};

struct KDTree {
    Node* root = nullptr;
    char key = 'x';

    KDTree() { root = new Node; }

    std::list<Node*> find_node(const RayTracing::Ray& r) {
        std::list<Node*> result;
        find(r, root, result);
        return result;
    }

    void find(const RayTracing::Ray& r, Node* n, std::list<Node*>& result) {
        auto dir = 1.0f / r.direction;
        NRenderer::Vec3 begin(n->begin[0], n->begin[1], n->begin[2]);
        NRenderer::Vec3 end(n->end[0], n->end[1], n->end[2]);

        auto min_index = min((begin - r.origin) * dir, (end - r.origin) * dir);
        auto max_index = max((begin - r.origin) * dir, (end - r.origin) * dir);
       
        if (std::max(std::max(min_index.x, min_index.y), min_index.z) < std::min(std::min(max_index.x, max_index.y), max_index.z)) {
            if (n->is_leaf_node) {
                result.push_back(n);
            }
            else {
                find(r, n->left, result);
                find(r, n->right, result);
            }
        }
    }

    void insert(std::vector<NRenderer::Sphere>& sps, std::vector<NRenderer::Triangle>& trs) {
        std::vector<Block*> blocks(sps.size() + trs.size());
        int i = 0;
        for (auto& sp : sps) {
            blocks[i++] = new Block(&sp);
        }
        for (auto& tr : trs) {
            blocks[i++] = new Block(&tr);
        }
        insert_node(blocks, 0, blocks.size(), root);
    }

    void insert_node(std::vector<Block*>& blocks, int begin, int end, Node* n) {
        if (begin == end)
            return;
        else if (end - begin > 3) {
            for (int i = begin; i < end; i++) {
                for (int j = 0; j < 3; j++) {
                    n->begin[j] = std::min(blocks[i]->begin[j], n->begin[j]);
                    n->end[j] = std::max(blocks[i]->end[j], n->end[j]);
                }
            }
            n->is_leaf_node = false;
            n->left = new Node;
            n->right = new Node;
            std::sort(blocks.begin() + begin, blocks.begin() + end, [&](const Block* A, const Block* B) {
                if (key == 'x')
                    return A->begin[0] < B->begin[0];
                else if (key == 'y')
                    return A->begin[1] < B->begin[1];
                else if (key == 'z')
                    return A->begin[2] < B->begin[2];
                });
            //Ñ­»··ÖÁÑÎ¬¶È
            if (key == 'x')
                key = 'y';
            else if (key == 'y')
                key = 'z';
            else if (key == 'z')
                key = 'x';
            int mid = (begin + end) / 2;
            insert_node(blocks, begin, mid, n->left);
            insert_node(blocks, mid, end, n->right);
        }
        else {
            for (int i = begin; i < end; i++) {
                for (int j = 0; j < 3; j++) {
                    n->begin[j] = std::min(blocks[i]->begin[j], n->begin[j]);
                    n->end[j] = std::max(blocks[i]->end[j], n->end[j]);
                }
                n->block_list.push_back(blocks[i]);
            }
        }
    }
};

#endif