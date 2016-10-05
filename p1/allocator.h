#include <stdexcept>
#include <string>
#include <iostream>
#include <memory>

enum class AllocErrorType {
    InvalidFree,
    NoMemory,
};

class Allocator; class Pointer; class PointerList; class AllocError;

class AllocError: std::runtime_error {
private:
    AllocErrorType type;

public:
    AllocError(AllocErrorType _type, std::string message):
            runtime_error(message),
            type(_type)
    {}

    AllocErrorType getType() const { return type; }
};

struct PointerInfo {
    size_t _offset;
    size_t _size;
    Pointer *_next;
    Pointer *_prev;
    bool _isFree;

    PointerInfo(size_t offset=0, size_t size=0, bool isFree=true, Pointer *next = nullptr, Pointer *prev = nullptr) :
        _offset(offset), _size(size), _isFree(isFree), _next(next), _prev(prev) {}
};

class Pointer {
    static void *_base;
    std::shared_ptr<PointerInfo> _innerStruct;

public:
    Pointer(size_t offset=0, size_t size=0, bool isFree=true, Pointer *next = nullptr, Pointer *prev = nullptr) {
        _innerStruct = std::make_shared<PointerInfo>(offset, size, isFree, next, prev);
    }

    void *get() const {
        if (isFree()) { return nullptr; }
        return (char *)_base + offset();
    }

    void setIsFree(bool isFree = true) {
        _innerStruct->_isFree = isFree;
    }

    void shiftF(size_t delta) {
        _innerStruct->_offset += delta;
        _innerStruct->_size -= delta;
    }

    Pointer *next() const { return _innerStruct->_next; }
    Pointer *prev() const { return _innerStruct->_prev; }

    void setNext(Pointer *ptr) { _innerStruct->_next = ptr; }
    void setPrev(Pointer *ptr) { _innerStruct->_prev = ptr; }
    void setSize(size_t size) { _innerStruct->_size = size; }
    void setOffset(size_t offset) { _innerStruct->_offset = offset; }

    void move(size_t new_offset) {
        memmove((char*)_base + new_offset, get(), size());
        setOffset(new_offset);
    }

    void copy(size_t new_offset) {
        memcpy((char*)_base + new_offset, get(), size());
    }

    void split(size_t N) {
        if (N < size()) {
            Pointer* new_empty = new Pointer(offset() + N, size() - N, true, next(), this);
            setSize(N);
            if (next() != nullptr) { next()->setPrev(new_empty); }
            setNext(new_empty);
        }
    }

    void setInnerStruct(std::shared_ptr<PointerInfo> &is) {
        _innerStruct = is;
    }

    std::shared_ptr<PointerInfo> &innerStruct() { return _innerStruct; }
    size_t offset() const { return _innerStruct->_offset; }
    size_t size() const { return _innerStruct->_size; }
    bool isFree() const { return _innerStruct->_isFree; }
    static void setBase(void *new_base) { _base = new_base; }
};


class Allocator {
    Pointer *pointers;
public:
    Allocator(void *base, size_t size) {
        Pointer::setBase(base);
        pointers = new Pointer(0, size);
    }
    ~Allocator() {
        while (pointers != nullptr) {
            Pointer *next = pointers->next();
            delete pointers;
            pointers = next;
        }
    }
    void print() {
        Pointer *ptr = pointers;
        while (ptr != nullptr) {
            std::cout << (ptr->isFree() ? "EMPTY " : "FILLED ") << ptr->offset() << " " << ptr->size() << " " << ptr->offset()+ptr->size() << std::endl;
            ptr = ptr->next();
        }
    }

    void merge(Pointer *p1, Pointer *p2) {
        Pointer *first = p1, *second = p2;

        if (first->offset() + first->size() != second->offset()) {
            print();
            std::cout << "MERGE: " << first->offset() + first->size() << " " << second->offset() << std::endl;
            std::cerr << "Wrong merge!" << std::endl;
            return;
        }

        first->setSize(first->size() + second->size());
        first->setOffset(first->offset());
        //if (first->prev() != nullptr) { first->prev()->setNext(first); }
        //else { pointers = first; }
        if (second->next() != nullptr) { second->next()->setPrev(first); }
        first->setNext(second->next());
        second->setInnerStruct(first->innerStruct());
        delete second;
    }

    Pointer alloc(size_t N) {
        Pointer *ptr;
        for (ptr = pointers; ptr != nullptr && (!ptr->isFree() || (N > ptr->size())); ptr = ptr->next()) {}
        if (ptr == nullptr) { throw AllocError(AllocErrorType::NoMemory, "can't alloc memory"); }
        if (ptr->size() != N) { ptr->split(N); }
        ptr->setIsFree(false);
        return *ptr;
    }

    void realloc(Pointer &p, size_t N) {
        if (N == p.size()) { }
        else if (N < p.size()) { p.split(N); }
        else {
            Pointer *ptr;
            if (p.next() != nullptr && p.next()->isFree() && (p.next()->size() >= N - p.size())) {
                p.next()->split(N-p.size());
                merge(p.next()->prev(), p.next());
            } else {
                Pointer pointer = alloc(N);
                if (pointer.prev() == nullptr) { ptr = pointers; }
                else { ptr = pointer.prev()->next(); }
                p.copy(ptr->offset());
                if (!p.isFree()) { free(p); }
            }
            p.setInnerStruct(ptr->innerStruct());
        }
    }

    void free(Pointer &p) {
        if (p.isFree()) { throw AllocError(AllocErrorType::InvalidFree, "Pointer is free"); }
        p.setIsFree(true);
        if (p.next() != nullptr && p.next()->isFree()) {
            merge(p.next()->prev(), p.next());
        }
        if (p.prev() != nullptr && p.prev()->isFree()) {
            merge(p.prev(), p.prev()->next());
        }
    }

    void defrag() {
        Pointer *ptr = pointers, *new_pointers = nullptr, *tail = nullptr, *next = nullptr;
        size_t offset = 0, empty = 0;
        while (ptr != nullptr) {
            if (!ptr->isFree()) {
                if (new_pointers == nullptr) {
                    new_pointers = ptr;
                } else {
                    ptr->setPrev(tail);
                    tail->setNext(ptr);
                }
                tail = ptr;
                tail->move(offset);
                offset += tail->size();
                ptr = ptr->next();
            } else {
                empty += ptr->size();
                next = ptr->next();
                delete ptr;
                ptr = next;
            }
        }
        if (empty != 0) {
            Pointer *empty_ptr = new Pointer(offset, empty, true, nullptr, tail);
            if (new_pointers == nullptr) { new_pointers = empty_ptr; }
            else { tail->setNext(empty_ptr); }
        }
        if (new_pointers != nullptr) {
            pointers = new_pointers;
        }
    }

    std::string dump() { return ""; }
};

