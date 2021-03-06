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

#include "pch.h"
#include "aha/front/lexer.hpp"

#include "aha/front/source.hpp"

#include <boost/iterator/transform_iterator.hpp>

#include "is_newline.h"
#include "ext.h"

namespace
{
    using namespace aha::front;

    template <typename TokenData>
    token make_token(TokenData&& data, source& src, source_position beg, source_position end)
    {
        token tok { &src, beg, end, std::forward<TokenData>(data) };
        return tok;
    }
}

namespace aha::front
{
    lexer::lexer()
    {
        init();
    }

    lexer::~lexer() = default;

    void lexer::init()
    {
        m_flags.interpol_string_after = false;
        m_flags.enable_interpol_block_end = false;

        m_last_result = lex_result::exhausted;
    }

    void lexer::clearBuffer()
    {
        m_buf.clear();
        m_str_token.clear();

        m_state = state::indent;
    }

    void lexer::clearAll()
    {
        clearBuffer();
        init();
    }

    std::optional<token> lexer::lex(source& src)
    {
        if (m_state == state::error)
            throw std::logic_error("lexer has an error");

        static const std::u32string_view punct_chars = U"~!@$%^&*()-=+[];:,./<>?|";

        static const auto toks_punct = ext::make_array<std::u32string_view>(
            U"~", U"!", U"@", U"$", U"%", U"^", U"&", U"*", U"(", U")", U"-", U"=", U"+",
            U"[", U"]", U";", U":", U",", U".", U"/", U"<", U">", U"?",
            U"++", U"--", U">>", U"<<", U"==", U"!=", U"<=", U">=", U"&&", U"||",
            U"+=", U"-=", U"*=", U"/=", U"%=", U"&=", U"|=", U"^=", U"<<=", U">>=", U":=:",
            U"::", U"->", U"=>", U"|>", U"&>", U"<&", U"?.");

        static const auto toks_keyword = ext::make_array<std::u32string_view>(
            U"module", U"import", U"class", U"interface", U"enum", U"static", U"final",
            U"public", U"private", U"protected", U"internal",
            U"func", U"in", U"let", U"var", U"this", U"event", U"curry", U"uncurry",
            U"byte", U"sbyte", U"short", U"ushort", U"int", U"uint", U"long", U"ulong",
            U"bool", U"object", U"string");

        static const auto toks_comment_line = ext::make_array<std::u32string_view>(
            U"#", U"//");

        static const std::u32string_view tok_comment_block_begin = U"/*";
        static const std::u32string_view tok_comment_block_end = U"*/";

        std::optional<token> ret;

        while (!ret)
        {
            char32_t ch;
            source_position pos;

            bool done = false;
            bool skip = false;

            auto revert = [this, &ch, &pos, &skip]() {
                if (!skip)
                    m_str_token.push_back(ch);

                m_buf_beg = m_tok_beg;
                m_buf.insert(m_buf.begin(), m_str_token.begin(), m_str_token.end());
                m_str_token.clear();
            };
            auto throwErrorWithRevert = [this, revert](auto&& ex) {
                revert();
                throwError(std::forward<decltype(ex)>(ex));
            };

            if (!m_buf.empty())
            {
                ch = m_buf.front();
                pos = m_buf_beg;
                
                m_buf.pop_front();
                m_buf_beg = m_buf_beg.next(src);
            }
            else
            {
                auto pr = src.readChar();
                
                if (pr)
                {
                    ch = pr->first;
                    pos = pr->second;
                }
                else
                {
                    if (src.getState() == source_state::eof)
                    {
                        if (!m_str_token.empty())
                        {
                            skip = true;
                            ch = U'\0';
                            pos = src.getEndpoint().prev(src);
                        }
                        else
                        {
                            m_last_result = lex_result::eof;
                            return { };
                        }
                    }
                    else
                    {
                        m_last_result = lex_result::exhausted;
                        return { };
                    }
                }
            }

            if (m_str_token.empty())
                m_tok_beg = pos;

            if (m_state == state::indent)
            {
                if (ch == U'\n' || src.getState() == source_state::eof)
                {
                    // empty line

                    assert(!ret);
                    ret = make_token(
                        token_newline { },
                        src, m_tok_beg, pos);

                    m_str_token.clear();
                    m_tok_beg = pos;
                    done = true;
                    skip = true;
                }
                else if (!isSeperator(ch))
                {
                    if (m_str_token.empty())
                    {
                        m_indent_pos.clear();
                        m_indent_str.clear();
                    }
                    else
                    {
                        if (m_str_token.size() == m_indent_str.size())
                        {
                            if (m_str_token != m_indent_str)
                                throwErrorWithRevert(lexer_error(src, m_tok_beg, "invalid indentation"));
                        }
                        else if (m_str_token.size() < m_indent_str.size())
                        {
                            auto it = m_indent_pos.end() - 1;
                            while (1)
                            {
                                if (m_str_token.size() > *it)
                                    throwErrorWithRevert(lexer_error(src, m_tok_beg, "invalid indentation"));

                                if (m_str_token.size() == *it)
                                {
                                    if (m_indent_str.compare(0, *it, m_str_token) != 0)
                                        throwErrorWithRevert(lexer_error(src, m_tok_beg, "invalid indentation"));

                                    break;
                                }

                                if (it == m_indent_pos.begin())
                                    throwErrorWithRevert(lexer_error(src, m_tok_beg, "invalid indentation"));

                                --it;
                            }

                            m_indent_pos.erase(it + 1, m_indent_pos.end());
                            m_indent_str = std::move(m_str_token);
                        }
                        else
                        {
                            if (m_str_token.compare(0, m_indent_str.size(), m_indent_str) != 0)
                                throwErrorWithRevert(lexer_error(src, m_tok_beg, "invalid indentation"));

                            m_indent_pos.push_back(m_str_token.size());
                            m_indent_str = std::move(m_str_token);
                        }

                        assert(!ret);
                        ret = make_token(
                            token_indent { m_indent_pos.size() },
                            src, m_tok_beg, pos);

                        m_str_token.clear();
                        m_tok_beg = pos;
                    }

                    done = true;
                    m_state = state::any;
                }
            }
            else if (m_state == state::any)
            {
                if (m_str_token.empty())
                {
                    if (isSeperator(ch))
                    {
                        skip = true;
                    }
                    else if (ch == '\n')
                    {
                        assert(!ret);
                        ret = make_token(
                            token_newline { },
                            src, m_tok_beg, pos);

                        m_str_token.clear();
                        m_tok_beg = pos;
                        done = true;
                        skip = true;

                        m_state = state::indent;
                    }
                    else
                    {
                        m_idx_float_sep = -1;
                        m_idx_float_exp = -1;
                        m_idx_num_postfix = -1;

                        m_flags.identifier = false;
                        m_flags.unknown_number = false;
                        m_flags.binary = false;
                        m_flags.octal = false;
                        m_flags.heximal = false;
                        m_flags.decimal = false;
                        m_flags.punct = false;
                        m_flags.normal_string = false;
                        m_flags.raw_string = false;
                        m_flags.interpol_string = false;
                        m_flags.comment_line = false;
                        m_flags.comment_block = false;
                        m_flags.comment_block_contains_newline = false;
                        m_flags.comment_block_might_closing = false;

                        if (isIdentifierFirstChar(ch))
                        {
                            m_flags.identifier = true;
                        }
                        else if (ch == U'0')
                        {
                            m_flags.unknown_number = true;
                        }
                        else if (U'1' <= ch && ch <= U'9')
                        {
                            m_flags.decimal = true;
                        }
                        else if (punct_chars.find(ch) != std::u32string_view::npos)
                        {
                            if (ch == U'/')
                            {
                                m_flags.comment_line = true;
                                m_flags.comment_block = true;
                            }
                            else if (ch == U'@')
                            {
                                m_flags.raw_string = true;
                            }

                            m_flags.punct = true;
                        }
                        else if (ch == U'#')
                        {
                            m_flags.comment_line = true;
                        }
                        else if (ch == U'\'' || ch == U'\"')
                        {
                            m_flags.normal_string = true;
                        }
                        else if (ch == U'`')
                        {
                            m_flags.interpol_string = true;
                        }
                        else if (m_flags.enable_interpol_block_end && ch == U'}')
                        {
                            m_flags.interpol_string = true;
                        }
                        else
                        {
                            throwErrorWithRevert(lexer_error(src, pos, "unexpected character"));
                        }
                    }
                }
                else
                {
                    if (m_flags.comment_line)
                    {
                        if (m_str_token == U"#")
                        {
                            m_flags.commented_out = true;
                        }
                        else if (m_str_token == U"/")
                        {
                            if (ch == U'*')
                            {
                                m_flags.comment_line = false;
                                m_flags.commented_out = true;
                            }
                            else if (ch == U'/')
                            {
                                m_flags.comment_block = false;
                                m_flags.commented_out = true;
                            }
                            else
                            {
                                m_flags.comment_line = false;
                                m_flags.comment_block = false;
                            }
                        }
                    }

                    bool commented_out = m_flags.commented_out;

                    if (m_flags.comment_line && ch == U'\n')
                    {
                        m_flags.comment_line = false;
                        m_flags.commented_out = false;

                        assert(!ret);
                        ret = make_token(
                            token_newline { },
                            src, m_tok_beg, pos);

                        m_str_token.clear();
                        m_tok_beg = pos;
                        done = true;
                        skip = true;

                        m_state = state::indent;
                    }
                    else if (m_flags.comment_block)
                    {
                        if (ch == U'*' && m_str_token.size() >= 2)
                        {
                            m_flags.comment_block_might_closing = true;
                        }
                        else if (m_flags.comment_block_might_closing && ch == U'/')
                        {
                            m_flags.comment_block = false;
                            m_flags.commented_out = false;

                            if (m_flags.comment_block_contains_newline)
                                m_state = state::after_comment;

                            m_str_token.clear();
                            m_tok_beg = pos;
                            skip = true;
                        }
                        else
                        {
                            if (ch == U'\n')
                            {
                                m_flags.comment_block_contains_newline = true;
                            }

                            m_flags.comment_block_might_closing = false;
                        }
                    }

                    if (!commented_out)
                    {
                        if (m_flags.raw_string && m_str_token.size() == 1)
                        {
                            if (ch != U'\'' && ch != U'\"')
                                m_flags.raw_string = false;
                        }

                        if (m_flags.raw_string)
                        {
                            if (m_str_token.size() >= 3 && m_str_token.back() == m_str_token[1] && ch != m_str_token[1])
                            {
                                auto delimiter = m_str_token[1];

                                // if not escaped
                                if ((m_str_token.size() - m_str_token.find_last_not_of(delimiter)) % 2 == 0)
                                {
                                    assert(!ret);
                                    ret = make_token(
                                        token_raw_string { m_str_token[1], m_str_token.substr(2, m_str_token.size() - 3) },
                                        src, m_tok_beg, pos);

                                    m_str_token.clear();
                                    m_tok_beg = pos;
                                    done = true;
                                }
                            }

                        }
                        else if (m_flags.normal_string)
                        {
                            if (ch != U' ' && (isSeperator(ch) || is_newline(ch)))
                            {
                                throwErrorWithRevert(lexer_error(src, pos,
                                    "non-raw string literal cannot contain seperator or newline character except space"));
                            }
                            else if (ch == m_str_token[0] && m_str_token.back() != U'\\')
                            {
                                assert(!ret);
                                ret = make_token(
                                    token_normal_string { m_str_token[0], m_str_token.substr(1) },
                                    src, m_tok_beg, pos);

                                m_str_token.clear();
                                m_tok_beg = pos;
                                done = true;
                                skip = true;
                            }
                        }
                        else if (m_flags.interpol_string)
                        {
                            // TODO: bugs
                            if (ch != U' ' && (isSeperator(ch) || is_newline(ch)))
                            {
                                throwErrorWithRevert(lexer_error(src, pos,
                                    "non-raw string literal cannot contain seperator or newline character except space"));
                            }
                            else if ((m_str_token.size() == 1 && m_str_token[0] == U'`') || (m_str_token.size() == 2 && m_str_token[0] == U'@'))
                            {
                                // nothing
                            }
                            else if (ch == U'`' && m_str_token.back() != U'\\')
                            {
                                assert(!ret);
                                ret = make_token(
                                    token_interpol_string_end { m_str_token.substr(1) },
                                    src, m_tok_beg, pos);

                                m_flags.interpol_string_after = false;
                                m_flags.enable_interpol_block_end = false;

                                m_str_token.clear();
                                m_tok_beg = pos;
                                done = true;
                                skip = true;
                            }
                            else if (m_str_token.back() == U'$' && ch == U'{')
                            {
                                auto str = m_str_token.substr(1, m_str_token.size() - 2);

                                if (m_str_token.front() == U'`')
                                {
                                    assert(!ret);
                                    ret = make_token(
                                        token_interpol_string_start { std::move(str) },
                                        src, m_tok_beg, pos);

                                    m_flags.interpol_string_after = true;
                                    m_flags.enable_interpol_block_end = true;
                                }
                                else
                                {
                                    assert(m_str_token.front() == U'}');

                                    assert(!ret);
                                    ret = make_token(
                                        token_interpol_string_mid { std::move(str) },
                                        src, m_tok_beg, pos);
                                }

                                m_str_token.clear();
                                m_tok_beg = pos;
                                done = true;
                                skip = true;
                            }
                        }
                        else if (m_flags.identifier)
                        {
                            if (!isIdentifierChar(ch))
                            {
                                assert(!ret);

                                auto it1 = std::find(m_contextual_keywords.begin(), m_contextual_keywords.end(), m_str_token);
                                if (it1 != m_contextual_keywords.end())
                                {
                                    ret = make_token(
                                        token_contextual_keyword { m_str_token },
                                        src, m_tok_beg, pos);
                                }
                                else
                                {
                                    auto it2 = std::find(toks_keyword.begin(), toks_keyword.end(), m_str_token);
                                    if (it2 != toks_keyword.end())
                                    {
                                        ret = make_token(
                                            token_keyword { m_str_token },
                                            src, m_tok_beg, pos);
                                    }
                                    else
                                    {
                                        ret = make_token(
                                            token_identifier { m_str_token },
                                            src, m_tok_beg, pos);
                                    }
                                }

                                m_str_token.clear();
                                m_tok_beg = pos;
                                done = true;
                            }
                        }
                        else if (m_flags.unknown_number)
                        {
                            if (ch == U'b' || ch == U'B')
                            {
                                m_flags.binary = true;
                            }
                            else if (ch == U'c' || ch == U'C')
                            {
                                m_flags.octal = true;
                            }
                            else if (ch == U'x' || ch == U'X')
                            {
                                m_flags.heximal = true;
                            }
                            else if (ch == U'd' || ch == U'D' || (U'0' <= ch && ch <= U'9'))
                            {
                                m_flags.decimal = true;
                            }
                            else if (ch == U'.' || ch == U'e')
                            {
                                // decimal floating-point
                                m_flags.decimal = true;

                                m_flags.unknown_number = false;
                                goto decimal_floating;
                            }
                            else if (isIdentifierFirstChar(ch))
                            {
                                m_flags.decimal = true;
                                m_idx_num_postfix = 1;
                            }
                            else
                            {
                                throwErrorWithRevert(lexer_error(src, pos, "unexpected character"));
                            }
                            m_flags.unknown_number = false;
                        }
                        else if (m_flags.binary || m_flags.octal || m_flags.decimal || m_flags.heximal)
                        {
                        decimal_floating:

                            std::u32string_view exp_chars = m_flags.decimal ? U"eE" : U"pP";
                            std::u32string_view num_chars;
                            if (m_flags.binary)
                                num_chars = U"01";
                            else if (m_flags.octal)
                                num_chars = U"01234567";
                            else if (m_flags.decimal)
                                num_chars = U"0123456789";
                            else if (m_flags.heximal)
                                num_chars = U"0123456789ABCDEFabcdef";

                            if (m_idx_num_postfix == -1)
                            {
                                if (num_chars.find(ch) != std::u32string_view::npos)
                                {
                                    // okay
                                }
                                else if (ch == U'.')
                                {
                                    if (m_idx_float_sep == -1 && m_idx_float_exp == -1)
                                    {
                                        m_idx_float_sep = m_str_token.size();
                                    }
                                    else
                                    {
                                        done = true;
                                    }
                                }
                                else if (m_idx_float_exp == -1 && exp_chars.find(ch) != std::u32string_view::npos)
                                {
                                    m_idx_float_exp = m_str_token.size();
                                }
                                else if (isIdentifierFirstChar(ch))
                                {
                                    m_idx_num_postfix = m_str_token.size();
                                }
                                else if (m_idx_float_exp == m_str_token.size() - 1 && isIdentifierChar(ch))
                                {
                                    // 'e', 'E', 'p', 'P' char is not exponent, but postfix
                                    m_idx_num_postfix = m_idx_float_exp;
                                    m_idx_float_exp = -1;
                                }
                                else
                                {
                                    done = true;
                                }

                                if (done)
                                {
                                    std::u32string_view prefix = U"bBcCdDxX";
                                    if (m_str_token.size() == 2 && m_str_token[0] == U'0' && prefix.find(m_str_token[1]) != std::u32string_view::npos)
                                    {
                                        throwErrorWithRevert(lexer_error(src, pos, "unexpected end of number literal"));
                                    }
                                }
                            }
                            else if (!isIdentifierChar(ch))
                            {
                                done = true;
                            }

                            if (done)
                            {
                                int beg1 = 0, end1 = m_str_token.size();
                                int beg2 = end1, end2 = end1;
                                int beg3 = end1, end3 = end1;
                                int beg4 = end1;

                                bool is_float = false;

                                if (m_str_token[0] == U'0' && m_str_token.size() >= 3)
                                {
                                    bool prefix = false;

                                    if (m_flags.binary && (m_str_token[1] == U'b' || m_str_token[1] == U'B'))
                                        prefix = true;
                                    else if (m_flags.octal && (m_str_token[1] == U'c' || m_str_token[1] == U'C'))
                                        prefix = true;
                                    else if (m_flags.decimal && (m_str_token[1] == U'd' || m_str_token[1] == U'D'))
                                        prefix = true;
                                    else if (m_flags.heximal && (m_str_token[1] == U'x' || m_str_token[1] == U'X'))
                                        prefix = true;

                                    beg1 = 2;
                                }

                                if (m_idx_float_sep != -1)
                                {
                                    is_float = true;

                                    end1 = m_idx_float_sep;
                                    beg2 = end1 + 1;
                                }
                                if (m_idx_float_exp != -1)
                                {
                                    is_float = true;

                                    end2 = m_idx_float_exp;
                                    beg3 = end2 + 1;

                                    if (beg2 > end2)
                                    {
                                        end1 = beg2 = end2;
                                    }
                                }
                                if (m_idx_num_postfix != -1)
                                {
                                    end3 = m_idx_num_postfix;
                                    beg4 = end3;

                                    if (beg3 > end3)
                                    {
                                        end2 = beg3 = end3;
                                        if (beg2 > end2)
                                        {
                                            end1 = beg2 = end2;
                                        }
                                    }
                                }

                                unsigned radix;
                                if (m_flags.binary)
                                    radix = 2;
                                else if (m_flags.octal)
                                    radix = 8;
                                else if (m_flags.decimal)
                                    radix = 10;
                                else // if (m_flags.heximal)
                                    radix = 16;

                                auto substr8 = [](std::u32string_view str, std::size_t pos, std::size_t count) {
                                    auto sub = str.substr(pos, count);

                                    auto fn = [](char32_t ch) { return static_cast<char>(ch); };
                                    auto tbeg = boost::iterators::make_transform_iterator(sub.begin(), fn);
                                    auto tend = boost::iterators::make_transform_iterator(sub.end(), fn);

                                    return std::string(tbeg, tend);
                                };

                                token_number tn;
                                tn.radix = radix;
                                tn.integer = substr8(m_str_token, beg1, end1 - beg1);
                                tn.fraction = substr8(m_str_token, beg2, end2 - beg2);
                                tn.exponent = substr8(m_str_token, beg3, end3 - beg3);
                                tn.postfix = substr8(m_str_token, beg4, std::u32string_view::npos);
                                tn.is_float = is_float;

                                assert(!ret);
                                ret = make_token(std::move(tn), src, m_tok_beg, pos);

                                m_str_token.clear();
                                m_tok_beg = pos;
                            }
                        }
                        else if (m_flags.punct)
                        {
                            if (punct_chars.find(ch) == std::u32string_view::npos)
                                done = true;

                            if (!m_str_token.empty())
                            {
                                std::u32string_view matched;
                                unsigned candidates = 0;

                                for (auto str : toks_punct)
                                {
                                    int sz = std::min(m_str_token.size(), str.size());
                                    if (m_str_token.compare(0, sz, str, 0, sz) == 0)
                                    {
                                        if (m_str_token.size() >= str.size())
                                        {
                                            if (str.size() >= matched.size())
                                            {
                                                matched = str;
                                            }
                                        }
                                        else
                                        {
                                            ++candidates;
                                        }
                                    }
                                }

                                if (done || candidates == 0)
                                {
                                    if (matched.empty())
                                        throwErrorWithRevert(lexer_error(src, m_tok_beg, "unexpected character"));

                                    assert(!ret);

                                    auto tok_end = m_tok_beg;
                                    for (unsigned i = 0; i < matched.size(); ++i)
                                        tok_end = tok_end.next(src);

                                    ret = make_token(
                                        token_punct { std::u32string { matched } },
                                        src, m_tok_beg, tok_end);

                                    m_str_token.erase(m_str_token.begin(), m_str_token.begin() + matched.size());
                                    m_tok_beg = tok_end;
                                    done = true;
                                }
                            }
                        }
                    }
                }
            }
            else if (m_state == state::after_comment)
            {
                if (ch == U'\n')
                {
                    assert(!ret);
                    ret = make_token(
                        token_newline { },
                        src, m_tok_beg, pos);

                    m_str_token.clear();
                    m_tok_beg = pos;
                    done = true;
                    skip = true;

                    m_state = state::indent;
                }
                else if (!isSeperator(ch))
                {
                    throwErrorWithRevert(lexer_error(src, pos, "the line which contains the end of multi-line comment must be empty"));
                }
            }

            if (!skip)
                m_str_token.push_back(ch);

            if (done)
            {
                m_buf_beg = m_tok_beg;
                m_buf.insert(m_buf.begin(), m_str_token.begin(), m_str_token.end());
                m_str_token.clear();
            }
        }

        m_last_result = lex_result::done;
        return ret;
    }

