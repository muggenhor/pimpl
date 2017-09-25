// Copyright (c) 2008 Vladimir Batov.
// Copyright (c) 2017 Giel van Schijndel.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#ifndef IMPL_PTR_DETAIL_COPIED_HPP
#define IMPL_PTR_DETAIL_COPIED_HPP

#include "./detail.hpp"

namespace impl_ptr_policy
{
    template<typename, typename =std::allocator<void>> struct copied;
}

template<typename impl_type, typename allocator>
struct impl_ptr_policy::copied
{
    using   this_type = copied;
    using traits_type = detail::traits::copyable<impl_type, allocator>;
    using    ptr_type = typename traits_type::ptr_type;
    using  alloc_type = typename traits_type::alloc_type;

    template<typename derived_type, typename... arg_types>
    void
    emplace(arg_types&&... args)
    {
        impl_ = traits_type::template make<derived_type>(std::allocator_arg, alloc_type(), std::forward<arg_types>(args)...);
    }

    template<typename... arg_types>
    copied(detail::in_place_type, arg_types&&... args)
    {
        emplace<impl_type>(std::forward<arg_types>(args)...);
    }

    copied (std::nullptr_t) {}
    copied (this_type&& o) = default;
    copied (this_type const& o)
    {
        alloc_type a;
        if (o.impl_)
            impl_ = traits_type::make(a, *o.impl_);
    }

    bool       operator< (this_type const& o) const { return impl_ < o.impl_; }
    this_type& operator= (this_type&& o) { swap(o); return *this; }
    this_type& operator= (this_type const& o)
    {
        alloc_type a;
        /**/ if ( impl_ ==  o.impl_);
        else if ( impl_ &&  o.impl_) traits_type::assign(impl_.get(), *o.impl_);
        else if ( impl_ && !o.impl_) impl_.reset();
        else if (!impl_ &&  o.impl_) impl_ = traits_type::make(a, *o.impl_);

        return *this;
    }

    void      swap (this_type& o) { std::swap(impl_, o.impl_); }
    impl_type* get () const { return boost::to_address(impl_.get()); }
    long use_count () const { return 1; }

    private: ptr_type impl_;
};

#endif // IMPL_PTR_DETAIL_COPIED_HPP
