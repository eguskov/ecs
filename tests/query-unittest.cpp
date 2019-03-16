#ifdef _DEBUG
#include <gtest/gtest.h>

#include <ecs/ecs.h>
#include <stages/update.stage.h>

#include <random>

struct QueryTest : public testing::Test
{
  static constexpr int gridCells[] = {8, 6, 4, 4, 4, 1, 7, 8, 6, 6, 1, 9, 8, 2, 2, 2, 1, 2, 3, 9, 4, 5, 5, 4, 1, 9, 8, 4, 10, 10};
  EntityVector eids;

  void SetUp() override
  {
    JDocument doc;

    int gridCellIndex = 0;

    JValue empltyValue(rapidjson::kObjectType);
    for (int i = 0; i < 5; ++i)
    {
      JValue value(rapidjson::kObjectType);
      JValue jv(rapidjson::kNumberType);
      jv.SetInt(gridCells[gridCellIndex++]);
      value.AddMember("grid_cell", eastl::move(jv), doc.GetAllocator());

      eids.emplace_back() = g_mgr->createEntitySync("test-templ-for-queries-1", value);
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

      {
        JValue jv(rapidjson::kNumberType);
        jv.SetInt(gridCells[gridCellIndex++]);
        value.AddMember("grid_cell", eastl::move(jv), doc.GetAllocator());
      }

      eids.emplace_back() = g_mgr->createEntitySync("test-templ-for-queries-2", value);
      ASSERT_TRUE(eids.back().handle != kInvalidEid);
    }

    for (int i = 0; i < 15; ++i)
    {
      JValue value(rapidjson::kObjectType);
      JValue jv(rapidjson::kNumberType);
      jv.SetInt(gridCells[gridCellIndex++]);
      value.AddMember("grid_cell", eastl::move(jv), doc.GetAllocator());

      eids.emplace_back() = g_mgr->createEntitySync("test-templ-for-queries-3", value);
      ASSERT_TRUE(eids.back().handle != kInvalidEid);
    }

    ASSERT_EQ(30ul, eids.size());
    ASSERT_EQ(30ul, g_mgr->entities.size());
  }

