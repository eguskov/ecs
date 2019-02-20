#pragma once

#include <EASTL/string.h>
#include <EASTL/vector.h>

struct QueryComponent
{
  eastl::string name;
};

using QueryComponents = eastl::vector<QueryComponent>;

eastl::string parse_where_query(const eastl::string &where, QueryComponents &out_components);