#pragma once
#include "Disco.h"
#include <vector>
#include <algorithm>
#include <stack>

template <class T>
struct Node {
    Node<T>* nodes[2];
    T value;
    int height;
    int hd;
    std::vector<Sector*> sectores;

    Node(T v, Sector* s) {
        value = v;
        nodes[0] = nodes[1] = nullptr;
        height = 1;
        hd = 0;
        sectores.push_back(s);
    }
};

template <class T>
struct AVL {

    bool insertar(T valor, Sector* sector, Node<T>*& raiz) {
        Node<T>** p_search = &raiz;

        std::vector<Node<T>**> path;
        while (*p_search && (*p_search)->value != valor) {
            path.push_back(p_search);
            p_search = &((*p_search)->nodes[valor > (*p_search)->value]);
        }

        if (*p_search) {
            (*p_search)->sectores.push_back(sector);
            return false;
        }

        *p_search = new Node<T>(valor, sector);

        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            Node<T>* actual = **it;
            int lh = actual->nodes[0] ? actual->nodes[0]->height : 0;
            int rh = actual->nodes[1] ? actual->nodes[1]->height : 0;
            actual->height = std::max(lh, rh) + 1;
            actual->hd = rh - lh;
            int heightdiff = actual->hd;

            if (heightdiff == 2 && actual->nodes[1]) {
                if (actual->nodes[1]->hd == 1) {
                    std::swap(actual->value, actual->nodes[1]->value);
                    std::swap(actual->sectores, actual->nodes[1]->sectores);
                    Node<T>* tmp = actual->nodes[1];
                    Node<T>* a = actual->nodes[0];
                    int arh = a && a->nodes[1] ? a->nodes[1]->height : 0;
                    int ch = actual->nodes[1]->nodes[1] ? actual->nodes[1]->nodes[1]->height : 0;
                    actual->nodes[1] = actual->nodes[1]->nodes[1];
                    actual->nodes[0] = tmp;
                    tmp->nodes[1] = tmp->nodes[0];
                    tmp->nodes[0] = a;
                    tmp->height = std::max(lh, arh) + 1;
                    actual->height = std::max(tmp->height, ch) + 1;
                    break;
                }
                else if (actual->nodes[1]->hd == -1) {
                    std::swap(actual->value, actual->nodes[1]->nodes[0]->value);
                    std::swap(actual->sectores, actual->nodes[1]->nodes[0]->sectores);
                    Node<T>* tmp = actual->nodes[0];
                    Node<T>* a = actual->nodes[1]->nodes[0];
                    Node<T>* ar = a->nodes[1];
                    int arh = ar ? ar->height : 0;
                    int alh = a->nodes[0] ? a->nodes[0]->height : 0;
                    int ch = actual->nodes[1]->nodes[1] ? actual->nodes[1]->nodes[1]->height : 0;
                    int blh = tmp ? tmp->height : 0;
                    actual->nodes[0] = a;
                    a->nodes[1] = a->nodes[0];
                    a->nodes[0] = tmp;
                    actual->nodes[1]->nodes[0] = ar;
                    a->height = std::max(blh, alh) + 1;
                    actual->nodes[1]->height = std::max(ch, arh) + 1;
                    actual->height = std::max(actual->nodes[1]->height, a->height) + 1;
                    break;
                }
            }
            else if (heightdiff == -2 && actual->nodes[0]) {
                if (actual->nodes[0]->hd == -1) {
                    std::swap(actual->value, actual->nodes[0]->value);
                    std::swap(actual->sectores, actual->nodes[0]->sectores);
                    Node<T>* tmp = actual->nodes[0];
                    Node<T>* b = actual->nodes[1];
                    int blh = b && b->nodes[0] ? b->nodes[0]->height : 0;
                    int ah = actual->nodes[0]->nodes[0] ? actual->nodes[0]->nodes[0]->height : 0;
                    actual->nodes[0] = actual->nodes[0]->nodes[0];
                    actual->nodes[1] = tmp;
                    tmp->nodes[0] = tmp->nodes[1];
                    tmp->nodes[1] = b;
                    tmp->height = std::max(lh, blh) + 1;
                    actual->height = std::max(tmp->height, ah) + 1;
                    break;
                }
                else if (actual->nodes[0]->hd == 1) {
                    std::swap(actual->value, actual->nodes[0]->nodes[1]->value);
                    std::swap(actual->sectores, actual->nodes[0]->nodes[1]->sectores);
                    Node<T>* tmp = actual->nodes[1];
                    Node<T>* a = actual->nodes[0]->nodes[1];
                    Node<T>* ar = a->nodes[0];
                    int arh = ar ? ar->height : 0;
                    int alh = a->nodes[1] ? a->nodes[1]->height : 0;
                    int ch = actual->nodes[0]->nodes[0] ? actual->nodes[0]->nodes[0]->height : 0;
                    int blh = tmp ? tmp->height : 0;
                    actual->nodes[1] = a;
                    a->nodes[0] = a->nodes[1];
                    a->nodes[1] = tmp;
                    actual->nodes[0]->nodes[1] = ar;
                    a->height = std::max(blh, alh) + 1;
                    actual->nodes[0]->height = std::max(ch, arh) + 1;
                    actual->height = std::max(actual->nodes[0]->height, a->height) + 1;
                    break;
                }
            }
        }
        return true;
    }

    std::vector<Sector*>* buscar(T x, Node<T>*& root) {
        Node<T>** p;
        for (p = &root; *p && (*p)->value != x; p = &((*p)->nodes[x > (*p)->value]));
        if (!*p) return nullptr;
        return &(*p)->sectores;
    }

    void buscar_rango(T from, T to, Node<T>* root, std::vector<Sector*>& result) {
        if (!root) return;

        if (root->value > from)
            buscar_rango(from, to, root->nodes[0], result);

        if (root->value >= from && root->value <= to)
            result.insert(result.end(), root->sectores.begin(), root->sectores.end());

        if (root->value < to)
            buscar_rango(from, to, root->nodes[1], result);
    }

    void liberar(Node<T>*& raiz) {
        if (!raiz) return;
        liberar(raiz->nodes[0]);
        liberar(raiz->nodes[1]);
        delete raiz;
        raiz = nullptr;
    }
};