#pragma once

namespace das
{
    struct SimNode;
    struct TypeInfo;
    struct FuncInfo;

    struct Block {
        uint32_t    stackOffset;
        uint32_t    argumentsOffset;
        SimNode *   body;
        void *      aotFunction;
        vec4f *     functionArguments;
        FuncInfo *  info;
        __forceinline bool operator == ( const Block & b ) const {
            return b.stackOffset==stackOffset && b.argumentsOffset==argumentsOffset
                && b.body==body && b.functionArguments==functionArguments;
        }
    };

    template <typename Result, typename ...Args>
    struct TBlock : Block {
        TBlock() {}
        TBlock( const TBlock & ) = default;
        TBlock( const Block & that ) { *(Block *)this = that; }
    };

    struct Func {
        int32_t     index;
        Func() : index(0) {}
        Func(int32_t idx) : index(idx) {}
        __forceinline operator bool () const { return index!=0; }
        __forceinline bool operator == ( void * ptr ) const {
            return !ptr && (index==0);
        }
        __forceinline bool operator != ( void * ptr ) const {
            return ptr || index;
        }
        __forceinline bool operator == ( const Func & b ) const {
            return index == b.index;
        }
        __forceinline bool operator != ( const Func & b ) const {
            return index != b.index;
        }
    };
    static_assert(sizeof(Func)==sizeof(int32_t), "has to be castable");

    template <typename Result, typename ...Args>
    struct TFunc : Func {
        TFunc()  {}
        TFunc( const TFunc & ) = default;
        TFunc( const Func & that ) { *(Func *)this = that; }
    };

    struct Lambda {
        Lambda() : capture(nullptr) {}
        Lambda(void * ptr) : capture((char *)ptr) {}
        char *      capture;
        __forceinline TypeInfo * getTypeInfo() const {
            return capture ? *(TypeInfo **)(capture-16) : nullptr;
        }
        __forceinline bool operator == ( const Lambda & b ) const {
            return capture == b.capture;
        }
        __forceinline bool operator != ( const Lambda & b ) const {
            return capture != b.capture;
        }
    };
    static_assert(sizeof(Lambda)==sizeof(void *), "has to be castable");

    template <typename Result, typename ...Args>
    struct TLambda : Lambda {
        TLambda()  {}
        TLambda( const TLambda & ) = default;
        TLambda( const Lambda & that ) { *(Lambda *)this = that; }
    };

    struct Tuple {
        Tuple() {}
    };

    struct Variant {
        Variant() {}
    };

    struct Array {
        char *      data;
        uint32_t    size;
        uint32_t    capacity;
        uint32_t    lock;
        union {
            struct {
                bool    shared : 1;
                bool    hopeless : 1;   // needs to be deleted without fuss (exceptions)
            };
            uint32_t    flags;
        };
        __forceinline bool isLocked() const { return lock; }
    };

    class Context;

    void array_lock ( Context & context, Array & arr );
    void array_unlock ( Context & context, Array & arr );
    void array_reserve ( Context & context, Array & arr, uint32_t newCapacity, uint32_t stride );
    void array_resize ( Context & context, Array & arr, uint32_t newSize, uint32_t stride, bool zero );
    void array_grow ( Context & context, Array & arr, uint32_t newSize, uint32_t stride );  // always grows
    void array_clear ( Context & context, Array & arr );

    struct Table : Array {
        char *      keys;
        uint32_t *  hashes;
        uint32_t    maxLookups;
        uint32_t    shift;
    };

    void table_clear ( Context & context, Table & arr );
    void table_lock ( Context & context, Table & arr );
    void table_unlock ( Context & context, Table & arr );

    struct Sequence;
    void builtin_table_keys ( Sequence & result, const Table & tab, int32_t stride, Context * __context__ );
    void builtin_table_values ( Sequence & result, const Table & tab, int32_t stride, Context * __context__ );

    template <typename TT>
    struct EnumStubAny  {
        typedef TT  BaseType;
        TT          value;
    };

    typedef EnumStubAny<int32_t> EnumStub;
    typedef EnumStubAny<int8_t>  EnumStub8;
    typedef EnumStubAny<int16_t> EnumStub16;

    struct Bitfield {
        uint32_t    value;
        __forceinline Bitfield ( int32_t v ) : value(uint32_t(v)) {}
        __forceinline Bitfield ( uint32_t v ) : value(v) {}
        __forceinline Bitfield ( int64_t v ) : value(uint32_t(v)) {}
        __forceinline Bitfield ( uint64_t v ) : value(uint32_t(v)) {}
        __forceinline Bitfield ( int8_t v ) : value(uint32_t(v)) {}
        __forceinline Bitfield ( uint8_t v ) : value(uint32_t(v)) {}
        __forceinline Bitfield ( int16_t v ) : value(uint32_t(v)) {}
        __forceinline Bitfield ( uint16_t v ) : value(uint32_t(v)) {}
        __forceinline Bitfield ( float v ) : value(uint32_t(v)) {}
        __forceinline Bitfield ( double v ) : value(uint32_t(v)) {}
        __forceinline operator uint32_t & () { return value; }
        __forceinline operator const uint32_t & () const { return value; }
        __forceinline operator float () const { return float(value); }
        __forceinline operator double () const { return double(value); }
        __forceinline operator int32_t () const { return int32_t(value); }
        __forceinline operator int16_t () const { return int16_t(value); }
        __forceinline operator uint16_t () const { return uint16_t(value); }
        __forceinline operator int64_t () const { return int64_t(value); }
        __forceinline operator uint64_t () const { return uint64_t(value); }
        __forceinline bool operator == ( const Bitfield & f ) const { return value==f.value; }
        __forceinline bool operator != ( const Bitfield & f ) const { return value!=f.value; }
        __forceinline bool operator == ( uint32_t f ) const { return value==f; }
        __forceinline bool operator != ( uint32_t f ) const { return value!=f; }
    };
    __forceinline bool operator == ( uint32_t f, const Bitfield & t ) { return t.value==f; }
    __forceinline bool operator != ( uint32_t f, const Bitfield & t ) { return t.value!=f; }

}

namespace std {
    template <> struct hash<das::Bitfield> {
        std::size_t operator() ( das::Bitfield b ) const {
            return hash<uint32_t>()(b.value);
        }
    };
}
