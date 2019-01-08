{
	function trim(s) { return s.replace(/^\s|\s$/g, ''); }
  function join(s) { return s.join(''); }
}

Start = _ prog:Program _ { return prog; }

Program = head:ProgramElement tail:(_ ProgramElement)* { return [head].concat(tail.map(function(t) { return t[1]; }));; }

ProgramElement =
  IncludeStatement /
  ClassDecl /
  Commnet /
  Func /
  BlockStatement

ClassDecl = Metadata? _ ClassToken _ Id _ BlockStatement { return { statement: "class" }; }

Commnet = CommnetLine { return {}; } / CommnetMultiLine { return {}; }
CommnetLine = "//" (!EOL Char)*
CommnetMultiLine = "/*" (!"*/" Char)* "*/"

Func = Metadata _ FuncReturn _ name:Id _ args:FuncArgs _ block:BlockStatement { return { statement: "function", location: location(), name: name, args: args, body: block }; }
FuncArgs = "(" _ ")" { return []; } / "(" _ head:FuncArg _ tail:("," _ FuncArg _)* _ ")" { return [head].concat(tail.map(function(t) { return t[2]; })); }
FuncArg = type:Type _ name:Id { return { statement: "argument", type: type, name: name }; }
FuncReturn = Type

Metadata = "[" meta:(MetadataText (_ Metadata _ MetadataText)*) "]" { return { statement: "metadata", body: meta }; }
MetadataText = (!("[" / "]") Char)+ { return text(); }

Statements = head:Statement? _ tail:(Statement _)* { return [head].concat(tail.map(function(t) { return t[0]; })); }
Statement =
  VarStatement /
  IfStatement /
  ForStatement /
  SwitchStatement /
  AssignStatement /
  IncompleteStatement /
  ValueStatement /
  FuncCallStatement /
  ReturnStatement /
  AccessStatement /
  EmptyStatement /
  Literal /
  BlockStatement

BlockStatement = "{" _ body:Statements _ "}" { return { statement: "block", location: location(), body: body }; }
IfStatement = IfToken _ "(" _ expr:ValueExpr _ ")" _ body:(BlockStatement / Statement) { return { statement: "if", condition: expr, body: body }; }
ForStatement = ForToken _ "(" _ vars:VarDecl? _ ";" AnyExpr ";" AnyExpr ")" _ body:(BlockStatement / Statement) { return { statement: "for", vars: vars, body: body }; }
SwitchStatement = SwitchToken _ "(" AnyExpr ")" _ body:BlockStatement
FuncCallStatement = func:FuncCall ";"  { return func; }
VarStatement = decl:VarDecl ";" { return decl; }
ReturnStatement = "return" _ VarValue? _ ";" { return text(); }
AssignStatement = (GetHandle / AccessExpr / Id) _ AssignOperator _ VarValue _ ";" { return text(); }
AccessStatement = expr:AccessExpr { return expr; }
EmptyStatement = ";" { return {}; }
InitStatement = "{" InitBody "}" { return text(); }
InitBody = InitText ("{" InitBody "}" InitText)* { return text(); }
InitText = [^{};]* { return text(); }
ValueStatement = expr:ValueExpr { return expr; }
IncludeStatement = "#include" _ "\"" path:[a-zA-Z0-9./]+ "\"" { return { statement: "include", body: join(path) } } 

IncompleteStatement = IncompleteAccess { return { statement: "incomplete", body: text() }; }
IncompleteAccess = Id "."

AnyExpr = head:AnyTextComma tail:("(" AnyExpr ")")* { return text(); }
AnyText = String _ ("+" _ (String / AccessExpr / Id))* { return text(); } / [^();]* { return text(); }
AnyTextComma = AnyText ("," AnyTextComma)* { return text(); }

AnyExprLine = head:AnyTextLineComma tail:("(" AnyExprLine ")")* { return text(); }
AnyTextLine = String _ ("+" _ (String / AccessExpr / Id))* { return text(); } / [^();\n]* { return text(); }
AnyTextLineComma = AnyTextLine ("," AnyTextLineComma)* { return text(); }

ValueExpr = Op { return { statement: "expr", body: text() }; }
Op = BinaryOp (_ BinaryOperator _ ValueExpr)* / Operand
UnaryOp = UnaryOperator Operand
BinaryOp = Operand _ BinaryOperator _ Operand
Operand = FuncCall / AccessExpr / UnaryOp / "(" _ ValueExpr _ ")" / IncompleteStatement / BlockStatement / Id / Literal
UnaryOperator = "!" / "+" / "-"
BinaryOperator = "+" / "-" / "*" / "/" / "==" / ">=" / "<=" / "<" / ">" / "&&" / "||" / "&" / "|" / "," / "=" / IsToken
AssignOperator = "+=" / "-=" / "*=" / "/=" / "="

VarDecl = type:Type _ name:Id _ ("=" _ VarValue)? _ ("," _ Id _ ("=" _ VarValue)?)* { return { statement: "var", type: type, name: name, decl: text() }; }
VarValue = Literal / GetHandle / AccessExpr / ValueExpr / FuncCall / InitStatement / Id

AccessExpr = (FuncCall / Id) ("." (FuncCall / Id))+

FuncCall = Id _ ("<" Id ">")? _ "(" _ ValueExpr* (_ "," _ ValueExpr)* _ ")" { return { statement: "functionCall", body: text() }; }

Id = !Keyword name:[a-zA-Z0-9_$]+ { return join(name); }
Letter = [a-zA-Z]
IdStart = Letter / "$" / "_"
DoToken = "do" !IdStart
IfToken = "if" !IdStart
ForToken = "for" !IdStart
WhileToken = "while" !IdStart
ReturnToken = "return" !IdStart
SwitchToken = "switch" !IdStart
CaseToken = "case" !IdStart
ClassToken = "class" !IdStart
IsToken = "is" !IdStart
Keyword = IfToken / ForToken / DoToken / WhileToken / ReturnToken / SwitchToken / CaseToken
GetHandle = "@" _ Id
Type = prefix:TypePrefix* _ name:TypeName _ suffix:TypeSuffix* { return { name: name, decl: trim(prefix+" "+name+" "+suffix) }; }
TypePrefix = "const"
TypeSuffix = "@" / "&" / "&in" / "&out" / "&inout"
TypeName = Id
Literal = Number / String
String = '"' AnyChar '"'
Number = Hex / Float / Int
Int = [0-9]+
Float = Int "." Int "f" / Int "." Int / Int ".f" / Int "."
Hex = "0x" [0-9a-fA-F]+
_ "whitespace" = [ \t\n\r]*
AnyChar "any character" = ('\\"' / [^"])*
Char = .
EOL = [\r\n]