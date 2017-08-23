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

#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <optional>
#include <variant>
#include <utility>

#include "source.hpp"

namespace aha::front
{
    class lexer_error : public source_positional_error
    {
    public:
        lexer_error(source& src, source_position pos, const std::string& msg)
            : source_positional_error(src, pos, "lexer error: " + msg)
        {
        }
    };

    enum class lex_result
    {
        done, exhausted, eof, error
    };

    struct token_indent
    {
        unsigned level;
    };
    struct token_newline
    {
    };
    struct token_punct
    {
        std::u32string str;
    };
    struct token_keyword
    {
        std::u32string str;
    };
    struct token_contextual_keyword
    {
        std::u32string str;
    };
    struct token_identifier
    {
        std::u32string str;
    };
    struct token_normal_string
    {
        char32_t delimiter;
        std::u32string str;
    };
    struct token_raw_string
    {
        char32_t delimiter;
        std::u32string str;
    };
    struct token_interpol_string_start
    {
        std::u32string str;
    };
    struct token_interpol_string_mid
    {
        std::u32string str;
    };
    struct token_interpol_string_end
    {
        std::u32string str;
    };
    struct token_number
    {
        unsigned radix;
        std::string integer, fraction, exponent, postfix;
        bool is_float;
    };

    struct token
    {
        source* ptr_src;
        source_position beg;
        source_position end;

        std::variant<
            token_indent,
            token_newline,
            token_punct,
            token_keyword,
            token_contextual_keyword,
            token_identifier,
            token_normal_string,
            token_raw_string,
            token_interpol_string_start,
            token_interpol_string_mid,
            token_interpol_string_end,
            token_number
            > data;
    };

    class lexer final
    {
    public:
        lexer(const lexer&) = delete;
        lexer& operator =(const lexer&) = delete;

        lexer();
        ~lexer();

        void clearBuffer();
        void clearAll();

        std::optional<token> lex(source& src);
        lex_result getLastResult() const;

        void enableInterpolatedBlockEnd(bool enable);

        void setContextualKeyword(std::vector<std::u32string> keywords);

    private:
        void init();

        enum class state
        {
            indent,
            any,
            after_comment,
            error
        };

        std::deque<char32_t> m_buf;
        source_position m_buf_beg;

        std::u32string m_str_token;
        source_position m_tok_beg;

        state m_state;

        std::u32string m_indent_str;
        std::vector<std::size_t> m_indent_pos;

        struct
        {
            bool identifier : 1;
            bool unknown_number : 1;
            bool binary : 1;
            bool octal : 1;
            bool heximal : 1;
            bool decimal : 1;
            bool punct : 1;
            bool normal_string : 1;
            bool raw_string : 1;
            bool interpol_string : 1;
            bool comment_line : 1;
            bool comment_block : 1;
            bool comment_block_contains_newline : 1;
            bool comment_block_might_closing : 1;
            bool commented_out : 1;

            bool interpol_string_after : 1;
            bool enable_interpol_block_end : 1;
        } m_flags;
        
        int m_idx_float_sep;
        int m_idx_float_exp;
        int m_idx_num_postfix;

        lex_result m_last_result;

        std::vector<std::u32string> m_contextual_keywords;

        static bool isSeperator(char32_t ch);
        static bool isIdentifierFirstChar(char32_t ch);
        static bool isIdentifierChar(char32_t ch);

        template <typename Exception>
        void throwError(Exception&& ex)
        {
            m_state = state::error;
            m_last_result = lex_result::error;
            throw std::forward<Exception>(ex);
        }
    };
}
