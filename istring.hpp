/*
copyright Barrett Adair 2016
All rights reserved (for now).
*/

// todo - make sure all reinterpret_casts and pointer dereferences are well-defined
// todo - implement istring_builder
// todo - implement many more operators and member functions
// todo - consider implementing a custom iterator

#ifndef ISTRING_HPP
#define ISTRING_HPP

#include <utility>
#include <array>
#include <iterator>
#include <ostream>
#include <memory>
#include <cstring>
#include <cstdint>

namespace istr {

    constexpr std::size_t max_string_length = static_cast<std::size_t>(1) << 31;

    // used to forward a parameter without copying it
    template<typename T>
    using forward_t = std::conditional_t< sizeof(void*) < sizeof(std::declval<T>())
        , std::add_lvalue_reference_t< std::add_const_t<T> >, T>;

    // istring_t<> is a type-erased reference to an immutable string object. The
    // specialization template of istring_t generates the immutable string classes,
    // which come in sizes of 2^N (TODO revisit the sizing strategy - too much code
    // bloat? too many wasted bytes?)
    template<typename Char = char>
    struct istring_t {

        using char_type = Char;

        inline istring_t(const istring_t&) = default;
        inline istring_t(istring_t&&) = default;

        template <std::size_t N>
        inline istring_t(const istring_t<std::array<Char, N>>& other)
            : ptr_vtable(&vtable_holder<istring_t<std::array<Char, N>>>::val_vtable),
            ptr(std::make_shared<istring_t<std::array<Char, N>>>(other)) {
        }

        template <std::size_t N>
        inline istring_t& operator=(const istring_t<std::array<Char, N>>& other) {
            ptr_vtable = &vtable_holder<istring_t<std::array<Char, N>>>::val_vtable;
            ptr = std::make_shared<istring_t<std::array<Char, N>>>(other);
            return *this;
        }

        inline istring_t& operator=(const istring_t& other) {
            ptr_vtable = other.ptr_vtable;
            ptr = other.ptr;
            return *this;
        }

        struct vtable {

            // These function pointers are initialized at compile-time.

            bool (*is_empty)(void*);
            const Char* (*begin)(void*);
            const Char* (*end)(void*);
            std::size_t (*size)(void*);
            istring_t (*clone)(void*);
            const Char& (*bracket_operator)(void*, std::size_t);
            istring_t (*resize)(void*);
            istring_t (*resize_to)(void*, std::size_t);
            std::size_t (*get_buffer_size)(void*);
        };

        // This template is instantiated for every class from which this interface
        // is constructed (see constructor above). This instantiation causes the
        // compiler to emit an initialized vtable for this type (via aggregate
        // assignment later in the code).
        template <class T>
        struct vtable_holder { static const vtable val_vtable; };

        inline bool
        is_empty() const {
            return ptr_vtable->is_empty(ptr.get());
        }

        inline const Char*
        begin() const {
            return ptr_vtable->begin(ptr.get());
        }

        inline const Char*
        end() const {
            return ptr_vtable->end(ptr.get());
        }

        inline std::size_t
        size() const {
            return ptr_vtable->size(ptr.get());
        }

        inline istring_t<>
        clone() const {
            return ptr_vtable->clone(ptr.get());
        }

        inline const Char&
        operator[](std::size_t index) const {
            return ptr_vtable->bracket_operator(ptr.get(), index);
        }

        inline const istring_t
        resize() const {
            return ptr_vtable->resize(ptr.get());
        }

        inline const istring_t
        resize_to(std::size_t s) const {
            return ptr_vtable->resize_to(ptr.get(), s);
        }

        inline const std::size_t
        get_buffer_size() const {
            return ptr_vtable->get_buffer_size(ptr.get());
        }

        //our two data members
        const vtable* ptr_vtable;
        std::shared_ptr<void> ptr;
    };

    // this is our type erasure "glue". member<...>::wrap is a static member function which
    // casts a void* back to its original object type, and invokes the appropriate member. This first
    // definition handles member functions.
    template<typename Pmf, Pmf PmfValue>
    struct member;

    template<typename T, typename... Args, typename C, T(C::*PmfValue)(Args...) const>
    struct member<T(C::*)(Args...) const, PmfValue> {
        static inline decltype(auto)
        wrap(void* c, forward_t<Args>... args) {
            return (reinterpret_cast<const C*>(c)->*PmfValue)(args...);
        };
    };

