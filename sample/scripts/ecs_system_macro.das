module ecs_system_macro

require ast
require daslib/ast_boost

[unsafe,private]
def prepend_cond(var fn:FunctionPtr; cond:string)
  var iblk <- fn.body
  var blk <- new [[ExprBlock() at=fn.at]]
  var ifm <- new [[ExprCall() at=fn.at, name:=cond]]
  var ife <- new [[ExprIfThenElse() at=fn.at, cond<-ExpressionPtr(ifm), if_true:=ExpressionPtr(iblk)]]
  push(blk.list,ExpressionPtr(ife))
  fn.body <- ExpressionPtr(blk)

[function_macro(name="es")]
class ES : AstFunctionAnnotation
  [unsafe] def override apply ( var func:FunctionPtr; var group:ModuleGroup; args:AnnotationArgumentList; var errors : das_string ) : bool
    print(">>>> APPLY\n")
    var cond = ""
    let argName = find_arg("cond", args)
    if !(argName is tString)
      panic("cond must be string")
    else
      prepend_cond(func,argName as tString)
    return true