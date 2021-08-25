#pragma once

#include <util/types.h>
#include <util/logger.h>

/**
 * Class encapsulating generic list
 */
template<typename T>
class List {

public:
    
    List() {};
    
    ~List() {

        // iterate and destroy all nodes
        Node *current = first;
        while(current != nullptr) {
            Node *toRemove = current;
            current = current->next;
            delete toRemove;
        }

    }

    void appendBack(T& newElement) {

        // create new node
        Node *newNode = new Node(newElement);

        // insert
        if(count == 0) {
            first = newNode;
            last = newNode;
        }
        else {
            last->next = newNode;
            newNode->previous = last;
            last = newNode;
        }

        // increment count
        count++;
    
    };

    void appendBack(T&& newElement) {

        // create new node
        Node *newNode = new Node(newElement);

        // insert
        if(count == 0) {
            first = newNode;
            last = newNode;
        }
        else {
            last->next = newNode;
            newNode->previous = last;
            last = newNode;
        }

        // increment count
        count++;

    };

    void appendFront(T& newElement) {

        // create new node
        Node *newNode = new Node(newElement);

        // insert
        if(count == 0) {
            first = newNode;
            last = newNode;
        }
        else {
            first->previous = newNode;
            newNode->next = first;
            first = newNode;
        }

        // increment count
        count++;

    };

    void appendFront(T&& newElement) {

        // create new node
        Node *newNode = new Node(newElement);

        // insert
        if(count == 0) {
            first = newNode;
            last = newNode;
        }
        else {
            first->previous = newNode;
            newNode->next = first;
            first = newNode;
        }

        // increment count
        count++;

    };

    void insertAt(T& newElement, usz index) {

        // if the list is empty or insertion would take place outside the list, just append back
        if(count == 0 || index >= count) appendBack(newElement);
        // if element is inserted at index 0, append it frontally
        else if(index == 0) appendFront(newElement);
        // otherwise insertion takes place in the middle of the list
        else {

            // get element before which insertion will pe performed
            Node *node = getReference(index);

            // create new node
            Node *newNode = new Node(newElement);

            // insert it
            newNode->next = node;
            newNode->previous = node->previous;
            node->previous->next = newNode;
            node->previous = newNode;

        }

    }

    void remove(usz index) {

        // get reference to element
        Node *nodeToRemove = getReference(index);

        // remove references to neighbour elements
        if(nodeToRemove->previous == nullptr) {
            // first element in list
            if(nodeToRemove->next != nullptr) nodeToRemove->next->previous = nullptr;
            first = nodeToRemove->next;
        }
        else if(nodeToRemove->next == nullptr) {
            // last element in list
            if(nodeToRemove->previous != nullptr) nodeToRemove->previous->next = nullptr;
            last = nodeToRemove->previous;
        }
        else {
            // element in the middle
            nodeToRemove->previous->next = nodeToRemove->next;
            nodeToRemove->next->previous = nodeToRemove->previous;
        }

        // decrement count
        count--;

        // free memory after node
        delete nodeToRemove;

    };

    usz size() const {
        return count;
    };

    T& get(usz index) {
        return getReference(index)->value;
    };

    const T& operator[](usz index) const {
        return get(index);
    };

    T& operator [](usz index) {
        return get(index);
    };


private:

    struct Node {
        Node(T & val) : value(val) {};

        T value;
        Node *previous;
        Node *next;
    };

    Node *first = nullptr;
    Node *last = nullptr;
    usz count = 0;

    Node *getReference(usz index) {

        // check bounds
        if(index >= count) {
            Logger::printFormat("[list] index out of range, aborting...\n");
            for(;;);
            // TODO: panic! 
        }

        // slight optimisation: check from where is shorter iteration
        bool directionForward = true;
        if(index > count / 2) directionForward = false;

        // iterate and return value
        Node *current = (directionForward) ? first : last;
        usz currentIndex = (directionForward) ? 0 : count - 1;
        while(currentIndex != index) {

            current = (directionForward) ? current->next : current->previous;
            if(directionForward) currentIndex++;
            else currentIndex--;

        }

        // return value
        return current;

    };

};
