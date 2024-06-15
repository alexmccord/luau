// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Id.h"
#include "Luau/LanguageHash.h"
#include "Luau/Slice.h"

#include <array>
#include <type_traits>
#include <utility>

#define LUAU_EQSAT_ATOM(name, t) \
    struct name : public ::Luau::EqSat::Atom<name, t> \
    { \
        static constexpr const char* tag = #name; \
        using Atom::Atom; \
    }

#define LUAU_EQSAT_NODE_ARRAY(name, ops) \
    struct name : public ::Luau::EqSat::NodeVector<name, std::array<::Luau::EqSat::Id, ops>> \
    { \
        static constexpr const char* tag = #name; \
        using NodeVector::NodeVector; \
    }

#define LUAU_EQSAT_NODE_VECTOR(name) \
    struct name : public ::Luau::EqSat::NodeVector<name, std::vector<::Luau::EqSat::Id>> \
    { \
        static constexpr const char* tag = #name; \
        using NodeVector::NodeVector; \
    }

#define LUAU_EQSAT_FIELD(name) \
    struct name : public ::Luau::EqSat::Field<name> \
    { \
    }

#define LUAU_EQSAT_NODE_FIELDS(name, ...) \
    struct name : public ::Luau::EqSat::NodeFields<name, __VA_ARGS__> \
    { \
        static constexpr const char* tag = #name; \
        using NodeFields::NodeFields; \
    }

namespace Luau::EqSat
{

template<typename Phantom, typename T>
struct Atom
{
    Atom(const T& value)
        : _value(value)
    {
    }

    const T& value() const
    {
        return _value;
    }

public:
    Slice<Id> operands()
    {
        return {};
    }

    Slice<const Id> operands() const
    {
        return {};
    }

    bool operator==(const Atom& rhs) const
    {
        return _value == rhs._value;
    }

    bool operator!=(const Atom& rhs) const
    {
        return !(*this == rhs);
    }

    struct Hash
    {
        size_t operator()(const Atom& value) const
        {
            return languageHash(value._value);
        }
    };

private:
    T _value;
};

template<typename Phantom, typename T>
struct NodeVector
{
    template<typename... Args>
    NodeVector(Args&&... args)
        : vector{std::forward<Args>(args)...}
    {
    }

    Id operator[](size_t i) const
    {
        return vector[i];
    }

public:
    Slice<Id> operands()
    {
        return Slice{vector.data(), vector.size()};
    }

    Slice<const Id> operands() const
    {
        return Slice{vector.data(), vector.size()};
    }

    bool operator==(const NodeVector& rhs) const
    {
        return vector == rhs.vector;
    }

    bool operator!=(const NodeVector& rhs) const
    {
        return !(*this == rhs);
    }

    struct Hash
    {
        size_t operator()(const NodeVector& value) const
        {
            return languageHash(value.vector);
        }
    };

private:
    T vector;
};

/// Empty base class just for static_asserts.
struct FieldBase
{
    FieldBase() = delete;

    FieldBase(FieldBase&&) = delete;
    FieldBase& operator=(FieldBase&&) = delete;

    FieldBase(const FieldBase&) = delete;
    FieldBase& operator=(const FieldBase&) = delete;
};

template<typename Phantom>
struct Field : FieldBase
{
};

template<typename Phantom, typename... Fields>
class NodeFields
{
    static_assert(std::conjunction<std::is_base_of<FieldBase, Fields>...>::value);

    std::array<Id, sizeof...(Fields)> array;

    template<typename T>
    static constexpr int getIndex()
    {
        constexpr int N = sizeof...(Fields);
        constexpr bool is[N] = {std::is_same_v<std::decay_t<T>, Fields>...};

        for (int i = 0; i < N; ++i)
            if (is[i])
                return i;

        return -1;
    }

public:
    template<typename... Args>
    NodeFields(Args&&... args)
        : array{std::forward<Args>(args)...}
    {
    }

    Slice<Id> operands()
    {
        return Slice{array};
    }

    Slice<const Id> operands() const
    {
        return Slice{array.data(), array.size()};
    }

    template<typename T>
    Id field() const
    {
        static_assert(std::disjunction_v<std::is_same<std::decay_t<T>, Fields>...>);
        return array[getIndex<T>()];
    }

    bool operator==(const NodeFields& rhs) const
    {
        return array == rhs.array;
    }

    bool operator!=(const NodeFields& rhs) const
    {
        return !(*this == rhs);
    }

    struct Hash
    {
        size_t operator()(const NodeFields& value) const
        {
            return languageHash(value.array);
        }
    };
};

template<typename... Ts>
class Language final
{
    const char* tag;
    void* data;

private:
    template<typename T>
    using WithinDomain = std::disjunction<std::is_same<std::decay_t<T>, Ts>...>;

    using FnCopy = void (*)(void*, const void*);
    using FnMove = void (*)(void*, void*);
    using FnDtor = void (*)(void*);
    using FnPred = bool (*)(const void*, const void*);
    using FnHash = size_t (*)(const void*);