    // this initializes the vtable at compile time for every class used to construct a
    // string_t<> object. Member functions and member data are both handled.
    template<typename Char>
    template<typename T>
    const typename istring_t<Char>::vtable istring_t<Char>::vtable_holder<T>::val_vtable = {
        &::istr::member< bool(T::*)()                       const,  &T::is_empty                        >::wrap,
        &::istr::member< const Char*(T::*)()                const,  &T::begin                           >::wrap,
        &::istr::member< const Char*(T::*)()                const,  &T::end                             >::wrap,
        &::istr::member< std::size_t(T::*)()                const,  &T::size                            >::wrap,
        &::istr::member< istring_t<Char>(T::*)()            const,  &T::template clone<istring_t<Char>> >::wrap,
        &::istr::member< const Char&(T::*)(std::size_t)     const,  &T::operator[]                      >::wrap,
        &::istr::member< istring_t<Char>(T::*)()            const,  &T::template resize<istring_t<Char>>>::wrap,
        &::istr::member< istring_t<Char>(T::*)(std::size_t) const,  &T::resize_to                       >::wrap,
        &::istr::member< std::size_t(T::*)()                const,  &T::get_buffer_size                 >::wrap
    };

    template<std::size_t Power>
    using set_bit = std::integral_constant<std::size_t, static_cast<std::size_t>(1) << Power>;

    template<std::size_t Input>
    struct next_power_of_two {

        static constexpr std::size_t value =
            // TODO use set_bit instead
            ((static_cast<std::size_t>(1) << 0)  >= Input ? static_cast<std::size_t>(1) << 0  :
            ((static_cast<std::size_t>(1) << 1)  >= Input ? static_cast<std::size_t>(1) << 1  :
            ((static_cast<std::size_t>(1) << 2)  >= Input ? static_cast<std::size_t>(1) << 2  :
            ((static_cast<std::size_t>(1) << 3)  >= Input ? static_cast<std::size_t>(1) << 3  :
            ((static_cast<std::size_t>(1) << 4)  >= Input ? static_cast<std::size_t>(1) << 4  :
            ((static_cast<std::size_t>(1) << 5)  >= Input ? static_cast<std::size_t>(1) << 5  :
            ((static_cast<std::size_t>(1) << 6)  >= Input ? static_cast<std::size_t>(1) << 6  :
            ((static_cast<std::size_t>(1) << 7)  >= Input ? static_cast<std::size_t>(1) << 7  :
            ((static_cast<std::size_t>(1) << 8)  >= Input ? static_cast<std::size_t>(1) << 8  :
            ((static_cast<std::size_t>(1) << 9)  >= Input ? static_cast<std::size_t>(1) << 9  :
            ((static_cast<std::size_t>(1) << 10) >= Input ? static_cast<std::size_t>(1) << 10 :
            ((static_cast<std::size_t>(1) << 11) >= Input ? static_cast<std::size_t>(1) << 11 :
            ((static_cast<std::size_t>(1) << 12) >= Input ? static_cast<std::size_t>(1) << 12 :
            ((static_cast<std::size_t>(1) << 13) >= Input ? static_cast<std::size_t>(1) << 13 :
            ((static_cast<std::size_t>(1) << 14) >= Input ? static_cast<std::size_t>(1) << 14 :
            ((static_cast<std::size_t>(1) << 15) >= Input ? static_cast<std::size_t>(1) << 15 :
            ((static_cast<std::size_t>(1) << 16) >= Input ? static_cast<std::size_t>(1) << 16 :
            ((static_cast<std::size_t>(1) << 17) >= Input ? static_cast<std::size_t>(1) << 17 :
            ((static_cast<std::size_t>(1) << 18) >= Input ? static_cast<std::size_t>(1) << 18 :
            ((static_cast<std::size_t>(1) << 19) >= Input ? static_cast<std::size_t>(1) << 19 :
            ((static_cast<std::size_t>(1) << 20) >= Input ? static_cast<std::size_t>(1) << 20 :
            ((static_cast<std::size_t>(1) << 21) >= Input ? static_cast<std::size_t>(1) << 21 :
            ((static_cast<std::size_t>(1) << 22) >= Input ? static_cast<std::size_t>(1) << 22 :
            ((static_cast<std::size_t>(1) << 23) >= Input ? static_cast<std::size_t>(1) << 23 :
            ((static_cast<std::size_t>(1) << 24) >= Input ? static_cast<std::size_t>(1) << 24 :
            ((static_cast<std::size_t>(1) << 25) >= Input ? static_cast<std::size_t>(1) << 25 :
            ((static_cast<std::size_t>(1) << 26) >= Input ? static_cast<std::size_t>(1) << 26 :
            ((static_cast<std::size_t>(1) << 27) >= Input ? static_cast<std::size_t>(1) << 27 :
            ((static_cast<std::size_t>(1) << 28) >= Input ? static_cast<std::size_t>(1) << 28 :
            ((static_cast<std::size_t>(1) << 29) >= Input ? static_cast<std::size_t>(1) << 29 :
            ((static_cast<std::size_t>(1) << 30) >= Input ? static_cast<std::size_t>(1) << 30 :
            ((static_cast<std::size_t>(1) << 31) >= Input ? static_cast<std::size_t>(1) << 31 :
            0))))))))))))))))))))))))))))))));
    };

