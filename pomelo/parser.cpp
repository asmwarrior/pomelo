//
//  parser.cpp
//  pomelo
//
//  Created by Edmund Kapusniak on 11/12/2017.
//  Copyright © 2017 Edmund Kapusniak. All rights reserved.
//

#include "parser.h"
#include <stdio.h>


parser::parser( errors_ptr errors, syntax_ptr syntax )
    :   _errors( errors )
    ,   _syntax( syntax )
{
}

parser::~parser()
{
}


void parser::parse( const char* path )
{
    // Open file.
    srcloc sloc = 0;
    _errors->new_file( sloc, path );
    FILE* f = fopen( path, "r" );
    if ( ! f )
    {
        _errors->error( sloc, "unable to open file '%s'", path );
        return;
    }
    
    // Parse source file.
    next();
    while ( 1 )
    {
        if ( _lexed == '%' )
        {
            parse_directive();
        }
        else if ( _lexed == TOKEN )
        {
            parse_nonterminal();
        }
        else if ( _lexed == EOF )
        {
            break;
        }
        else
        {
            expected( nullptr );
            next();
        }
    }

    // Check that all nonterminals have been defined.
    for ( const auto& entry : _syntax->nonterminals )
    {
        nonterminal* nterm = entry.second.get();
        
        if ( nterm->rules.empty() )
        {
            _errors->error
            (
                nterm->name.sloc,
                "nonterminal '%s' has not been defined",
                _syntax->source->text( nterm->name )
            );
        }

    }
}


void parser::parse_directive()
{
    srcloc dloc = _tloc;
    next();
    
    if ( _lexed != TOKEN )
    {
        expected( "directive" );
        return;
    }
    
    const char* text = _syntax->source->text( _token );
    directive* directive = nullptr;
    if ( strcmp( text, "include" ) == 0 )
    {
        directive = &_syntax->include;
    }
    else if ( strcmp( text, "class_name" ) == 0 )
    {
        directive = &_syntax->class_name;
    }
    else if ( strcmp( text, "token_prefix" ) == 0 )
    {
        directive = &_syntax->token_prefix;
    }
    else if ( strcmp( text, "token_type" ) == 0 )
    {
        directive = &_syntax->token_type;
    }
    else if ( strcmp( text, "left" ) == 0 )
    {
        parse_precedence( ASSOC_LEFT );
        return;
    }
    else if ( strcmp( text, "right" ) == 0 )
    {
        parse_precedence( ASSOC_RIGHT );
        return;
    }
    else if ( strcmp( text, "nonassoc" ) == 0 )
    {
        parse_precedence( ASSOC_NONASSOC );
        return;
    }
    else
    {
        expected( "directive" );
    }
    
    if ( ! directive->text.empty() )
    {
        _errors->error( dloc, "repeated directive '%%%s'", text );
    }
    
    directive->keyword = _token;
    
    next();
    if ( _lexed != BLOCK )
    {
        expected( "code block" );
        return;
    }

    directive->text = _block;
    next();
}


void parser::parse_precedence( associativity associativity )
{
    int precedence = _precedence;
    precedence += 1;
    
    next();
    while ( true )
    {
        if ( _lexed == TOKEN )
        {
            terminal* terminal = declare_terminal( _token );
            terminal->associativity = associativity;
            terminal->precedence = precedence;
            next();
        }
        else if ( _lexed == '.' )
        {
            next();
            break;
        }
        else
        {
            expected( "terminal symbol" );
            break;
        }
    }
}

void parser::parse_nonterminal()
{
    nonterminal* nonterminal = declare_nonterminal( _token );

    if ( nonterminal->defined )
    {
        const char* name = _syntax->source->text( nonterminal->name );
        _errors->error( nonterminal->name.sloc, "nonterminal '%s' has already been defined", name );
    }
    nonterminal->defined = true;

    next();
    if ( _lexed == BLOCK )
    {
        nonterminal->type = _block;
        next();
    }
    
    if ( _lexed != '[' )
    {
        expected( "'['" );
        return;
    }
    
    next();
    while ( true )
    {
        if ( _lexed == TOKEN || _lexed == '.' )
        {
            parse_rule( nonterminal );
        }
        else if ( _lexed == ']' )
        {
            next();
            break;
        }
        else
        {
            expected( "rule or ']'" );
            next();
        }
    }
    
    if ( nonterminal->rules.empty() )
    {
        const char* name = _syntax->source->text( nonterminal->name );
        _errors->error( nonterminal->name.sloc, "nonterminal '%s' has no rules", name );
    }
}

void parser::parse_rule( nonterminal* nonterminal )
{
    rule_ptr rule = std::make_unique< ::rule >();
    rule->nonterminal = nonterminal;
    rule->precedence = nullptr;
    rule->precetoken = NULL_TOKEN;

    while ( true )
    {
        if ( _lexed == TOKEN )
        {
            rule_symbol rs = { declare_symbol( _token ), _token, NULL_TOKEN };
            
            if ( rs.symbol->is_terminal && ! rule->precedence )
            {
                rule->precedence = (terminal*)rs.symbol;
                rule->precetoken = _token;
            }
            
            next();
            if ( _lexed == '(' )
            {
                next();
                if ( _lexed == TOKEN )
                {
                    rs.sparam = _token;

                    next();
                    if ( _lexed == ')' )
                    {
                        next();
                    }
                    else
                    {
                        expected( "')'" );
                    }
                }
                else
                {
                    expected( "parameter name" );
                }
            }

            rule->symbols.push_back( rs );
        }
        else if ( _lexed == '.' )
        {
            next();
            break;
        }
        else
        {
            expected( "symbol or '.'" );
        }
    }

    if ( _lexed == '[' )
    {
        next();
        if ( _lexed == TOKEN )
        {
            rule->precedence = declare_terminal( _token );
            rule->precetoken = _token;

            next();
            if ( _lexed == ']' )
            {
                next();
            }
            else
            {
                expected( "']'" );
            }
        }
        else
        {
            expected( "terminal precdence symbol" );
        }
    }

    if ( _lexed == BLOCK )
    {
        rule->action = _block;
        next();
    }

    nonterminal->rules.push_back( std::move( rule ) );
}



