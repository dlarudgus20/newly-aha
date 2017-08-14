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

#include <iostream>
#include <string>
#include <type_traits>
#include <variant>
#include <locale>

#include <boost/program_options.hpp>
#include <boost/iterator/transform_iterator.hpp>
namespace bpo = boost::program_options;

#include "../libahafront/aha/front/source.hpp"
#include "../libahafront/aha/front/lexer.hpp"
#include "../libahafront/aha/front/parser.hpp"

using namespace aha::front;

inline std::string utf32narrow(std::u32string_view str)
{
    auto fn = [](char32_t ch) { return static_cast<char>(ch); };
    auto tbeg = boost::iterators::make_transform_iterator(str.begin(), fn);
    auto tend = boost::iterators::make_transform_iterator(str.end(), fn);
    return std::string(tbeg, tend);
};

int main(int argc, char* argv[])
{
    /*bpo::options_description opt("Options:");
    opt.add_options()
        ("help,h", "Display this information")
        ("input", bpo::value<std::string>()->required(), "specify input source file")
        ;
    bpo::positional_options_description pos;
    pos.add("input", -1);

    bpo::variables_map vm;

    try
    {
        bpo::command_line_parser parser(argc, argv);
        parser.options(opt);
        parser.positional(pos);

        bpo::store(parser.run(), vm);
        bpo::notify(vm);
    }
    catch (bpo::error& e)
    {
        std::cerr << e.what() << "\n\n" << opt << "\n";
        return -1;
    }

    if (vm.count("help"))
    {
        std::cout << opt << "\n";
        return 0;
    }*/

    repl_source src;
    lexer ll;
    parser yy;

    auto get_input = [&src] (bool fresh) {
        std::string str;
        std::cout << (fresh ? ">> " : "-- ");

        if (!getline(std::cin, str))
        {
            src.feedEof();
        }
        else
        {
            if (fresh && str == ":{")
            {
                while (true)
                {
                    std::cout << "-- ";
                    if (!getline(std::cin, str))
                    {
                        src.feedEof();
                        break;
                    }
                    else if (str == ":}")
                    {
                        break;
                    }
                    else
                    {
                        src.feedString(str + "\n");
                    }
                }
            }
            else
            {
                src.feedString(str + "\n");
            }
        }
    };


    while (true)
    {
        try
        {
            bool fresh = true;

            while (ll.getTokenQueueSize() == 0)
            {
                auto rs = ll.lex(src);
                if (rs == lex_result::exhausted)
                {
                    get_input(fresh);
                    fresh = false;
                }
                else if (rs == lex_result::eof)
                {
                    // clear all
                    throw std::logic_error("not implemented");
                }
            }

            auto tok = ll.popToken();

            if (std::holds_alternative<token_indent>(tok.data))
            {
                auto t = std::get<token_indent>(tok.data);
                std::cout << "indent { " << t.level << " }\n";
            }
            else if (std::holds_alternative<token_newline>(tok.data))
            {
                auto t = std::get<token_newline>(tok.data);
                std::cout << "newline {}\n";
            }
            else if (std::holds_alternative<token_punct>(tok.data))
            {
                auto t = std::get<token_punct>(tok.data);
                auto str = utf32narrow(t.str);
                std::cout << "punct { '" << str << "' }\n";
            }
            else if (std::holds_alternative<token_keyword>(tok.data))
            {
                auto t = std::get<token_keyword>(tok.data);
                auto str = utf32narrow(t.str);
                std::cout << "keyword { '" << str << "' }\n";
            }
            else if (std::holds_alternative<token_contextual_keyword>(tok.data))
            {
                auto t = std::get<token_contextual_keyword>(tok.data);
                auto str = utf32narrow(t.str);
                std::cout << "contextual keyword { '" << str << "' }\n";
            }
            else if (std::holds_alternative<token_identifier>(tok.data))
            {
                auto t = std::get<token_identifier>(tok.data);
                auto str = utf32narrow(t.str);
                std::cout << "identifier { '" << str << "' }\n";
            }
            else if (std::holds_alternative<token_number>(tok.data))
            {
                auto t = std::get<token_number>(tok.data);
                if (!t.is_float)
                {
                    std::cout << "integer [radix:" << t.radix << "] { " << t.integer << t.postfix << " }\n";
                }
                else
                {
                    std::cout << "float [radix:" << t.radix << "] { " << t.integer;
                    if (!t.fraction.empty())
                        std::cout << "." << t.fraction;
                    if (!t.exponent.empty())
                        std::cout << (t.radix == 10 ? "e" : "p") << t.exponent;
                    std::cout << t.postfix << " }\n";
                }
            }
        }
        catch (source_positional_error& ex)
        {
            auto pos = ex.getPosition();
            std::cerr << ex.getSource().getName() << ":" << (pos.line + 1) << ":" << (pos.col + 1) << ": " << ex.what() << std::endl;

            std::string tmp;
            getline(std::cin, tmp);
            return -1;
        }
    }

    return 0;
}