    template<typename T, std::size_t Input>
    using regular_array = std::array<T, next_power_of_two<Input>::value>;

    /*too much executable bloat?
    template<typename Str, std::size_t N>
    inline constexpr typename Str::underlying_type
    make_char_array(const char(&str)[N]) {
        std::array<typename Str::char_type, Str::buffer_size> temp{};
        using std::copy;
        copy(str, str + N, temp.begin());
        temp[N] = '\0';
        return{ temp };
    }*/

    template<typename Str>
    inline constexpr typename Str::underlying_type
        make_char_array(const char* a) {
        std::array<typename Str::char_type, Str::buffer_size> temp{};
        auto size = strlen(a);
        copy(a, a + size, temp.begin());
        temp[size + 1] = Str::nullchr;
        return temp;
    }

    template<typename Char, std::size_t N>
    struct istring_t<std::array<Char, N>> {

        using char_type = Char;
        using underlying_type = std::array<Char, N>;
        static constexpr auto buffer_size = N;
        static constexpr auto nullchr = Char{};

        istring_t() = default;
        istring_t(const istring_t&) = default;
        istring_t(istring_t&&) = default;

        istring_t(const char* a, int l)
            : buf(make_char_array<istring_t>(a)), len(l) {
        }

        istring_t(const std::array<Char, N>& a, int l)
            : buf(a), len(l) {}

        /*bloat? See make_char_array above
        template<std::size_t M>
        istring_t(const char(&str)[M])
            : buf(make_char_array<istring_t>(str)), len(M) {}
        */

        using next_array = std::array<Char, (N * 2 < N) ? N : N * 2>;

        inline constexpr bool
        is_empty() const {
            return N == 0 || buf[0] == nullchr;
        }

        inline constexpr const Char*
        begin() const {
            return &buf[0];
        }

        inline constexpr const Char*
        end() const {
            return &buf[0] + len;
        }

        inline constexpr std::size_t
        size() const {
            return len;
        }

        template<typename Result = istring_t>
        inline constexpr Result
        clone() const {
            return *this;
        }

        inline constexpr const Char&
        operator[](std::size_t index) const {
            return buf[index];
        }

        template<typename T = istring_t<next_array>>
        inline constexpr T
        resize() const {

            if (istring_t<next_array>::buffer_size == max_string_length) {
                throw std::logic_error{ "string too large; cannot resize" };
            }

            next_array temp{};
            std::copy(buf.begin(), buf.end(), temp.begin());
            return istring_t<next_array>{ temp, len };
        }

