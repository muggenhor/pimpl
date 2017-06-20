// Copyright (c) 2006-2016 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#ifndef AUXILIARY_PIMPL_HPP
#define AUXILIARY_PIMPL_HPP

#include <boost/assert.hpp>
#include <boost/utility.hpp>
#include <boost/convert/detail/has_member.hpp>
#include <type_traits>
#include <memory>

namespace detail
{
    template<typename> struct  shared;
    template<typename> struct  unique;
    template<typename> struct     cow; // copy_on_write
    template<typename> struct onstack;

    struct null_type {};

    template <typename first_type =void, typename...> struct first { using type = first_type; };

    template<typename, typename =void>
    struct is_allocator { BOOST_STATIC_CONSTANT(bool, value = false); };

    template<typename class_type>
    struct is_allocator<class_type, typename std::enable_if<std::is_class<class_type>::value, void>::type>
    {
        BOOST_DECLARE_HAS_MEMBER(has_allocate, allocate);
        BOOST_DECLARE_HAS_MEMBER(has_deallocate, deallocate);

        BOOST_STATIC_CONSTANT(bool, value = has_allocate<class_type>::value && has_deallocate<class_type>::value);
    };
}

template<typename impl_type>
struct detail::onstack
{
    // Proof of concept
    // Need to extract storage size from more_types
    char storage_[32];

    template<typename... arg_types>
    void
    construct(arg_types&&... args)
    {
        new (storage_) impl_type(std::forward<arg_types>(args)...);
    }
    onstack () =default;
};

template<typename impl_type>
struct detail::shared : std::shared_ptr<impl_type>
{
    using this_type = shared;
    using base_type = std::shared_ptr<impl_type>;
    using  base_ref = base_type&;

    template<typename... arg_types>
    typename std::enable_if<is_allocator<typename first<arg_types...>::type>::value, void>::type
    construct(arg_types&&... args)
    {
        base_ref(*this) = std::allocate_shared<impl_type>(std::forward<arg_types>(args)...);
    }
    template<typename... arg_types>
    typename std::enable_if<!is_allocator<typename first<arg_types...>::type>::value, void>::type
    construct(arg_types&&... args)
    {
        base_ref(*this) = std::make_shared<impl_type>(std::forward<arg_types>(args)...);
    }
    template<typename derived_type> void construct(std::shared_ptr<derived_type>&&      p) { base_ref(*this) = p; }
    template<typename derived_type> void construct(std::shared_ptr<derived_type> const& p) { base_ref(*this) = p; }
    template<typename derived_type> void construct(std::shared_ptr<derived_type>&       p) { base_ref(*this) = p; }
};

template<typename impl_type>
struct detail::unique
{
    // Smart-pointer with the value-semantics behavior.
    // The incomplete-type management technique is originally by Peter Dimov.

    using this_type = unique;

//  template<typename alloc_type, typename... arg_types>
//  typename std::enable_if<is_allocator<alloc_type>::value, void>::type
//  construct(alloc_type&& alloc, arg_types&&... args)
//  {
//      void* mem = std::allocator_traits<alloc_type>::allocate(alloc, 1);
//      reset(new(mem) impl_type(std::forward<arg_types>(args)...));
//  }
    template<typename... arg_types>
    typename std::enable_if<!is_allocator<typename first<arg_types...>::type>::value, void>::type
    construct(arg_types&&... args)
    {
        reset(new impl_type(std::forward<arg_types>(args)...));
    }

   ~unique () { traits_->destroy(impl_); }
    unique () : impl_(nullptr), traits_(traits()) {}
    unique (impl_type* p) : impl_(p), traits_(deep_copy()) {}
    unique (this_type const& that) : impl_(that.traits_->copy(that.impl_)), traits_(that.traits_) {}
    unique (this_type&& that) : unique() { swap(that); }

    this_type& operator= (this_type&& that) { swap(that); return *this; }
    this_type& operator= (this_type const& that) { traits_ = that.traits_; traits_->assign(impl_, that.impl_); return *this; }
    bool       operator< (this_type const& that) const { return impl_ < that.impl_; }

    void     reset (impl_type* p) { this_type(p).swap(*this); }
    void      swap (this_type& that) { std::swap(impl_, that.impl_), std::swap(traits_, that.traits_); }
    impl_type* get () const { return impl_; }
    long use_count () const { return 1; }

    private:

    struct traits
    {
        virtual ~traits() =default;

        virtual void    destroy (impl_type*&) const {}
        virtual impl_type* copy (impl_type const*) const { return nullptr; }
        virtual void     assign (impl_type*&, impl_type const*) const {}

        operator traits const*() { static traits trait; return &trait; }
    };
    struct deep_copy : traits
    {
        void    destroy (impl_type*& p) const { boost::checked_delete(p); p = 0; }
        impl_type* copy (impl_type const* p) const { return p ? new impl_type(*p) : 0; }