bool parser::terminal_token( token token )
{
    const char* text = _syntax->source->text( token );
    return isupper( text[ 0 ] );
}

symbol* parser::declare_symbol( token token )
{
    if ( terminal_token( token ) )
    {
        return declare_terminal( token );
    }
    else
    {
        return declare_nonterminal( token );
    }
}

terminal* parser::declare_terminal( token token )
{
    auto i = _syntax->terminals.find( token );
    if ( i != _syntax->terminals.end() )
    {
        return i->second.get();
    }
    
    terminal_ptr usym = std::make_unique< terminal >();
    usym->name          = token;
    usym->value         = -1;
    usym->is_terminal   = true;
    usym->precedence    = -1;
    usym->associativity = ASSOC_NONE;
    
    terminal* psym = usym.get();
    _syntax->terminals.emplace( token, std::move( usym ) );
    return psym;
}

nonterminal* parser::declare_nonterminal( token token )
{
    auto i = _syntax->nonterminals.find( token );
    if ( i != _syntax->nonterminals.end() )
    {
        return i->second.get();
    }
    
    nonterminal_ptr usym = std::make_unique< nonterminal >();
    usym->name          = token;
    usym->value         = -1;
    usym->is_terminal   = false;
    usym->defined       = false;
    
    nonterminal* psym = usym.get();
    _syntax->nonterminals.emplace( token, std::move( usym ) );
    return psym;
}



void parser::next()
{
    while ( true )
    {
        int c = fgetc( _file );
        _tloc = _sloc;
        _sloc += 1;

        if ( c == ' ' || c == '\t' )
        {
            continue;
        }
        else if ( c == '\r' )
        {
            c = fgetc( _file );
            _sloc += 1;
            
            if ( c != '\n' )
            {
                ungetc( c, _file );
                _sloc -= 1;
            }
        
            _errors->new_line( _sloc );
            continue;
        }
        else if ( c == '\n' )
        {
            _errors->new_line( _sloc );
            continue;
        }
        else if ( c == '/' )
        {
            c = fgetc( _file );
            _sloc += 1;

            if ( c == '*' )
            {
                bool was_asterisk = false;
                while ( ! was_asterisk || c != '/' )
                {
                    c = fgetc( _file );
                    _sloc += 1;
                    
                    was_asterisk = ( c == '*' );

                    if ( c == '\r' )
                    {
                        c = fgetc( _file );
                        _sloc += 1;
                        
                        if ( c != '\n' )
                        {
                            ungetc( c, _file );
                            _sloc -= 1;
                        }
                    
                        _errors->new_line( _sloc );
                    }
                    else if ( c == '\n' )
                    {
                        _errors->new_line( _sloc );
                    }
                }
                continue;
            }
            else if ( c == '/' )
            {
                while ( c != '\r' && c != '\n' && c != EOF )
                {
                    c = fgetc( _file );
                    _sloc += 1;
                }
                
                ungetc( c, _file );
                _sloc -= 1;
                continue;
            }
            else
            {
                ungetc( c, _file );
                _sloc -= 1;
                
                _lexed = '/';
                return;
            }
        }
        else if ( c == '{' )
        {
            c = fgetc( _file );
            _sloc += 1;
            
            _block.clear();
            while ( c != '}' )
            {
                _block.push_back( c );
                c = fgetc( _file );
                _sloc += 1;
            }
            
            _lexed = BLOCK;
            return;
        }
        else if ( c == '_'
                || ( c >= 'a' && c <= 'z' )
                || ( c >= 'A' && c <= 'Z' ) )
        {
            _block.clear();
            while ( c == '_'
                    || ( c >= 'a' && c <= 'z' )
                    || ( c >= 'A' && c <= 'Z' )
                    || ( c >= '0' && c <= '9' ) )
            {
                _block.push_back( c );
                c = fgetc( _file );
                _sloc += 1;
            }
            
            ungetc( c, _file );
            _sloc -= 1;
            
            _lexed = TOKEN;
            _token = _syntax->source->new_token( _tloc, _block );
            return;
        }
        else
        {
            _lexed = c;
            return;
        }
    }
}

void parser::expected( const char* expected )
{
    std::string message;
    if ( expected )
    {
        message = "expected ";
        message += expected;
        message += ", not ";
    }
    else
    {
        message = "unexpected ";
    }
    
    if ( _lexed == TOKEN )
    {
        message += "'";
        message += _syntax->source->text( _token );
        message += "'";
    }
    else if ( _lexed == BLOCK )
    {
        message += "code block";
    }
    else
    {
        message += "'";
        message.push_back( _lexed );
        message += "'";
    }
    
    _errors->error( _tloc, "%s", message.c_str() );
}











