"""
 Author:
College: Berea College
 Course: CSC420 Programming Languages (Summer 2023)
Purpose: A simple language interpreter

  Notes:

History:
            2023-04-05, DMW, added inline comment
            2023-04-05, DMW, created
"""

import ply.lex as lex
import ply.yacc as yacc
from ASTNODE import ASTNODE
from MiniAST import MiniAST

# reserved words / keywords
reserved = {
    "pass": "PASS"
}

# tokens
tokens = [
    'DQ_STRING', 'NAME', 'NUMBER', 'SEMICOLON', 'SQ_STRING'
]

# all tokens, reserved words and keywords, combined
tokens += list(reserved.values())

# literal characters recognized by the lexer and parser
literals = ['=', '+', '-', '*', '/', '(', ')', ';']

# ---------------------------------------------------------- TOKEN DEFINITIONS

# a dictionary to save comments for tying to emitted code output
comments = {}


# TOKEN FOR NAME (or IDENTIFIER)
# noinspection PyPep8Naming
# noinspection PySingleQuotedDocstring
def t_NAME(t):
    r'[a-zA-Z_][a-zA-Z0-9_]*'
    t.type = reserved.get(t.value, 'NAME')    # Check for reserved words
    return t


# Helper function for NUMBER token
def str_to_num(s):
    ans = 0

    try:
        ans = int(s)
    except ValueError:
        ans = float(s)

    return ans


# noinspection PyPep8Naming
# noinspection PySingleQuotedDocstring
def t_NUMBER(t):
    r'[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?'
    # 2021-05-29, DMW, regular expression is from:
    # https://www.regular-expressions.info/floatingpoint.html
    # r'\d+' # original regular expression

    t.value = str_to_num(t.value)

    return t


# noinspection PyPep8Naming
# noinspection PySingleQuotedDocstring
def t_SEMICOLON(t):
    """;"""
    # r'(\;|\s)+'
    return t


# 2021-11-11, DMW, added the ability to handle single quote strings
# based on: https://stackoverflow.com/questions/5695240/php-regex-to-ignore-escaped-quotes-within-quotes
# noinspection PyPep8Naming
# noinspection PySingleQuotedDocstring
def t_SQ_STRING(t):
    r"'[^'\\]*(?:\\.[^'\\]*)*'"
    return t


# 2021-11-11, DMW, added the ability to handle double quote strings
# based on: https://stackoverflow.com/questions/5695240/php-regex-to-ignore-escaped-quotes-within-quotes
# noinspection PyPep8Naming
def t_DQ_STRING(t):
    # noinspection PySingleQuotedDocstring
    r'"[^"\\]*(?:\\.[^"\\]*)*"'
    return t


# noinspection PyPep8Naming
# noinspection PySingleQuotedDocstring
def t_INLINE_COMMENT(t):
    r'//.*'
    comments[t.lexer.lineno] = t.value
    # return t


# Whitespace to ignore
t_ignore = " \f\t\v"


# noinspection PyPep8Naming
# noinspection PySingleQuotedDocstring
def t_EOL(t):
    r'\n+'
    t.lexer.lineno += t.value.count("\n")
    # return t


def t_error(t):
    print("Illegal character '%s'" % t.value[0])
    t.lexer.skip(1)


# Build the lexer
lexer = lex.lex()

# ---------------------------------------------------------- DEFINE OPERATOR PRECEDENCE FOR MATH EXPRESSIONS

# noinspection SpellCheckingInspection
precedence = (
    # ('nonassoc', '?', ':'), # 'EQUALS', 'LESSTHAN', 'GREATERTHAN', 'LTEQ', 'GTEQ',
    # ('left', 'EQ', 'LT', 'GT', 'LTEQ', 'GTEQ'),
    ('left', '+', '-'),
    ('left', '*', '/'),  # '%', 'INTDIV'),
    ('left', '^'),  # add other similar precedence like factorial
    ('right', 'UMINUS', 'UPOSITIVE')
)

