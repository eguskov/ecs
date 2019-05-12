#include "queryparser.h"

#include <cassert>
#include <iostream>
#include <map>
#include <sstream>

#include <pegtl.hpp>
#include <pegtl/analyze.hpp>
#include <pegtl/contrib/abnf.hpp>

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/functional.h>

namespace pegtl = tao::TAO_PEGTL_NAMESPACE;

namespace queryparser
{

enum class order : int
{
};

struct operand
{
  enum class Type
  {
    INTEGER,
    REAL,
    BOOLEAN,
    COMPONENT,
    RESULT
  };

  Type type;
  eastl::string value;
};

struct op
{
  order p;
  eastl::function<eastl::string (const operand&, const operand&)> f;
};

struct component_desc
{
  operand::Type type;
  eastl::string name;
  eastl::string typeStr;
};

using components_t = eastl::vector<component_desc>;

struct stack
{
  void push(const op &b, components_t &components)
  {
    while ((!m_o.empty()) && (m_o.back().p <= b.p))
    {
      reduce(components);
    }
    m_o.push_back(b);
  }

  void push(const eastl::string &s, operand::Type t)
  {
    m_l.push_back({t, s});
  }

  operand finish(components_t &components)
  {
    while (!m_o.empty())
    {
      reduce(components);
    }
    assert(m_l.size() == 1);
    const auto r = m_l.back();
    m_l.clear();
    return r;
  }

private:
  eastl::vector<op> m_o;
  eastl::vector<operand> m_l;

  void reduce(components_t &components)
  {
    assert(!m_o.empty());
    assert(m_l.size() > 1);

    const auto r = m_l.back();
    m_l.pop_back();
    const auto l = m_l.back();
    m_l.pop_back();
    const auto o = m_o.back();
    m_o.pop_back();

    if (r.type == operand::Type::COMPONENT && l.type != operand::Type::COMPONENT)
    {
      for (auto &c : components)
        if (c.name == r.value)
        {
          assert(c.type == operand::Type::COMPONENT);
          c.type = l.type;
          break;
        }
    }
    else if (l.type == operand::Type::COMPONENT && r.type != operand::Type::COMPONENT)
    {
      for (auto &c : components)
        if (c.name == l.value)
        {
          assert(c.type == operand::Type::COMPONENT);
          c.type = r.type;
          break;
        }
    }

    m_l.push_back({operand::Type::RESULT, "(" + o.f(l, r) + ")"});
  }
};

struct stacks
{
  stacks()
  {
    open();
  }

  void open()
  {
    m_v.emplace_back();
  }

  void push(const op &t, components_t &components)
  {
    assert(!m_v.empty());
    m_v.back().push(t, components);
  }

  void push(const eastl::string &t, operand::Type type)
  {
    assert(!m_v.empty());
    m_v.back().push(t, type);
  }

  void close(components_t &components)
  {
    assert(m_v.size() > 1);
    const auto r = m_v.back().finish(components);
    m_v.pop_back();
    m_v.back().push(r.value, r.type);
  }

  operand finish(components_t &components)
  {
    assert(m_v.size() == 1);
    return m_v.back().finish(components);
  }

private:
  eastl::vector<stack> m_v;
};

struct operators
{
  operators()
  {
    insert(".", order(1), [](const operand& l, const operand& r) { return l.value + "." + r.value; });
    insert("<", order(8), [](const operand& l, const operand& r) { return l.value + " < " + r.value; });
    insert(">", order(8), [](const operand& l, const operand& r) { return l.value + " > " + r.value; });
    insert("<=", order(8), [](const operand& l, const operand& r) { return l.value + " <= " + r.value; });
    insert(">=", order(8), [](const operand& l, const operand& r) { return l.value + " >= " + r.value; });
    insert("==", order(9), [](const operand& l, const operand& r) { return l.value + " == " + r.value; });
    insert("!=", order(9), [](const operand& l, const operand& r) { return l.value + " != " + r.value; });
    insert("&&", order(13), [](const operand &l, const operand &r) { return l.value + " && " + r.value; });
    insert("||", order(14), [](const operand &l, const operand &r) { return l.value + " || " + r.value; });
  }

  void insert(const std::string &name, const order p, const eastl::function<eastl::string(const operand&, const operand&)> &f)
  {
    assert(!name.empty());
    m_ops.insert({name, {p, f}});
  }

  const std::map<std::string, op> &ops() const
  {
    return m_ops;
  }

private:
  std::map<std::string, op> m_ops;
};

using namespace tao::pegtl;

struct comment : if_must<one<'#'>, until<eolf>>
{
};

struct ignored : sor<space, comment>
{
};

struct infix
{
  using analyze_t = analysis::generic<analysis::rule_type::ANY>;

