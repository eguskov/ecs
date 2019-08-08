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
    ASSERT_EQ(30ul, g_mgr->entitiesCount);
  }

  void TearDown() override
  {
    for (const auto &eid : eids)
      ecs::delete_entity(eid);
    eids.clear();
    ecs::tick();
  }
};

#define CHECK_QUERY_COUNT(q, cnt) \
  { \
    Query q = ecs::perform_query(desc); \
    EXPECT_EQ(cnt, q.entitiesCount); \
  } \

TEST_F(QueryTest, Select_All)
{
  QueryDescription desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_component("eid")->size, find_component("eid")});

  CHECK_QUERY_COUNT(q, 30ul);
}

TEST_F(QueryTest, Select_Have)
{
  QueryDescription desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_component("eid")->size, find_component("eid")});
  desc.haveComponents.push_back({-1, hash::cstr("vel"), find_component("vec3")->size, find_component("vec3")});

  CHECK_QUERY_COUNT(q, 5ul);
}

TEST_F(QueryTest, Select_Have_Tag)
{
  QueryDescription desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_component("eid")->size, find_component("eid")});
  desc.haveComponents.push_back({-1, hash::cstr("tag-test"), find_component("Tag")->size, find_component("Tag")});

  CHECK_QUERY_COUNT(q, 15ul);
}

TEST_F(QueryTest, Select_Not_Have)
{
  QueryDescription desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_component("eid")->size, find_component("eid")});
  desc.notHaveComponents.push_back({-1, hash::cstr("vel"), find_component("vec3")->size, find_component("vec3")});

  CHECK_QUERY_COUNT(q, 25ul);
}

// TODO: Replace with Filter
// TEST_F(QueryTest, Select_Is_True)
// {
//   QueryDescription desc;
//   desc.components.push_back({-1, hash::cstr("eid"), find_component("eid")->size, find_component("eid")});
//   desc.isTrueComponents.push_back({-1, hash::cstr("flag"), find_component("bool")->size, find_component("bool")});

//   CHECK_QUERY_COUNT(q, 4ul);
// }

// TODO: Replace with Filter
// TEST_F(QueryTest, Select_Is_False)
// {
//   QueryDescription desc;
//   desc.components.push_back({-1, hash::cstr("eid"), find_component("eid")->size, find_component("eid")});
//   desc.isFalseComponents.push_back({-1, hash::cstr("flag"), find_component("bool")->size, find_component("bool")});

//   CHECK_QUERY_COUNT(q, 6ul);
// }

