//
//  main.cpp
//  pomelo
//
//  Created by Edmund Kapusniak on 09/12/2017.
//  Copyright © 2017 Edmund Kapusniak. All rights reserved.
//


#include <stdlib.h>
#include "errors.h"
#include "syntax.h"
#include "parser.h"
#include "lalr1.h"


int main( int argc, const char* argv[] )
{
    const char* path = argv[ 1 ];

    source_ptr source = std::make_shared< ::source >( path );
    errors_ptr errors = std::make_shared< ::errors >( source.get(), stderr );
    syntax_ptr syntax = std::make_shared< ::syntax >( source );
    parser_ptr parser = std::make_shared< ::parser >( errors, syntax );
    parser->parse( path );

    if ( errors->has_error() )
    {
        return EXIT_FAILURE;
    }
    
    lalr1_ptr lalr1 = std::make_shared< ::lalr1 >( errors, syntax );
    automata_ptr automata = lalr1->construct();
    automata->print();

    return EXIT_SUCCESS;
}
