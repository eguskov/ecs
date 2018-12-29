{
	function trim(s) { return s.replace(/^\s|\s$/g, ''); }
}

Start = block:BlockStatement { return block; }

Statements = head:Statement? _ tail:(Statement _)* { return [head].concat(tail.map(function(t) { return t[0]; })); }
Statement =
  VarStatement /
  IfStatement /
  ForStatement /
  SwitchStatement /
  FuncCallStatement /
  ReturnStatement /
  AssignStatement /
  AccessStatement /
  EmptyStatement /
  IncompleteStatement /
  Literal /
  BlockStatement

BlockStatement = "{" _ body:Statements _ "}" { return { statement: "block", body: body }; }
IfStatement = "if" _ "(" _ expr:AnyExpr _ ")" _ body:(BlockStatement / Statement) { return { statement: "if", condition: expr, body: body }; }
ForStatement = "for" _ "(" _ vars:VarDecl? _ ";" AnyExpr ";" AnyExpr ")" _ body:(BlockStatement / Statement) { return { statement: "for", vars: vars, body: body }; }
SwitchStatement = "switch" _ "(" AnyExpr ")" _ body:BlockStatement
FuncCallStatement = func:FuncCall ";"  { return func; }
VarStatement = decl:VarDecl ";" { return decl; }
ReturnStatement = "return" _ VarValue? _ ";" { return text(); }
AssignStatement = (GetHandle / Id) _ "=" _ VarValue _ ";" { return text(); }
AccessStatement = expr:AccessExpr { return expr; }
EmptyStatement = ";" { return {}; }
InitStatement = "{" InitBody "}" { return text(); }
InitBody = InitText ("{" InitBody "}" InitText)* { return text(); }
InitText = [^{};]* { return text(); }

IncompleteStatement = Id ("." / "(" AnyExprLine)* { return { statement: "incomplete", location: location(), body: text() }; }

AnyExpr = head:AnyTextComma tail:("(" AnyExpr ")")* { return text(); }
AnyText = String _ ("+" _ (String / AccessExpr / Id))* { return text(); } / [^();]* { return text(); }
AnyTextComma = AnyText ("," AnyTextComma)* { return text(); }

AnyExprLine = head:AnyTextLineComma tail:("(" AnyExprLine ")")* { return text(); }
AnyTextLine = String _ ("+" _ (String / AccessExpr / Id))* { return text(); } / [^();\n]* { return text(); }
AnyTextLineComma = AnyTextLine ("," AnyTextLineComma)* { return text(); }

VarDecl = type:Type _ name:Id _ ("=" _ VarValue)? _ ("," _ Id _ ("=" _ VarValue)?)* { return { statement: "var", type: type, name: name, decl: text() }; }
VarValue = Literal / GetHandle / AccessExpr / FuncCall / InitStatement / Id

AccessExpr = (FuncCall / Id) ("." (FuncCall / Id))+

FuncCall = Id _ ("<" Id ">")? _ "(" AnyExpr ")" { return { statement: "functionCall", body: text() }; }

Id = !Keyword [a-zA-Z0-9_$]+ { return text(); }
Keyword = "if" / "for"/ "do" / "while" / "return" / "switch" / "case"
GetHandle = "@" _ Id
Type = prefix:TypePrefix* _ name:TypeName _ suffix:TypeSuffix* { return { name: name, decl: trim(prefix+" "+name+" "+suffix) }; }
TypePrefix = "const"
TypeSuffix = "@" / "&" / "&in" / "&out" / "&inout"
TypeName = "int" / "float" / "void" / "bool" / Id
Literal = Number / String
String = '"' AnyChar '"'
Number = Hex / Float / Int
Int = [0-9]+
Float = Int "." Int "f" / Int "." Int / Int ".f" / Int "."
Hex = "0x" [0-9a-fA-F]+
_ "whitespace" = [ \t\n\r]*
AnyChar "any character" = ('\\"' / [^"])*