  void TearDown() override
  {
    for (const auto &eid : eids)
      g_mgr->deleteEntity(eid);
    eids.clear();
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

struct TestView
{
  int grid_cell;
  float float_comp;
};

struct TestIndexSystem
{
  int entitiesCount = 0;
  int entitiesIterCount = 0;
  int errorsCount = 0;

  template <typename Builder>
  void run(const UpdateStage &stage, int grid_cell, QueryIterable<TestView, Builder> &&items)
  {
    const int count = items.count();
    entitiesCount += count;

    for (TestView item : items)
    {
      ++entitiesIterCount;
      errorsCount += item.grid_cell == grid_cell ? 0 : 1;
    }
  }
};

static constexpr ConstCompDesc TestIndex_components[] = {
  {HASH("grid_cell"), Desc<int>::Size},
  {HASH("float_comp"), Desc<float>::Size},
};
static constexpr ConstQueryDesc TestIndex_query_desc = {
  make_const_array(TestIndex_components),
  empty_desc_array,
  empty_desc_array,
  empty_desc_array,
  empty_desc_array,
  empty_desc_array,
};
RegIndex _reg_index_test(HASH("test_index"), HASH("grid_cell"), TestIndex_query_desc);

TEST_F(QueryTest, Index)
{
  Index *index = g_mgr->getIndexByName(HASH("test_index"));
  g_mgr->rebuildIndex(*index);

  EXPECT_EQ(index->items.size(), 10ul);
  EXPECT_EQ(index->queries.size(), 10ul);

  using TestViewBuilder = StructBuilder<
    StructField<int, INDEX_OF_COMPONENT(TestIndex, grid_cell)>,
    StructField<float, INDEX_OF_COMPONENT(TestIndex, float_comp)>
  >;

  for (int i = 1; i < (int)index->items.size(); ++i)
    EXPECT_TRUE(index->items[i - 1].value < index->items[i].value);

  TestIndexSystem testSystem;
  UpdateStage stage;
  for (const Index::Item &item : index->items)
    testSystem.run(stage, *(int*)(uint8_t*)&item.value, QueryIterable<TestView, TestViewBuilder>(index->queries[item.queryId]));

  EXPECT_EQ(testSystem.entitiesIterCount, 30);
  EXPECT_EQ(testSystem.entitiesCount, 30);
  EXPECT_EQ(testSystem.errorsCount, 0);

  Index::Item item = { -1, 6 };
  auto res = eastl::lower_bound(index->items.begin(), index->items.end(), item);
  EXPECT_TRUE(res != index->items.end() && *res == item);
  if (res != index->items.end() && *res == item)
  {
    TestIndexSystem testSystem;
    UpdateStage stage;
    testSystem.run(stage, *(int*)(uint8_t*)&item.value, QueryIterable<TestView, TestViewBuilder>(index->queries[res->queryId]));

    EXPECT_EQ(testSystem.entitiesIterCount, 3);
    EXPECT_EQ(testSystem.entitiesCount, 3);
    EXPECT_EQ(testSystem.errorsCount, 0);
  }
}

TEST_F(QueryTest, Index_Delete)
{
  Index *index = g_mgr->getIndexByName(HASH("test_index"));

  EXPECT_EQ(gridCells[8], 6);
  g_mgr->deleteEntity(eids[8]);
  g_mgr->tick();

  EXPECT_EQ(index->items.size(), 10ul);
  EXPECT_EQ(index->queries.size(), 10ul);

  using TestViewBuilder = StructBuilder<
    StructField<int, INDEX_OF_COMPONENT(TestIndex, grid_cell)>,
    StructField<float, INDEX_OF_COMPONENT(TestIndex, float_comp)>
  >;

  for (int i = 1; i < (int)index->items.size(); ++i)
    EXPECT_TRUE(index->items[i - 1].value < index->items[i].value);

  TestIndexSystem testSystem;
  UpdateStage stage;
  for (const Index::Item &item : index->items)
    testSystem.run(stage, *(int*)(uint8_t*)&item.value, QueryIterable<TestView, TestViewBuilder>(index->queries[item.queryId]));

  EXPECT_EQ(testSystem.entitiesIterCount, 29);
  EXPECT_EQ(testSystem.entitiesCount, 29);
  EXPECT_EQ(testSystem.errorsCount, 0);

  Index::Item item = { -1, 6 };
  auto res = eastl::lower_bound(index->items.begin(), index->items.end(), item);
  EXPECT_TRUE(res != index->items.end() && *res == item);
  if (res != index->items.end() && *res == item)
  {
    TestIndexSystem testSystem;
    UpdateStage stage;
    testSystem.run(stage, *(int*)(uint8_t*)&item.value, QueryIterable<TestView, TestViewBuilder>(index->queries[res->queryId]));

    EXPECT_EQ(testSystem.entitiesIterCount, 2);
    EXPECT_EQ(testSystem.entitiesCount, 2);
    EXPECT_EQ(testSystem.errorsCount, 0);
  }
}

TEST_F(QueryTest, Index_ChangeDetection)
{
  Index *index = g_mgr->getIndexByName(HASH("test_index"));
  g_mgr->rebuildIndex(*index);

  FrameSnapshot snapshot;
  g_mgr->fillFrameSnapshot(snapshot);

  // call systems
  {
    const auto &e = g_mgr->entities[eids[8].index];
    auto &type = g_mgr->archetypes[e.archetypeId];
    int &gridCell = type.storages[type.getComponentIndex(HASH("grid_cell"))]->getByIndex<int>(e.indexInArchetype);
    gridCell = 5;
  }

  g_mgr->checkFrameSnapshot(snapshot);

  g_mgr->tick();

  EXPECT_EQ(index->items.size(), 10ul);
  EXPECT_EQ(index->queries.size(), 10ul);

  using TestViewBuilder = StructBuilder<
    StructField<int, INDEX_OF_COMPONENT(TestIndex, grid_cell)>,
    StructField<float, INDEX_OF_COMPONENT(TestIndex, float_comp)>
  >;

  for (int i = 1; i < (int)index->items.size(); ++i)
    EXPECT_TRUE(index->items[i - 1].value < index->items[i].value);

  TestIndexSystem testSystem;
  UpdateStage stage;
  for (const Index::Item &item : index->items)
    testSystem.run(stage, *(int*)(uint8_t*)&item.value, QueryIterable<TestView, TestViewBuilder>(index->queries[item.queryId]));

  EXPECT_EQ(testSystem.entitiesIterCount, 30);
  EXPECT_EQ(testSystem.entitiesCount, 30);
  EXPECT_EQ(testSystem.errorsCount, 0);

  Index::Item item = { -1, 6 };
  auto res = eastl::lower_bound(index->items.begin(), index->items.end(), item);
  EXPECT_TRUE(res != index->items.end() && *res == item);
  if (res != index->items.end() && *res == item)
  {
    TestIndexSystem testSystem;
    UpdateStage stage;
    testSystem.run(stage, *(int*)(uint8_t*)&item.value, QueryIterable<TestView, TestViewBuilder>(index->queries[res->queryId]));

    EXPECT_EQ(testSystem.entitiesIterCount, 2);
    EXPECT_EQ(testSystem.entitiesCount, 2);
    EXPECT_EQ(testSystem.errorsCount, 0);
  }
}
#endif