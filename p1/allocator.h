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

    void *get() const;

    void setIsFree(bool isFree = true) { _innerStruct->_isFree = isFree; }

    Pointer *next() const { return _innerStruct->_next; }
    Pointer *prev() const { return _innerStruct->_prev; }

    void setNext(Pointer *ptr) { _innerStruct->_next = ptr; }
    void setPrev(Pointer *ptr) { _innerStruct->_prev = ptr; }
    void setSize(size_t size) { _innerStruct->_size = size; }
    void setOffset(size_t offset) { _innerStruct->_offset = offset; }

    void move(size_t new_offset);
    void copy(size_t new_offset) { memcpy((char*)_base + new_offset, get(), size()); }

    void merge();
    void split(size_t N);

    void setInnerStruct(std::shared_ptr<PointerInfo> &is) { _innerStruct = is; }

    std::shared_ptr<PointerInfo> &innerStruct() { return _innerStruct; }
    size_t offset() const { return _innerStruct->_offset; }
    size_t size() const { return _innerStruct->_size; }
    bool isFree() const { return _innerStruct->_isFree; }
    static void setBase(void *new_base) { _base = new_base; }
};


class Allocator {
    void *_base;
    size_t _size;
    Pointer *pointers;
public:
    Allocator(void *base, size_t size) : _base(base), _size(size) {
        Pointer::setBase(base);
        pointers = new Pointer(0, size);
    }
    ~Allocator();

    void print();

    Pointer alloc(size_t N);
    void realloc(Pointer &p, size_t N);
    void free(Pointer &p);
    void defrag();

    std::string dump();
};