        void assign (impl_type*& a, impl_type const* b) const
        {
            if (a != b) destroy(a), a = copy(b);
        }
        // TODO: add selection using is_copy_assignable
//        void assign (impl_type*& a, impl_type const* b) const
//        {
//            /**/ if ( a ==  b);
//            else if ( a &&  b) *a = *b;
//            else if (!a &&  b) a = copy(b);
//            else if ( a && !b) destroy(a);
//        }
        operator traits const*() { static deep_copy trait; return &trait; }
    };

    impl_type*      impl_;
    traits const* traits_;
};

template<typename user_type>
struct impl_ptr
{
    struct          implementation;
    template<typename> struct base;

    using   unique = base<detail::unique <implementation>>;
    using   shared = base<detail::shared <implementation>>;
    using      cow = base<detail::cow    <implementation>>;
    using  onstack = base<detail::onstack<implementation>>;
    using yes_type = boost::type_traits::yes_type;
    using  no_type = boost::type_traits::no_type;
    using ptr_type = typename std::remove_reference<user_type>::type*;

    template<typename Y>
    static yes_type test (Y*, typename Y::impl_ptr_check_type* =nullptr);
    static no_type  test (...);

    BOOST_STATIC_CONSTANT(bool, value = (1 == sizeof(test(ptr_type(nullptr)))));

    static user_type null()
    {
        using null_type = typename user_type::null_type;
        using base_type = typename user_type::base_type;

        static_assert(impl_ptr<user_type>::value, "");
        static_assert(sizeof(user_type) == sizeof(base_type), "");

        null_type  arg;
        base_type null (arg);

        return *(user_type*) &null;
    }
};

template<typename user_type>
template<typename policy_type>
struct impl_ptr<user_type>::base
{
    using      implementation = typename impl_ptr<user_type>::implementation;
    using impl_ptr_check_type = base;
    using           base_type = base;
    using           null_type = detail::null_type;

    template<typename T> using     rm_ref = typename std::remove_reference<T>::type;
    template<typename T> using is_base_of = typename std::is_base_of<base_type, rm_ref<T>>;
    template<typename T> using is_derived = typename std::enable_if<is_base_of<T>::value, null_type*>::type;

    bool         operator! () const { return !impl_.get(); }
    explicit operator bool () const { return  impl_.get(); }

    // Comparison Operators.
    // base::op==() transfers the comparison to 'impl_'. Consequently,
    // ptr-semantics (shared_ptr-based) pimpls are comparable due to shared_ptr::op==().
    // However, value-semantics (unique-based) pimpls are NOT COMPARABLE BY DEFAULT --
    // the standard value-semantics behavior -- due to NO unique::op==().
    // If a value-semantics class T needs to be comparable, then it has to provide
    // T::op==(T const&) EXPLICITLY as part of its public interface.
    // Trying to call this base::op==() for unique-based impl_ptr will fail to compile (no unique::op==())
    // and will indicate that the user forgot to declare T::operator==(T const&).
    bool operator==(base_type const& that) const { return impl_ == that.impl_; }
    bool operator!=(base_type const& that) const { return impl_ != that.impl_; }
    bool operator< (base_type const& that) const { return impl_  < that.impl_; }

    void  swap (base_type& that) { impl_.swap(that.impl_); }

//    template<class Y>                   void reset (Y* p) { impl_.reset(p); }
//    template<class Y, class D>          void reset (Y* p, D d) { impl_.reset(p, d); }
//    template<class Y, class D, class A> void reset (Y* p, D d, A a) { impl_.reset(p, d, a); }

    template<typename... arg_types>
    void
    reset(arg_types&&... args) { impl_.construct(std::forward<arg_types>(args)...); }

    // Access To the Implementation.
    // 1) These methods are only useful and meaningful in the implementation code,
    //    i.e. where impl_ptr<>::implementation is visible.
    // 2) For better or worse the original deep-constness behavior has been changed
    //    to match std::shared_ptr, etc. to avoid questions, confusion, etc.
    implementation* operator->() const { BOOST_ASSERT(impl_.get()); return  impl_.get(); }
    implementation& operator *() const { BOOST_ASSERT(impl_.get()); return *impl_.get(); }

    long use_count() const { return impl_.use_count(); }

    protected:

    template<typename> friend class impl_ptr;

    base (null_type) {}

    template<class arg_type>
    base(arg_type&& arg, is_derived<arg_type> =nullptr) : impl_(arg.impl_) {}

    template<typename... arg_types>
    base(arg_types&&... args) { impl_.construct(std::forward<arg_types>(args)...); }

    private: policy_type impl_;
};

namespace boost
{
    template<typename user_type> using impl_ptr = ::impl_ptr<user_type>;
}

#endif // AUXILIARY_PIMPL_HPP