# ---------------------------------------------------------- PARSER DEFINITIONS (GRAMMAR, PRODUCTIONS, RULES)
# The following list of to do items aren't currently supported by the simple grammar provided
# However, they can be added if needed
# TODO: no support for block comments
# TODO: Other than recognizing strings as expressions, this grammar doesn't do anything with strings
# TODO: variables are implicitly (dynamically) typed
# TODO: no static typing support
# TODO: no support for constants
# TODO: no IF ELSE support

start = "program"
root = None


def p_program(p):
    """program : statement_list"""
    global root
    root = ASTNODE("program", children=[p[1]])


def p_statement_list2(p):
    """statement_list : statement_list statement"""
    p[0] = ASTNODE("statement_list", children=[p[1], p[2]])


def p_statement_list(p):
    """statement_list : statement"""
    p[0] = p[1]


# def p_statement(p):
#     """statement : simple_statement SEMICOLON INLINE_COMMENT"""
#     p[0] = p[1]


def p_statement_no_comment(p):
    """statement : simple_statement SEMICOLON"""
    p[0] = p[1]


def p_statement_no_code_no_comment(p):
    """statement : SEMICOLON"""
    p[0] = ASTNODE("pass")


# do nothing
def p_simple_statement_pass(p):
    """simple_statement : PASS"""
    p[0] = ASTNODE("pass")


def p_simple_statement_assign(p):
    """simple_statement : NAME "=" expression"""
    p[0] = ASTNODE("assign", value=p[1], children=[p[3]])


# noinspection SpellCheckingInspection
def p_expression_binop(p):
    """expression : expression '+' expression
                  | expression '-' expression
                  | expression '*' expression
                  | expression '/' expression
                  | expression '^' expression"""
    p[0] = ASTNODE("binop", value=p[2], children=[p[1], p[3]])


# noinspection SpellCheckingInspection
def p_expression_uminus(p):
    """expression : '-' expression %prec UMINUS"""
    p[0] = ASTNODE("negate", children=[p[2]])


# noinspection SpellCheckingInspection
def p_expression_upositive(p):
    """expression : '+' expression %prec UPOSITIVE"""
    p[0] = p[2]


def p_expression_group(p):
    """expression : '(' expression ')'"""
    p[0] = p[2]


def p_expression_number(p):
    """expression : NUMBER"""
    p[0] = ASTNODE("number", value=p[1])


def p_expression_dq_string(p):
    """expression : DQ_STRING"""
    p[0] = ASTNODE("string", children=[p[1]])


def p_expression_sq_string(p):
    """expression : SQ_STRING"""
    p[0] = ASTNODE("string", children=[p[1]])


def p_expression_name(p):
    """expression : NAME"""
    p[0] = ASTNODE("name", value=p[1])


# p_error() is required
def p_error(p):
    try:
        print("{1}: Syntax error on '{0}'".format(p.value, lexer.lineno))
    except AttributeError:
        print("{0}: Syntax error on line".format(lexer.lineno))


parser = yacc.yacc()

# ---------------------------------------------------------- PROCESS INPUT

if __name__ == "__main__":

    # a test program; the program can be read from a file, too;
    program = """
        // top of the program
        a = 3.174; b = 5; pass;
        // comment by itself
        c = 7 
            + a; // multiline assignment with expression :-)
        d = 2 * (a + b) * c; e = -d; ; ;;
    """

    root = None
    yacc.parse(program)  # computes root

    mini_ast = MiniAST(root)  # ---------- set up AST
    print(mini_ast)  # ------------------- show the AST
    mini_ast.reserved_names = tokens  # -- tell the AST which words are reserved/keywords/tokens
    mini_ast.process_ast()  # ------------ Execute the program (or emit it if writing a compiler)

    # diagnostic information
    print("AST stack: {}".format(mini_ast.stack.stack))
    print("Names:\n", mini_ast.names)
    print("comments:\n", comments)
