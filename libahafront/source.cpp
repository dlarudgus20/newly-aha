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
#include "aha/front/source.hpp"

#include "is_newline.h"

namespace aha::front
{
    source::~source() = default;

    repl_source::repl_source(std::string name /* = "<repl>" */)
        : m_name(std::move(name))
    {
    }

    repl_source::~repl_source() = default;

    void repl_source::feedString(std::string_view line)
    {
        if (m_error)
            throw std::logic_error("source has an error");
        if (m_input_end)
            throw std::logic_error("repl_source was already fed EOF");

        m_input.insert(m_input.end(), line.begin(), line.end());
        m_input.push_back('\n');
    }

    void repl_source::feedEof()
    {
        if (m_error)
            throw std::logic_error("source has an error");
        if (m_input_end)
            throw std::logic_error("repl_source was already fed EOF");

        m_input_end = true;
    }

    std::string_view repl_source::getName()
    {
        return m_name;
    }

    std::optional<std::pair<char32_t, source_position>> repl_source::readChar()
    {
        if (m_error)
            throw std::logic_error("source has an error");

        char32_t ret;

        unsigned char b[4];
        int count = 0;
        if (!popByte(b[0]))
            return { };

        auto revert = [this, &b, &count]() {
            for (int i = count - 1; i >= 0; --i)
            {
                m_input.push_front(b[i]);
            }
        };
        auto popFollowing = [this, &b, &count, &revert](unsigned n) {
            count = 0;
            for (; n > 0; --n)
            {
                if (popByte(b[count]))
                {
                    if ((b[count++] & 0xc0) != 0x80)
                    {
                        revert();
                        m_error = true;
                        throw invalid_byteseq(*this, getEndpoint());
                    }
                }
                else
                {
                    revert();
                    return false;
                }
            }
            return true;
        };

        if ((b[0] & 0x80) == 0)
        {
            ret = static_cast<char32_t>(b[0]);
        }
        else if ((b[0] & 0xe0) == 0xc0)
        {
            if (!popFollowing(1))
                return { };
            ret = static_cast<char32_t>(((b[0] & 0x1f) << 6) | (b[1] & 0x3f));
        }
        else if ((b[0] & 0xf0) == 0xe0)
        {
            if (!popFollowing(2))
                return { };
            ret = static_cast<char32_t>(((b[0] & 0x0f) << 12) | ((b[1] & 0x3f) << 6) | (b[2] & 0x3f));
        }
        else if ((b[0] & 0xf8) == 0xf0)
        {
            if (!popFollowing(3))
                return { };
            ret = static_cast<char32_t>(((b[0] & 0x07) << 18) | ((b[1] & 0x3f) << 12) | ((b[2] & 0x3f) << 6) | (b[3] & 0x3f));
        }
        else
        {
            revert();
            m_error = true;
            throw invalid_byteseq(*this, getEndpoint());
        }

        if (m_prev_is_CR)
        {
            m_prev_is_CR = false;
            if (ret == U'\n')
            {
                // ignore and get next
                return readChar();
            }
        }

        if (is_newline(ret))
        {
            if (ret == U'\r')
                m_prev_is_CR = true;

            auto pos = getEndpoint();

            m_chars.push_back(U'\n');
            m_lines.push_back(m_chars.size());

            return std::make_pair(U'\n', pos);
        }
        else
        {
            auto pos = getEndpoint();

            m_chars.push_back(ret);

            return std::make_pair(ret, pos);
        }
    }

    source_state repl_source::getState() const
    {
        if (m_error)
            return source_state::error;

        if (!m_lines.empty())
            return source_state::some;

        if (m_input_end)
            return source_state::eof;
        else
            return source_state::exhausted;
    }

    char32_t repl_source::getChar(source_position pos) const
    {
        if (m_error)
            throw std::logic_error("source has an error");

        if (pos.col >= getLineSize(pos.line))
            throw std::out_of_range("col is out of range");

        return m_chars[m_lines[pos.line] + pos.col];
    }

    bool repl_source::popByte(unsigned char& ch)
    {
        if (m_input.empty())
            return false;

        ch = static_cast<unsigned char>(m_input.front());
        m_input.pop_front();

        return true;
    }

    unsigned repl_source::getLineSize(unsigned line) const
    {
        unsigned lineEnd;

        if (line + 1 < m_lines.size())
            lineEnd = m_lines[line + 1];
        else
            lineEnd = m_chars.size();

        return lineEnd - m_lines[line];
    }

    source_position repl_source::getEndpoint() const
    {
        return { m_lines.size() - 1, m_chars.size() - m_lines.back() };
    }

    source_position source_position::next(source& src) const
    {
        if (col + 1 < src.getLineSize(line))
            return { line, col + 1 };
        else
            return { line + 1, 0 };
    }

    source_position source_position::prev(source& src) const
    {
        if (col == 0)
            return { line - 1, src.getLineSize(line - 1) - 1 };
        else
            return { line, col - 1 };
    }
}
