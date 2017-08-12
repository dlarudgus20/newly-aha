// The MIT License (MIT)
//
// Copyright (c) 2016 Im Kyeong-Hyeon (dlarudgus20@naver.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <string_view>
#include <string>
#include <array>
#include <type_traits>
#include <cstddef>

namespace ext
{
    // http://en.cppreference.com/w/cpp/experimental/make_array
    namespace detail
    {
        template<class> struct is_ref_wrapper : std::false_type
        {
        };
        template<class T> struct is_ref_wrapper<std::reference_wrapper<T>> : std::true_type
        {
        };

        template<class T>
        using not_ref_wrapper = std::negation<is_ref_wrapper<std::decay_t<T>>>;

        template <class D, class...> struct return_type_helper
        {
            using type = D;
        };
        template <class... Types>
        struct return_type_helper<void, Types...> : std::common_type<Types...>
        {
            static_assert(std::conjunction_v<not_ref_wrapper<Types>...>,
                "Types cannot contain reference_wrappers when D is void");
        };

        template <class D, class... Types>
        using return_type = std::array<typename return_type_helper<D, Types...>::type,
            sizeof...(Types)>;
    }
    template <class D = void, class... Types>
    constexpr auto make_array(Types&&... t)
        -> detail::return_type<D, Types...>
    {
        return { std::forward<Types>(t)... };
    }

    // https://stackoverflow.com/questions/25068481/c11-constexpr-flatten-list-of-stdarray-into-array
    namespace detail
    {
        template <std::size_t... List>
        struct seq { };
        template <std::size_t N, std::size_t... List>
        struct gen_seq : gen_seq<N - 1, N - 1, List...> { };
        template <std::size_t... List>
        struct gen_seq<0, List...> : seq<List...> { };

        template <typename T, std::size_t N1, std::size_t... Indices1, std::size_t N2, std::size_t... Indices2>
        constexpr auto concat_array(const std::array<T, N1>& ar1, seq<Indices1...> seq1, const std::array<T, N2>& ar2, seq<Indices2...> seq2)
        {
            return std::array<T, N1 + N2> { ar1[Indices1]..., ar2[Indices2]... };
        }
    }
    template <typename T, std::size_t N>
    constexpr auto concat_array(const std::array<T, N>& ar)
    {
        return ar;
    }
    template <typename T, std::size_t N1, std::size_t N2, std::size_t... Ns>
    constexpr auto concat_array(const std::array<T, N1>& ar1, const std::array<T, N2>& ar2, const std::array<T, Ns>&... other)
    {
        return concat_array(detail::concat_array(ar1, detail::gen_seq<N1> { }, ar2, detail::gen_seq<N2> { }), other...);
    }

    namespace op
    {
        template <typename CharT, typename Traits>
        auto operator +(std::basic_string_view<CharT, Traits> lhs, std::basic_string_view<CharT, Traits> rhs)
            -> std::basic_string<CharT, Traits>
        {
            std::basic_string<CharT, Traits> ret;
            ret.reserve(lhs.size() + rhs.size());
            ret = lhs;
            ret += rhs;
            return ret;
        }

        template <typename CharT, typename Traits, typename Alloc>
        auto operator +(std::basic_string<CharT, Traits, Alloc> lhs, std::basic_string_view<CharT, Traits> rhs)
            -> std::basic_string<CharT, Traits, Alloc>
        {
            lhs += rhs;
            return lhs;
        }

        template <typename CharT, typename Traits, typename Alloc>
        auto operator +(std::basic_string_view<CharT, Traits> lhs, const std::basic_string<CharT, Traits, Alloc>& rhs)
            -> std::basic_string<CharT, Traits, Alloc>
        {
            std::basic_string<CharT, Traits> ret;
            ret.reserve(lhs.size() + rhs.size());
            ret = lhs;
            ret += rhs;
            return ret;
        }

        template <typename CharT, typename Traits>
        auto operator +(std::basic_string_view<CharT, Traits> lhs, const CharT* rhs)
            -> std::basic_string<CharT, Traits>
        {
            std::basic_string<CharT, Traits> ret = lhs;
            ret += rhs;
            return ret;
        }

        template <typename CharT, typename Traits>
        auto operator +(const CharT* lhs, std::basic_string_view<CharT, Traits> rhs)
            -> std::basic_string<CharT, Traits>
        {
            std::basic_string<CharT, Traits> ret = lhs;
            ret += rhs;
            return ret;
        }

        template <typename CharT, typename Traits>
        auto operator +(std::basic_string_view<CharT, Traits> lhs, CharT rhs)
            -> std::basic_string<CharT, Traits>
        {
            std::basic_string<CharT, Traits> ret;
            ret.reserve(lhs.size() + 1);
            ret = lhs;
            ret += rhs;
            return ret;
        }

        template <typename CharT, typename Traits>
        auto operator +(CharT lhs, std::basic_string_view<CharT, Traits> rhs)
            -> std::basic_string<CharT, Traits>
        {
            std::basic_string<CharT, Traits> ret;
            ret.reserve(1 + rhs.size());
            ret = lhs;
            ret += rhs;
            return ret;
        }
    }
}