  template <apply_mode,
            rewind_mode,
            template <typename...>
            class Action,
            template <typename...>
            class Control,
            typename Input,
            typename... States>
  static bool match(Input &in, const operators &b, stacks &s, components_t &components, States &&... /*unused*/)
  {
    return match(in, b, s, components, std::string());
  }

private:
  template <typename Input>
  static bool match(Input &in, const operators &b, stacks &s, components_t &components, std::string t)
  {
    if (in.size(t.size() + 1) > t.size())
    {
      t += in.peek_char(t.size());
      const auto i = b.ops().lower_bound(t);
      if (i != b.ops().end())
      {
        if (match(in, b, s, components, t))
        {
          return true;
        }
        if (i->first == t)
        {
          s.push(i->second, components);
          in.bump(t.size());
          return true;
        }
      }
    }
    return false;
  }
};

struct integer : seq<opt<one<'+', '-'>>, plus<digit>>
{
};

struct digits : plus<abnf::DIGIT> {};
struct exp : seq<one<'e', 'E'>, opt<one<'-', '+'>>, must<digits>> {};
struct frac : if_must<one<'.'>, digits> {};
struct int_ : sor<one<'0'>, digits> {};
struct real : seq<opt<one<'-'>>, int_, opt<frac>, opt<exp>> {};

struct true_ : string<'t', 'r', 'u', 'e'> {};
struct false_ : string<'f', 'a', 'l', 's', 'e'> {};

struct boolean : sor<true_, false_>
{
};

struct component : identifier
{
};

struct expression;

struct bracket : if_must<one<'('>, expression, one<')'>>
{
};

struct atomic : sor<real, integer, boolean, component, bracket>
{
};

struct expression : list<atomic, infix, ignored>
{
};

struct grammar : must<expression, eof>
{
};

template <typename Rule>
struct action : pegtl::nothing<Rule>
{
};

template <>
struct action<integer>
{
  template <typename Input>
  static void apply(const Input &in, const operators & /*unused*/, stacks &s, components_t&)
  {
    s.push(in.string().c_str(), operand::Type::INTEGER);
  }
};

template <>
struct action<real>
{
  template <typename Input>
  static void apply(const Input &in, const operators & /*unused*/, stacks &s, components_t&)
  {
    s.push(in.string().c_str(), operand::Type::REAL);
  }
};

template <>
struct action<boolean>
{
  template <typename Input>
  static void apply(const Input &in, const operators & /*unused*/, stacks &s, components_t&)
  {
    s.push(in.string().c_str(), operand::Type::BOOLEAN);
  }
};

template <>
struct action<component>
{
  template <typename Input>
  static void apply(const Input &in, const operators & /*unused*/, stacks &s, components_t &components)
  {
    eastl::string str(in.string().c_str());
    auto res = eastl::find_if(components.cbegin(), components.cend(), [&](const component_desc &c) { return c.name == str; });
    if (res == components.end())
      components.push_back({operand::Type::COMPONENT, str});
    s.push(str, operand::Type::COMPONENT);
  }
};

template <>
struct action<one<'('>>
{
  static void apply0(const operators & /*unused*/, stacks &s, components_t&)
  {
    s.open();
  }
};

template <>
struct action<one<')'>>
{
  static void apply0(const operators & /*unused*/, stacks &s, components_t &components)
  {
    s.close(components);
  }
};

}

eastl::string parse_where_query(const eastl::string &where, QueryComponents &out_components)
{
  if (pegtl::analyze<queryparser::grammar>() != 0)
    return "nullptr";

  queryparser::stacks s;
  queryparser::operators b;
  queryparser::components_t components;

  try
  {
    pegtl::memory_input<> in(where.c_str(), "");
    pegtl::parse<queryparser::grammar, queryparser::action>(in, b, s, components);
  }
  catch (std::runtime_error e)
  {
    std::cout << "Error: " << e.what() << std::endl;
  }

  std::ostringstream oss;

  auto result = s.finish(components);
  for (auto &c : components)
  {
    out_components.emplace_back() = { c.name };

    if (c.type == queryparser::operand::Type::INTEGER)
      c.typeStr = "int";
    else if (c.type == queryparser::operand::Type::REAL)
      c.typeStr = "float";
    else if (c.type == queryparser::operand::Type::BOOLEAN)
      c.typeStr = "bool";
  }

  oss << "\n[](const Archetype &type, int entity_idx";

  oss << ")\n{\n";

  for (const auto &c : components)
  {
    oss << "  GET_COMPONENT_VALUE(" << c.name.c_str() << ", " << c.typeStr.c_str() << ");\n";
  }

  for (const auto &c : components)
    if (c.type == queryparser::operand::Type::COMPONENT)
    {
      // TODO: Generate compare
      assert(false);
      oss << "  static const auto *" << c.name.c_str() << "Desc = find_component(\"" << c.name.c_str() << "\");";
      oss << "\n";
    }

  oss << "  return " << where.c_str() << ";\n";
  oss << "}";

  return oss.str().c_str();
}