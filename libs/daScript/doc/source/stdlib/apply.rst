
.. _stdlib_apply:

========================
Apply reflection pattern
========================

.. include:: detail/apply.rst

Apply module implements `apply` pattern, i.e. static reflection dispatch for structures and other data types.

All functions and symbols are in "apply" module, use require to get access to it. ::

    require daslib/apply


+++++++
Classes
+++++++

.. _struct-apply-ApplyMacro:

.. das:attribute:: ApplyMacro : AstCallMacro

|class-apply-ApplyMacro|

.. das:function:: ApplyMacro.visit(self: AstCallMacro; prog: ProgramPtr; mod: rtti::Module? const; expr: smart_ptr<ast::ExprCallMacro> const)

visit returns  :ref:`ExpressionPtr <alias-ExpressionPtr>` 

+--------+-----------------------------------------------------------------------+
+argument+argument type                                                          +
+========+=======================================================================+
+self    + :ref:`ast::AstCallMacro <struct-ast-AstCallMacro>`                    +
+--------+-----------------------------------------------------------------------+
+prog    + :ref:`ProgramPtr <alias-ProgramPtr>`                                  +
+--------+-----------------------------------------------------------------------+
+mod     + :ref:`rtti::Module <handle-rtti-Module>` ? const                      +
+--------+-----------------------------------------------------------------------+
+expr    +smart_ptr< :ref:`ast::ExprCallMacro <handle-ast-ExprCallMacro>` > const+
+--------+-----------------------------------------------------------------------+


|method-apply-ApplyMacro.visit|


