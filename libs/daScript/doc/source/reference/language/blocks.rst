.. _blocks:

=====
Block
=====

Block is a nameless function which captures local context by reference.
Blocks offer significant performance advantage over lambda  (see :ref:`Lambda <lambdas>`).

Block type can be declared with a function-like syntax::

    block_type ::= block { optional_block_type }
    optional_block_type ::= < { optional_block_arguments } { : return_type } >
    optional_block_arguments := ( block_argument_list )
    block_argument_list := argument_name : type | block_argument_list ; argument_name : type

    block < (arg1:int;arg2:float&):bool >

Blocks capture current stack so block can be passed, but never returned.
Block variables can only be passed as arguments.
Global or local block variables are prohibited, returning block type is prohibited::

    def goo ( b : block )
        ...

    def foo ( b : block < (arg1:int; arg2:float&) : bool >
        ...

Blocks can be called via ``invoke``::

    def radd(var ext:int&;b:block<(var arg:int&):int>):int
        return invoke(b,ext)

Typeless blocks are typically declared via pipe syntax::

    goo() <|                                // block without arguments
        print("inside goo")

.. _blocks_declarations:

Similarly typed blocks are typically declared via pipe syntax::

    var v1 = 1                              // block with arguments
    res = radd(v1) <| $(var a:int&):int
        return a++

Blocks can also be declared via inline syntax::

    res = radd(v1, $(var a:int&) : int { return a++; }) // equivalent to example above

There is simplified syntax for the blocks containing return expression only::

    res = radd(v1, $(var a:int&) : int => a++ )         // equivalent to example above

If block is sufficiently specified in the generic or function,
block types will be automatically inferred::

    res = radd(v1, $(a) => a++ )                        // equivalent to example above

Nested blocks are allowed::

    def passthroughFoo(a:Foo; blk:block<(b:Foo):void>)
        invoke(blk,a)

    passthrough(1) <| $ ( a )
        assert(a==1)
        passthrough(2) <| $ ( b )
            assert(a==1 && b==2)
            passthrough(3) <| $ ( c )
                assert(a==1 && b==2 && c==3)

Loop control expressions are not allowed to cross block boundaries::

    while true
        take_any() <|
            break               // 30801, captured block can't break outside the block

Blocks can have annotations::

    def queryOne(dt:float=1.0f)
        testProfile::queryEs() <| $ [es] (var pos:float3&;vel:float3 const)  // [es] is annotation
            pos += vel * dt

Block annotations can be implemented via appropriate macro (see :ref:`Macro <macros>`).