    lex_result lexer::getLastResult() const
    {
        return m_last_result;
    }

    void lexer::enableInterpolatedBlockEnd(bool enable)
    {
        if (!m_flags.interpol_string_after)
            throw std::logic_error("dis/enabling interpolated block end can be done only during interpolated string");

        m_flags.enable_interpol_block_end = enable;
    }

    void lexer::setContextualKeyword(std::vector<std::u32string> keywords)
    {
        m_contextual_keywords = std::move(keywords);
    }

    bool lexer::isSeperator(char32_t ch)
    {
        return u_isblank(ch);
    }

    bool lexer::isIdentifierFirstChar(char32_t ch)
    {
        auto category = u_charType(ch);

        if (ch == U'_')
            return true;
        if (category == U_UPPERCASE_LETTER)
            return true;
        if (category == U_LOWERCASE_LETTER)
            return true;
        if (category == U_TITLECASE_LETTER)
            return true;
        if (category == U_MODIFIER_LETTER)
            return true;
        if (category == U_OTHER_LETTER)
            return true;
        if (category == U_LETTER_NUMBER)
            return true;

        return false;
    }

    bool lexer::isIdentifierChar(char32_t ch)
    {
        if (isIdentifierFirstChar(ch))
            return true;

        auto category = u_charType(ch);

        if (category == U_NON_SPACING_MARK)
            return true;
        if (category == U_COMBINING_SPACING_MARK)
            return true;
        if (category == U_DECIMAL_DIGIT_NUMBER)
            return true;
        if (category == U_CONNECTOR_PUNCTUATION)
            return true;
        if (category == U_FORMAT_CHAR)
            return true;

        return false;
    }
}
