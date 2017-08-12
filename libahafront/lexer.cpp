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
    lexer::lexer() = default;
    lexer::~lexer() = default;

    lex_result lexer::lex(source& src)
    {
        if (m_state == state::error)
            throw std::logic_error("lexer has an error");

        static const std::u32string_view punct_chars = U"!@$%^&*()-=+[]{};:,./<>?|";

        static const auto toks_punct = ext::make_array<std::u32string_view>(
            U"!", U"@", U"$", U"%", U"^", U"&", U"*", U"(", U")", U"-", U"=", U"+",
            U"[", U"]", U"{", U"}", U";", U":", U",", U".", U"/", U"<", U">", U"?",
            U"++", U"--", U">>", U"<<", U"==", U"!=", U"<=", U">=", U"&&", U"||",
            U"+=", U"-=", U"*=", U"/=", U"%=", U"&=", U"|=", U"^=", U"<<=", U">>=", U":=:",
            U"::", U"->", U"=>", U"|>", U"&>", U"<&", U"?.");

        static const auto toks_keyword = ext::make_array<std::u32string_view>(
            U"module", U"import", U"class", U"interface", U"enum", U"static", U"final"
            U"public", U"private", U"protected", U"internal",
            U"func", U"in", U"let", U"var", U"this", U"event", U"curry", U"uncurry",
            U"byte", U"sbyte", U"short", U"ushort", U"int", U"uint", U"long", U"ulong",
            U"bool", U"object", U"string");

        static const auto toks_comment_line = ext::make_array<std::u32string_view>(
            U"#", U"//");

        static const std::u32string_view tok_comment_block_begin = U"/*";
        static const std::u32string_view tok_comment_block_end = U"*/";

        auto prev_queue_size = m_tok_queue.size();
        while (m_tok_queue.size() == prev_queue_size)
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
                            return lex_result::eof;
                        }
                    }
                    else
                    {
                        return lex_result::exhausted;
                    }
                }
            }

            if (ch == U'\n')
                skip = true;
            if (m_str_token.empty())
                m_tok_beg = pos;

            if (m_state == state::indent)
            {
                if (ch == U'\n' || src.getState() == source_state::eof)
                {
                    // empty line

                    m_tok_queue.push_back(
                        make_token(
                            token_newline { },
                            src, m_tok_beg, pos));

                    m_str_token.clear();
                    m_tok_beg = pos;
                    done = true;
                }
                else if (!isSeperator(ch))
                {
                    if (!m_str_token.empty())
                    {
                        if (m_str_token.size() == m_indent_str.size())
                        {
                            if (m_str_token != m_indent_str)
                                throwErrorWithRevert(lexer_error(src, m_tok_beg, "invalid indentation"));
                        }
                        else if (m_str_token.size() < m_indent_str.size())
                        {
                            while (1)
                            {
                                m_indent_pos.pop_back();

if (m_indent_pos.empty())
{
    // no indent

    if (!m_str_token.empty())
        throwErrorWithRevert(lexer_error(src, m_tok_beg, "invalid indentation"));

    break;
}

if (m_indent_str.compare(0, m_indent_pos.back(), m_str_token) != 0)
throwErrorWithRevert(lexer_error(src, m_tok_beg, "invalid indentation"));
                            }

                            m_indent_str = std::move(m_str_token);
                        }
                        else
                        {
                            if (m_str_token.compare(0, m_indent_str.size(), m_indent_str) != 0)
                                throwErrorWithRevert(lexer_error(src, m_tok_beg, "invalid indentation"));

                            m_indent_pos.push_back(m_str_token.size());
                            m_indent_str = std::move(m_str_token);
                        }

                        m_tok_queue.push_back(
                            make_token(
                                token_indent { m_indent_pos.size() },
                                src, m_tok_beg, pos));

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
                    if (isSeperator(ch) || ch == '\n')
                    {
                        skip = true;
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

                            m_flags.punct = true;
                        }
                        else if (ch == U'#')
                        {
                            m_flags.comment_line = true;
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
                        if (m_str_token.empty() && ch == U'#')
                        {
                            m_flags.commented_out = true;
                        }
                        else if (!m_flags.commented_out && m_str_token.size() == 1)
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

                        m_tok_queue.push_back(
                            make_token(
                                token_newline { },
                                src, m_tok_beg, pos));

                        m_str_token.clear();
                        m_tok_beg = pos;
                        done = true;
                    }
                    else if (m_flags.comment_block)
                    {
                        if (ch == U'*')
                        {
                            m_flags.comment_block_might_closing = true;
                        }
                        else if (m_flags.comment_block_might_closing && ch == U'/')
                        {
                            m_flags.comment_block = false;
                            m_flags.commented_out = false;

                            m_str_token.clear();
                            m_tok_beg = pos;
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
                        if (m_flags.identifier)
                        {
                            if (!isIdentifierChar(ch))
                            {
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

                                if (done || m_idx_float_sep != -1 || m_idx_float_exp != -1 || m_idx_num_postfix != -1)
                                {
                                    if (m_str_token.size() == 2 && num_chars.find(m_str_token[1]) == std::u32string_view::npos)
                                    {
                                        throwErrorWithRevert(lexer_error(src, pos, "unexpected end of number literal"));
                                    }
                                }
                            }
                            else if (!isIdentifierChar(ch))
                            {
                                done = true;
                            }
                        }
                        else if (m_flags.punct)
                        {
                            done = (punct_chars.find(ch) != std::u32string_view::npos);

                            if (!m_str_token.empty())
                            {
                                std::u32string_view matched;
                                unsigned candidates = 0;

                                for (auto str : toks_punct)
                                {
                                    int sz = std::min(m_str_token.size(), str.size());
                                    if (m_str_token.compare(0, sz, str) == 0)
                                    {
                                        if (m_str_token.size() <= str.size())
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

                                    auto tok_end = m_tok_beg;
                                    for (unsigned i = 0; i < matched.size(); ++i)
                                        tok_end = tok_end.next(src);

                                    m_tok_queue.push_back(
                                        make_token(
                                            token_punct { std::u32string { matched } },
                                            src, m_tok_beg, tok_end));

                                    m_str_token.erase(m_str_token.begin(), m_str_token.begin() + matched.size());
                                    m_tok_beg = tok_end;
                                }
                            }
                        }

                        if (done)
                        {
                            // identifier & number
                            // punct token has made already

                            token tok;

                            if (m_flags.identifier)
                            {
                                auto it1 = std::find(m_contextual_keywords.begin(), m_contextual_keywords.end(), m_str_token);
                                if (it1 != m_contextual_keywords.end())
                                {
                                    tok = make_token(
                                        token_contextual_keyword { m_str_token },
                                        src, m_tok_beg, pos);
                                }
                                else
                                {
                                    auto it2 = std::find(toks_keyword.begin(), toks_keyword.end(), m_str_token);
                                    if (it2 != toks_keyword.end())
                                    {
                                        tok = make_token(
                                            token_keyword { m_str_token },
                                            src, m_tok_beg, pos);
                                    }
                                    else
                                    {
                                        tok = make_token(
                                            token_identifier { m_str_token },
                                            src, m_tok_beg, pos);
                                    }
                                }
                            }
                            else // if number
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
                                    is_float = true;

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

                                tok = make_token(std::move(tn), src, m_tok_beg, pos);
                            }

                            m_tok_queue.push_back(tok);

                            m_str_token.clear();
                            m_tok_beg = pos;
                        }
                    }

                    if (m_flags.comment_block_contains_newline)
                        m_state = state::after_comment;
                    else if (ch == U'\n')
                        m_state = state::indent;
                }
            }
            else if (m_state == state::after_comment)
            {
                if (ch == U'\n')
                {
                    m_tok_queue.push_back(
                        make_token(
                            token_newline { },
                            src, m_tok_beg, pos));

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

        return lex_result::done;
    }

    std::size_t lexer::getTokenQueueSize() const
    {
        return m_tok_queue.size();
    }

    token lexer::popToken()
    {
        if (m_tok_queue.empty())
            throw std::logic_error("token queue is empty");

        token ret = std::move(m_tok_queue.front());
        m_tok_queue.pop_front();
        return ret;
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
