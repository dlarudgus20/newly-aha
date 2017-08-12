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
#include <stdexcept>

namespace aha::front
{
    class source;

    struct source_position
    {
        unsigned line;
        unsigned col;

        source_position next(source& src) const;
        source_position prev(source& src) const;
    };

    class source_positional_error : public std::runtime_error
    {
    public:
        source_positional_error(source& src, source_position pos, const std::string& msg)
            : m_src(src), m_pos(pos), std::runtime_error(msg)
        {
        }

        source& getSource() const
        {
            return m_src;
        }
        source_position getPosition() const
        {
            return m_pos;
        }

    private:
        source& m_src;
        source_position m_pos;
    };

    class invalid_byteseq : public source_positional_error
    {
    public:
        invalid_byteseq(source& src, source_position pos)
            : source_positional_error(src, pos, "invalid byteseq")
        {
        }
    };

    enum class source_state
    {
        some, exhausted, eof, error
    };

    class source
    {
    public:
        source() = default;
        source(const source&) = delete;
        source& operator =(const source&) = delete;

        virtual ~source();

        virtual std::string_view getName() = 0;

        virtual std::optional<std::pair<char32_t, source_position>> readChar() = 0;
        virtual source_state getState() const = 0;

        virtual char32_t getChar(source_position pos) const = 0;
        virtual unsigned getLineSize(unsigned line) const = 0;
        virtual source_position getEndpoint() const = 0;
    };

    class repl_source final : public source
    {
    public:
        explicit repl_source(std::string name = "<REPL>");
        virtual ~repl_source();

        void feedString(std::string_view line);
        void feedEof();

        virtual std::string_view getName() override;

        virtual std::optional<std::pair<char32_t, source_position>> readChar() override;
        virtual source_state getState() const override;

        virtual char32_t getChar(source_position pos) const override;
        virtual unsigned getLineSize(unsigned line) const override;
        virtual source_position getEndpoint() const override;

    private:
        std::string m_name;

        std::deque<char32_t> m_chars;
        std::vector<unsigned> m_lines { 0 };
        bool m_prev_is_CR = false;

        std::deque<char> m_input;
        bool m_input_end = false;
        bool m_error = false;

        bool popByte(unsigned char& ch);
    };
}