        inline istring_t<>
        resize_to(std::size_t Size) const {
            
            auto target_size = std::max(Size, N);

#define ISTRING_RESIZE_TO(bit) \
 if (target_size <= set_bit<bit>::value) \
    return istring_t<std::array<Char, set_bit<bit>::value>>{ \
        reinterpret_cast<const Char*>(&*buf.begin()), len } \
/**/

            ISTRING_RESIZE_TO(1);
            ISTRING_RESIZE_TO(2);
            ISTRING_RESIZE_TO(3);
            ISTRING_RESIZE_TO(4);
            ISTRING_RESIZE_TO(5);
            ISTRING_RESIZE_TO(6);
            ISTRING_RESIZE_TO(7);
            ISTRING_RESIZE_TO(8);
            ISTRING_RESIZE_TO(9);
            ISTRING_RESIZE_TO(10);
            ISTRING_RESIZE_TO(11);
            ISTRING_RESIZE_TO(12);
            ISTRING_RESIZE_TO(13);
            ISTRING_RESIZE_TO(14);
            ISTRING_RESIZE_TO(15);
            ISTRING_RESIZE_TO(16);
            ISTRING_RESIZE_TO(17);
            ISTRING_RESIZE_TO(18);
            ISTRING_RESIZE_TO(19);
            ISTRING_RESIZE_TO(20);
            ISTRING_RESIZE_TO(21);
            ISTRING_RESIZE_TO(22);
            ISTRING_RESIZE_TO(23);
            ISTRING_RESIZE_TO(24);
            ISTRING_RESIZE_TO(25);
            ISTRING_RESIZE_TO(26);
            ISTRING_RESIZE_TO(27);
            ISTRING_RESIZE_TO(28);
            ISTRING_RESIZE_TO(29);
            ISTRING_RESIZE_TO(30);
            ISTRING_RESIZE_TO(31);
            throw std::logic_error{ "cannot resize to a string of this size." };
        }

        inline constexpr std::size_t
        get_buffer_size() const {
            return buffer_size;
        }

        inline operator istring_t<next_array>() const {
            return resize();
        }

        inline operator istring_t<Char>() const {
            return { *this };
        }

        const std::array<Char, N> buf = {{ nullchr }};
        const std::size_t len;
    };

    template<typename T>
    struct lazy {
        using type = T;
    };

    template<std::int64_t Size = -1, typename Char = char>
    using istring = istring_t<typename std::conditional<Size == -1,
        lazy<Char>, lazy<regular_array<Char, static_cast<std::size_t>(Size)>>>::type::type>;

    template<std::size_t N>
    inline constexpr istring<N, char>
    make_istring(const char(&str)[N]) {
        using std::copy;
        regular_array<char, N> temp{};
        copy( str, str + N, temp.begin() );
        temp[N] = '\0';
        return{ temp, N };
    }

    /*todo - implementation requires istring_builder
    template<typename Char>
    istring<> make_istring(const Char* str, std::size_t bounds) {
        using std::copy;
        istring<> result = istring<1, Char>{""};

        int i = 0;

        for(; str[i] != Char{} && i < bounds; ++i) {
            
            if (result.size() == i) {
                result = result.resize();
            }

            result[i] = str[i];
        }

        result[i] = Char{};
        result.len = i;

        return{ result };
    }*/

    template<typename Char, std::size_t Size>
    inline ::std::ostream& operator<<(
        ::std::ostream& out, ::istr::istring_t<std::array<Char, Size>> const & str) {
        return out << reinterpret_cast<const Char*>(&*str.begin());
    }

    template<typename Char>
    inline ::std::ostream& operator<<(
        ::std::ostream& out, ::istr::istring_t<Char> const & str) {
        return out << reinterpret_cast<const Char*>(&*str.begin());
    }

    template<typename Char, std::size_t LeftSize, std::size_t RightSize>
    inline bool operator==(
        const ::istr::istring_t<std::array<Char, LeftSize>>& left,
        const ::istr::istring_t<std::array<Char, RightSize>>& right) {
        return strcmp(left.begin(), right.begin()) == 0;
    }

    template<typename Char, std::size_t Size>
    inline bool operator==(
        const ::istr::istring_t<std::array<Char, Size>>& left,
        const ::istr::istring<>& right) {
        return strcmp(left.begin(), right.begin()) == 0;
    }

    template<typename Char, std::size_t Size>
    inline bool operator==(
        const ::istr::istring<>& left,
        const ::istr::istring_t<std::array<Char, Size>>& right) {
        return strcmp(left.begin(), right.begin()) == 0;
    }

    inline bool operator==(
        const ::istr::istring<>& left,
        const ::istr::istring<>& right) {
        return strcmp(left.begin(), right.begin()) == 0;
    }
}

#endif //#ifndef ISTRING_HPP
