// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/Bump.h"
#include <new>
#include <vcruntime_new.h>

namespace Luau::EqSat
{

BumpAllocator::~BumpAllocator()
{
    for (Page& page : pages)
    {
        for (uintptr_t ptr = 0; ptr < page.current; ptr += sizeof(Data<void>))
        {
            Data<void>* data = page.data + ptr;
            dtors[data->dtorId](data->datum);
            ::operator delete(data->datum, data->size + 1);
        }

        ::operator delete(page.data, kPageSize);
    }
}

BumpAllocator::Page* BumpAllocator::allocatePage()
{
    void* data = ::operator new(kPageSize, std::nothrow);
    if (!data)
        throw std::bad_alloc();

    pages.push_back(Page{static_cast<Data<void>*>(data), 0});
    available.push_back(&pages.back());
    return &pages.back();
}

} // namespace Luau::EqSat
