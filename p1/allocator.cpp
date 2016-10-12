#include "allocator.h"

char * Pointer::_base = nullptr;

void * Pointer::get() const {
    if (isFree()) { return nullptr; }
    return _base + offset();
}

void Pointer::move(size_t new_offset) {
    memmove(_base + new_offset, get(), size());
    setOffset(new_offset);
}

void Pointer::split(size_t N) {
    if (N < size()) {
        Pointer* new_empty = new Pointer(offset() + N, size() - N, true, next(), this);
        setSize(N);
        if (next() != nullptr) { next()->setPrev(new_empty); }
        setNext(new_empty);
    }
}

void Pointer::merge() {
    Pointer *second = next();
    if (second != nullptr) {
        if (offset() + size() != second->offset()) {
            std::cerr << "Wrong merge!" << std::endl;
            return;
        }
        setSize(size() + second->size());
        if (second->next() != nullptr) { second->next()->setPrev(this); }
        setNext(second->next());
        second->setInnerStruct(innerStruct());
        delete second;
    }
}

Allocator::~Allocator() {
    while (pointers != nullptr) {
        Pointer *next = pointers->next();
        delete pointers;
        pointers = next;
    }
}

void Allocator::print() {
    Pointer *ptr = pointers;
    while (ptr != nullptr) {
        std::cout << (ptr->isFree() ? "EMPTY " : "FILLED ") << ptr->offset() << " " << ptr->size() << " " << ptr->offset()+ptr->size() << std::endl;
        ptr = ptr->next();
    }
}

Pointer Allocator::alloc(size_t N) {
    Pointer *ptr;
    for (ptr = pointers; ptr != nullptr && (!ptr->isFree() || (N > ptr->size())); ptr = ptr->next()) {}
    if (ptr == nullptr) { throw AllocError(AllocErrorType::NoMemory, "can't alloc memory"); }
    if (ptr->size() != N) { ptr->split(N); }
    ptr->setIsFree(false);
    return *ptr;
}

void Allocator::realloc(Pointer &p, size_t N) {
    if (N == p.size()) { }
    else if (N < p.size()) { p.split(N); }
    else {
        Pointer *ptr;
        if (p.next() != nullptr && p.next()->isFree() && (p.next()->size() >= N - p.size())) {
            p.next()->split(N-p.size());
            p.merge();
        } else {
            Pointer pointer = alloc(N);
            if (pointer.prev() == nullptr) { ptr = pointers; }
            else { ptr = pointer.prev()->next(); }
            p.copy(ptr->offset());
            if (!p.isFree()) { free(p); }
            p.setInnerStruct(ptr->innerStruct());
        }
    }
}

void Allocator::free(Pointer &p) {
    if (p.isFree()) { throw AllocError(AllocErrorType::InvalidFree, "Pointer is free"); }
    p.setIsFree(true);
    if (p.next() != nullptr && p.next()->isFree()) { p.merge(); }
    if (p.prev() != nullptr && p.prev()->isFree()) { p.prev()->merge(); }
}

void Allocator::defrag() {
    Pointer *ptr = pointers, *new_pointers = nullptr, *tail = nullptr, *next = nullptr;
    size_t offset = 0, empty = 0;
    while (ptr != nullptr) {
        if (!ptr->isFree()) {
            if (new_pointers == nullptr) { new_pointers = ptr; }
            else {
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
    if (new_pointers != nullptr) { pointers = new_pointers; }
}

std::string Allocator::dump() {
    std::string d(_base, _size);
    return d;
}
