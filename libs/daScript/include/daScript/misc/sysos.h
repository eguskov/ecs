#pragma once

namespace das {
    string getExecutableFileName ( void );
    string getDasRoot ( void );
    void setDasRoot ( const string & dr );

    string get_prefix ( const string & req );   // blah.... \ foo.bar - returns blah....
    string get_suffix ( const string & req );   // blah.... \ foo.bar - returns foo.bar
}
