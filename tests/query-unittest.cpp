#include <gtest/gtest.h>

#include <ecs/ecs.h>

struct QueryTest : public testing::Test
{
  EntityVector eids;

  void SetUp() override
  {
    JDocument doc;

    JValue empltyValue(rapidjson::kObjectType);
    for (int i = 0; i < 5; ++i)
    {
      eids.emplace_back() = g_mgr->createEntitySync("test-templ-for-queries-1", empltyValue);
      ASSERT_TRUE(eids.back().handle != kInvalidEid);
    }

    for (int i = 0; i < 10; ++i)
    {
      JValue value(rapidjson::kObjectType);
      if (i > 0 && (i % 2) == 0)
      {
        JValue jv(rapidjson::kTrueType);
        value.AddMember("flag", eastl::move(jv), doc.GetAllocator());
      }
      else
      {
        JValue jv(rapidjson::kFalseType);
        value.AddMember("flag", eastl::move(jv), doc.GetAllocator());
      }

      eids.emplace_back() = g_mgr->createEntitySync("test-templ-for-queries-2", value);
      ASSERT_TRUE(eids.back().handle != kInvalidEid);
    }

    for (int i = 0; i < 15; ++i)
    {
      eids.emplace_back() = g_mgr->createEntitySync("test-templ-for-queries-3", empltyValue);
      ASSERT_TRUE(eids.back().handle != kInvalidEid);
    }

    ASSERT_EQ(30ul, eids.size());
    ASSERT_EQ(30ul, g_mgr->entities.size());
  }

  void TearDown() override
  {
    for (const auto &eid : eids)
      g_mgr->deleteEntity(eid);
    g_mgr->tick();
  }
};

#define CHECK_QUERY_COUNT(q, cnt) \
  { \
    Query q; \
    q.desc = desc; \
    g_mgr->performQuery(q); \
    EXPECT_EQ(cnt, q.entitiesCount); \
  } \

TEST_F(QueryTest, Select_All)
{
  QueryDesc desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_comp("eid")->size, find_comp("eid")});

  CHECK_QUERY_COUNT(q, 30ul);
}

TEST_F(QueryTest, Select_Have)
{
  QueryDesc desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_comp("eid")->size, find_comp("eid")});
  desc.haveComponents.push_back({-1, hash::cstr("vel"), find_comp("vec3")->size, find_comp("vec3")});

  CHECK_QUERY_COUNT(q, 5ul);
}

TEST_F(QueryTest, Select_Have_Tag)
{
  QueryDesc desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_comp("eid")->size, find_comp("eid")});
  desc.haveComponents.push_back({-1, hash::cstr("tag-test"), find_comp("tag")->size, find_comp("tag")});

  CHECK_QUERY_COUNT(q, 15ul);
}

TEST_F(QueryTest, Select_Not_Have)
{
  QueryDesc desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_comp("eid")->size, find_comp("eid")});
  desc.notHaveComponents.push_back({-1, hash::cstr("vel"), find_comp("vec3")->size, find_comp("vec3")});

  CHECK_QUERY_COUNT(q, 25ul);
}

TEST_F(QueryTest, Select_Is_True)
{
  QueryDesc desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_comp("eid")->size, find_comp("eid")});
  desc.isTrueComponents.push_back({-1, hash::cstr("flag"), find_comp("bool")->size, find_comp("bool")});

  CHECK_QUERY_COUNT(q, 4ul);
}

TEST_F(QueryTest, Select_Is_False)
{
  QueryDesc desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_comp("eid")->size, find_comp("eid")});
  desc.isFalseComponents.push_back({-1, hash::cstr("flag"), find_comp("bool")->size, find_comp("bool")});

  CHECK_QUERY_COUNT(q, 6ul);
}

TEST_F(QueryTest, Select_Join)
{
  QueryDesc desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_comp("eid")->size, find_comp("eid")});

  CHECK_QUERY_COUNT(q, 30ul);
}