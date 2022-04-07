//
// Created by ringo on 2022/4/6.
//

#ifndef BREAKOUT_LINK_H
#define BREAKOUT_LINK_H

template <class T>
class Node
{
public:
    T* node;
    Node<T>* next;
    Node<T>* prev;

    bool is_erase = false;

    Node(T* t)
    {
        this->node = t;
        this->prev = nullptr;
        this->next = nullptr;
    }
    Node(T* t, Node<T>* prev)
    {
        this->node = t;
        this->prev = prev;
        this->next = nullptr;
    }
    Node(T* t, Node<T>* prev, Node<T>* next)
    {
        this->node = t;
        this->prev = prev;
        this->next = next;
    }
    ~Node()
    {
        delete this->node;
        this->node = nullptr;
    }
};

template <class T>
class Link
{
public:
    Node<T>* head;
    Node<T>* end;
    Link()
    {
        head = nullptr;
        end = nullptr;
    }
    Link(T* t)
    {
        head = new Node<T>(t);
        end = head;
    }

    void push(T* t)
    {
        if (this->head == nullptr)
        {
            head = new Node<T>(t);
            end = head;
        }
        else
        {
            Node<T>* last_end = this->end;
            this->end = new Node<T>(t, last_end);
            last_end->next = this->end;
        }
    }

    void erase(Node<T>* node)
    {
        if (node->prev == nullptr && node->next == nullptr)
        {
            this->head = nullptr;
            this->end = nullptr;
        }
        else if (node->prev == nullptr)
        {
            this->head = node->next;
            this->head->prev = nullptr;
        }
        else if (node->next == nullptr)
        {
            this->end = node->prev;
            this->end->next = nullptr;
        }
        else
        {
            Node<T>* prev = node->prev;
            Node<T>* next = node->next;
            prev->next = next;
            next->prev = prev;
        }

        delete node;
    }

    void erase(T* t)
    {
        Node<T> *next, *node;
        node = this->head;
        for(;;)
        {
            if (node == nullptr) break;
            next = node->next;
            if (node->node == t)
            {
                this->erase(node);
                break;
            }
            node = next;
        }
    }

    void pre_erase(Node<T>* node)
    {
        node->is_erase = true;
    }

    void pre_erase(T* t)
    {
        Node<T> *next, *node;
        node = this->head;
        for(;;)
        {
            if (node == nullptr) break;
            next = node->next;
            if (node->node == t)
            {
                this->pre_erase(node);
                break;
            }
            node = next;
        }
    }

    void exec_erase()
    {
        Node<T> *next, *node;
        node = this->head;
        for(;;)
        {
            if (node == nullptr) break;
            next = node->next;
            if (node->is_erase) this->erase(node);
            node = next;
        }
    }

    bool empty()
    {
        return this->head == nullptr;
    }
};

#endif //BREAKOUT_LINK_H