TEST_F(QueryTest, Select_Join)
{
  QueryDescription desc;
  desc.components.push_back({-1, hash::cstr("eid"), find_component("eid")->size, find_component("eid")});

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

static constexpr ConstComponentDescription TestIndex_components[] = {
  {HASH("grid_cell"), ComponentType<int>::Size},
  {HASH("float_comp"), ComponentType<float>::Size},
};
static constexpr ConstQueryDescription TestIndex_query_desc = {
  make_const_array(TestIndex_components),
  empty_desc_array,
  empty_desc_array,
  empty_desc_array,
};
IndexDescription _reg_index_test(HASH("test_index"), HASH("grid_cell"), TestIndex_query_desc);

TEST_F(QueryTest, Index)
{
  Index *index = ecs::find_index(HASH("test_index"));
  g_mgr->rebuildIndex(*index);

  EXPECT_EQ(index->itemsMap.size(), 10ul);
  EXPECT_EQ(index->queries.size(), 10ul);

  using TestViewBuilder = StructBuilder<
    StructField<int, INDEX_OF_COMPONENT(TestIndex, grid_cell)>,
    StructField<float, INDEX_OF_COMPONENT(TestIndex, float_comp)>
  >;

  TestIndexSystem testSystem;
  UpdateStage stage;
  for (const auto &item : index->itemsMap)
    testSystem.run(stage, *(int*)(uint8_t*)&item.first, QueryIterable<TestView, TestViewBuilder>(index->queries[item.second]));

  EXPECT_EQ(testSystem.entitiesIterCount, 30);
  EXPECT_EQ(testSystem.entitiesCount, 30);
  EXPECT_EQ(testSystem.errorsCount, 0);

  auto res = index->itemsMap.find(6);
  EXPECT_TRUE(res != index->itemsMap.end() && res->first == 6);
  if (res != index->itemsMap.end() && res->first == 6)
  {
    TestIndexSystem testSystem;
    UpdateStage stage;
    testSystem.run(stage, *(int*)(uint8_t*)&res->first, QueryIterable<TestView, TestViewBuilder>(index->queries[res->second]));

    EXPECT_EQ(testSystem.entitiesIterCount, 3);
    EXPECT_EQ(testSystem.entitiesCount, 3);
    EXPECT_EQ(testSystem.errorsCount, 0);
  }
}

TEST_F(QueryTest, Index_Delete)
{
  Index *index = ecs::find_index(HASH("test_index"));

  EXPECT_EQ(gridCells[8], 6);
  ecs::delete_entity(eids[8]);
  ecs::tick();

  EXPECT_EQ(index->itemsMap.size(), 10ul);
  EXPECT_EQ(index->queries.size(), 10ul);

  using TestViewBuilder = StructBuilder<
    StructField<int, INDEX_OF_COMPONENT(TestIndex, grid_cell)>,
    StructField<float, INDEX_OF_COMPONENT(TestIndex, float_comp)>
  >;

  TestIndexSystem testSystem;
  UpdateStage stage;
  for (const auto &item : index->itemsMap)
    testSystem.run(stage, *(int*)(uint8_t*)&item.first, QueryIterable<TestView, TestViewBuilder>(index->queries[item.second]));

  EXPECT_EQ(testSystem.entitiesIterCount, 29);
  EXPECT_EQ(testSystem.entitiesCount, 29);
  EXPECT_EQ(testSystem.errorsCount, 0);

  auto res = index->itemsMap.find(6);
  EXPECT_TRUE(res != index->itemsMap.end() && res->first == 6);
  if (res != index->itemsMap.end() && res->first == 6)
  {
    TestIndexSystem testSystem;
    UpdateStage stage;
    testSystem.run(stage, *(int*)(uint8_t*)&res->first, QueryIterable<TestView, TestViewBuilder>(index->queries[res->second]));

    EXPECT_EQ(testSystem.entitiesIterCount, 2);
    EXPECT_EQ(testSystem.entitiesCount, 2);
    EXPECT_EQ(testSystem.errorsCount, 0);
  }
}

TEST_F(QueryTest, Index_ChangeDetection)
{
  Index *index = ecs::find_index(HASH("test_index"));
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

  ecs::tick();

  EXPECT_EQ(index->itemsMap.size(), 10ul);
  EXPECT_EQ(index->queries.size(), 10ul);

  using TestViewBuilder = StructBuilder<
    StructField<int, INDEX_OF_COMPONENT(TestIndex, grid_cell)>,
    StructField<float, INDEX_OF_COMPONENT(TestIndex, float_comp)>
  >;

  TestIndexSystem testSystem;
  UpdateStage stage;
  for (const auto &item : index->itemsMap)
    testSystem.run(stage, *(int*)(uint8_t*)&item.first, QueryIterable<TestView, TestViewBuilder>(index->queries[item.second]));

  EXPECT_EQ(testSystem.entitiesIterCount, 30);
  EXPECT_EQ(testSystem.entitiesCount, 30);
  EXPECT_EQ(testSystem.errorsCount, 0);

  auto res = index->itemsMap.find(6);
  EXPECT_TRUE(res != index->itemsMap.end() && res->first == 6);
  if (res != index->itemsMap.end() && res->first == 6)
  {
    TestIndexSystem testSystem;
    UpdateStage stage;
    testSystem.run(stage, *(int*)(uint8_t*)&res->first, QueryIterable<TestView, TestViewBuilder>(index->queries[res->second]));

    EXPECT_EQ(testSystem.entitiesIterCount, 2);
    EXPECT_EQ(testSystem.entitiesCount, 2);
    EXPECT_EQ(testSystem.errorsCount, 0);
  }
}
#endif