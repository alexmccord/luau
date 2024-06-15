// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Common.h"

#include <algorithm>
#include <cstdint>
#include <new>
#include <utility>
#include <vector>

namespace Luau::EqSat
{

class BumpAllocator
{
    static constexpr size_t kPageSize = 4096;
    static constexpr size_t kDatumSize = 256;

    template<typename T>
    struct Data
    {
        T* datum;
        uint8_t size;
        uint8_t dtorId;
    };

    struct Page
    {
        Data<void>* data;
        uintptr_t current;
    };

    static_assert(kPageSize % sizeof(Page) == 0);

    std::vector<Page> pages;
    std::vector<Page*> available;
    std::vector<void (*)(void*)> dtors;

    template<typename T>
    static void fnDtor(void* data)
    {
        static_cast<T*>(data)->~T();
    }

    template<typename T>
    uint8_t dtorof()
    {
        for (uint8_t i = 0; i < dtors.size(); ++i)
        {
            if (dtors[i] == fnDtor<T>)
                return i;
        }

        if (dtors.size() >= UINT8_MAX + 1)
            throw std::bad_alloc(); // too many unique types allocated

        dtors.push_back(fnDtor<T>);
        return dtors.size() - 1;
    }

public:
    BumpAllocator() = default;

    BumpAllocator(const BumpAllocator&) = delete;
    BumpAllocator& operator=(const BumpAllocator&) = delete;

    BumpAllocator(BumpAllocator&&) = default;
    BumpAllocator& operator=(BumpAllocator&&) = default;

    ~BumpAllocator();

    template<typename T, typename... Args>
    T* allocate(Args&&... args)
    {
        // The bump allocator needs to keep track of size for each individual datum,
        // and for memory compactness, we want to use uint8_t. Hence this assertion.
        static_assert(sizeof(T) <= kDatumSize, "datum size too big");

        for (Page* page : available)
        {
            if (T* data = tryAllocateInto<T>(page, std::forward<Args>(args)...))
                return data;
        }

        T* data = tryAllocateInto<T>(allocatePage(), std::forward<Args>(args)...);
        LUAU_ASSERT(data);
        return data;
    }

private:
    template<typename T, typename... Args>
    T* tryAllocateInto(Page* page, Args&&... args)
    {
        if (page->current + sizeof(Data<T>) > kPageSize)
            return nullptr;

        Data<T>* data = new (page->data + page->current) Data<T>{
            static_cast<T*>(::operator new(sizeof(T), std::nothrow)),
            // uint8_t stores up to 255, but sizeof(T) can be up to 256. We
            // add back 1 when walking through each constituent in this page
            sizeof(T) - 1,
            dtorof<T>(),
        };

        new (data->datum) T{std::forward<Args>(args)...};

        page->current += sizeof(Data<T>);
        if (page->current == kPageSize)
            available.erase(std::find(available.begin(), available.end(), page));

        return data->datum;
    }

    Page* allocatePage();
};

} // namespace Luau::EqSat