    template<typename T>
    using FnSlice = Slice<T> (*)(std::conditional_t<std::is_const_v<T>, const void*, void*>);

    template<typename T>
    static void fnCopy(void* dst, const void* src) noexcept
    {
        // TODO: FIXME FIXME CRASH OVER HERE
        new (dst) T(*static_cast<const T*>(src));
    }

    template<typename T>
    static void fnMove(void* dst, void* src) noexcept
    {
        // TODO: FIXME FIXME CRASH OVER HERE
        new (dst) T(static_cast<T&&>(*static_cast<T*>(src)));
    }

    template<typename T>
    static void fnDtor(void* dst) noexcept
    {
        // TODO: FIXME FIXME DOUBLE FREE OVER HERE
        static_cast<T*>(dst)->~T();
    }

    template<typename T>
    static bool fnPred(const void* lhs, const void* rhs) noexcept
    {
        return *static_cast<const T*>(lhs) == *static_cast<const T*>(rhs);
    }

    template<typename T>
    static size_t fnHash(const void* data) noexcept
    {
        return typename T::Hash{}(*static_cast<const T*>(data));
    }

    template<typename S, typename T>
    static Slice<S> fnOperands(std::conditional_t<std::is_const_v<S>, const void*, void*> data) noexcept
    {
        return static_cast<std::conditional_t<std::is_const_v<S>, const T*, T*>>(data)->operands();
    }

    static constexpr FnCopy tableCopy[sizeof...(Ts)] = {&fnCopy<Ts>...};
    static constexpr FnMove tableMove[sizeof...(Ts)] = {&fnMove<Ts>...};
    static constexpr FnDtor tableDtor[sizeof...(Ts)] = {&fnDtor<Ts>...};
    static constexpr FnPred tablePred[sizeof...(Ts)] = {&fnPred<Ts>...};
    static constexpr FnHash tableHash[sizeof...(Ts)] = {&fnHash<Ts>...};
    static constexpr FnSlice<Id> tableSliceId[sizeof...(Ts)] = {&fnOperands<Id, Ts>...};
    static constexpr FnSlice<const Id> tableSliceConstId[sizeof...(Ts)] = {&fnOperands<const Id, Ts>...};

    static constexpr int getIndexFromTag(const char* tag) noexcept
    {
        constexpr int N = sizeof...(Ts);
        constexpr const char* is[N] = {Ts::tag...};

        for (int i = 0; i < N; ++i)
            if (is[i] == tag)
                return i;

        return -1;
    }

public:
    template<typename T>
    Language(const T* t, std::enable_if_t<WithinDomain<T>::value>* = 0) noexcept
        : tag(std::decay_t<T>::tag)
        , data(const_cast<void*>(static_cast<const void*>(t)))
    {
    }

    Language(const Language& other) noexcept
    {
        tag = other.tag;
        tableCopy[getIndexFromTag(tag)](data, other.data);
    }

    Language& operator=(const Language& other) noexcept
    {
        Language copy{other};
        *this = static_cast<Language&&>(copy);
        return *this;
    }

    Language(Language&& other) noexcept
    {
        tag = other.tag;
        tableMove[getIndexFromTag(tag)](data, other.data);
    }

    Language& operator=(Language&& other) noexcept
    {
        if (this != &other)
        {
            tableDtor[getIndexFromTag(tag)](data);
            tag = other.tag;
            tableMove[getIndexFromTag(tag)](data, other.data); // nothrow
        }
        return *this;
    }

    ~Language() noexcept
    {
        tableDtor[getIndexFromTag(tag)](data);
    }

    int index() const noexcept
    {
        return getIndexFromTag(tag);
    }

    /// You should never call this function with the intention of mutating the `Id`.
    /// Reading is ok, but you should also never assume that these `Id`s are stable.
    Slice<Id> operands() noexcept
    {
        return tableSliceId[getIndexFromTag(tag)](data);
    }

    Slice<const Id> operands() const noexcept
    {
        return tableSliceConstId[getIndexFromTag(tag)](data);
    }

    template<typename T>
    const T* get() const noexcept
    {
        static_assert(WithinDomain<T>::value);
        return tag == T::tag ? reinterpret_cast<const T*>(data) : nullptr;
    }

    bool operator==(const Language& rhs) const noexcept
    {
        return tag == rhs.tag && tablePred[getIndexFromTag(tag)](data, rhs.data);
    }

    bool operator!=(const Language& rhs) const noexcept
    {
        return !(*this == rhs);
    }

public:
    struct Hash
    {
        size_t operator()(const Language& language) const
        {
            size_t seed = std::hash<const char*>{}(language.tag);
            hashCombine(seed, tableHash[getIndexFromTag(language.tag)](language.data));
            return seed;
        }
    };
};

} // namespace Luau::EqSat
