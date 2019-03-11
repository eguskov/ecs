#pragma once

#include <iostream>

#include <EASTL/vector.h>
#include <EASTL/string.h>

namespace utils
{
  static bool startsWith(const eastl::string &str, const eastl::string &prefix)
  {
    return str.size() >= prefix.size() &&
      str.compare(0, prefix.size(), prefix) == 0;
  }

  static bool endsWith(const eastl::string &str, const eastl::string &suffix)
  {
    return str.size() >= suffix.size() &&
      str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  template<typename T>
  static eastl::string join(const eastl::vector<T> &v, const eastl::string &token) {
    std::ostringstream result;
    for (typename eastl::vector<T>::const_iterator i = v.begin(); i != v.end(); i++) {
      if (i != v.begin())
        result << token;
      result << *i;
    }

    return eastl::string(result.str().c_str());
  }

  template<typename T, typename C>
  static eastl::string join(const eastl::vector<T> &v, C func, const eastl::string &token)
  {
    std::ostringstream result;
    for (typename eastl::vector<T>::const_iterator i = v.begin(); i != v.end(); i++)
    {
      auto s = func(i);
      if (i != v.begin())
        result << token;
      result << s;
    }

    return eastl::string(result.str().c_str());
  }

  template<typename T, typename C>
  static eastl::string join(
    const typename eastl::vector<T>::const_iterator &from,
    const typename eastl::vector<T>::const_iterator &to,
    C func,
    const eastl::string &token)
  {
    std::ostringstream result;
    for (typename eastl::vector<T>::const_iterator i = from; i != to; i++)
    {
      auto s = func(i);
      if (i != from)
        result << token;
      result << s;
    }

    return eastl::string(result.str().c_str());
  }

  static eastl::vector<eastl::string> split(const eastl::string &str, char delim)
  {
    eastl::vector<eastl::string> elems;
    std::stringstream ss(str.c_str());

    std::string item;
    while (std::getline(ss, item, delim))
      elems.emplace_back(item.c_str());

    return elems;
  }
}

inline std::ostream& operator<<(std::ostream &out, const eastl::string &s)
{
  out << s.c_str();
  return out;
}