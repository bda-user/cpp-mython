#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

struct Lexer::Helper {

    static Token Id(Lexer& lexer) {

        if(lexer.indent_equal_) {
            lexer.indent_equal_ = false;
        } else // multi Dedent
        if((lexer.token_.Is<token_type::Newline>() ||
            lexer.token_.Is<token_type::Dedent>()) &&
            lexer.indent_count_) {

            --lexer.indent_count_;
            return token_type::Dedent{};
        }

        char c = lexer.input_.peek();
        std::string s;

        while('_' == lexer.input_.peek() || std::isalnum(lexer.input_.peek())) {
            s.push_back(static_cast<char>(lexer.input_.get()));
        }

        switch (c) {
        case '_':
            return token_type::Id{s};
        case 'c':
            if(s == "class"s) return token_type::Class();
            break;
        case 'r':
            if(s == "return"s) return token_type::Return();
            break;
        case 'i':
            if(s == "if"s) return token_type::If();
            break;
        case 'e':
            if(s == "else"s) return token_type::Else();
            break;
        case 'd':
            if(s == "def"s) return token_type::Def();
            break;
        case 'p':
            if(s == "print"s) return token_type::Print();
            break;
        case 'a':
            if(s == "and"s) return token_type::And();
            break;
        case 'o':
            if(s == "or"s) return token_type::Or();
            break;
        case 'n':
            if(s == "not"s) return token_type::Not();
            break;
        case 'N':
            if(s == "None"s) return token_type::None();
            break;
        case 'T':
            if(s == "True"s) return token_type::True();
            break;
        case 'F':
            if(s == "False"s) return token_type::False();
            break;
        }
        return token_type::Id{s};
    }

    static Token Number(std::istream& input) {
        std::string s;
        while (std::isdigit(input.peek())) {
            s += static_cast<char>(input.get());
        }
        return token_type::Number{std::stoi(s)};
    }

    static Token String(std::istream& input) {
        char quot = input.get();
        std::string s;

        char c;
        while (quot != (c = input.get())) {
            // escaped_char
            if (c == '\\') {
                char escaped_char = input.get();
                switch (escaped_char) {
                case 'n':
                    s.push_back('\n');
                    break;
                case 't':
                    s.push_back('\t');
                    break;
                case 'r':
                    s.push_back('\r');
                    break;
                case '"':
                    s.push_back('"');
                    break;
                case '\'':
                    s.push_back('\'');
                    break;
                case '\\':
                    s.push_back('\\');
                    break;
                default:
                    throw LexerError("Unrecognized escape sequence \\"s + escaped_char);
                }
            } else {
                s.push_back(static_cast<char>(c));
            }
        }

        return token_type::String{s};
    }

    static Token Indent(Lexer& lexer) {

        lexer.indent_equal_ = false;

        int space_count = 0;
        while (' ' == lexer.input_.peek()) {
            lexer.input_.get();
            ++space_count;
        }

        if('\n' == lexer.input_.peek())
            return lexer.NextToken();

        if(space_count % 2) throw LexerError("Indent(): odd spaces"s);

        int space_pair = space_count >> 1;

        if(space_pair == lexer.indent_count_) {
            lexer.indent_equal_= true;
            return lexer.NextToken();
        }

        if(space_pair && lexer.token_.Is<token_type::Indent>()) {
            ++lexer.indent_count_;
            while(0 < space_count--) lexer.input_.putback(' ');
            return token_type::Indent{};
        }

        if(space_pair && lexer.token_.Is<token_type::Dedent>()) {
            --lexer.indent_count_;
            while(0 < space_count--) lexer.input_.putback(' ');
            return token_type::Dedent{};
        }

        if(space_pair > lexer.indent_count_) {
            ++lexer.indent_count_;
            while(0 < space_count--) lexer.input_.putback(' ');
            return token_type::Indent{};
        }

        --lexer.indent_count_;
        while(0 < space_count--) lexer.input_.putback(' ');

        return token_type::Dedent{};
    }

    static Token Operation(std::istream& input) {
        char c = input.get();
        switch (c) {
        case '=': {
            char c2 = input.get();
            if(c2 == '=') return token_type::Eq{};
            input.putback(c2);
            return token_type::Char{'='};
        }
        case '!': {
            char c2 = input.get();
            if(c2 == '=') return token_type::NotEq{};
            throw LexerError("Operation(): '!?'"s);
        }
        case '>': {
            char c2 = input.get();
            if(c2 == '=') return token_type::GreaterOrEq{};
            input.putback(c2);
            return token_type::Char{'>'};
        }
        case '<': {
            char c2 = input.get();
            if(c2 == '=') return token_type::LessOrEq{};
            input.putback(c2);
            return token_type::Char{'<'};
        }
        }
        return token_type::Eof{};
    }

    static Token Space(Lexer& lexer) {
        if(lexer.token_.Is<token_type::Newline>() ||
           lexer.token_.Is<token_type::Indent>() ||
           lexer.token_.Is<token_type::Dedent>()) {
            return Indent(lexer);
        }
        while (' ' == lexer.input_.peek()) lexer.input_.get();
        return lexer.NextToken();
    }

    static Token Comment(Lexer& lexer) {
        std::string s;
        getline(lexer.input_, s);
        lexer.input_.putback('\n');
        return lexer.NextToken();
    }

    static Token Char(Lexer& lexer) {
        char c = lexer.input_.get();
        switch (c) {
        case '\n': {
            // start position
            if(lexer.token_.Is<token_type::Eof>() ||
               lexer.token_.Is<token_type::Newline>()) return lexer.NextToken();
            return token_type::Newline{};
        }
        case '+':
            return token_type::Char{'+'};
        case '-':
            return token_type::Char{'-'};
        case '*':
            return token_type::Char{'*'};
        case '/':
            return token_type::Char{'/'};
        case '.':
            return token_type::Char{'.'};
        case ',':
            return token_type::Char{','};
        case ':':
            return token_type::Char{':'};
        case '(':
            return token_type::Char{'('};
        case ')':
            return token_type::Char{')'};
        case '=':
            lexer.input_.putback(c);
            return Operation(lexer.input_);
        case '!':
            lexer.input_.putback(c);
            return Operation(lexer.input_);
        case '>':
            lexer.input_.putback(c);
            return Operation(lexer.input_);
        case '<':
            lexer.input_.putback(c);
            return Operation(lexer.input_);
        case ' ':
            lexer.input_.putback(c);
            return Space(lexer);
        case '#':
            return Comment(lexer);
        case '_':
            lexer.input_.putback(c);
            return Helper::Id(lexer);
        }
        return token_type::Eof{};
    }

};

Lexer::Lexer(std::istream& input)
    : input_(input) {
    NextToken();
}

const Token& Lexer::CurrentToken() const {
    return token_;
}

Token Lexer::NextToken() {
    char c = input_.peek();

    if (c == -1) {
        if(indent_count_) {
            --indent_count_;
            token_ = token_type::Dedent{};
        } else
        if(!token_.Is<token_type::Eof>() &&
           !token_.Is<token_type::Dedent>() &&
           !token_.Is<token_type::Newline>()) {

            token_ = token_type::Newline{};
        } else {
            token_ = token_type::Eof{};
        }
        return token_;
    }

    if(std::isalpha(c)) token_ = Helper::Id(*this);
    else if(std::isdigit(c)) token_ = Helper::Number(input_);
    else if(c == '"' || c == '\'') token_ = Helper::String(input_);
    else token_ = Helper::Char(*this);

    return token_;
}

}  // namespace parse
